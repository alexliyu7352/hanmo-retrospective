"""验证完整内容代际、所有权和发布故障；真实prepare用于测试，不替代Godot运行验收。"""
import hashlib
import json
from pathlib import Path
import subprocess

import pytest

from tests.test_content_discovery import prepare_root

MANIFEST = 'content-stage.json'


def tree_bytes(folder):
    """记录整个目录的相对文件和原字节，目录不存在时返回空集合。"""
    return {p.relative_to(folder).as_posix(): p.read_bytes() for p in folder.rglob('*') if p.is_file()}


def staged(tmp_path, *, tiny_resources=False):
    """真实管线建立完整一代；默认真实美术，工具用例可显式选择小资源。"""
    tool = prepare_root(tmp_path, tiny_resources=tiny_resources)
    tool.stage_content(tmp_path)
    return tool, tmp_path / 'game/data/generated'


def extra_source(root):
    """添加独立测试作者包，避免删除正式入口或改变原作者内容。"""
    source = root / 'content/foundation/removed.json'
    pack = json.loads((source.parent / 'two_stories.json').read_text())
    pack['id'] = 'foundation.removed'
    source.write_text(json.dumps(pack))
    return source


def test_deleted_author_pack_is_absent_from_next_stage(tmp_path):
    """曾由manifest拥有的目录必须随源删除退出下一代，清单不能保留历史包。"""
    tool = prepare_root(tmp_path, tiny_resources=True)
    source = extra_source(tmp_path)
    tool.stage_content(tmp_path)
    generated = tmp_path / 'game/data/generated'
    assert (generated / 'removed.hmcatalog.json').exists()
    source.unlink()
    tool.stage_content(tmp_path)
    assert not (generated / 'removed.hmcatalog.json').exists()
    manifest = json.loads((generated / MANIFEST).read_bytes())
    assert manifest['format'] == 'HM-CONTENT-STAGE/1'
    assert 'removed.hmcatalog.json' not in {row['path'] for row in manifest['files']}


def test_failed_prepare_preserves_previous_complete_stage(tmp_path):
    """前包变化且后包无效时保持全部旧字节，不能先删旧代或只保留manifest。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    before = tree_bytes(generated)
    first = tmp_path / 'content/foundation/alpha_life.json'
    first.write_bytes(first.read_bytes() + b'\n')
    later = tmp_path / 'content/foundation/two_stories.json'
    pack = json.loads(later.read_text()); pack['actors'][0]['initial_place'] = 'nonexistent'
    later.write_text(json.dumps(pack))
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before
    assert not list((tmp_path / 'game/data').glob('.content-*'))
    assert not list((tmp_path / 'build/content').glob('.stage-*'))


def test_stage_never_deletes_unowned_assets_or_source(tmp_path):
    """源、资源、独立编辑器产物和generated用户文件都不是自动清理对象。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    paths = [tmp_path / 'game/assets/custom.txt', tmp_path / 'game/data/user.json',
             tmp_path / 'build/content/editor-preview.hmcatalog.json', generated / 'user/keep.json',
             generated / 'mystery.hmcatalog.json']
    for path in paths:
        path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'unowned')
    before = {p: p.read_bytes() for p in paths + list((tmp_path / 'content').rglob('*.json'))}
    tool.stage_content(tmp_path)
    assert all(p.read_bytes() == data for p, data in before.items())
    manifest = json.loads((generated / MANIFEST).read_bytes())
    assert not {'mystery.hmcatalog.json', 'user/keep.json'} & {r['path'] for r in manifest['files']}


def test_manifest_and_stage_are_independent_of_discovery_order_and_checkout(tmp_path, monkeypatch):
    """逆枚举及另一绝对路径得到逐字相同的整代文件，manifest不记录时钟或临时路径。"""
    left = tmp_path / 'left'; right = tmp_path / 'right'; left.mkdir(); right.mkdir()
    tool = prepare_root(left, tiny_resources=True); prepare_root(right, tiny_resources=True)
    tool.stage_content(left)
    original = tool.discover_author_sources
    monkeypatch.setattr(tool, 'discover_author_sources', lambda root: list(reversed(original(root))))
    tool.stage_content(right)
    assert (left / 'game/data/generated' / MANIFEST).exists()
    assert tree_bytes(left / 'game/data/generated') == tree_bytes(right / 'game/data/generated')


def test_success_without_fresh_prepare_cannot_reuse_old_output(tmp_path, monkeypatch):
    """零退出但未写本次产物也必须失败，不能复用build目录或上一代的成功索引。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    monkeypatch.setattr(tool.subprocess, 'run', lambda *a, **k: subprocess.CompletedProcess(a[0], 0, '', ''))
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


@pytest.mark.parametrize('field', ['source_sha256', 'schema_sha256', 'rule_capabilities_sha256', 'catalog_sha256', 'content_id'])
def test_prepared_digest_or_identity_mismatch_preserves_stage(tmp_path, monkeypatch, field):
    """prepare成功仍需核验身份和摘要；合法JSON的篡改不能以成功退出码放行。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    original = tool.subprocess.run

    def corrupt(command, **kwargs):
        """在真实prepare完成后注入错误字节，模拟边界返回错误产物。"""
        result = original(command, **kwargs)
        target = Path(command[-1]); value = json.loads(target.read_text())
        value[field] = 'wrong' if field == 'content_id' else '0' * 64
        target.write_text(json.dumps(value))
        return result

    monkeypatch.setattr(tool.subprocess, 'run', corrupt)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


def test_source_membership_change_during_prepare_is_rejected(tmp_path, monkeypatch):
    """准备过程中新增作者包必须重新开始，不能发布混合的源集合。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    original = tool.subprocess.run

    def add_source(command, **kwargs):
        """先完成真实编译，再模拟作者追加文件。"""
        result = original(command, **kwargs); extra_source(tmp_path); return result

    monkeypatch.setattr(tool.subprocess, 'run', add_source)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


def test_publication_failure_restores_previous_complete_generation(tmp_path, monkeypatch):
    """备份已建立而候选切换失败时恢复完整旧代，不能只测编译前失败。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    source = tmp_path / 'content/foundation/alpha_life.json'; source.write_bytes(source.read_bytes() + b'\n')
    original = Path.replace
    backed_up = []

    def fail_publish(path, target):
        """真实移动旧目录后仅阻断候选发布，允许回滚恢复。"""
        target = Path(target)
        if path == generated:
            result = original(path, target); backed_up.append(target); return result
        if target == generated and path not in backed_up:
            assert backed_up and not generated.exists()
            raise OSError('injected candidate publication failure')
        return original(path, target)

    monkeypatch.setattr(Path, 'replace', fail_publish)
    with pytest.raises(RuntimeError, match='publication'):
        tool.stage_content(tmp_path)
    assert backed_up, '必须实际越过旧目录备份边界'
    assert tree_bytes(generated) == before
    assert not list(generated.parent.glob('.content-*'))


def test_failed_rollback_retains_recoverable_backup(tmp_path, monkeypatch):
    """恢复也失败时保留最后完整副本并报告具体位置，不能finally清空旧代。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    original = Path.replace; backups = []

    def fail_both(path, target):
        """允许旧目录移出，阻断发布和恢复以检查可恢复副本。"""
        if path == generated:
            result = original(path, target); backups.append(Path(target)); return result
        if Path(target) == generated:
            raise OSError('injected restore failure')
        return original(path, target)

    monkeypatch.setattr(Path, 'replace', fail_both)
    with pytest.raises(RuntimeError) as failure:
        tool.stage_content(tmp_path)
    assert backups and tree_bytes(backups[0]) == before
    assert str(backups[0]) in str(failure.value)


def test_legacy_bootstrap_preserves_unknown_and_rejects_collision(tmp_path):
    """只有与当前产物逐字相同的旧无清单文件可被接管，用户同名文件不可覆盖。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    (generated / MANIFEST).unlink(missing_ok=True)
    mystery = generated / 'mystery.hmcatalog.json'; mystery.write_bytes(b'user content')
    tool.stage_content(tmp_path)
    assert mystery.read_bytes() == b'user content'
    (generated / MANIFEST).unlink(missing_ok=True)
    (generated / 'alpha_life.hmcatalog.json').write_bytes(b'user collision')
    before = tree_bytes(generated)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


def test_manifest_cannot_claim_arbitrary_preexisting_user_file(tmp_path):
    """仅在manifest添路径和摘要不能获得删除权，即便攻击文件名看似catalog。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    manifest_path = generated / MANIFEST
    assert manifest_path.is_file(), '缺少所有权manifest'
    arbitrary = generated / 'personal.hmcatalog.json'; arbitrary.write_bytes(b'{"private":"keep"}')
    manifest = json.loads(manifest_path.read_text())
    manifest['files'].append({'path': arbitrary.name, 'sha256': hashlib.sha256(arbitrary.read_bytes()).hexdigest(),
                              'source': {'path': 'content/foundation/personal.json', 'sha256': '0' * 64}})
    manifest['files'].sort(key=lambda row: row['path'])
    manifest_path.write_text(json.dumps(manifest))
    before = tree_bytes(generated)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


@pytest.mark.parametrize('change', ['source', 'schema', 'capability'])
def test_input_bytes_changed_during_prepare_are_rejected(tmp_path, monkeypatch, change):
    """准备过程中输入变化即使仍是合法JSON，也不得发布跨输入代际的索引。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    path = tmp_path / {'source': 'content/foundation/alpha_life.json', 'schema': 'schemas/content.schema.json',
                       'capability': 'game/data/rule_capabilities.json'}[change]
    original = tool.subprocess.run; changed = False

    def change_input(command, **kwargs):
        """在真实首包产出后改变输入原字节，不伪造prepare执行。"""
        nonlocal changed
        result = original(command, **kwargs)
        if not changed:
            path.write_bytes(path.read_bytes() + b'\n'); changed = True
        return result

    monkeypatch.setattr(tool.subprocess, 'run', change_input)
    with pytest.raises(RuntimeError): tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


def test_unknown_symlink_is_rejected_without_touching_target(tmp_path):
    """生成目录内的链接无论扩展名如何都不可跟随或接管。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    sentinel = tmp_path / 'private'; sentinel.write_bytes(b'keep')
    link = generated / 'linked.json'; link.symlink_to(sentinel)
    before = tree_bytes(generated)
    with pytest.raises(RuntimeError): tool.stage_content(tmp_path)
    assert link.is_symlink() and sentinel.read_bytes() == b'keep'
    assert tree_bytes(generated) == before


def test_author_filename_does_not_need_an_ascii_whitelist(tmp_path):
    """沿用发现任意合法作者包的边界，中文及点号文件名不需要另建白名单。"""
    tool = prepare_root(tmp_path, tiny_resources=True)
    path = extra_source(tmp_path); path.rename(path.with_name('支线.story.json'))
    tool.stage_content(tmp_path)
    assert (tmp_path / 'game/data/generated/支线.story.hmcatalog.json').exists()


def test_late_presentation_reference_preserves_whole_stage(tmp_path):
    """前包已变化但后包引用失效时保留全部旧代，不能只保留一个输出。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    early = tmp_path / 'content/foundation/alpha_life.json'; early.write_bytes(early.read_bytes() + b'\n')
    later = tmp_path / 'content/foundation/two_stories.json'
    pack = json.loads(later.read_text()); pack['actors'][0]['presentation_profile_id'] = 'missing'
    later.write_text(json.dumps(pack))
    with pytest.raises(RuntimeError, match='presentation'):
        tool.stage_content(tmp_path)
    assert tree_bytes(generated) == before


@pytest.mark.parametrize('kind', ['manifest', 'schema', 'resource'])
def test_presentation_input_change_prevents_publication(tmp_path, monkeypatch, kind):
    """真实prepare后修改清单、Schema或资源即使仍有效，也不得发布混合一代。"""
    tool, generated = staged(tmp_path, tiny_resources=True); before = tree_bytes(generated)
    original = tool.subprocess.run; changed = False
    manifest = tmp_path / 'game/data/presentation.json'
    resource = tmp_path / 'game' / json.loads(manifest.read_text())['portraits']['merchant']['path'][6:]
    target = {'manifest': manifest, 'schema': tmp_path / 'game/data/presentation.schema.json', 'resource': resource}[kind]

    def edit_input(command, **kwargs):
        """编译真实内容后模拟同一次暂存期间的作者资源编辑。"""
        nonlocal changed
        result = original(command, **kwargs)
        if not changed:
            target.write_bytes(target.read_bytes() + b'\n'); changed = True
        return result

    monkeypatch.setattr(tool.subprocess, 'run', edit_input)
    with pytest.raises(RuntimeError):
        tool.stage_content(tmp_path)
    assert changed and tree_bytes(generated) == before
