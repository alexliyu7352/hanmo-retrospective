"""用真实tiny PNG组合代验证受信安装索引、精确并存与整代失败回滚。"""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import build, content_stage
from tools.content.pack_compose import compose_pack_set

FIXTURE = ROOT / 'tests/fixtures/content_packs/compose/new_packs/base'


def tree_bytes(directory):
    """比较实际整目录字节，包含索引和ownership marker，不只信工具返回摘要。"""
    return {path.relative_to(directory).as_posix(): path.read_bytes()
            for path in directory.rglob('*') if path.is_file()}


def stage_root(path):
    """准备单份真实采购源和两张PNG，不复制或导出完整美术集合。"""
    path.mkdir()
    for name in ('tools', 'schemas', 'game/core'):
        shutil.copytree(ROOT / name, path / name, ignore=shutil.ignore_patterns('__pycache__'))
    data = path / 'game/data'
    data.mkdir(parents=True)
    for name in ('rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json'):
        shutil.copyfile(ROOT / 'game/data' / name, data / name)
    shutil.copyfile(FIXTURE / 'presentation.json', data / 'presentation.json')
    shutil.copytree(FIXTURE / 'assets', path / 'game/assets')
    authors = path / 'content/foundation'
    authors.mkdir(parents=True)
    shutil.copyfile(FIXTURE / 'content.json', authors / 'procurement.json')
    return path


def generation(path, version='1.0.0'):
    """通过公开composer生成确切原代，版本变化自然产生另一个锁身份。"""
    author = path / 'author'
    shutil.copytree(FIXTURE, author)
    manifest = json.loads((author / 'pack.json').read_text())
    manifest['version'] = version
    (author / 'pack.json').write_text(json.dumps(manifest))
    output = path / 'composed'
    result = compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                              'packs': [{'id': 'base', 'version': version}]}, output)
    return output, result['pack_set_digest']


def test_real_compose_stages_exact_files_and_retains_multiple_versions(tmp_path):
    """同namespace两锁各居独立摘要目录，普通下一次stage仍保留完整原字节。"""
    root = stage_root(tmp_path / 'project')
    first, first_sha = generation(tmp_path / 'first')
    second, second_sha = generation(tmp_path / 'second', '2.0.0')
    first_bytes, second_bytes = tree_bytes(first), tree_bytes(second)
    build.stage_content(root, content_sets=[second, first])
    installed = root / 'game/data/generated'
    index = json.loads((installed / content_stage.INSTALLED_INDEX).read_text())
    assert [row['pack_set_digest'] for row in index['sets']] == sorted([first_sha, second_sha])
    for sha, original in [(first_sha, first_bytes), (second_sha, second_bytes)]:
        assert tree_bytes(installed / 'content-sets' / sha) == original
    manifest = content_stage.validate_stage(root, build.discover_author_sources(root))
    assert (installed / content_stage.PACK_SET_SCHEMA).read_bytes() == (root / 'schemas/pack-set.schema.json').read_bytes()
    packaged = content_stage.package_files(root, manifest)
    assert any(row['path'].endswith('/installed-content.json') for row in packaged)
    before = tree_bytes(installed)
    build.stage_content(root)
    assert tree_bytes(installed) == before
    build.stage_content(root, content_sets=[first, second])
    assert tree_bytes(installed) == before
    assert tree_bytes(first) == first_bytes and tree_bytes(second) == second_bytes


@pytest.mark.parametrize('name', ['catalog.hmcatalog.json', 'pack-set.lock.json', 'presentation.json',
                                  'assets/art/packs/base/assets/art/pixel.png'])
def test_tampered_compose_artifacts_keep_previous_stage(tmp_path, name):
    """任一真实输入字节变动都阻止安装，已经发布的完整旧代不变。"""
    root = stage_root(tmp_path / 'project')
    source, _ = generation(tmp_path / 'input')
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output)
    (source / name).write_bytes((source / name).read_bytes() + b' ')
    with pytest.raises(RuntimeError, match='ownership or bytes mismatch'):
        build.stage_content(root, content_sets=[source])
    assert tree_bytes(output) == before


def test_installed_index_rejects_path_substitution_and_changed_bytes(tmp_path):
    """固定digest目录不能被索引改成任意路径，合法目录也必须核实际资源摘要。"""
    source, sha = generation(tmp_path / 'input')
    staged = tmp_path / 'staged'
    staged.mkdir()
    shutil.copyfile(ROOT / 'schemas/pack-set.schema.json', staged / content_stage.PACK_SET_SCHEMA)
    shutil.copytree(source, staged / 'content-sets' / sha)
    entry = content_stage.composed_set_record(source)
    index = {'format': 'HM-INSTALLED-CONTENT', 'format_version': 1, 'sets': [entry]}
    target = staged / content_stage.INSTALLED_INDEX
    target.write_bytes(content_stage.canonical(index))
    content_stage.installed_index_records(staged)
    bad = deepcopy(index)
    bad['sets'][0]['directory'] = '../input/composed'
    target.write_bytes(content_stage.canonical(bad))
    with pytest.raises(ValueError, match='directory or digest'):
        content_stage.installed_index_records(staged)
    target.write_bytes(content_stage.canonical(index))
    (staged / entry['directory'] / 'presentation.json').write_bytes(b'{}')
    with pytest.raises(ValueError, match='ownership or bytes mismatch'):
        content_stage.installed_index_records(staged)


def test_source_changed_after_validation_preserves_previous_stage(tmp_path, monkeypatch):
    """候选代通过回验后输入变化仍在rename前被发现，不能发布过时快照。"""
    root = stage_root(tmp_path / 'project')
    source, _ = generation(tmp_path / 'input')
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output)
    original = content_stage.validate_stage

    def validate_then_change(*args, **kwargs):
        """只在真实校验返回后模拟外部作者修改安装输入。"""
        result = original(*args, **kwargs)
        path = source / 'presentation.json'
        path.write_bytes(path.read_bytes() + b' ')
        return result

    monkeypatch.setattr(content_stage, 'validate_stage', validate_then_change)
    with pytest.raises(RuntimeError, match='inputs changed'):
        build.stage_content(root, content_sets=[source])
    assert tree_bytes(output) == before


def test_publish_failure_restores_old_installed_set(tmp_path, monkeypatch):
    """完整候选代rename失败沿既有回滚恢复，旧索引和资源均保留。"""
    root = stage_root(tmp_path / 'project')
    first, _ = generation(tmp_path / 'first')
    second, _ = generation(tmp_path / 'second', '2.0.0')
    build.stage_content(root, content_sets=[first])
    output = root / 'game/data/generated'
    before = tree_bytes(output)
    original = Path.replace

    def fail_candidate(path, target):
        """只让本次候选发布失败，旧代的恢复rename仍执行真实系统调用。"""
        if path.name.startswith('.content-candidate-'):
            raise OSError('injected publication failure')
        return original(path, target)

    monkeypatch.setattr(Path, 'replace', fail_candidate)
    with pytest.raises(RuntimeError, match='previous stage restored'):
        build.stage_content(root, content_sets=[second])
    assert tree_bytes(output) == before


def test_rewritten_ownership_marker_cannot_change_locked_resource(tmp_path):
    """重算ownership只能描述文件归属，不能替换原pack lock绑定的资源字节。"""
    import hashlib
    source, _ = generation(tmp_path / 'input')
    target = source / 'assets/art/packs/base/assets/art/pixel.png'
    target.write_bytes(target.read_bytes() + b'changed resource')
    marker_path = source / '.pack-generation.json'
    marker = json.loads(marker_path.read_text())
    marker['files'][target.relative_to(source).as_posix()] = hashlib.sha256(target.read_bytes()).hexdigest()
    marker_path.write_bytes(content_stage.canonical(marker))
    with pytest.raises(ValueError, match='resource digest'):
        content_stage.composed_set_record(source)


def test_legacy_resource_pool_requires_exact_trusted_manifest_hash():
    """独立摘要单元探针使用真实适配manifest，不冒充正式legacy组合产物。"""
    import hashlib
    from tools.content.packs import _digest_files
    manifest_raw = (ROOT / 'content/packs/legacy/procurement/pack.json').read_bytes()
    manifest = json.loads(manifest_raw)
    pixels = (FIXTURE / 'assets/art/pixel.png').read_bytes()
    original = {'game/assets/art/pixel.png': pixels}
    legacy = {key: manifest[key] for key in ('id', 'namespace', 'version')}
    legacy.update(manifest_sha256=hashlib.sha256(manifest_raw).hexdigest(),
                  resources_sha256=_digest_files(original, list(original)))
    content_stage._verify_resource_digests({'assets/art/pixel.png': pixels}, [legacy], ROOT)
    forged = deepcopy(legacy)
    forged['manifest_sha256'] = '0' * 64
    with pytest.raises(ValueError, match='resource digest'):
        content_stage._verify_resource_digests({'assets/art/pixel.png': pixels}, [forged], ROOT)
    legacy['namespace'] = 'pretend'
    with pytest.raises(ValueError, match='UntrustedLegacyArtifact'):
        content_stage._verify_resource_digests({'assets/art/pixel.png': pixels}, [legacy], ROOT)


def test_unknown_script_is_not_owned_by_rewritten_compose_marker(tmp_path):
    """重写marker不能把执行文件伪装成可安装资源，即使它已列入自报文件清单。"""
    import hashlib
    source, _ = generation(tmp_path / 'input')
    target = source / 'injected.gd'
    target.write_bytes(b'extends Node\n')
    marker_path = source / '.pack-generation.json'
    marker = json.loads(marker_path.read_text())
    marker['files']['injected.gd'] = hashlib.sha256(target.read_bytes()).hexdigest()
    marker_path.write_bytes(content_stage.canonical(marker))
    with pytest.raises(ValueError, match='Unrecognized composed content file'):
        content_stage.composed_set_record(source)


def test_installed_index_never_traverses_symlinked_set_parent(tmp_path):
    """固定digest本身不是链接也不允许父content-sets改成通往别处的链接。"""
    stage = tmp_path / 'generated'
    stage.mkdir()
    elsewhere = tmp_path / 'other-sets'
    elsewhere.mkdir()
    (stage / 'content-sets').symlink_to(elsewhere, target_is_directory=True)
    with pytest.raises(ValueError, match='must not traverse symlinks'):
        content_stage.installed_index_records(stage)


def add_observed_png_imports(directory, sha):
    """复用真实4.7.2导入原件参数，只将固定源路径和派生MD5重绑定到本次测试锁。"""
    import hashlib
    import re
    for path in (ROOT / 'tests/fixtures/content_packs/installed_import').glob('*.png.import'):
        original = path.read_text()
        old_source = re.search(r'^source_file="([^"]+)"$', original, re.MULTILINE).group(1)
        old_target = re.search(r'^path="([^"]+)"$', original, re.MULTILINE).group(1)
        source_name = path.name.removesuffix('.import')
        relative = 'assets/art/packs/base/assets/art/' + source_name
        resource = 'res://data/generated/content-sets/' + sha + '/' + relative
        target = 'res://.godot/imported/' + source_name + '-' + hashlib.md5(resource.encode()).hexdigest() + '.ctex'
        (directory / relative).with_name(path.name).write_text(original.replace(old_source, resource).replace(old_target, target))


def test_observed_png_import_sidecars_survive_next_stage_without_ownership(tmp_path):
    """真实引擎参数形状可跨下一次stage保留；锁、文件索引和删除所有权不纳派生sidecar。"""
    root = stage_root(tmp_path / 'project')
    source, sha = generation(tmp_path / 'input')
    build.stage_content(root, content_sets=[source])
    generated = root / 'game/data/generated'
    installed = generated / 'content-sets' / sha
    add_observed_png_imports(installed, sha)
    before = tree_bytes(generated)
    manifest = content_stage.validate_stage(root, build.discover_author_sources(root))
    assert not any(row['path'].endswith('.import') for row in manifest['files'])
    build.stage_content(root)
    assert tree_bytes(generated) == before
    assert content_stage.validate_stage(root, build.discover_author_sources(root)) == manifest
    # 安装来源仍须是composer原代，派生元数据只在已受信的installed目录识别。
    with pytest.raises(ValueError, match='ownership or bytes mismatch'):
        content_stage.composed_set_record(installed)


@pytest.mark.parametrize('change', ['source', 'target', 'parameters', 'unknown'])
def test_unrecognized_import_sidecars_are_never_ignored_or_removed(tmp_path, change):
    """相似后缀、错误源/缓存目标或用户改动参数均明确拒绝，原文件原字节保留。"""
    root = stage_root(tmp_path / 'project')
    source, sha = generation(tmp_path / 'input')
    build.stage_content(root, content_sets=[source])
    generated = root / 'game/data/generated'
    source = generated / 'content-sets' / sha
    add_observed_png_imports(source, sha)
    path = source / 'assets/art/packs/base/assets/art/pixel.png.import'
    raw = path.read_bytes()
    if change == 'source':
        path.write_bytes(raw.replace(b'source_file="res://data/generated/', b'source_file="res://elsewhere/'))
    elif change == 'target':
        path.write_bytes(raw.replace(b'res://.godot/imported/', b'res://assets/'))
    elif change == 'parameters':
        path.write_bytes(raw.replace(b'process/size_limit=0', b'process/size_limit=1'))
    else:
        (source / 'user-notes.import').write_text('belongs to user')
    before = tree_bytes(generated)
    with pytest.raises(RuntimeError, match='ownership or bytes mismatch'):
        build.stage_content(root)
    assert tree_bytes(generated) == before
