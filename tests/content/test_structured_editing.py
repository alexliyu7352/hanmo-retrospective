"""验证结构化字段新增与可选表首建，仍通过原作者模型执行原子编辑。"""
import copy
import json
from pathlib import Path

import pytest

from tools.content.document import AuthorDocument, DocumentError
from tools.content.editor_protocol import EditorSession
from tools.content.validation import SCHEMA

ROOT = Path(__file__).resolve().parents[2]


def open_document(tmp_path):
    """复制真实作者包到独立目录，测试不得修改已发布的源内容。"""
    path = tmp_path / 'structured.json'
    path.write_bytes((ROOT / 'content/foundation/lived_city.json').read_bytes())
    return AuthorDocument.open(path)


def test_nested_optional_field_undo_save_reopen(tmp_path):
    """现有岗位活动对象可逐层新增，保存与撤销不产生第二份作者模型。"""
    document = open_document(tmp_path)
    document.add_field('roles', 'role.office', 'activities', {})
    before = document.snapshot()
    terms = {'wallet_id': 'wallet.office', 'cost': 0, 'wage': 0,
             'duration': 1, 'hp_gain': 0, 'fatigue_delta': 0}
    document.add_field('roles', 'role.office', 'rest', terms, parent_pointer='/activities')
    assert document.record('roles', 'role.office')['activities']['rest'] == terms
    document.undo()
    assert document.snapshot() == before
    document.redo()
    document.save()
    assert AuthorDocument.open(document.path).snapshot() == document.snapshot()


def test_array_union_parent_uses_matching_schema(tmp_path, monkeypatch):
    """嵌套数组中的分型对象按实际kind选择规则，避免套用另一分支的可选字段。"""
    document = open_document(tmp_path)
    node = copy.deepcopy(SCHEMA['$defs']['node'])
    node['oneOf'][0]['properties']['optional_note'] = {'type': 'string'}
    monkeypatch.setitem(SCHEMA['$defs'], 'node', node)
    dialogue = document.snapshot()['dialogues'][0]
    say_index = next(index for index, value in enumerate(dialogue['nodes']) if value['kind'] == 'say')
    document.add_field('dialogues', dialogue['id'], 'optional_note', '仅当前节点',
                       parent_pointer=f'/nodes/{say_index}')
    assert document.record('dialogues', dialogue['id'])['nodes'][say_index]['optional_note'] == '仅当前节点'
    end_index = next(index for index, value in enumerate(dialogue['nodes']) if value['kind'] == 'end')
    before = document.snapshot()
    with pytest.raises(DocumentError) as error:
        document.add_field('dialogues', dialogue['id'], 'optional_note', '不应写入',
                           parent_pointer=f'/nodes/{end_index}')
    assert error.value.code == 'UNKNOWN_FIELD'
    assert document.snapshot() == before


@pytest.mark.parametrize('pointer,field', [('/missing', 'rest'), ('/services/-1', 'rest'),
                                        ('/services/01', 'rest'), ('/services/0', 'rest'),
                                        ('', 'id'), ('', 'unknown')])
def test_invalid_nested_add_is_atomic(tmp_path, pointer, field):
    """错误路径、身份和未声明字段均不能留下部分编辑或消耗撤销历史。"""
    document = open_document(tmp_path)
    before, status = document.snapshot(), document.status()
    with pytest.raises(DocumentError):
        document.add_field('roles', 'role.office', field, {}, parent_pointer=pointer)
    assert document.snapshot() == before
    assert document.status() == status


def test_nested_add_rejects_overwrite_and_stale_revision(tmp_path):
    """新增入口保留字段覆盖保护，并在任何结构编辑前拒绝陈旧版本。"""
    document = open_document(tmp_path)
    document.add_field('roles', 'role.office', 'activities', {})
    revision = document.revision
    document.add_field('roles', 'role.office', 'rest', {}, parent_pointer='/activities')
    before = document.snapshot()
    for arguments, code in [({'expected_revision': revision}, 'STALE_REVISION'), ({}, 'FIELD_EXISTS')]:
        with pytest.raises(DocumentError) as error:
            document.add_field('roles', 'role.office', 'rest', {}, parent_pointer='/activities', **arguments)
        assert error.value.code == code
        assert document.snapshot() == before


def test_optional_table_first_insert_undo_and_protocol_list(tmp_path, monkeypatch):
    """缺失可选表在首建之前保持原字节；首建后撤销恢复缺失，而非永久写入空表。"""
    monkeypatch.setitem(SCHEMA, 'required', [name for name in SCHEMA['required'] if name != 'events'])
    path = tmp_path / 'optional.json'
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    del pack['events']
    path.write_text(json.dumps(pack, ensure_ascii=False))
    original = path.read_bytes()
    session = EditorSession()
    assert session.handle({'id': 1, 'op': 'open', 'args': {'path': str(path)}})['ok']
    assert session.handle({'id': 2, 'op': 'list', 'args': {'table': 'events'}})['result'] == []
    document = session.document
    assert 'events' not in document.snapshot()
    assert not document.status()['dirty']
    with pytest.raises(DocumentError) as error:
        document.record('events', 'events.new')
    assert error.value.code == 'UNKNOWN_RECORD'
    document.add_record('events', {'id': 'events.new'})
    assert document.record('events', 'events.new') == {'id': 'events.new'}
    document.undo()
    assert 'events' not in document.snapshot()
    assert not document.status()['dirty']
    assert path.read_bytes() == original


def test_nested_field_protocol_preserves_document_guard(tmp_path):
    """新增父路径经原协议传递，错误文档身份不能借新参数绕过防护。"""
    document = open_document(tmp_path)
    session = EditorSession()
    response = session.handle({'id': 1, 'op': 'open', 'args': {'path': str(document.path)}})
    state = response['document']
    request = {'id': 2, 'op': 'add_field', 'document_id': state['document_id'], 'revision': state['revision'],
               'args': {'table': 'roles', 'record_id': 'role.office', 'field': 'activities', 'value': {}}}
    response = session.handle(request)
    assert response['ok']
    request.update(id=3, revision=response['document']['revision'])
    request['args'].update(field='rest', parent_pointer='/activities', value={})
    response = session.handle(request)
    assert response['ok']
    request.update(id=4, revision=response['document']['revision'], document_id='stale')
    assert session.handle(request)['error']['code'] == 'STALE_DOCUMENT'
