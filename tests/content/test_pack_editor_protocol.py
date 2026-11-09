"""通过正式本地worker验证声明包协议及旧单文档共存，不替代工坊UI验收。"""
import hashlib
import json
import shutil

from tests.content.test_editor_protocol import worker, open_copy
from tests.content.test_pack_compose import ROOT, manifest, read_generation


def call(w, operation, arguments=None, **overrides):
    """包写请求明确携带最近返回的manifest字节token，不伪造聚合输入摘要。"""
    if w.doc and w.doc.get('kind') == 'pack':
        overrides.setdefault('source_sha256', w.doc['source_sha256'])
    return w.call(operation, arguments, **overrides)


def fixture(tmp_path):
    """只复制已核双PNG普通包，供真实worker读取及组合。"""
    root = tmp_path / 'pack'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base', root)
    return root


def test_worker_create_edit_undo_save_reopen_pack(tmp_path):
    """实际进程管线创建草稿、复用子编辑器撤销重做并保存重开。"""
    root = tmp_path / 'new'
    with worker(tmp_path) as w:
        opened = call(w, 'create_pack', {'directory': str(root), 'manifest': manifest()})
        assert opened['ok'] and opened['document']['kind'] == 'pack'
        assert w.doc['source_sha256'] == hashlib.sha256((root / 'pack.json').read_bytes()).hexdigest()
        edit = {'entry': 'content.json', 'operation': 'add_record',
                'arguments': {'table': 'actors', 'record': {'id': 'actor.draft'}}}
        assert call(w, 'edit', edit)['ok']
        assert call(w, 'edit', {'entry': 'content.json', 'operation': 'undo', 'arguments': {}})['ok']
        assert call(w, 'edit', {'entry': 'content.json', 'operation': 'redo', 'arguments': {}})['ok']
        assert call(w, 'save')['ok']
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        value = call(w, 'snapshot')['result']
        assert value['documents']['content.json']['actors'] == [{'id': 'actor.draft'}]
        failed = call(w, 'compose', {'output': str(tmp_path / 'incomplete')})
        assert not failed['ok'] and failed['error']['code'] != 'INTERNAL_ERROR'
        assert not (tmp_path / 'incomplete').exists()


def test_worker_real_pack_compose_uses_exact_saved_inputs(tmp_path):
    """正式worker调用生产组合，作者未变且产物锁与直接调用完全一致。"""
    from tools.content.pack_compose import compose_pack_set
    root = fixture(tmp_path)
    before = read_generation(root)
    output = tmp_path / 'preview'
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        result = call(w, 'compose', {'output': str(output)})
        assert result['ok'] and result['result']['runtime_verified'] is False
    direct = tmp_path / 'direct'
    compose_pack_set([root], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                             'packs': [{'id': 'base', 'version': '1.0.0'}]}, direct)
    assert read_generation(output) == read_generation(direct)
    assert read_generation(root) == before


def test_worker_switching_preserves_dirty_or_failed_session(tmp_path):
    """打开/创建失败及未确认切换都保留当前包；放弃成功才切换到旧单文档。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        assert call(w, 'set_manifest', {'pointer': '/author', 'value': '内存作者'})['ok']
        token = w.doc['document_id']
        result = call(w, 'open_pack', {'path': str(tmp_path / 'missing/pack.json')})
        assert result['error']['code'] == 'UNSAVED_CHANGES'
        result = call(w, 'open_pack', {'path': str(tmp_path / 'missing/pack.json'), 'discard': True})
        assert not result['ok'] and w.doc['document_id'] == token and w.doc['dirty']
        result = call(w, 'create_pack', {'directory': str(root), 'manifest': manifest(), 'discard': True})
        assert result['error']['code'] == 'DESTINATION_EXISTS' and w.doc['document_id'] == token
        result = call(w, 'open', {'path': str(root / 'content.json'), 'discard': True})
        assert result['ok'] and 'kind' not in w.doc and w.doc['document_id'] != token
        assert w.call('set_text', {'key': 'old.mode', 'value': '仍走原单文档'})['ok']


def test_worker_pack_guards_and_whitelist_cannot_be_bypassed(tmp_path):
    """过期文档、版本、manifest token和任意子操作都不能绕包门面写入。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        edit = {'entry': 'content.json', 'operation': 'set_text', 'arguments': {'key': 'new.text', 'value': '新'}}
        for overrides, code in [({'document_id': 'stale'}, 'STALE_DOCUMENT'),
                                ({'revision': True}, 'STALE_REVISION'),
                                ({'source_sha256': '0' * 64}, 'SOURCE_CHANGED'),
                                ({'source_sha256': None}, 'SOURCE_CHANGED')]:
            rejected = call(w, 'edit', edit, **overrides)
            assert not rejected['ok'] and rejected['error']['code'] == code
        for operation, args, code in [
                ('edit', {**edit, 'operation': '__dict__'}, 'UNKNOWN_OPERATION'),
                ('edit', {**edit, 'arguments': {**edit['arguments'], 'expected_revision': 0}}, 'INVALID_ARGUMENTS'),
                ('set_text', edit['arguments'], 'UNKNOWN_OPERATION'),
                ('save', {'path': str(tmp_path / 'escape.json')}, 'BAD_ARGUMENTS'),
                ('edit', {**edit, 'entry': 'presentation.json'}, 'UNKNOWN_ENTRY'),
                ('edit', {**edit, 'operation': []}, 'BAD_ARGUMENTS')]:
            rejected = call(w, operation, args)
            assert not rejected['ok'] and rejected['error']['code'] == code
        assert not w.doc['dirty'] and w.doc['revision'] == 0
        assert not (tmp_path / 'escape.json').exists()
        assert call(w, 'edit', edit)['ok']
        rejected = call(w, 'edit', edit, revision=0)
        assert rejected['error']['code'] == 'STALE_REVISION'


def test_worker_manifest_token_updates_after_save(tmp_path):
    """manifest保存后的token绑定实际新字节；旧token即使配新revision也失效。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        old = w.doc['source_sha256']
        assert call(w, 'set_manifest', {'pointer': '/author', 'value': '新作者'})['ok']
        assert w.doc['source_sha256'] == old
        assert call(w, 'save')['ok']
        assert w.doc['source_sha256'] == hashlib.sha256((root / 'pack.json').read_bytes()).hexdigest()
        assert w.doc['source_sha256'] != old
        result = call(w, 'save', source_sha256=old)
        assert result['error']['code'] == 'SOURCE_CHANGED' and not w.doc['dirty']


def test_worker_forwards_manifest_and_dependency_errors(tmp_path):
    """包结构与缺依赖的原错误码直接透传，不降成内部错误或成功。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        malformed = call(w, 'set_manifest', {'pointer': '/namespace', 'value': 'base\n'})
        assert malformed['error']['code'] == 'InvalidPackManifest'
        dependency = {'id': 'absent', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}
        assert call(w, 'set_manifest', {'pointer': '/dependencies', 'value': [dependency]})['ok']
        assert call(w, 'save')['ok']
        result = call(w, 'compose', {'output': str(tmp_path / 'preview')})
        assert result['error']['code'] == 'MissingPackDependency'
        assert not (tmp_path / 'preview').exists()


def test_worker_external_change_preserves_existing_generation_and_session(tmp_path):
    """打开后外部改源时worker保留完整旧预览和当前会话，只返回来源冲突。"""
    root = fixture(tmp_path)
    output = tmp_path / 'preview'
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        token = w.doc['document_id']
        assert call(w, 'compose', {'output': str(output)})['ok']
        previous = read_generation(output)
        path = root / 'content.json'
        outside = path.read_bytes() + b'\n'
        path.write_bytes(outside)
        for operation, arguments in [('save', {}), ('compose', {'output': str(output)})]:
            result = call(w, operation, arguments)
            assert result['error']['code'] == 'SOURCE_CHANGED'
            assert w.doc['document_id'] == token
        assert read_generation(output) == previous and path.read_bytes() == outside


def test_worker_preview_refuses_dirty_or_protected_output(tmp_path):
    """预览必须先保存，目标也不能进入作者目录或正式安装目录。"""
    root = fixture(tmp_path)
    before = read_generation(root)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        for destination in (root, root / 'preview', ROOT / 'game/data/generated'):
            result = call(w, 'compose', {'output': str(destination)})
            assert result['error']['code'] == 'ProtectedPackOutput'
        assert call(w, 'edit', {'entry': 'content.json', 'operation': 'set_text',
                                'arguments': {'key': 'new.text', 'value': '未保存'}})['ok']
        result = call(w, 'compose', {'output': str(tmp_path / 'preview')})
        assert result['error']['code'] == 'UNSAVED_CHANGES'
        assert not (tmp_path / 'preview').exists() and read_generation(root) == before


def test_worker_single_author_to_pack_switch_and_describe_contract(tmp_path):
    """旧文档未存时创建包不落文件；描述接口提供唯一包参数表和编辑白名单。"""
    target = tmp_path / 'new'
    with worker(tmp_path) as w:
        open_copy(w, tmp_path)
        assert w.call('set_text', {'key': 'old.text', 'value': '原草稿'})['ok']
        token = w.doc['document_id']
        result = call(w, 'create_pack', {'directory': str(target), 'manifest': manifest()})
        assert result['error']['code'] == 'UNSAVED_CHANGES' and not target.exists()
        result = call(w, 'create_pack', {'directory': str(target), 'manifest': manifest(), 'discard': True}, revision=0)
        assert result['error']['code'] == 'STALE_REVISION' and w.doc['document_id'] == token
        result = call(w, 'create_pack', {'directory': str(target), 'manifest': manifest(), 'discard': True})
        assert result['ok'] and w.doc['kind'] == 'pack'
        described = call(w, 'describe')['result']
        assert described['protocol'] == 'HM-AUTHOR/1'
        assert described['pack_operations']['edit'] == {'required': ['entry', 'operation', 'arguments'], 'optional': []}
        assert described['pack_write_guards'] == ['document_id', 'revision', 'source_sha256']
        assert {'add_record', 'set_value', 'undo', 'redo'} <= set(described['pack_edit_operations'])
        assert 'save' not in described['pack_edit_operations']


def test_worker_preview_only_uses_current_session_composition(tmp_path):
    """真实worker只把当前已核组合交正式准备器，响应不接受任意compose路径。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        arguments = {'destination': str(tmp_path / 'preview'), 'expected_pack_set_digest': '0' * 64}
        assert call(w, 'preview_prepare', arguments)['error']['code'] == 'COMPOSE_REQUIRED'
        composed = call(w, 'compose', {'output': str(tmp_path / 'composed')})
        arguments['expected_pack_set_digest'] = composed['result']['pack_set_digest']
        rejected = call(w, 'preview_prepare', {**arguments, 'composed': '/tmp/foreign'})
        assert rejected['error']['code'] == 'BAD_ARGUMENTS'
        result = call(w, 'preview_prepare', arguments)
        assert result['ok'], result
        assert result['result']['status'] == 'PREVIEW_PREPARED'
        assert result['result']['runtime_verified'] is False
        game = tmp_path / 'preview/game'
        assert game.is_dir() and (game / 'data/generated/installed-content.json').is_file()
        assert result['result']['catalog_path'].startswith('res://data/generated/content-sets/')
        assert result['result']['storage_profile'].startswith('pack-preview-')


def test_worker_preview_refuses_changed_author_or_old_revision_composition(tmp_path):
    """编辑后撤销也须重新组合，外部改源不能借旧锁准备新运行工程。"""
    root = fixture(tmp_path)
    with worker(tmp_path) as w:
        assert call(w, 'open_pack', {'path': str(root / 'pack.json')})['ok']
        composed = call(w, 'compose', {'output': str(tmp_path / 'composed')})
        arguments = {'destination': str(tmp_path / 'preview'), 'expected_pack_set_digest': composed['result']['pack_set_digest']}
        assert call(w, 'edit', {'entry': 'content.json', 'operation': 'set_text', 'arguments': {'key': 'test.new', 'value': '草稿'}})['ok']
        assert call(w, 'preview_prepare', arguments)['error']['code'] == 'UNSAVED_CHANGES'
        assert call(w, 'edit', {'entry': 'content.json', 'operation': 'undo', 'arguments': {}})['ok']
        assert call(w, 'preview_prepare', arguments)['error']['code'] == 'COMPOSE_REQUIRED'
        assert call(w, 'compose', {'output': str(tmp_path / 'composed')})['ok']
        path = root / 'content.json'
        path.write_bytes(path.read_bytes() + b'\n')
        assert call(w, 'preview_prepare', arguments)['error']['code'] == 'SOURCE_CHANGED'
        assert not (tmp_path / 'preview').exists()
