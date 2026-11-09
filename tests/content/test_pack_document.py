"""验证包级版本、子文档复用与真实组合边界，不替代工坊操作验收。"""
import hashlib
import json
from pathlib import Path
import shutil

import pytest

from tests.content.test_pack_compose import manifest, read_generation, ROOT
from tools.content.document import DocumentError
from tools.content.pack_document import PackDocument
from tools.content.pack_errors import PackError


def open_fixture(tmp_path):
    """复制既有双PNG真实输入中的独立base包，保留生产声明及摘要。"""
    root = tmp_path / 'pack'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base', root)
    return PackDocument.open_pack(root / 'pack.json'), root


def test_create_edit_save_reopen_draft(tmp_path):
    """空草稿可由既有编辑器添加记录、撤销重做并保存重开。"""
    document = PackDocument.create_pack(tmp_path / 'new', manifest())
    assert not document.status()['dirty']
    document.edit('content.json', 'add_record', {'table': 'actors', 'record': {'id': 'actor.draft'}}, 0)
    document.edit('content.json', 'undo', {}, 1)
    assert document.snapshot()['documents']['content.json']['actors'] == []
    document.edit('content.json', 'redo', {}, 2)
    result = document.save(3)
    assert result['runtime_verified'] is False and result['revision'] == 4
    reopened = PackDocument.open_pack(tmp_path / 'new/pack.json')
    assert reopened.snapshot()['documents']['content.json']['actors'] == [{'id': 'actor.draft'}]
    with pytest.raises(PackError):
        reopened.compose(tmp_path / 'invalid-output', 0)
    assert not (tmp_path / 'invalid-output').exists()


def test_pack_revision_and_snapshots_cannot_bypass_author_editor(tmp_path):
    """返回对象均为副本；只读、任意方法和过期包版本不能改子文档。"""
    document, _root = open_fixture(tmp_path)
    copied = document.snapshot()
    copied['documents']['content.json']['actors'].clear()
    copied['manifest']['author'] = '外部修改'
    document.status()['entries'].clear()
    assert document.snapshot() != copied
    document.set_manifest('/author', '作者修改', 0)
    with pytest.raises(DocumentError, match='refresh') as caught:
        document.edit('content.json', 'set_text', {'key': 'new.text', 'value': '测试'}, 0)
    assert caught.value.code == 'STALE_REVISION'
    with pytest.raises(DocumentError) as caught:
        document.edit('content.json', 'save', {'path': '/tmp/escape.json'}, 1)
    assert caught.value.code == 'UNKNOWN_OPERATION'
    document.edit('content.json', 'set_text', {'key': 'new.text', 'value': '测试'}, 1)
    assert document.status()['revision'] == 2
    assert document.status()['entries']['content.json']['revision'] == 1


def test_real_compose_matches_official_output(tmp_path):
    """包门面和正式组合产物逐字相同，且组合不修改作者源。"""
    from tools.content.pack_compose import compose_pack_set
    document, root = open_fixture(tmp_path)
    before = read_generation(root)
    output = tmp_path / 'pack-output'
    result = document.compose(output, 0)
    direct = tmp_path / 'direct-output'
    compose_pack_set([root], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                             'packs': [{'id': 'base', 'version': '1.0.0'}]}, direct)
    assert read_generation(output) == read_generation(direct)
    assert read_generation(root) == before
    assert result['runtime_verified'] is False


@pytest.mark.parametrize('relative', ['pack.json', 'content.json', 'presentation.json', 'assets/art/pixel.png'])
def test_external_input_change_blocks_edit_save_and_compose(tmp_path, relative):
    """manifest、内容、表现和原图外改均阻断所有写入口，并保留旧目标。"""
    from tools.content.pack_compose import _publish
    document, root = open_fixture(tmp_path)
    output = tmp_path / 'old'
    _publish(output, {'old.txt': b'previous'}, {})
    old = read_generation(output)
    target = root / relative
    altered = target.read_bytes() + b'\n'
    target.write_bytes(altered)
    for operation in (lambda: document.edit('content.json', 'set_text', {'key': 'new.text', 'value': '新'}, 0),
                      lambda: document.set_manifest('/author', '新作者', 0),
                      lambda: document.save(0), lambda: document.compose(output, 0)):
        with pytest.raises(DocumentError) as caught:
            operation()
        assert caught.value.code == 'SOURCE_CHANGED'
    assert read_generation(output) == old
    assert target.read_bytes() == altered
    assert document.status()['revision'] == 0


def test_optional_missing_resource_appearance_is_source_change(tmp_path):
    """缺省可选资源也是打开快照的一部分，后来出现不能混入旧版本预览。"""
    document, root = open_fixture(tmp_path)
    value = document.snapshot()['manifest']
    value['resources'].append({'path': 'extra.json', 'kind': 'data', 'required': False,
                               'sha256': hashlib.sha256(b'{}').hexdigest(), 'license': 'CC0'})
    (root / 'pack.json').write_text(json.dumps(value))
    document = PackDocument.open_pack(root / 'pack.json')
    (root / 'extra.json').write_bytes(b'{}')
    with pytest.raises(DocumentError) as caught:
        document.compose(tmp_path / 'output', 0)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert not (tmp_path / 'output').exists()


@pytest.mark.parametrize('replace', ['file', 'parent', 'delete'])
def test_source_link_or_deletion_is_never_followed(tmp_path, replace):
    """删除或把入口及父目录改成链接都不能重定向保存及读取。"""
    document, root = open_fixture(tmp_path)
    outside = tmp_path / 'outside'
    if replace == 'parent':
        root.rename(outside)
        root.symlink_to(outside, target_is_directory=True)
    else:
        raw = (root / 'content.json').read_bytes()
        (root / 'content.json').unlink()
        outside.write_bytes(raw)
        if replace == 'file':
            (root / 'content.json').symlink_to(outside)
    with pytest.raises(DocumentError) as caught:
        document.save(0)
    assert caught.value.code == 'SOURCE_CHANGED'


def test_create_refuses_existing_directory_and_missing_original_resources(tmp_path):
    """创建只拥有新目录，不覆盖既有作者，也不猜测表现与资源原件。"""
    existing = tmp_path / 'existing'
    existing.mkdir()
    (existing / 'keep.txt').write_bytes(b'original')
    with pytest.raises(DocumentError) as caught:
        PackDocument.create_pack(existing, manifest())
    assert caught.value.code == 'DESTINATION_EXISTS'
    assert read_generation(existing) == {'keep.txt': b'original'}
    with pytest.raises(DocumentError) as caught:
        PackDocument.create_pack(tmp_path / 'new', manifest(presentation_entries=['presentation.json']))
    assert caught.value.code == 'CREATE_INPUTS_REQUIRED'
    assert not (tmp_path / 'new').exists()


@pytest.mark.parametrize('entry', ['../escape.json', '/tmp/escape.json', 'a/../b.json', 'code.py', 'pack.json', 'compiled.hmcatalog.json'])
def test_create_rejects_bad_entry_paths_before_writing(tmp_path, entry):
    """规范路径与作者文件限制在创建目录前失败。"""
    with pytest.raises((DocumentError, PackError)):
        PackDocument.create_pack(tmp_path / 'new', manifest(content_entries=[entry]))
    assert not (tmp_path / 'new').exists()


def test_open_rejects_legacy_manifest_without_reading_external_mapping(tmp_path, monkeypatch):
    """受信旧包适配不属于作者打开入口，拒绝发生在发现映射之前。"""
    from tools.content import packs
    root = tmp_path / 'legacy'
    root.mkdir()
    value = manifest(content_entries=[], legacy_source='procurement')
    (root / 'pack.json').write_text(json.dumps(value))
    def forbidden_discovery(_roots):
        """若调用发现器则测试失败，证明旧映射尚未被读取。"""
        pytest.fail('Legacy source mapping was consulted')
    monkeypatch.setattr(packs, 'discover_packs', forbidden_discovery)
    with pytest.raises(DocumentError) as caught:
        PackDocument.open_pack(root / 'pack.json')
    assert caught.value.code == 'LEGACY_PACK_READ_ONLY'


def test_set_manifest_checks_complete_schema_without_mutation(tmp_path):
    """完整Schema、规范字符串和固定输入拓扑均在替换内存之前校验。"""
    document, _root = open_fixture(tmp_path)
    original = document.snapshot()
    for pointer, value, code in [('/namespace', 'base\n', 'InvalidPackManifest'),
                                  ('/rules_api/min_version', '2.0.0', 'InvalidVersionRange'),
                                  ('/content_entries', ['another.json'], 'INPUT_LAYOUT_CHANGED'),
                                  ('/author', '\ud800', 'INVALID_JSON')]:
        with pytest.raises((DocumentError, PackError)) as caught:
            document.set_manifest(pointer, value, 0)
        assert caught.value.code == code
    assert document.snapshot() == original and document.status()['revision'] == 0


def test_manifest_save_keeps_original_and_undo_after_content_save_is_dirty(tmp_path):
    """manifest保存留原字节，子文档保存后撤销仍沿原编辑历史标脏。"""
    document, root = open_fixture(tmp_path)
    raw = (root / 'pack.json').read_bytes()
    document.set_manifest('/author', '新作者', 0)
    document.edit('content.json', 'set_text', {'key': 'new.text', 'value': '草稿'}, 1)
    result = document.save(2)
    assert not document.status()['dirty']
    assert (root / '.pack.json.hanmo.previous').read_bytes() == raw
    assert PackDocument.open_pack(root / 'pack.json').snapshot() == document.snapshot()
    document.edit('content.json', 'undo', {}, result['revision'])
    with pytest.raises(DocumentError) as caught:
        document.compose(tmp_path / 'output', document.status()['revision'])
    assert caught.value.code == 'UNSAVED_CHANGES'
    assert document.status()['dirty']


def test_partial_save_reports_successful_entries_and_retains_dirty_failure(tmp_path, monkeypatch):
    """第二文件保存失败时第一项及摘要准确更新，重试能继续且不伪称原子保存。"""
    from tools.content import document_io
    document = PackDocument.create_pack(tmp_path / 'new', manifest(content_entries=['one.json', 'two.json']))
    for entry in ('one.json', 'two.json'):
        document.edit(entry, 'set_text', {'key': 'draft.text', 'value': entry}, document.status()['revision'])
    real_save = document_io.save_document
    def fail_second(child, destination=None):
        """只拒第二个文件，首文件执行真正的作者保存。"""
        if child.path.name == 'two.json':
            raise DocumentError('WRITE_FAILED', 'Simulated second file failure')
        return real_save(child, destination)
    monkeypatch.setattr(document_io, 'save_document', fail_second)
    with pytest.raises(DocumentError) as caught:
        document.save(2)
    assert caught.value.code == 'WRITE_FAILED'
    assert set(caught.value.details['saved_entries']) == {'one.json'}
    status = caught.value.details['pack_status']
    assert status['revision'] == 3
    assert not status['entries']['one.json']['dirty'] and status['entries']['two.json']['dirty']
    assert status['entries']['one.json']['source_sha256'] == hashlib.sha256((tmp_path / 'new/one.json').read_bytes()).hexdigest()
    monkeypatch.setattr(document_io, 'save_document', real_save)
    document.save(3)
    assert not document.status()['dirty']


def test_single_pack_does_not_search_sibling_dependency(tmp_path):
    """邻接目录中即使有依赖包也不自动扫描或补选版本。"""
    document, root = open_fixture(tmp_path)
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/addon', tmp_path / 'addon')
    dependency = {'id': 'addon', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}
    document.set_manifest('/dependencies', [dependency], 0)
    result = document.save(1)
    with pytest.raises(PackError) as caught:
        document.compose(tmp_path / 'output', result['revision'])
    assert caught.value.code == 'MissingPackDependency'
    assert (root / 'pack.json').is_file() and not (tmp_path / 'output').exists()


@pytest.mark.parametrize('location', ['self', 'child', 'parent'])
def test_compose_cannot_write_over_author_directory(tmp_path, location):
    """作者根、其子目录与包含它的父目录都不是合法预览发布目标。"""
    document, root = open_fixture(tmp_path)
    output = {'self': root, 'child': root / 'generated', 'parent': tmp_path}[location]
    before = read_generation(root)
    with pytest.raises(PackError) as caught:
        document.compose(output, 0)
    assert caught.value.code == 'ProtectedPackOutput'
    assert read_generation(root) == before


@pytest.mark.parametrize('restore', [False, True])
def test_change_between_precheck_and_composer_cannot_publish(tmp_path, monkeypatch, restore):
    """首次检查之后被换入的有效内容，即使编译后恢复原件也不能混入本次锁。"""
    from tools.content import pack_compose
    document, root = open_fixture(tmp_path)
    output = tmp_path / 'old'
    pack_compose._publish(output, {'old.txt': b'previous'}, {})
    previous = read_generation(output)
    real_compose = pack_compose.compose_pack_set
    original = (root / 'content.json').read_bytes()
    def changed_before_discovery(roots, selection, target):
        """保留真实生产组合，仅模拟检查与发现间的外部文本变更。"""
        changed = json.loads(original)
        changed['texts']['race.prose'] = '外部编辑'
        (root / 'content.json').write_text(json.dumps(changed))
        result = real_compose(roots, selection, target)
        if restore:
            (root / 'content.json').write_bytes(original)
        return result
    monkeypatch.setattr(pack_compose, 'compose_pack_set', changed_before_discovery)
    with pytest.raises(DocumentError) as caught:
        document.compose(output, 0)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert read_generation(output) == previous


def test_late_change_before_final_publication_preserves_old_generation(tmp_path, monkeypatch):
    """私有代完成后发生的外部修改仍由最终发布快照门阻止。"""
    from tools.content import pack_compose
    document, root = open_fixture(tmp_path)
    output = tmp_path / 'old'
    pack_compose._publish(output, {'old.txt': b'previous'}, {})
    previous = read_generation(output)
    publish = pack_compose._publish
    def change_before_final(target, artifacts, snapshots, validate=None):
        """只在目标发布前改变真实表现文件，内部组合仍完整执行。"""
        if target == output:
            path = root / 'presentation.json'
            path.write_bytes(path.read_bytes() + b'\n')
        return publish(target, artifacts, snapshots, validate)
    monkeypatch.setattr(pack_compose, '_publish', change_before_final)
    with pytest.raises(DocumentError) as caught:
        document.compose(output, 0)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert read_generation(output) == previous


def test_open_rejects_script_and_duplicate_manifest_keys(tmp_path):
    """包内脚本不执行且直接拒绝，重复manifest字段不能后写覆盖身份。"""
    _document, root = open_fixture(tmp_path)
    (root / 'payload.py').write_text('raise RuntimeError("never execute")')
    with pytest.raises(PackError) as caught:
        PackDocument.open_pack(root / 'pack.json')
    assert caught.value.code == 'UnsupportedPackResource'
    (root / 'payload.py').unlink()
    raw = (root / 'pack.json').read_text()
    (root / 'pack.json').write_text(raw.replace('"id": "base"', '"id": "hidden", "id": "base"', 1))
    with pytest.raises(PackError) as caught:
        PackDocument.open_pack(root / 'pack.json')
    assert caught.value.code == 'InvalidPackJson'


def test_noop_and_bool_revision_do_not_advance_history(tmp_path):
    """未改变字段不产生新版本；True不能冒充整数版本一。"""
    document, _root = open_fixture(tmp_path)
    author = document.snapshot()['manifest']['author']
    assert not document.set_manifest('/author', author, 0)['changed']
    document.set_manifest('/author', '新作者', 0)
    with pytest.raises(DocumentError) as caught:
        document.save(True)
    assert caught.value.code == 'STALE_REVISION'
    assert document.status()['revision'] == 1


def test_manifest_write_failure_reports_saved_content_without_claiming_clean(tmp_path, monkeypatch):
    """内容已保存但manifest替换失败时恢复副本和包状态都保留真实边界。"""
    from tools.content import document_io
    document, root = open_fixture(tmp_path)
    original = (root / 'pack.json').read_bytes()
    document.set_manifest('/author', '新作者', 0)
    document.edit('content.json', 'set_text', {'key': 'new.text', 'value': '已保存内容'}, 1)
    real_replace = document_io.os.replace
    def fail_manifest(source, target, **kwargs):
        """只拒manifest最终替换，内容及两类恢复副本按真实流程生成。"""
        if Path(target).name == 'pack.json':
            raise OSError('manifest replace failed')
        return real_replace(source, target, **kwargs)
    monkeypatch.setattr(document_io.os, 'replace', fail_manifest)
    with pytest.raises(DocumentError) as caught:
        document.save(2)
    assert caught.value.code == 'WRITE_FAILED'
    assert caught.value.details['saved_entries']['content.json']['changed']
    assert document.status()['manifest_dirty'] and not document.status()['entries']['content.json']['dirty']
    assert (root / 'pack.json').read_bytes() == original
    assert (root / '.pack.json.hanmo.previous').read_bytes() == original


def test_transient_trusted_rule_bytes_cannot_publish_under_old_snapshot(tmp_path, monkeypatch):
    """内层组合读到临时规则字节后即使原件恢复，也须按产物锁摘要拒绝发布。"""
    from tools.content import pack_compose
    document, _root = open_fixture(tmp_path)
    trusted = tmp_path / 'trusted'
    for relative in ('schemas/content.schema.json', 'game/data/presentation.schema.json',
                     'game/data/rule_capabilities.json', 'game/data/pack_runtime.json'):
        target = trusted / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes((pack_compose.ROOT / relative).read_bytes())
    monkeypatch.setattr(pack_compose, 'ROOT', trusted)
    rule_path = trusted / 'game/data/rule_capabilities.json'
    baseline = rule_path.read_bytes()
    compose = pack_compose.compose_pack_set
    output = tmp_path / 'old'
    pack_compose._publish(output, {'old.txt': b'previous'}, {})
    previous = read_generation(output)
    def temporary_rules(roots, selection, target):
        """只改测试受信副本，执行真实组合后恢复原字节。"""
        rule_path.write_bytes(baseline + b'\n')
        try:
            return compose(roots, selection, target)
        finally:
            rule_path.write_bytes(baseline)
    monkeypatch.setattr(pack_compose, 'compose_pack_set', temporary_rules)
    with pytest.raises(DocumentError) as caught:
        document.compose(output, 0)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert rule_path.read_bytes() == baseline
    assert read_generation(output) == previous


@pytest.mark.parametrize('entry', ['content.json', 'pack.json'])
@pytest.mark.parametrize('moment', ['before_stage', 'inside_open'])
def test_parent_swap_cannot_redirect_author_or_manifest_write(tmp_path, monkeypatch, entry, moment):
    """在旧审查时点及更晚的真正打开时替换父目录，外部目录必须逐字未动。"""
    from tools.content import document_io
    document, root = open_fixture(tmp_path)
    outside, moved = tmp_path / 'outside', tmp_path / 'original-root'
    shutil.copytree(root, outside)
    original = read_generation(root)
    if entry == 'content.json':
        document.edit(entry, 'set_text', {'key': 'race.text', 'value': '不得写到外部'}, 0)
    else:
        document.set_manifest('/author', '不得写到外部', 0)
    swapped = False
    def swap_parent():
        """只移动测试作者目录并换链接，模拟外部进程改变路径绑定。"""
        nonlocal swapped
        root.rename(moved)
        root.symlink_to(outside, target_is_directory=True)
        swapped = True
    if moment == 'before_stage':
        real_stage = document_io._stage
        def swap_before_stage(path, data, mode, **kwargs):
            """复现原P1：进入实际暂存前才替换父目录。"""
            if path.name == entry and not swapped:
                swap_parent()
            return real_stage(path, data, mode, **kwargs)
        monkeypatch.setattr(document_io, '_stage', swap_before_stage)
    else:
        real_open = document_io.os.open
        def swap_inside_open(path, flags, *args, **kwargs):
            """绕过最后一次路径检查的时间点，真实open仍必须使用原目录FD。"""
            if str(path).startswith('.author-') and not swapped:
                swap_parent()
            return real_open(path, flags, *args, **kwargs)
        monkeypatch.setattr(document_io.os, 'open', swap_inside_open)
    with pytest.raises(DocumentError) as caught:
        document.save(1)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert swapped and document.status()['dirty']
    assert read_generation(outside) == original
    assert read_generation(moved) == original
    assert not any(result['changed'] for result in caught.value.details['saved_entries'].values())


def test_swap_at_replace_keeps_external_bytes_and_recoverable_owned_source(tmp_path, monkeypatch):
    """最后检查之后替换目录仍只写原持有目录，失败不误报saved或更新摘要。"""
    from tools.content import document_io
    document, root = open_fixture(tmp_path)
    outside, moved = tmp_path / 'outside', tmp_path / 'original-root'
    shutil.copytree(root, outside)
    original = read_generation(root)
    document.edit('content.json', 'set_text', {'key': 'race.text', 'value': '只写原持有目录'}, 0)
    replace = document_io.os.replace
    def swap_at_replace(source, target, **kwargs):
        """在实际系统调用之前才换路径，检验replace本身的FD约束。"""
        if target == 'content.json':
            root.rename(moved)
            root.symlink_to(outside, target_is_directory=True)
        return replace(source, target, **kwargs)
    monkeypatch.setattr(document_io.os, 'replace', swap_at_replace)
    with pytest.raises(DocumentError) as caught:
        document.save(1)
    assert caught.value.code == 'SOURCE_CHANGED'
    assert caught.value.details['saved_entries'] == {}
    assert document.status()['dirty']
    assert document.status()['entries']['content.json']['source_sha256'] == hashlib.sha256(original['content.json']).hexdigest()
    assert read_generation(outside) == original
    assert (moved / '.content.json.hanmo.previous').read_bytes() == original['content.json']
    assert json.loads((moved / 'content.json').read_bytes())['texts']['race.text'] == '只写原持有目录'


def test_save_as_parent_swap_cannot_create_external_file(tmp_path, monkeypatch):
    """另存的最终link之前换父目录，排他创建仍只作用于原持有目录。"""
    from tools.content import document_io
    from tools.content.document import AuthorDocument
    _pack, root = open_fixture(tmp_path)
    document = AuthorDocument.open(root / 'content.json')
    original_path = document.path
    document.set_text('race.text', '另存草稿')
    destination = tmp_path / 'destination'
    outside, moved = tmp_path / 'outside', tmp_path / 'moved-destination'
    destination.mkdir()
    outside.mkdir()
    link = document_io.os.link
    def swap_before_link(source, target, **kwargs):
        """在目录检查后、link系统调用前改变另存目录路径。"""
        destination.rename(moved)
        destination.symlink_to(outside, target_is_directory=True)
        return link(source, target, **kwargs)
    monkeypatch.setattr(document_io.os, 'link', swap_before_link)
    with pytest.raises(DocumentError) as caught:
        document.save(destination / 'new.json')
    assert caught.value.code == 'SOURCE_CHANGED'
    assert list(outside.iterdir()) == []
    assert document.path == original_path and document.status()['dirty']
    assert json.loads((moved / 'new.json').read_bytes())['texts']['race.text'] == '另存草稿'


def test_create_parent_swap_cannot_leave_files_in_external_directory(tmp_path, monkeypatch):
    """新包创建期间换根链接时，共享FD写原语仍不向外部目录创建草稿。"""
    from tools.content import document_io
    target, outside, moved = tmp_path / 'new', tmp_path / 'outside', tmp_path / 'moved'
    outside.mkdir()
    stage = document_io._stage
    def swap_before_creation(path, data, mode, **kwargs):
        """首个创建暂存前替换测试新目录，检验共享写原语。"""
        target.rename(moved)
        target.symlink_to(outside, target_is_directory=True)
        return stage(path, data, mode, **kwargs)
    monkeypatch.setattr(document_io, '_stage', swap_before_creation)
    with pytest.raises(DocumentError) as caught:
        PackDocument.create_pack(target, manifest())
    assert caught.value.code == 'SOURCE_CHANGED'
    assert list(outside.iterdir()) == [] and list(moved.iterdir()) == []


def test_parent_traversal_cannot_change_the_reported_save_destination(tmp_path):
    """拒绝含链接加上层组件的目录，不在规范化时改写实际保存目标。"""
    from tools.content.document import AuthorDocument
    _pack, root = open_fixture(tmp_path)
    document = AuthorDocument.open(root / 'content.json')
    original_path = document.path
    document.set_text('race.text', '不改写目标语义')
    local, outside = tmp_path / 'local', tmp_path / 'outside'
    local.mkdir()
    (outside / 'child').mkdir(parents=True)
    (local / 'alias').symlink_to(outside / 'child', target_is_directory=True)
    with pytest.raises(DocumentError) as caught:
        document.save(local / 'alias/../draft.json')
    assert caught.value.code == 'INVALID_DESTINATION'
    assert not (local / 'draft.json').exists() and not (outside / 'draft.json').exists()
    assert document.path == original_path and document.status()['dirty']
    with pytest.raises(DocumentError) as caught:
        PackDocument.create_pack(local / 'alias/../new', manifest())
    assert caught.value.code == 'INVALID_DESTINATION' and not (outside / 'new').exists()
