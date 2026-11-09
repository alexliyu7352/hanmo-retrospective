"""点验从显式定义生成初态的作者边界，不调用Factory或随机世界。"""
from copy import deepcopy
import hashlib
import json

import pytest

from tests.content.test_pack_compose import ROOT, manifest, write_pack
from tests.content.test_pack_editor_protocol import call
from tests.content.test_editor_protocol import worker
from tools.content.document import DocumentError
from tools.content.io import canonical
from tools.content.pack_document import PackDocument
from tools.content.scenario_authoring import preview_snapshot


def author_fixture():
    """复用真实双年代作者定义，选择原西城剧本显式活动集。"""
    source = json.loads((ROOT / 'tests/fixtures/content_packs/second_scenario/two_eras.json').read_text())
    scenario = source['scenarios'][0]
    selection = {field: deepcopy(scenario[field]) for field in ('enabled', 'playable_actor_ids', 'start_date')}
    selection['actor_options'] = {ident: {'alive': True, 'available': True, 'copy_initial_goods': False}
                                  for ident in selection['enabled']['actors']}
    selection['role_options'] = {ident: {'enabled': True} for ident in selection['enabled']['roles']}
    return source, selection


def open_author(tmp_path):
    """写独立声明包供正式文档及协议读取，不改现有作者或工坊产物。"""
    source, selection = author_fixture()
    root = write_pack(tmp_path / 'pack', manifest(required_capabilities=source['required_capabilities']), source)
    return PackDocument.open_pack(root / 'pack.json'), root, source, selection


def test_preview_is_read_only_complete_diff_and_explicit_selection():
    """只投影显式人物和资产，并完整暴露被替换的自定义旧初态。"""
    source, selection = author_fixture()
    source['scenarios'][0]['initial']['wallets'][0]['initial_balance'] = 12345
    before = deepcopy(source)
    result = preview_snapshot({'content.json': source}, 'content.json', 'scenario.west', selection)
    assert result['can_apply'] and result['runtime_verified'] is False
    assert source == before and result['proposal']['enabled'] == selection['enabled']
    assert 'city.east' not in result['proposal']['enabled']['settlements']
    assert result['before']['initial']['wallets'][0]['initial_balance'] == 12345
    difference = next(row for row in result['differences'] if row['path'] == '/initial/wallets')
    assert difference['before'][0]['initial_balance'] == 12345
    assert difference['after'] == result['proposal']['initial']['wallets']
    people = result['proposal']['initial']['actors']
    assert all(row['combat'] == {} and row['initial_goods'] == [] for row in people)
    assert len(result['notes']) == len(people) and 'combat={}' in result['notes'][0]['message']
    result['proposal']['initial']['actors'][0]['attributes'].clear()
    assert source == before


def test_missing_source_and_dependency_are_diagnostics_not_guesses():
    """年龄外貌缺项定位到定义，活动依赖未勾选不能被自动补入。"""
    source, selection = author_fixture()
    del source['actors'][0]['initial_age_years']; del source['actors'][0]['appearance_definition']
    result = preview_snapshot({'people.json': source}, 'people.json', 'scenario.west', selection)
    missing = {row['path']: row for row in result['issues'] if row['code'] == 'MISSING_SNAPSHOT_SOURCE'}
    assert missing['/actors/actor.player/initial_age_years']['entry'] == 'people.json'
    assert '/actors/actor.player/appearance_definition' in missing and not result['can_apply']
    source, selection = author_fixture()
    selection['enabled']['places'].remove('west.office')
    result = preview_snapshot({'content.json': source}, 'content.json', 'scenario.west', selection)
    assert not result['can_apply'] and result['issues']
    assert 'west.office' not in result['proposal']['enabled']['places']


def test_explicit_flags_goods_and_required_options():
    """存活可用岗位及初货完全沿明确选择，禁止默认漏掉人物确认。"""
    source, selection = author_fixture()
    source['actors'][0]['initial_goods'] = [{'item_id': 'item.grain', 'quantity': 2}]
    selection['actor_options']['actor.player']['copy_initial_goods'] = True
    selection['actor_options']['actor.deceased'].update(alive=False, available=False)
    selection['role_options']['role.west']['enabled'] = False
    result = preview_snapshot({'content.json': source}, 'content.json', 'scenario.west', selection)
    assert result['can_apply']
    people = {row['actor_id']: row for row in result['proposal']['initial']['actors']}
    assert people['actor.player']['initial_goods'] == source['actors'][0]['initial_goods']
    assert people['actor.deceased']['alive'] is False and people['actor.deceased']['available'] is False
    assert result['proposal']['initial']['roles'][0]['enabled'] is False
    del selection['actor_options']['actor.player']
    with pytest.raises(DocumentError) as caught:
        preview_snapshot({'content.json': source}, 'content.json', 'scenario.west', selection)
    assert caught.value.code == 'INVALID_SNAPSHOT_SELECTION'


def test_apply_is_one_undo_and_source_unchanged_until_save(tmp_path):
    """确认一次提交四组字段，一次撤销完整恢复旧初态，保存才改变原件。"""
    document, root, source, selection = open_author(tmp_path)
    raw = (root / 'content.json').read_bytes()
    preview = document.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    assert document.status()['revision'] == 0 and not document.status()['dirty']
    applied = document.apply_scenario_snapshot(preview['token'], 0)
    assert applied['changed'] and applied['revision'] == 1
    assert document.status()['entries']['content.json']['undo_count'] == 1
    assert (root / 'content.json').read_bytes() == raw
    after = document.snapshot()
    document.edit('content.json', 'undo', {}, 1)
    assert document.snapshot()['documents']['content.json'] == source
    document.edit('content.json', 'redo', {}, 2)
    assert document.snapshot() == after
    document.save(3)
    reopened = PackDocument.open_pack(root / 'pack.json')
    assert reopened.snapshot() == after and not reopened.status()['dirty']


@pytest.mark.parametrize('change', ['revision', 'source', 'selection', 'token'])
def test_stale_or_changed_preview_cannot_replace_state(tmp_path, change):
    """版本、原字节、另一次选择与未知token都不能复用旧确认。"""
    document, root, _source, selection = open_author(tmp_path)
    preview = document.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    token = preview['token']
    if change == 'revision':
        document.edit('content.json', 'set_text', {'key': 'name.scenario.west', 'value': '已修改'}, 0)
    elif change == 'source':
        (root / 'content.json').write_bytes((root / 'content.json').read_bytes() + b'\n')
    elif change == 'selection':
        selection['start_date']['year'] += 1
        document.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    else:
        token = '0' * 64
    before = document.snapshot()
    with pytest.raises(DocumentError) as caught:
        document.apply_scenario_snapshot(token, document.status()['revision'])
    assert caught.value.code in ('SOURCE_CHANGED', 'STALE_SNAPSHOT_PREVIEW')
    assert document.snapshot() == before


def test_invalid_preview_and_payload_bypass_are_refused(tmp_path):
    """存在诊断的候选不得确认，edit白名单不能直接传任意初态payload。"""
    document, _root, _source, selection = open_author(tmp_path)
    selection['playable_actor_ids'] = []
    preview = document.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    assert not preview['can_apply']
    with pytest.raises(DocumentError) as caught:
        document.apply_scenario_snapshot(preview['token'], 0)
    assert caught.value.code == 'INVALID_SCENARIO_SNAPSHOT'
    with pytest.raises(DocumentError) as caught:
        document.edit('content.json', 'set_scenario_snapshot', {'scenario_id': 'scenario.west', 'snapshot': preview['proposal']}, 0)
    assert caught.value.code == 'UNKNOWN_OPERATION' and document.status()['revision'] == 0


def test_worker_preview_apply_guards_and_save(tmp_path):
    """真实worker只接受已预览token及三guard，旧协议编辑与保存路径继续复用。"""
    _document, root, _source, selection = open_author(tmp_path)
    with worker(tmp_path) as process:
        assert call(process, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        result = call(process, 'scenario_snapshot_preview', {'entry': 'content.json', 'scenario_id': 'scenario.west', 'selection': selection})
        assert result['ok'] and result['result']['can_apply']
        token = result['result']['token']
        assert not call(process, 'apply_scenario_snapshot', {'token': token}, source_sha256='bad')['ok']
        extra = call(process, 'apply_scenario_snapshot', {'token': token, 'proposal': result['result']['proposal']})
        assert not extra['ok'] and extra['error']['code'] == 'BAD_ARGUMENTS'
        applied = call(process, 'apply_scenario_snapshot', {'token': token})
        assert applied['ok'] and applied['document']['revision'] == 1
        assert call(process, 'edit', {'entry': 'content.json', 'operation': 'undo', 'arguments': {}})['ok']
        assert call(process, 'edit', {'entry': 'content.json', 'operation': 'redo', 'arguments': {}})['ok']
        assert call(process, 'save')['ok']
    assert canonical(json.loads((root / 'content.json').read_text())) != canonical(_source)


def test_multi_entry_qualified_references_preserve_authored_values(tmp_path):
    """同包限定引用沿注解解析；多入口投影不改普通文字或原定义字节。"""
    source, selection = author_fixture()
    source['actors'][0]['initial_place'] = 'base::west.square'
    people = deepcopy(source)
    for field in people:
        if isinstance(people[field], list) and field != 'actors': people[field] = []
        elif isinstance(people[field], dict): people[field] = {}
    source['actors'] = []
    root = write_pack(tmp_path / 'pack', manifest(content_entries=['content.json', 'people.json']), source)
    (root / 'people.json').write_bytes(canonical(people))
    document = PackDocument.open_pack(root / 'pack.json')
    result = document.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    assert result['can_apply']
    assert result['proposal']['initial']['actors'][0]['place_id'] == 'base::west.square'
    document.apply_scenario_snapshot(result['token'], 0)
    assert not document.status()['entries']['people.json']['dirty']
    assert document.snapshot()['documents']['people.json'] == people


def test_preview_token_binds_resource_bytes_and_schema(tmp_path, monkeypatch):
    """同manifest的可选资源从缺席变原件也改变token；Schema变化拒绝旧确认。"""
    source, selection = author_fixture()
    declaration = manifest(resources=[{'path': 'assets/pinned.json', 'kind': 'data', 'required': False,
                                       'sha256': hashlib.sha256(b'{"original":true}').hexdigest(), 'license': 'project-local-authorized-use'}])
    root = write_pack(tmp_path / 'pack', declaration, source)
    (root / 'assets').mkdir()
    first = PackDocument.open_pack(root / 'pack.json')
    preview = first.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    (root / 'assets/pinned.json').write_bytes(b'{"original":true}')
    second = PackDocument.open_pack(root / 'pack.json')
    assert second.status()['manifest_sha256'] == first.status()['manifest_sha256']
    newer = second.scenario_snapshot_preview('content.json', 'scenario.west', selection)
    assert newer['token'] != preview['token']
    from tools.content import validation
    different = tmp_path / 'changed-schema.json'; different.write_bytes(validation.SCHEMA_BYTES + b'\n')
    monkeypatch.setattr(validation, 'SCHEMA_PATH', different)
    with pytest.raises(DocumentError) as caught:
        second.apply_scenario_snapshot(newer['token'], 0)
    assert caught.value.code == 'SOURCE_CHANGED' and second.status()['revision'] == 0
