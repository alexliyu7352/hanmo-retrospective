"""用真实作者与两张PNG区分当前五包安装和六份不可变历史身份，保持版本拒绝边界。"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import build, content_stage
from tools.content.io import canonical
from tools.content.pack_compose import _lock
from tools.content.references import _iter_annotated
from tests.test_installed_content_staging import stage_root, tree_bytes, FIXTURE


def digest(raw):
    """测试只按原字节生成SHA，不从工具成功状态推断身份一致。"""
    return hashlib.sha256(raw).hexdigest()


def foundation_root(path):
    """复制真实作者和归档原件，显式创建测试专用tiny表现绑定，不声称是生产历史美术锁。"""
    root = stage_root(path)
    shutil.copytree(ROOT / 'content/packs', root / 'content/packs')
    for name in ('alpha_life', 'journey_life', 'lived_city', 'procurement', 'two_stories'):
        shutil.copyfile(ROOT / 'content/foundation' / (name + '.json'), root / 'content/foundation' / (name + '.json'))
    archive = root / content_stage.FOUNDATION_ARCHIVE
    binding = json.loads((archive / 'identity-bindings.json').read_text())
    backgrounds, portraits = set(), set()
    for path in (archive).glob('*.source.json'):
        source = json.loads(path.read_text())
        for _path, group, reference in _iter_annotated(source, annotation='x-hanmo-presentation'):
            (backgrounds if group == 'backgrounds' else portraits).add(reference)
    presentation = {'version': 1, 'default_background': sorted(backgrounds)[0], 'default_portrait': sorted(portraits)[0],
                    'backgrounds': {key: {'path': 'res://assets/art/pixel.png'} for key in sorted(backgrounds)},
                    'portraits': {key: {'path': 'res://assets/art/portrait.png'} for key in sorted(portraits)}}
    raw = canonical(presentation)
    (archive / 'presentation.json').write_bytes(raw)
    (root / 'game/data/presentation.json').write_bytes(raw)
    binding['presentation_sha256'] = digest(raw)
    binding['resources'] = [{'path': 'game/assets/art/' + name,
                            'sha256': digest((root / 'game/assets/art' / name).read_bytes())}
                           for name in ('pixel.png', 'portrait.png')]
    for entry in binding['entries']:
        name = entry['source_name']; slot = entry['slot']
        source_raw = (archive / (slot + '.source.json')).read_bytes()
        header = json.loads((archive / (slot + '.hmcatalog.json')).read_text())
        manifest = json.loads((root / 'content/packs/legacy' / name / 'pack.json').read_text())
        content_hashes = [{'path': 'content/foundation/' + name + '.json', 'sha256': digest(source_raw)},
                          {'path': 'game/data/presentation.json', 'sha256': digest(raw)}]
        pack = {'manifest': manifest, 'manifest_sha256': entry['manifest_sha256'],
                'content_sha256': digest(canonical(content_hashes)),
                'resources_sha256': digest(canonical(binding['resources']))}
        lock = _lock([pack], binding['rules_api'], header['catalog'], raw,
                     entry['schema_sha256'], binding['rule_capabilities_sha256'])
        entry['pack_set_digest'] = digest(canonical(lock))
    (archive / 'identity-bindings.json').write_bytes(canonical(binding))
    return root, binding


def test_fixed_archive_contains_six_unique_real_identities():
    """四份旧/当前数据身份合一，旧Alpha f001与当前Alpha3638分别来自各自真实源。"""
    archive = ROOT / content_stage.FOUNDATION_ARCHIVE
    binding = json.loads((archive / 'identity-bindings.json').read_text())
    entries = binding['entries']
    assert len(entries) == len({content_stage._legacy_identity(row) for row in entries}) == 6
    by_slot = {row['slot']: row for row in entries}
    assert by_slot['alpha_life']['catalog_sha256'] == 'f0017626a226b2c1b322b55af128d722943e3d9da086ee3cdb1e5fbe99bc4516'
    assert by_slot['alpha_life.current']['catalog_sha256'] == '36388b3cd4b4cfd7cf19c7a154b53f5be8fcc1ef761919ff61540cc2126b2b15'
    assert by_slot['alpha_life']['pack_set_digest'] != by_slot['alpha_life.current']['pack_set_digest']
    for row in entries:
        raw = (archive / (row['slot'] + '.hmcatalog.json')).read_bytes()
        source = (archive / (row['slot'] + '.source.json')).read_bytes()
        assert digest(raw) == row['catalog_file_sha256']
        assert digest(source) == row['source_sha256']
        assert json.loads(raw)['source_sha256'] == digest(source)
        assert 'pack_set_digest' not in json.loads(raw)


def archived_generation(root, path):
    """只在独立目录核验历史原件，不把旧描述的集合放入当前运行索引。"""
    build.stage_content(root, foundation_sets=False)
    path.mkdir()
    for source in (root / 'game/data/generated').glob('*.hmcatalog.json'):
        shutil.copyfile(source, path / source.name)
    installed = {}
    content_stage.stage_archived_foundation_sets(root, path, installed)
    return installed


def test_default_stage_installs_all_five_current_foundations_without_rebinding_archives(tmp_path):
    """正常入口全部五包使用当前规则、Schema和新packset身份；六份历史原件逐字不变。"""
    root, binding = foundation_root(tmp_path / 'project')
    sources_before = tree_bytes(root / 'content/foundation')
    archive_before = tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)
    build.stage_content(root)
    output = root / 'game/data/generated'
    installed = json.loads((output / 'installed-content.json').read_text())
    assert len(installed['sets']) == 5
    names = set()
    for entry in installed['sets']:
        target = output / entry['directory']
        header = json.loads((target / 'catalog.hmcatalog.json').read_text())
        lock = json.loads((target / 'pack-set.lock.json').read_text())
        name = lock['packs'][0]['namespace']; names.add(name)
        prepared = json.loads((output / (name + '.hmcatalog.json')).read_text())
        assert header['catalog'] == prepared['catalog']
        assert header['catalog_sha256'] == prepared['catalog_sha256']
        assert header['content_id'] == 'packset.' + entry['pack_set_digest']
        assert header['content_version'] == '1.0.0'
        assert header['schema_sha256'] == digest((root / content_stage.SCHEMA).read_bytes())
        assert header['rule_capabilities_sha256'] == digest((root / content_stage.CAPABILITY).read_bytes())
        assert header['source_sha256'] == digest(canonical(lock['packs']))
        assert not (target / 'legacy-identity.json').exists()
        assert entry['pack_set_digest'] not in {row['pack_set_digest'] for row in binding['entries']}
        origins = json.loads((target / 'source-map.json').read_text())['entries']
        assert origins['/catalog/scenarios/' + next(iter(header['catalog']['scenarios'])) + '/player_actor']['file'] == 'content/foundation/' + name + '.json'
    assert names == {'alpha_life', 'journey_life', 'lived_city', 'procurement', 'two_stories'}
    assert sources_before == tree_bytes(root / 'content/foundation')
    assert archive_before == tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)
    content_stage.validate_stage(root, build.discover_author_sources(root))
    before = tree_bytes(output)
    build.stage_content(root)
    assert tree_bytes(output) == before


def test_current_presentation_change_gets_new_locks_and_preserves_existing_generations(tmp_path):
    """当前美术变化产生新锁，既有完整代保留；旧v1原件不被重绑。"""
    root, _ = foundation_root(tmp_path / 'project')
    archive_before = tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output / 'content-sets')
    (root / 'game/assets/art/pixel.png').write_bytes((root / 'game/assets/art/portrait.png').read_bytes())
    path = root / 'game/data/presentation.json'
    presentation = json.loads(path.read_text())
    presentation['default_background'] = sorted(presentation['backgrounds'])[-1]
    path.write_bytes(canonical(presentation))
    build.stage_content(root)
    after = tree_bytes(output / 'content-sets')
    assert all(after[name] == raw for name, raw in before.items())
    assert len(json.loads((output / 'installed-content.json').read_text())['sets']) == 10
    assert archive_before == tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)


def test_exact_archived_descriptor_preserves_original_six_headers_and_locks(tmp_path):
    """恢复出的旧描述只参与原件复核，不替换当前描述，也不宣称旧规则可运行。"""
    root, binding = foundation_root(tmp_path / 'project')
    archive = root / content_stage.FOUNDATION_ARCHIVE
    descriptor = archive / (binding['rule_capabilities_sha256'] + '.rule-capabilities.json')
    assert digest(descriptor.read_bytes()) == binding['rule_capabilities_sha256']
    assert descriptor.read_bytes() != (root / content_stage.CAPABILITY).read_bytes()
    output = tmp_path / 'archive-output'
    installed = archived_generation(root, output)
    assert len(installed) == 6
    for entry in binding['entries']:
        target = output / 'content-sets' / entry['pack_set_digest']
        assert (target / 'catalog.hmcatalog.json').read_bytes() == (archive / (entry['slot'] + '.hmcatalog.json')).read_bytes()
        identity = json.loads((target / 'legacy-identity.json').read_text())
        assert identity['pack_set_digest'] == entry['pack_set_digest']
    assert json.loads((root / 'game/data/generated/installed-content.json').read_text())['sets'] == []


def test_archive_reconstruction_is_independent_of_current_author_edits(tmp_path):
    """历史原件只依赖已封存输入；当前作者修改后仍可在空目录独立复核全部六个锁。"""
    root, binding = foundation_root(tmp_path / 'project')
    source_path = root / 'content/foundation/alpha_life.json'
    source = json.loads(source_path.read_text())
    source['texts'][next(iter(source['texts']))] += '当前新文案'
    source_path.write_bytes(canonical(source))
    output = tmp_path / 'archive-output'; output.mkdir()
    installed = {}
    content_stage.stage_archived_foundation_sets(root, output, installed)
    assert set(installed) == {entry['pack_set_digest'] for entry in binding['entries']}
    for entry in binding['entries']:
        assert digest((output / installed[entry['pack_set_digest']]['directory'] / 'catalog.hmcatalog.json').read_bytes()) == entry['catalog_file_sha256']
    assert not (output / content_stage.INSTALLED_INDEX).exists()


@pytest.mark.parametrize('change', ['old_alpha_source', 'resource', 'missing_binding', 'descriptor'])
def test_missing_or_changed_archive_original_remains_an_explicit_failure(tmp_path, change):
    """历史原件验证仍核真实字节；损坏的描述、源、资源或绑定不能变成受信原件。"""
    root, binding = foundation_root(tmp_path / 'project')
    archive = root / content_stage.FOUNDATION_ARCHIVE
    if change == 'old_alpha_source':
        (archive / 'alpha_life.source.json').write_bytes((archive / 'alpha_life.current.source.json').read_bytes())
    elif change == 'resource':
        (root / 'game/assets/art/pixel.png').write_bytes((root / 'game/assets/art/portrait.png').read_bytes())
    elif change == 'descriptor':
        (archive / (binding['rule_capabilities_sha256'] + '.rule-capabilities.json')).write_bytes((root / content_stage.CAPABILITY).read_bytes())
    else:
        (archive / 'identity-bindings.json').unlink()
    with pytest.raises(ValueError, match='(?:Missing|Incomplete)LegacyBinding'):
        archived_generation(root, tmp_path / 'archive-output')
    assert json.loads((root / 'game/data/generated/installed-content.json').read_text())['sets'] == []


def test_new_current_content_gets_new_identity_without_mutating_old_binding(tmp_path):
    """正常作者修改可以生成新当前集合；历史三元组和已安装原代都不会被冒用。"""
    root, _ = foundation_root(tmp_path / 'project')
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output / 'content-sets')
    archive_before = tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)
    source = root / 'content/foundation/alpha_life.json'
    value = json.loads(source.read_text())
    value['texts'][next(iter(value['texts']))] += '新文案'
    source.write_bytes(canonical(value))
    build.stage_content(root)
    assert len(json.loads((output / 'installed-content.json').read_text())['sets']) == 6
    after = tree_bytes(output / 'content-sets')
    assert all(after[name] == raw for name, raw in before.items())
    assert archive_before == tree_bytes(root / content_stage.FOUNDATION_ARCHIVE)


def test_self_consistent_prepared_catalog_cannot_misstate_current_source(tmp_path, monkeypatch):
    """即使重算数据摘要和保留源摘要，编译输出也必须等于该源的真实投影，失败保留旧代。"""
    root, _ = foundation_root(tmp_path / 'project')
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output)
    original = build.subprocess.run

    def corrupt_current_alpha(command, **kwargs):
        """真实prepare成功后篡改Alpha数据并重算自报摘要，模拟不诚实但内部一致的输出。"""
        result = original(command, **kwargs)
        target = Path(command[-1])
        if target.name == 'alpha_life.hmcatalog.json':
            header = json.loads(target.read_text())
            header['catalog']['texts'][next(iter(header['catalog']['texts']))] += '伪造'
            header['catalog_sha256'] = digest(canonical(header['catalog']))
            target.write_bytes(canonical(header))
        return result

    monkeypatch.setattr(build.subprocess, 'run', corrupt_current_alpha)
    with pytest.raises(RuntimeError, match='CurrentFoundationCatalogMismatch'):
        build.stage_content(root)
    assert tree_bytes(output) == before


def test_historical_descriptor_cannot_enter_or_survive_current_installed_index(tmp_path):
    """明确拒绝不受当前规则支持的旧安装项，导入和已有代都不得静默丢失或绕过摘要。"""
    root, _ = foundation_root(tmp_path / 'project')
    archive_output = tmp_path / 'archive-output'
    archived = archived_generation(root, archive_output)
    source = archive_output / next(iter(archived.values()))['directory']
    build.stage_content(root)
    output = root / 'game/data/generated'
    before = tree_bytes(output)
    with pytest.raises(RuntimeError, match='unregistered schema or rule capability contract'):
        build.stage_content(root, content_sets=[source])
    assert tree_bytes(output) == before
    entry = content_stage.composed_set_record(source)
    shutil.copytree(source, output / entry['directory'])
    index_path = output / 'installed-content.json'
    index = json.loads(index_path.read_text()); index['sets'].append(entry)
    index['sets'].sort(key=lambda row: row['pack_set_digest'])
    index_path.write_bytes(canonical(index))
    (output / content_stage.MANIFEST).write_bytes(canonical(content_stage.make_manifest(root, output, build.discover_author_sources(root))))
    before = tree_bytes(output)
    with pytest.raises(RuntimeError, match='InstalledRuleCapabilitiesMismatch'):
        build.stage_content(root)
    assert tree_bytes(output) == before
