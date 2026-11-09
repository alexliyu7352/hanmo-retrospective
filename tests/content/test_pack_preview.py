"""真实tiny compose代的独立试玩准备边界；不启动引擎、不伪造锁或安装索引。"""
from pathlib import Path
import hashlib
import json
import shutil
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import content_stage as staging
from tools.content import pack_preview as preview
from tools.content.pack_compose import compose_pack_set
from tools.content.pack_errors import PackError
from tools.presentation import validate_registry


@pytest.fixture(scope='module')
def real_composed(tmp_path_factory):
    """公开composer实际生成两PNG测试包，后续所有测试复用其真实身份。"""
    base = tmp_path_factory.mktemp('preview-composer')
    author = base / 'author'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base', author)
    output = base / 'composed'
    result = compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                              'packs': [{'id': 'base', 'version': '1.0.0'}]}, output)
    return output, result['pack_set_digest'], author


def hashes(directory):
    """摘要全部普通文件，用于证明原输入、已有目标和实际输出没有被隐式替换。"""
    return {path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in directory.rglob('*') if path.is_file()}


def copied_composed(tmp_path, real_composed):
    """反例只改独立源副本，模块级真实composer原代永久保持只读。"""
    path = tmp_path / 'composed'
    shutil.copytree(real_composed[0], path)
    return path


def test_prepares_real_exact_installed_runtime_and_ready_global_title(tmp_path, real_composed):
    """返回工程、所有同源合同与全局标题资源可逐字复核，仍明确未运行游戏。"""
    composed, digest, author = real_composed
    source_before, author_before = hashes(composed), hashes(author)
    destination = tmp_path / 'preview'
    result = preview.prepare_preview(composed, destination, expected_pack_set_digest=digest)
    assert set(result) == {'status', 'runtime_verified', 'game_path', 'catalog_path',
                           'pack_set_digest', 'storage_profile', 'source_manifest_sha256'}
    assert result['status'] == 'PREVIEW_PREPARED' and result['runtime_verified'] is False
    game = Path(result['game_path'])
    assert game == destination / 'game'
    assert result['catalog_path'] == 'res://data/generated/content-sets/' + digest + '/catalog.hmcatalog.json'
    assert result['pack_set_digest'] == digest
    installed = game / 'data/generated'
    staging.installed_index_records(installed)
    assert hashes(installed / 'content-sets' / digest) == source_before
    for path in preview.RUNTIME_FILES:
        assert (destination / path).read_bytes() == (ROOT / path).read_bytes()
    for group in preview.RUNTIME_TREES:
        assert {p.relative_to(ROOT).as_posix() for p in (ROOT / group).rglob('*.gd')} == {
            p.relative_to(destination).as_posix() for p in (destination / group).rglob('*.gd')}
    for file in ['content.schema.json', 'pack-set.schema.json']:
        assert (installed / file).read_bytes() == (ROOT / 'schemas' / file).read_bytes()
    staging.schema_index_records(installed)
    assert (game / 'data/presentation.json').read_bytes() == (composed / 'presentation.json').read_bytes()
    presentation = json.loads((game / 'data/presentation.json').read_text())
    assert presentation['default_background'] == 'base::city'
    assert validate_registry(presentation, game) == []
    for name in source_before:
        if name.startswith('assets/'):
            assert (game / name).read_bytes() == (composed / name).read_bytes()
    project = (game / 'project.godot').read_text()
    assert 'config/use_custom_user_dir=true' in project
    assert 'hanmo-preview/' + result['storage_profile'] in project
    assert result['catalog_path'] in project and '[editor_plugins]' not in project
    assert 'player-alpha' not in project and 'hanmo-f05' not in project
    source_manifest = destination / preview.SOURCE_MANIFEST
    assert hashlib.sha256(source_manifest.read_bytes()).hexdigest() == result['source_manifest_sha256']
    manifest = json.loads(source_manifest.read_text())
    assert manifest['composed'] == source_before
    for name, sha in manifest['runtime'].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == sha
    outputs = json.loads((destination / preview.OUTPUT_MANIFEST).read_text())['files']
    assert hashes(destination) == {**outputs, preview.OUTPUT_MANIFEST: staging.digest(destination / preview.OUTPUT_MANIFEST)}
    assert hashes(composed) == source_before and hashes(author) == author_before


def test_repeated_preview_uses_distinct_profiles_and_keeps_first(tmp_path, real_composed):
    """同一包连续准备不同新目录，每次profile和user目录独立，第一份原字节保留。"""
    composed, digest, _ = real_composed
    first = tmp_path / 'first'
    one = preview.prepare_preview(composed, first, expected_pack_set_digest=digest)
    before = hashes(first)
    two = preview.prepare_preview(composed, tmp_path / 'second', expected_pack_set_digest=digest)
    assert one['storage_profile'] != two['storage_profile']
    assert one['source_manifest_sha256'] == two['source_manifest_sha256']
    assert hashes(first) == before


@pytest.mark.parametrize('digest, code', [('0' * 64, 'PreviewDigestMismatch'), ('bad', 'InvalidPreviewDigest')])
def test_expected_digest_is_exact_and_fails_before_publication(tmp_path, real_composed, digest, code):
    """调用方期待身份缺失或不符立即失败，不把另一compose代冒作成功。"""
    destination = tmp_path / 'preview'
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], destination, expected_pack_set_digest=digest)
    assert error.value.code == code and not destination.exists()


@pytest.mark.parametrize('file', ['catalog.hmcatalog.json', 'presentation.json', 'pack-set.lock.json',
                                  'assets/art/packs/base/assets/art/pixel.png'])
def test_changed_input_bytes_preserve_existing_directories(tmp_path, real_composed, file):
    """锁/目录/表现/PNG任一原字节变化都阻断，失败不会回写源或碰已有试玩。"""
    source = copied_composed(tmp_path, real_composed)
    (source / file).write_bytes((source / file).read_bytes() + b' ')
    before = hashes(source)
    old = tmp_path / 'old-preview'; old.mkdir(); (old / 'mine').write_bytes(b'keep')
    with pytest.raises(PackError):
        preview.prepare_preview(source, tmp_path / 'new-preview', expected_pack_set_digest=real_composed[1])
    assert not (tmp_path / 'new-preview').exists() and hashes(source) == before
    assert (old / 'mine').read_bytes() == b'keep'
    assert not list(tmp_path.glob('.pack-preview-*'))


def test_existing_destination_never_becomes_owned(tmp_path, real_composed):
    """已有完整或空目标均不接管；失败不得删改其中任何文件。"""
    for name in ['empty', 'occupied']:
        destination = tmp_path / name; destination.mkdir()
        if name == 'occupied': (destination / 'mine').write_bytes(b'keep')
        before = hashes(destination)
        with pytest.raises(PackError, match='全新目录'):
            preview.prepare_preview(real_composed[0], destination, expected_pack_set_digest=real_composed[1])
        assert destination.is_dir() and hashes(destination) == before


def test_destinations_cannot_overlap_author_compose_or_game(tmp_path, real_composed):
    """拒绝包作者、原compose与正式源码内部的新目录，不因当前路径为空就取得权限。"""
    composed, digest, author = real_composed
    for destination in [composed / 'preview', author / 'preview', ROOT / 'game/preview', ROOT / 'tools/preview']:
        with pytest.raises(PackError) as error:
            preview.prepare_preview(composed, destination, expected_pack_set_digest=digest)
        assert error.value.code == 'ProtectedPreviewPath' and not destination.exists()


@pytest.mark.parametrize('kind', ['source', 'parent', 'destination', 'member'])
def test_symlink_inputs_and_outputs_are_rejected(tmp_path, real_composed, kind):
    """目录链接与资源成员链接均不能逃逸；实际外部文件保持原字节。"""
    source = copied_composed(tmp_path, real_composed)
    destination = tmp_path / 'preview'
    if kind == 'source':
        link = tmp_path / 'source-link'; link.symlink_to(source, target_is_directory=True); source = link
    elif kind == 'parent':
        link = tmp_path / 'parent-link'; link.symlink_to(tmp_path, target_is_directory=True); destination = link / 'preview'
    elif kind == 'destination':
        destination.symlink_to(tmp_path / 'missing', target_is_directory=True)
    else:
        asset = source / 'assets/art/packs/base/assets/art/pixel.png'
        raw = asset.read_bytes(); asset.unlink(); external = tmp_path / 'external.png'; external.write_bytes(raw); asset.symlink_to(external)
    with pytest.raises(PackError):
        preview.prepare_preview(source, destination, expected_pack_set_digest=real_composed[1])
    assert not (tmp_path / 'preview/game').exists()
    assert not list(tmp_path.glob('.pack-preview-*'))


def test_script_cannot_be_smuggled_with_rewritten_ownership(tmp_path, real_composed):
    """包重新声明所有权也不能把脚本复制成runtime；脚本只能来自固定ROOT。"""
    source = copied_composed(tmp_path, real_composed)
    script = source / 'injected.gd'; script.write_bytes(b'extends Node\n')
    marker = source / '.pack-generation.json'; value = json.loads(marker.read_text())
    value['files']['injected.gd'] = staging.digest(script); marker.write_bytes(staging.canonical(value))
    with pytest.raises(PackError):
        preview.prepare_preview(source, tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert not (tmp_path / 'preview').exists()


def test_source_change_during_validation_prevents_publish(tmp_path, real_composed, monkeypatch):
    """真实候选已校验后输入再改变仍阻止发布；外部改变的输入原样保留供作者处理。"""
    source = copied_composed(tmp_path, real_composed)
    original = preview.validate_registry

    def validate_then_change(*args):
        """完成实际资源校验后模拟作者改源，不伪造验证成功。"""
        result = original(*args)
        (source / 'presentation.json').write_bytes((source / 'presentation.json').read_bytes() + b' ')
        return result

    monkeypatch.setattr(preview, 'validate_registry', validate_then_change)
    with pytest.raises(PackError) as error:
        preview.prepare_preview(source, tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'PreviewInputsChanged' and not (tmp_path / 'preview').exists()
    assert (source / 'presentation.json').read_bytes().endswith(b' ')
    assert not list(tmp_path.glob('.pack-preview-*'))


def test_destination_created_during_prepare_is_preserved(tmp_path, real_composed, monkeypatch):
    """其他操作者途中创建目标后必须拒绝，不以rename覆盖新出现的目录。"""
    destination = tmp_path / 'preview'; original = preview.validate_registry

    def validate_then_create(*args):
        """实际校验后创建另一所有者的目标，测试发布前重检。"""
        result = original(*args); destination.mkdir(); (destination / 'mine').write_bytes(b'keep')
        return result

    monkeypatch.setattr(preview, 'validate_registry', validate_then_create)
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], destination, expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'PreviewDestinationExists'
    assert hashes(destination) == {'mine': hashlib.sha256(b'keep').hexdigest()}


def test_failed_publication_keeps_destination_absent(tmp_path, real_composed, monkeypatch):
    """发布rename失败只清本次候选，已有源和旁边目录完整保留且目标未创建。"""
    before = hashes(real_composed[0]); destination = tmp_path / 'preview'; original = preview._rename_no_replace

    def fail_candidate(path, target):
        """仅阻断本次候选最终rename，其他文件系统操作保持真实。"""
        if path.name.startswith('.pack-preview-'): raise OSError('injected preview publication failure')
        return original(path, target)

    monkeypatch.setattr(preview, '_rename_no_replace', fail_candidate)
    with pytest.raises(PackError):
        preview.prepare_preview(real_composed[0], destination, expected_pack_set_digest=real_composed[1])
    assert not destination.exists() and not list(tmp_path.glob('.pack-preview-*'))
    assert hashes(real_composed[0]) == before


def trusted_runtime_copy(tmp_path, monkeypatch):
    """测试才替换模块ROOT；复制真实源码/合同字节后只在自有受信树制造变动。"""
    target = tmp_path / 'trusted-runtime'
    for path, raw in preview._runtime_sources().items():
        destination = target / path.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True); destination.write_bytes(raw)
    monkeypatch.setattr(preview, 'ROOT', target)
    return target


@pytest.mark.parametrize('mutation', ['changed-script', 'added-script'])
def test_trusted_runtime_change_during_prepare_never_publishes(tmp_path, real_composed, monkeypatch, mutation):
    """固定ROOT的源码字节或脚本成员变化都阻断，不能将混代runtime宣布为准备成功。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    original = preview.validate_registry

    def validate_then_mutate(*args):
        """真实表现校验完成后模拟源码更新，不触碰真实仓库。"""
        result = original(*args)
        if mutation == 'changed-script':
            source = trusted / 'game/ui/main.gd'; source.write_bytes(source.read_bytes() + b'\n')
        else:
            (trusted / 'game/core/new_module.gd').write_bytes(b'extends RefCounted\n')
        return result

    monkeypatch.setattr(preview, 'validate_registry', validate_then_mutate)
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'PreviewInputsChanged' and not (tmp_path / 'preview').exists()
    assert not list(tmp_path.glob('.pack-preview-*'))


def test_missing_fixed_audio_cannot_prepare_partial_runtime(tmp_path, real_composed, monkeypatch):
    """受信固定音频缺失须失败，不重现标题可启动但资源缺件的假成功。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    (trusted / 'game/assets/audio/ambience.wav').unlink()
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'MissingPreviewRuntime' and not (tmp_path / 'preview').exists()


def test_trusted_script_symlink_is_rejected(tmp_path, real_composed, monkeypatch):
    """即使链接目标字节一样，固定源码树的脚本链接也不能作为受信runtime被复制。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    script = trusted / 'game/ui/main.gd'; external = tmp_path / 'external.gd'
    external.write_bytes(script.read_bytes()); script.unlink(); script.symlink_to(external)
    before = external.read_bytes()
    with pytest.raises(PackError):
        preview.prepare_preview(real_composed[0], tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert not (tmp_path / 'preview').exists() and external.read_bytes() == before


@pytest.mark.parametrize('missing', ['game/ui/main.gd', 'game/core/base/value_checks.gd',
                                     'game/ui/scene_stage.gd'])
def test_missing_static_runtime_dependency_cannot_publish(tmp_path, real_composed, monkeypatch, missing):
    """真实入口或深层preload缺件均拒绝，存在的脚本集合不能冒充完整runtime闭包。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    (trusted / missing).unlink()
    before = hashes(real_composed[0])
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'MissingPreviewRuntime'
    assert not (tmp_path / 'preview').exists() and not list(tmp_path.glob('.pack-preview-*'))
    assert hashes(real_composed[0]) == before


def test_runtime_rules_api_must_match_composed_lock(tmp_path, real_composed, monkeypatch):
    """固定runtime声明改成2.0.0而真实compose仍锁1.0.0时，准备阶段拒绝错误组合。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    declaration = trusted / 'game/data/pack_runtime.json'
    value = json.loads(declaration.read_text()); value['version'] = '2.0.0'
    declaration.write_bytes(staging.canonical(value))
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], tmp_path / 'preview', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'IncompatibleRulesApi'
    assert not (tmp_path / 'preview').exists() and not list(tmp_path.glob('.pack-preview-*'))


def test_publication_never_replaces_foreign_empty_directory(tmp_path, monkeypatch):
    """复现检查后另一个所有者创建空目标的竞争；原子发布不得覆盖该目录inode。"""
    candidate = tmp_path / '.pack-preview-candidate'; candidate.mkdir(); (candidate / 'mine').write_bytes(b'candidate')
    destination = tmp_path / 'preview'
    if hasattr(preview, '_rename_no_replace'):
        original = preview._rename_no_replace

        def publish_after_foreign_creation(source, target):
            """在真正系统原子rename之前插入另一所有者的空目标。"""
            target.mkdir(); foreign_inode.append(target.stat().st_ino)
            return original(source, target)

        monkeypatch.setattr(preview, '_rename_no_replace', publish_after_foreign_creation)
    else:
        original = Path.replace

        def replace_after_foreign_creation(source, target):
            """精确复现旧版自有保留目录被移开并同名出现外部空目录。"""
            target.rename(tmp_path / 'former-reservation'); target.mkdir(); foreign_inode.append(target.stat().st_ino)
            return original(source, target)

        monkeypatch.setattr(Path, 'replace', replace_after_foreign_creation)
    foreign_inode = []
    with pytest.raises(PackError):
        preview._publish(candidate, destination)
    assert destination.stat().st_ino == foreign_inode[0] and list(destination.iterdir()) == []
    assert (candidate / 'mine').read_bytes() == b'candidate'


def test_static_resource_closure_copies_required_asset_and_rejects_missing_leaf(tmp_path, real_composed, monkeypatch):
    """真实受信源码的preload→tres→PNG传递闭包缺叶子失败，补齐原件后逐字复制。"""
    trusted = trusted_runtime_copy(tmp_path, monkeypatch)
    main = trusted / 'game/ui/main.gd'
    main.write_bytes(main.read_bytes() + b'\nconst PreviewDependency = preload("res://ui/preview_chain.tres")\n')
    resource = trusted / 'game/ui/preview_chain.tres'
    resource.write_text('[gd_resource type="Resource" load_steps=2 format=3]\n'
                        '[ext_resource type="Texture2D" path="res://assets/preview_leaf.png" id="1"]\n'
                        '[resource]\nmetadata/asset = ExtResource("1")\n')
    with pytest.raises(PackError) as error:
        preview.prepare_preview(real_composed[0], tmp_path / 'missing', expected_pack_set_digest=real_composed[1])
    assert error.value.code == 'MissingPreviewRuntime' and not (tmp_path / 'missing').exists()
    leaf = trusted / 'game/assets/preview_leaf.png'
    leaf.parent.mkdir(parents=True, exist_ok=True)
    leaf.write_bytes((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/assets/art/pixel.png').read_bytes())
    result = preview.prepare_preview(real_composed[0], tmp_path / 'complete', expected_pack_set_digest=real_composed[1])
    game = Path(result['game_path'])
    assert (game / 'ui/preview_chain.tres').read_bytes() == resource.read_bytes()
    assert (game / 'assets/preview_leaf.png').read_bytes() == leaf.read_bytes()
    manifest = json.loads((game.parent / preview.SOURCE_MANIFEST).read_text())
    assert manifest['runtime']['game/assets/preview_leaf.png'] == staging.digest(leaf)
