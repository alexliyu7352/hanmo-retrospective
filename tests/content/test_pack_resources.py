"""用明确合成许可来源fixture核资源闭包与字节token，不冒充生产素材授权。"""
import json
from pathlib import Path
import shutil

import pytest

from tools.content import pack_resources as api
from tools.content.document import DocumentError
from tools.content.io import canonical
from tools.content.packs import _sha
from tools.content.pack_document import PackDocument
from tests.content.test_pack_compose import manifest

ROOT = Path(__file__).resolve().parents[2]


def project_fixture(tmp_path, monkeypatch):
    """以两张真实tinyPNG及测试专属原来源/pin构造独立项目，不改生产清单。"""
    root = tmp_path / 'project'
    fixture = ROOT / 'tests/fixtures/content_packs/compose/new_packs/base'
    game = root / 'game'
    (game / 'data').mkdir(parents=True)
    shutil.copytree(fixture / 'assets', game / 'assets')
    (game / 'data/presentation.json').write_bytes((fixture / 'presentation.json').read_bytes())
    (game / 'data/presentation.schema.json').write_bytes((ROOT / 'game/data/presentation.schema.json').read_bytes())
    rows = [{'path': path.relative_to(root).as_posix(), 'sha256': _sha(path.read_bytes())}
            for path in sorted((game / 'assets/art').glob('*.png'))]
    (game / 'assets/art/manifest.json').write_bytes(canonical({'origin': 'Synthetic project-local test provenance', 'resources': rows}))
    pins = root / api.ORIGINAL_PINS
    pins.parent.mkdir(parents=True)
    pins.write_bytes(canonical({'format': 'HM-FOUNDATION-IDENTITY-BINDINGS', 'format_version': 1, 'resources': rows}))
    monkeypatch.setattr(api, 'ROOT', root)
    return root


def test_query_freezes_source_images_schema_and_original_provenance(tmp_path, monkeypatch):
    """token包含所有实际输入，包中只留相对原来源引用，不复制生成日志正文。"""
    root = project_fixture(tmp_path, monkeypatch)
    record = api.project_snapshot([])
    result = record['result']
    assert result['use'] == 'project-local-authorized-use'
    assert result['runtime_verified'] is False and result['external_distribution_verified'] is False
    assert {row['path'] for row in result['inputs']} == {
        api.ORIGINAL_PINS, 'game/data/presentation.json', 'game/data/presentation.schema.json',
        'game/assets/art/manifest.json', 'game/assets/art/pixel.png', 'game/assets/art/portrait.png'}
    assert len(result['selection']) == 2
    note = json.loads(record['artifacts']['assets/art/project/provenance.json'])
    assert len(note['resources']) == 2
    assert all(row['generation_manifest'] == 'game/assets/art/manifest.json' for row in note['resources'])
    assert str(root) not in json.dumps(note)
    assert b'Synthetic project-local test provenance' not in record['artifacts']['assets/art/project/provenance.json']
    assert all(entry['license'] == api.LOCAL_USE for entry in result['resources'])


def test_provenance_byte_change_changes_token_without_changing_registry(tmp_path, monkeypatch):
    """原来源记录即使只变空白，仍不是用户刚查看的同一资源快照。"""
    root = project_fixture(tmp_path, monkeypatch)
    before = api.project_resources([])
    path = root / 'game/assets/art/manifest.json'
    path.write_bytes(path.read_bytes() + b'\n')
    after = api.project_resources([])
    assert before['project_sha256'] != after['project_sha256']
    assert before['selection'] == after['selection']


def test_unpinned_or_changed_image_is_not_claimed_licensed(tmp_path, monkeypatch):
    """改过像素或字节的旧路径不能借原名称取得项目原创复用许可。"""
    root = project_fixture(tmp_path, monkeypatch)
    path = root / 'game/assets/art/pixel.png'
    path.write_bytes(path.read_bytes() + b'\n')
    with pytest.raises(DocumentError) as caught:
        api.project_resources([])
    assert caught.value.code == 'UNKNOWN_PROJECT_PROVENANCE'


@pytest.mark.parametrize('selection', [[{'path': '/tmp/foreign.png'}], [{'group': 'backgrounds', 'id': 'missing'}], [{'group': 'scripts', 'id': 'anything'}]])
def test_query_accepts_only_current_registered_ids(tmp_path, monkeypatch, selection):
    """资源选择不能传任意源文件、脚本组或未注册ID。"""
    project_fixture(tmp_path, monkeypatch)
    with pytest.raises(DocumentError) as caught:
        api.project_resources(selection)
    assert caught.value.code == 'INVALID_RESOURCE_SELECTION'


def content_document(tmp_path):
    """使用既有内容源创建无表现入口的测试包，导入必须走生产API。"""
    root = tmp_path / 'author-pack'
    root.mkdir()
    (root / 'pack.json').write_bytes(canonical(manifest()))
    (root / 'content.json').write_bytes((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/content.json').read_bytes())
    return PackDocument.open_pack(root / 'pack.json'), root


def test_import_then_save_reopen_and_compose_exact_resources(tmp_path, monkeypatch):
    """真实导入只改声明包，必须保存后才能由正式组合发布带原图摘要的锁。"""
    project = project_fixture(tmp_path, monkeypatch)
    document, root = content_document(tmp_path)
    registry = json.loads((project / 'game/data/presentation.json').read_bytes())
    selection = [{'group': group, 'id': identifier} for group in ('backgrounds', 'portraits') for identifier in registry[group]]
    queried = document.project_resources(selection)
    before = (root / 'pack.json').read_bytes()
    result = document.import_project_resources(selection, queried['project_sha256'], 0)
    assert result['requires_save'] and result['runtime_verified'] is False
    assert (root / 'pack.json').read_bytes() == before
    assert document.status()['dirty'] and not document.status()['import_pending']
    with pytest.raises(DocumentError) as caught:
        document.compose(tmp_path / 'not-saved', result['revision'])
    assert caught.value.code == 'UNSAVED_CHANGES'
    document.save(result['revision'])
    reopened = PackDocument.open_pack(root / 'pack.json')
    manifest_value = reopened.snapshot()['manifest']
    assert manifest_value['presentation_entries'] == [api.PRESENTATION_ENTRY]
    assert {entry['license'] for entry in manifest_value['resources']} == {api.LOCAL_USE}
    for entry in manifest_value['resources']:
        assert _sha((root / entry['path']).read_bytes()) == entry['sha256']
    composed = reopened.compose(tmp_path / 'preview', 0)
    assert composed['runtime_verified'] is False and (tmp_path / 'preview/pack-set.lock.json').is_file()
    token = reopened.project_resources(selection)['project_sha256']
    assert not reopened.import_project_resources(selection, token, 0)['changed']


@pytest.mark.parametrize('relative', ['game/data/presentation.json', 'game/data/presentation.schema.json',
                                     'game/assets/art/pixel.png', 'game/assets/art/manifest.json'])
def test_import_rejects_any_original_changed_after_selection(tmp_path, monkeypatch, relative):
    """PNG、清单、Schema与原来源任意字节变化都必须在首次包写入前拒绝。"""
    project = project_fixture(tmp_path, monkeypatch)
    document, root = content_document(tmp_path)
    token = document.project_resources([])['project_sha256']
    path = project / relative
    path.write_bytes(path.read_bytes() + b'\n')
    with pytest.raises(DocumentError) as caught:
        document.import_project_resources([], token, 0)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert not (root / 'assets').exists() and not document.status()['dirty']


def test_import_refuses_different_existing_file_before_any_write(tmp_path, monkeypatch):
    """同路径不同文件不被覆盖，完整冲突预检不会先写其他资源。"""
    project_fixture(tmp_path, monkeypatch)
    document, root = content_document(tmp_path)
    token = document.project_resources([])['project_sha256']
    target = root / 'assets/art/project/pixel.png'
    target.parent.mkdir(parents=True)
    target.write_bytes(b'existing author bytes')
    with pytest.raises(DocumentError) as caught:
        document.import_project_resources([], token, 0)
    assert caught.value.code == 'IMPORT_DESTINATION_CONFLICT'
    assert target.read_bytes() == b'existing author bytes'
    assert not (root / 'assets/art/project/portrait.png').exists()
    assert not document.status()['dirty']


def test_partial_import_is_reported_and_can_resume_without_overwrite(tmp_path, monkeypatch):
    """逐项失败明确保留已复制项及pending状态，重试补齐后才允许保存声明。"""
    from tools.content import pack_document
    project_fixture(tmp_path, monkeypatch)
    document, root = content_document(tmp_path)
    token = document.project_resources([])['project_sha256']
    writer = pack_document._write_author_bytes
    def fail_second(target, data, **kwargs):
        """第一张真实图完成后只拒第二项，不伪造成功回执。"""
        if target.name == 'portrait.png':
            raise DocumentError('WRITE_FAILED', 'Injected second resource failure')
        return writer(target, data, **kwargs)
    monkeypatch.setattr(pack_document, '_write_author_bytes', fail_second)
    with pytest.raises(DocumentError) as caught:
        document.import_project_resources([], token, 0)
    assert caught.value.code == 'WRITE_FAILED'
    assert caught.value.details['imported'] == ['assets/art/project/pixel.png']
    assert document.status()['import_pending'] and document.status()['dirty']
    assert document.snapshot()['manifest']['presentation_entries'] == []
    with pytest.raises(DocumentError) as caught:
        document.save(document.status()['revision'])
    assert caught.value.code == 'INCOMPLETE_IMPORT'
    monkeypatch.setattr(pack_document, '_write_author_bytes', writer)
    result = document.import_project_resources([], token, document.status()['revision'])
    assert not document.status()['import_pending']
    document.save(result['revision'])
    assert PackDocument.open_pack(root / 'pack.json').snapshot()['manifest']['presentation_entries'] == [api.PRESENTATION_ENTRY]


def test_resource_budget_does_not_relax_author_json_default(tmp_path):
    """资源私有上限允许大于2MiB的原件，普通作者写入仍严格限制默认预算。"""
    from tools.content.document_io import _write_author_bytes
    from tools.content.io import MAX_BYTES
    from tools.content.packs import MAX_RESOURCE_BYTES
    raw = b'original resource bytes' * (MAX_BYTES // 20)
    assert len(raw) > MAX_BYTES
    with pytest.raises(DocumentError) as caught:
        _write_author_bytes(tmp_path / 'author.json', raw)
    assert caught.value.code == 'DOCUMENT_LIMIT' and not (tmp_path / 'author.json').exists()
    _write_author_bytes(tmp_path / 'resource.bin', raw, _max_bytes=MAX_RESOURCE_BYTES)
    assert (tmp_path / 'resource.bin').read_bytes() == raw


def test_protocol_import_uses_same_guarded_pack_facade(tmp_path, monkeypatch):
    """现行协议仅传选择/token和三道写门，没有可自由设置的文件预算或源路径。"""
    from tools.content.editor_protocol import EditorSession
    project_fixture(tmp_path, monkeypatch)
    _document, root = content_document(tmp_path)
    session = EditorSession()
    opened = session.handle({'id': 1, 'op': 'open_pack', 'args': {'path': str(root / 'pack.json')}})
    queried = session.handle({'id': 2, 'op': 'project_resources', 'args': {'selection': []}})
    assert queried['ok']
    request = {'id': 3, 'op': 'import_project_resources', 'args': {'selection': [], 'expected_project_sha256': queried['result']['project_sha256']},
               'document_id': opened['document']['document_id'], 'revision': 0,
               'source_sha256': opened['document']['source_sha256']}
    rejected = session.handle({**request, 'args': {**request['args'], '_max_bytes': 999999999}})
    assert rejected['error']['code'] == 'BAD_ARGUMENTS'
    imported = session.handle(request)
    assert imported['ok'] and imported['document']['dirty'] and not imported['runtime_verified']
