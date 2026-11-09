"""局部引用沿原作者文档、撤销和校验工作，不以字符串相等冒充正确作用域。"""
from copy import deepcopy
import json

import pytest

from tests.test_military_content import military_pack
from tools.content.document import AuthorDocument, DocumentError
from tools.content.editor_protocol import EditorSession
from tools.content.references import local_reference_issues, local_targets, iter_references


def local_pack():
    """复用军事两配置夹具，增加中文局部名称和真实第二部队，源 Alpha 文件不改动。"""
    pack = military_pack()
    for number, topology in enumerate(pack['siege_topologies']):
        key = 'name.local.topology.' + str(number)
        pack['texts'][key] = '西城城防' if number == 0 else '东城城防'
        topology['name_key'] = key
        for node in topology['nodes']:
            key = 'name.local.node.' + node['kind']
            pack['texts'][key] = {'approach': '城外道路', 'gate': '正门', 'side': '侧门', 'wall': '城墙', 'inner': '内城', 'supply': '补给口'}[node['kind']]
            node['name_key'] = key
    second = pack['siege_topologies'][1]
    second['nodes'][-1]['id'] = 'east_supply'
    second['supply_node_ids'] = ['east_supply']
    for edge in second['edges']:
        for field in ('from', 'to'):
            if edge[field] == 'supply': edge[field] = 'east_supply'
    camp = pack['garrisons'][1]
    actor = deepcopy(next(a for a in pack['actors'] if a['id'] == camp['units'][0]['commander_actor_id']))
    actor.pop('military_plan'); actor['id'] = 'actor.local.second_commander'
    pack['actors'].append(actor)
    unit = deepcopy(camp['units'][0]); unit.update(id='unit.local.second', commander_actor_id=actor['id'])
    camp['units'].append(unit)
    for number, camp in enumerate(pack['garrisons']):
        key = 'name.local.camp.' + str(number); pack['texts'][key] = '西营' if number == 0 else '东营'; camp['name_key'] = key
        for offset, unit in enumerate(camp['units']):
            key = 'name.local.unit.' + str(number) + '.' + str(offset)
            pack['texts'][key] = '前部' if offset == 0 else '后部'; unit['name_key'] = key
    return pack


def opened(tmp_path):
    """在独立位置创建真实作者文件，所有测试通过原文档接口修改。"""
    source = tmp_path / 'local-references.json'
    source.write_text(json.dumps(local_pack(), ensure_ascii=False), encoding='utf-8')
    return AuthorDocument.open(source)


def test_scopes_and_empty_union_are_not_global_text_matches(tmp_path):
    """两个城防同名节点各归本记录，跨驻军引用则枚举所有唯一部队；明确空阻挡合法。"""
    doc = opened(tmp_path); pack = doc.snapshot()
    assert {r['id'] for r in local_targets(pack, pack['siege_topologies'][0], 'nodes')} == {'approach', 'gate', 'wall', 'side', 'inner', 'supply'}
    assert {r['id'] for r in local_targets(pack, pack['siege_topologies'][1], 'nodes')} == {'approach', 'gate', 'wall', 'side', 'inner', 'east_supply'}
    assert len(local_targets(pack, {}, 'garrisons[*].units')) == 3
    assert local_reference_issues(pack, 'source.json') == []
    doc.set_value('siege_topologies', 'topology.military.a', '/edges/1/blocker_id', '')
    assert local_reference_issues(doc.snapshot(), 'source.json') == []
    assert not any(path[-1] in ('blocker_id', 'unit_ids') for path, _, _ in iter_references(pack))


def test_diagnostic_pointer_history_save_reopen(tmp_path):
    """删改目标后引用字段精确报错，撤销恢复目标与引用，保存重开保留同一记录身份。"""
    doc = opened(tmp_path); before = doc.snapshot()
    doc.set_value('siege_topologies', 'topology.military.a', '/nodes/1/id', 'renamed_gate')
    report = doc.validate()
    issue = next(row for row in report['issues'] if row['code'] == 'LOCAL_REFERENCE_MISSING' and row['path'].endswith('/edges/1/blocker_id'))
    assert issue['record_id'] == 'topology.military.a'
    assert issue['path'] == str(doc.path) + '#/siege_topologies/0/edges/1/blocker_id'
    assert report['status'] == 'INVALID_CONTENT'
    state = doc.status(); doc.validate(); assert doc.status() == state
    doc.undo(); assert doc.snapshot() == before
    doc.redo(); doc.save()
    assert AuthorDocument.open(doc.path).snapshot() == doc.snapshot()


def test_missing_and_ambiguous_cross_garrison_units(tmp_path):
    """被引用部队重名或删除须定位人物计划数组项，不能任选另一营同名部队。"""
    doc = opened(tmp_path)
    doc.set_value('garrisons', 'garrison.military.b', '/units/1/id', 'unit.military.a')
    issues = [row for row in doc.validate()['issues'] if row['code'] == 'LOCAL_REFERENCE_AMBIGUOUS']
    assert any(row['record_id'] == 'actor.military.a.commander' and row['path'].endswith('/military_plan/unit_ids/0') for row in issues)
    doc.undo()
    doc.set_value('garrisons', 'garrison.military.a', '/units', [])
    assert any(row['code'] == 'LOCAL_REFERENCE_MISSING' and row['record_id'] == 'actor.military.a.commander' for row in doc.validate()['issues'])


def test_half_edited_arrays_and_scope_errors_are_diagnostics(tmp_path, monkeypatch):
    """半成品集合不令校验进程异常，坏元数据作用域明确报错而非当作空合法引用。"""
    doc = opened(tmp_path); pack = doc.snapshot()
    doc.set_value('siege_topologies', 'topology.military.a', '/nodes', 3)
    assert any(row['code'] == 'LOCAL_REFERENCE_MISSING' for row in doc.validate()['issues'])
    from tools.content.validation import SCHEMA
    fragment = SCHEMA['$defs']['siege_topology']['properties']['attacker_entry_id']
    monkeypatch.setitem(fragment, 'x-hanmo-local-ref', '../foreign')
    assert any(row['code'] == 'LOCAL_REFERENCE_SCOPE' for row in local_reference_issues(pack, 'draft'))


def test_protocol_keeps_document_revision_for_local_edit(tmp_path):
    """引用选择仍用原 set_value 协议；陈旧版本不能跨文档写入，语义读取不消耗版本。"""
    doc = opened(tmp_path); session = EditorSession()
    response = session.handle({'id': 1, 'op': 'open', 'args': {'path': str(doc.path)}})
    request = {'id': 2, 'op': 'set_value', 'document_id': response['document']['document_id'], 'revision': 0,
               'args': {'table': 'siege_topologies', 'record_id': 'topology.military.a', 'pointer': '/edges/1/blocker_id', 'value': ''}}
    response = session.handle(request); assert response['ok']
    request['id'] = 3
    assert session.handle(request)['error']['code'] == 'STALE_REVISION'
    revision = session.document.revision
    assert session.handle({'id': 4, 'op': 'validate', 'args': {}})['ok']
    assert session.document.revision == revision
