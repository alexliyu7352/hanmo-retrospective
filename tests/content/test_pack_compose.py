"""用真实包输入验证组合、来源与原子发布，不以文件存在代替运行验收。"""
from copy import deepcopy
import importlib
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.content.pack_errors import PackError
from tools.content.packs import discover_packs, resolve_pack_set, load_pack_runtime


def api():
    """只使用已实际存在的生产组合模块。"""
    return importlib.import_module('tools.content.pack_compose')


def manifest(identifier='base', **updates):
    """提供与现有采购内容能力一致的测试包声明。"""
    result = {'format': 'HM-PACK', 'format_version': 1, 'id': identifier, 'version': '1.0.0',
              'namespace': identifier, 'rules_api': {'id': 'hanmo.rules', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'},
              'dependencies': [], 'conflicts': [], 'required_capabilities': ['multi_actor', 'places', 'dialogue', 'tasks.procurement', 'economy'],
              'content_entries': ['content.json'], 'presentation_entries': [], 'resources': [], 'author': '组合测试', 'license': 'CC0-1.0'}
    result.update(updates)
    return result


def write_pack(root, value, source):
    """测试写真实manifest和原内容字节，发现阶段由生产读取器冻结。"""
    root.mkdir(parents=True)
    (root / 'pack.json').write_text(json.dumps(value, ensure_ascii=False), encoding='utf8')
    (root / 'content.json').write_text(json.dumps(source, ensure_ascii=False), encoding='utf8')
    return root


def source():
    """复用已有可校验内容，仅作为新声明包输入，不伪造旧产物。"""
    return json.loads((ROOT / 'content/foundation/procurement.json').read_text())


def records(roots):
    """显式选取发现的每个版本，并沿生产规则API能力解析。"""
    packs = discover_packs(roots)
    selected = [{'id': pack['manifest']['id'], 'version': pack['manifest']['version']} for pack in packs]
    return resolve_pack_set(packs, selected, load_pack_runtime())['packs']


def empty_source():
    """保留同一根结构和全部必需表，空表只在包源结构阶段允许。"""
    data = source()
    for key, value in data.items():
        if isinstance(value, list): data[key] = []
        elif isinstance(value, dict): data[key] = {}
    return data


def test_typed_qualification_preserves_local_nodes_and_text(tmp_path):
    """实体和注解引用被限定，普通文字与对白node数组不被字符串替换。"""
    original = source()
    original['texts']['office.hello'] = 'actor.player is ordinary prose'
    pack = records([write_pack(tmp_path / 'base', manifest(), original)])[0]
    merged, origins = api()._merge_content([pack])
    assert merged['actors'][0]['id'].startswith('base::')
    assert merged['scenarios'][0]['player_actor'] == 'base::actor.player'
    assert merged['texts']['base::office.hello'] == 'actor.player is ordinary prose'
    actual = next(row for row in merged['dialogues'] if row['id'] == 'base::' + original['dialogues'][0]['id'])
    assert [node['id'] for node in actual['nodes']] == [node['id'] for node in original['dialogues'][0]['nodes']]
    assert any(origin['reference_chain'] for origin in origins.values())
    assert 'base::' not in (tmp_path / 'base/content.json').read_text()


def test_partial_source_does_not_require_fake_scenario(tmp_path):
    """扩展可以不带剧本或必需实体，仍保持记录字段约束。"""
    partial = empty_source()
    api().validate_pack_source(partial)
    partial['actors'] = [{'id': 'actor.incomplete'}]
    with pytest.raises(PackError) as caught:
        api().validate_pack_source(partial)
    assert caught.value.code == 'InvalidPackSource'


def test_defined_ids_cannot_arrive_prequalified():
    """源定义不得借合法运行ID语法冒充其他namespace。"""
    partial = empty_source()
    partial['texts'] = {'other::text': 'bad'}
    with pytest.raises(PackError) as caught:
        api().validate_pack_source(partial)
    assert caught.value.code == 'QualifiedPackDefinition'


def test_cross_package_reference_requires_direct_dependency(tmp_path):
    """包都已选择也不能越过直接依赖范围引用。"""
    base = source()
    base['scenarios'][0]['player_actor'] = 'addon::actor.player'
    packs = records([write_pack(tmp_path / 'base', manifest(), base),
                     write_pack(tmp_path / 'addon', manifest('addon'), source())])
    with pytest.raises(PackError) as caught:
        api()._merge_content(packs)
    assert caught.value.code == 'UndeclaredPackDependency'
    assert caught.value.details['target_pack'] == 'addon'


def test_duplicate_definition_reports_both_sources(tmp_path):
    """同包两入口同ID不会后写覆盖，并保留双方来源。"""
    root = write_pack(tmp_path / 'base', manifest(content_entries=['content.json', 'second.json']), source())
    (root / 'second.json').write_bytes((root / 'content.json').read_bytes())
    with pytest.raises(PackError) as caught:
        api()._merge_content(records([root]))
    assert caught.value.code == 'DuplicatePackDefinition'
    assert {origin['file'] for origin in caught.value.details['sources']} == {'content.json', 'second.json'}


def test_manifest_capabilities_cover_all_entries(tmp_path):
    """源文件不能在manifest遗漏能力声明时悄悄注册运行能力。"""
    root = write_pack(tmp_path / 'base', manifest(required_capabilities=[]), source())
    with pytest.raises(PackError) as caught:
        api()._merge_content(records([root]))
    assert caught.value.code == 'MissingPackCapabilityDeclaration'


def read_generation(path):
    """逐文件读回测试生成代，比较真实字节而非摘要自报。"""
    return {file.relative_to(path).as_posix(): file.read_bytes() for file in path.rglob('*') if file.is_file()}


def test_generation_replacement_is_clean_and_deterministic(tmp_path):
    """替换整代删除旧受控残留，相同输入在不同目录逐字一致。"""
    output = tmp_path / 'generation'
    api()._publish(output, {'old.json': b'old'}, {})
    actual = {'catalog.hmcatalog.json': b'{}\n', 'resources/new.txt': b'new'}
    api()._publish(output, actual, {})
    second = tmp_path / 'second'
    api()._publish(second, dict(reversed(list(actual.items()))), {})
    assert read_generation(output) == read_generation(second)
    assert not (output / 'old.json').exists()


def test_source_changes_keep_previous_generation(tmp_path):
    """包括可选资源由缺失变存在的变化，都在发布前阻断且旧代不变。"""
    output = tmp_path / 'generation'
    api()._publish(output, {'old.json': b'old'}, {})
    previous = read_generation(output)
    source_file = tmp_path / 'source.json'
    source_file.write_bytes(b'new source')
    for expected in (b'old source', None):
        with pytest.raises(PackError) as caught:
            api()._publish(output, {'new.json': b'new'}, {source_file: expected})
        assert caught.value.code == 'SourceChanged'
        assert read_generation(output) == previous
    assert not list(tmp_path.glob('.generation.stage-*'))
    assert not (tmp_path / '.generation.compose.lock').exists()


def test_unowned_files_block_replacement(tmp_path):
    """现有未知文件属于用户，不能借新生成任务覆盖或清理。"""
    output = tmp_path / 'generation'
    api()._publish(output, {'old.json': b'old'}, {})
    (output / 'personal.txt').write_bytes(b'keep me')
    previous = read_generation(output)
    with pytest.raises(PackError) as caught:
        api()._publish(output, {'new.json': b'new'}, {})
    assert caught.value.code == 'UnownedPackOutput'
    assert read_generation(output) == previous


def test_failed_final_rename_rolls_back_exact_old_bytes(tmp_path, monkeypatch):
    """最终目录切换失败可恢复上一代，不留半新目录。"""
    output = tmp_path / 'generation'
    api()._publish(output, {'old.json': b'old'}, {})
    previous = read_generation(output)
    replace = api().os.replace
    def fail_stage(source_path, target):
        """只注入最终stage切换失败，真实备份和回滚仍用OS操作。"""
        if '.stage-' in Path(source_path).name and Path(target) == output:
            raise OSError('injected final rename failure')
        return replace(source_path, target)
    monkeypatch.setattr(api().os, 'replace', fail_stage)
    with pytest.raises(PackError) as caught:
        api()._publish(output, {'new.json': b'new'}, {})
    assert caught.value.code == 'PackPublishFailed'
    assert read_generation(output) == previous


def test_post_publish_cleanup_failure_does_not_report_publish_failure(tmp_path, monkeypatch):
    """切换已完成后备份清理失败仅报告待清理，不把已发布新代伪称旧代。"""
    output = tmp_path / 'generation'
    api()._publish(output, {'old.json': b'old'}, {})
    remove = api().shutil.rmtree
    def fail_backup(path, *args, **kwargs):
        """仅模拟旧备份清理失败，不影响实际stage写入和切换。"""
        if '.previous-' in Path(path).name:
            raise OSError('injected cleanup failure')
        return remove(path, *args, **kwargs)
    monkeypatch.setattr(api().shutil, 'rmtree', fail_backup)
    warnings = api()._publish(output, {'new.json': b'new'}, {})
    assert (output / 'new.json').read_bytes() == b'new'
    assert warnings == ['PreviousGenerationCleanupPending']


def test_lock_hash_does_not_include_header_or_itself(tmp_path):
    """锁hash来自固定数据hash，兼容header身份最后生成且不产生循环。"""
    packs = records([write_pack(tmp_path / 'base', manifest(), source())])
    merged, _ = api()._merge_content(packs)
    data = api()._catalog(merged)
    lock = api()._lock(packs, {'id': 'hanmo.rules', 'version': '1.0.0'}, data, b'{}\n', 'a' * 64, 'b' * 64)
    header = api()._new_header(data, lock)
    assert header['pack_set_digest'] == api()._sha(api().canonical(lock))
    assert header['catalog_sha256'] == api()._sha(api().canonical(data))
    assert header['content_id'] == 'packset.' + header['pack_set_digest']
    assert 'digest' not in lock and 'pack_set_digest' not in lock
    assert b'/workspace/' not in api().canonical(lock)


def test_outputs_cannot_overlap_source_roots(tmp_path):
    """编译输出不能覆盖输入目录、其祖先或工程源码。"""
    root = tmp_path / 'author'
    root.mkdir()
    for output in (root, root / 'generated', tmp_path, ROOT / 'game/data/generated'):
        with pytest.raises(PackError) as caught:
            api()._protected_output(output, [root])
        assert caught.value.code == 'ProtectedPackOutput'


@pytest.mark.parametrize('name', ['procurement', 'two_stories', 'lived_city', 'alpha_life', 'journey_life'])
def test_true_legacy_golden_headers_are_preserved_byte_for_byte(name, monkeypatch):
    """五份真实原件逐字读取；Alpha使用ae523bb真旧源，不拿当前源伪造旧身份。"""
    baseline = ROOT / 'tests/fixtures/content_packs/compose/legacy_baseline'
    monkeypatch.setattr(api(), 'LEGACY_BASELINE_ROOT', baseline)
    source_raw = (baseline / (name + '.source.json')).read_bytes()
    source_data = json.loads(source_raw)
    logical = 'content/foundation/' + name + '.json'
    pack = {'manifest': {'legacy_source': name, 'id': source_data['id'], 'version': source_data['version']},
            'content_paths': [logical], 'files': {logical: source_raw}}
    snapshots = {}
    raw, schema = api()._legacy_header(pack, snapshots)
    expected = (baseline / (name + '.hmcatalog.json')).read_bytes()
    assert raw == expected
    assert 'pack_set_digest' not in json.loads(raw)
    assert api()._sha(schema) == json.loads(raw)['schema_sha256']
    assert snapshots[baseline / (name + '.hmcatalog.json')] == expected


def test_current_alpha_cannot_borrow_old_alpha_identity(monkeypatch):
    """同名同版本但源字节已变化必须拒绝生成旧sidecar。"""
    baseline = ROOT / 'tests/fixtures/content_packs/compose/legacy_baseline'
    monkeypatch.setattr(api(), 'LEGACY_BASELINE_ROOT', baseline)
    logical = 'content/foundation/alpha_life.json'
    current = (ROOT / logical).read_bytes()
    assert current != (baseline / 'alpha_life.source.json').read_bytes()
    pack = {'manifest': {'legacy_source': 'alpha_life', 'id': 'foundation.alpha_life', 'version': '0.7.0'},
            'content_paths': [logical], 'files': {logical: current}}
    with pytest.raises(PackError) as caught:
        api()._legacy_header(pack, {})
    assert caught.value.code == 'LegacySourceChanged'


def test_corrupt_legacy_catalog_or_schema_cannot_gain_sidecar(tmp_path, monkeypatch):
    """可信索引也必须核实际原件摘要，篡改文件不能靠自报身份通过。"""
    import shutil
    baseline = ROOT / 'tests/fixtures/content_packs/compose/legacy_baseline'
    shutil.copytree(baseline, tmp_path / 'archive')
    archive = tmp_path / 'archive'
    monkeypatch.setattr(api(), 'LEGACY_BASELINE_ROOT', archive)
    logical = 'content/foundation/procurement.json'
    pack = {'manifest': {'legacy_source': 'procurement', 'id': 'foundation.procurement', 'version': '0.3.1'},
            'content_paths': [logical], 'files': {logical: (archive / 'procurement.source.json').read_bytes()}}
    catalog = archive / 'procurement.hmcatalog.json'
    catalog.write_bytes(catalog.read_bytes() + b' ')
    with pytest.raises(PackError) as caught:
        api()._legacy_header(pack, {})
    assert caught.value.code == 'InvalidLegacyArtifact'


def test_source_map_contains_relative_author_pointers_only(tmp_path):
    """来源图可定位原局部ID和字段，不泄漏机器路径或改写语义数组。"""
    packs = records([write_pack(tmp_path / 'base', manifest(), source())])
    merged, origins = api()._merge_content(packs)
    location = origins['/catalog/scenarios/base::scenario.procurement/player_actor']
    assert location['pack_id'] == 'base'
    assert location['file'] == 'content.json'
    assert location['field_path'] == '/scenarios/0/player_actor'
    assert location['reference_chain'] == ['actor.player', 'base::actor.player']
    assert str(tmp_path) not in json.dumps(origins)


def new_fixture(tmp_path):
    """复制含两张2×2真PNG的新包fixture，不导出或解码全套旧美术。"""
    import shutil
    root = tmp_path / 'inputs'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs', root)
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                 'packs': [{'id': 'addon', 'version': '1.0.0'}, {'id': 'base', 'version': '1.0.0'}]}
    return root, selection


def test_compose_new_pack_set_is_byte_deterministic(tmp_path):
    """真实入口组合、两张图片解码、锁回验与新代发布在逆序输入时逐字一致。"""
    root, selection = new_fixture(tmp_path)
    one, two = tmp_path / 'one', tmp_path / 'two'
    result = api().compose_pack_set([root / 'base', root / 'addon'], selection, one)
    reversed_selection = {**selection, 'packs': selection['packs'][::-1]}
    second = api().compose_pack_set([root / 'addon', root / 'base'], reversed_selection, two)
    assert result == second
    assert result['runtime_verified'] is False
    assert read_generation(one) == read_generation(two)
    header = json.loads((one / 'catalog.hmcatalog.json').read_bytes())
    lock = json.loads((one / 'pack-set.lock.json').read_bytes())
    assert header['pack_set_digest'] == api()._sha(api().canonical(lock))
    assert 'addon::actor.extension' in header['catalog']['actors']
    assert header['catalog']['actors']['addon::actor.extension']['initial_place'] == 'base::west.grain'
    assert [pack['id'] for pack in lock['packs']] == ['base', 'addon']
    assert not (one / 'data').exists()
    presentation = json.loads((one / 'presentation.json').read_bytes())
    assert presentation['default_background'] == 'base::city'
    assert (one / presentation['backgrounds']['base::city']['path'].removeprefix('res://')).is_file()


def test_source_edit_during_compile_keeps_old_complete_generation(tmp_path, monkeypatch):
    """真实临时代校验结束后改源，发布前复核必须阻断并保留旧代。"""
    root, selection = new_fixture(tmp_path)
    output = tmp_path / 'generated'
    api().compose_pack_set([root], selection, output)
    previous = read_generation(output)
    validate = api()._validate_products
    def edit_after_validation(stage, merged, schema):
        """真实校验仍执行，只在发布前模拟作者编辑。"""
        validate(stage, merged, schema)
        source_file = root / 'addon/content.json'
        source_file.write_bytes(source_file.read_bytes() + b'\n')
    monkeypatch.setattr(api(), '_validate_products', edit_after_validation)
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, output)
    assert caught.value.code == 'SourceChanged'
    assert read_generation(output) == previous


def test_deleted_source_cannot_publish_stale_generated_content(tmp_path):
    """删除作者源后不会从旧产物倒灌继续生成，旧代虽保留也不伪称新成功。"""
    root, selection = new_fixture(tmp_path)
    output = tmp_path / 'generated'
    api().compose_pack_set([root], selection, output)
    previous = read_generation(output)
    (root / 'addon/content.json').unlink()
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, output)
    assert caught.value.code == 'InvalidPackJson'
    assert read_generation(output) == previous


def test_invalid_cross_reference_reports_author_source_and_preserves_output(tmp_path):
    """组合后未知引用仍走共享全局检查，带真实作者来源且不改旧代。"""
    root, selection = new_fixture(tmp_path)
    output = tmp_path / 'generated'
    api().compose_pack_set([root], selection, output)
    previous = read_generation(output)
    path = root / 'addon/content.json'
    data = json.loads(path.read_bytes())
    data['actors'][0]['initial_place'] = 'base::missing.place'
    path.write_text(json.dumps(data), encoding='utf8')
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, output)
    assert caught.value.code == 'InvalidComposedContent'
    assert caught.value.details['issues'][0]['source']['file'] == 'content.json'
    assert read_generation(output) == previous


def test_shared_input_path_cannot_hide_different_frozen_bytes(tmp_path):
    """两个入口共用物理文件但读取间发生变化时，不能覆盖先前快照掩盖竞态。"""
    root = write_pack(tmp_path / 'base', manifest(), source())
    first = records([root])[0]
    path = root / 'content.json'
    path.write_bytes(path.read_bytes() + b'\n')
    second = records([root])[0]
    with pytest.raises(PackError) as caught:
        api()._input_snapshots([first, second], {})
    assert caught.value.code == 'SourceChanged'


def v2_partial_presentation():
    """复用既有v2结构fixture，只抽记录测试注解，不重新导出或解码美术。"""
    source_data = json.loads((ROOT / 'tests/fixtures/presentation/layered-v2.json').read_bytes())
    component = deepcopy(source_data['components']['child.body'])
    component['after'] = ['child.mask']
    component['geometry']['mask_ids'] = ['child.mask']
    scene = deepcopy(source_data['scene_templates']['fixture.room'])
    scene['layers'][1]['target_ref'] = {'kind': 'actor', 'id': 'actor.player'}
    scene['layers'][1]['extent'] = [20, 20]
    scene['layers'][2]['candidate_selector'] = {'command_kind': 'talk', 'params': {'role_id': 'role.office'}}
    return {'version': 2, 'components': {'child.body': component},
            'character_profiles': {'fixture.person': source_data['character_profiles']['fixture.person']},
            'scene_templates': {'fixture.room': scene}}


def test_v2_global_refs_qualify_but_local_ids_and_semantics_do_not(tmp_path):
    """anyOf旁的全局字段和场景内容引用不能漏改，局部层与年龄身份不得被前缀污染。"""
    pack = records([write_pack(tmp_path / 'base', manifest(), source())])[0]
    schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_bytes())
    partial = v2_partial_presentation()
    qualified, _chains, _assets = api()._qualify_presentation(partial, pack, 'presentation.json', {'base': pack}, schema)
    component = qualified['components']['base::child.body']
    assert component['asset_id'] == 'portraits/base::fixture.child-body'
    assert component['after'] == ['base::child.mask']
    assert component['geometry']['mask_ids'] == ['base::child.mask']
    assert component['body_basis'] == 'child.front' and component['tags'] == ['child']
    profile = qualified['character_profiles']['base::fixture.person']
    assert profile['age_variants'][0]['id'] == 'young'
    assert profile['age_variants'][0]['components'] == ['base::child.body']
    assert profile['attire_sets'][0]['id'] == 'plain'
    assert profile['battle_profile']['profile_id'] == 'base::fixture.battle'
    scene = qualified['scene_templates']['base::fixture.room']
    assert scene['layers'][1]['profile_id'] == 'base::fixture.person'
    assert scene['layers'][1]['after'] == ['back']
    assert scene['layers'][1]['target_ref']['id'] == 'base::actor.player'
    assert scene['layers'][2]['target_layer_id'] == 'person'
    assert scene['layers'][2]['candidate_selector'] == {'command_kind': 'talk', 'params': {'role_id': 'base::role.office'}}


@pytest.mark.parametrize('target', ['age', 'layer'])
def test_v2_local_ids_still_reject_namespaces(tmp_path, target):
    """全局ID放宽不扩及年龄变体或场景局部层。"""
    pack = records([write_pack(tmp_path / 'base', manifest(), source())])[0]
    schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_bytes())
    partial = v2_partial_presentation()
    if target == 'age':
        partial['character_profiles']['fixture.person']['age_variants'][0]['id'] = 'base::young'
    else:
        partial['scene_templates']['fixture.room']['layers'][0]['id'] = 'base::back'
    with pytest.raises(PackError) as caught:
        api()._qualify_presentation(partial, pack, 'presentation.json', {'base': pack}, schema)
    assert caught.value.code == 'InvalidPackPresentation'


def add_tiny_scene(root):
    """复用两张tiny PNG构造真实绑定场景，不引入新美术或任意表达式。"""
    path = root / 'base/presentation.json'
    presentation = json.loads(path.read_bytes())
    presentation.update(version=2, components={}, character_profiles={}, scene_templates={})
    geometry = {'source_size': [2, 2], 'logical_canvas': [2, 2], 'alpha_bounds': [0, 0, 2, 2],
                'safe_crop': [0, 0, 2, 2], 'support_kind': 'foot', 'support_point': [50, 100],
                'pose': 'idle', 'facing': 'front', 'view': 'scene', 'mask_ids': [], 'palette_slots': []}
    presentation['scene_templates']['room'] = {
        'id': 'room', 'view_kind': 'interior', 'logical_canvas': [2, 2], 'ui_safe_areas': [],
        'layers': [
            {'id': 'back', 'kind': 'background', 'asset_id': 'backgrounds/city', 'anchor': [50, 50], 'z_index': 0, 'after': [], 'geometry': geometry},
            {'id': 'speaker', 'kind': 'actor', 'position': [50, 80], 'target_ref': {'kind': 'actor', 'id': 'actor.chen'}, 'extent': [20, 20], 'after': ['back']},
            {'id': 'talk', 'kind': 'hotspot', 'rect': [20, 20, 40, 60], 'target_layer_id': 'speaker', 'after': ['speaker'],
             'candidate_selector': {'command_kind': 'talk', 'params': {'role_id': 'role.office'}}},
        ]}
    path.write_text(json.dumps(presentation), encoding='utf8')
    content_path = root / 'base/content.json'
    content = json.loads(content_path.read_bytes())
    content['places'][1]['scene_template_id'] = 'room'
    content_path.write_text(json.dumps(content), encoding='utf8')


def test_scene_content_refs_and_local_layers_survive_full_compose(tmp_path):
    """真实小场景经生产校验和发布，actor/role限定但局部layer与命令枚举不变。"""
    root, selection = new_fixture(tmp_path)
    add_tiny_scene(root)
    output = tmp_path / 'generated'
    api().compose_pack_set([root], selection, output)
    presentation = json.loads((output / 'presentation.json').read_bytes())
    scene = presentation['scene_templates']['base::room']
    assert scene['layers'][1]['target_ref'] == {'kind': 'actor', 'id': 'base::actor.chen'}
    assert scene['layers'][2]['candidate_selector'] == {'command_kind': 'talk', 'params': {'role_id': 'base::role.office'}}
    assert scene['layers'][2]['target_layer_id'] == 'speaker'
    assert scene['layers'][1]['after'] == ['back']


@pytest.mark.parametrize('target', ['actor', 'role'])
def test_missing_scene_content_ref_blocks_publication(tmp_path, target):
    """场景typed内容引用须实际存在，Schema字符串合法不能掩盖坏actor或role。"""
    root, selection = new_fixture(tmp_path)
    add_tiny_scene(root)
    path = root / 'base/presentation.json'
    data = json.loads(path.read_bytes())
    layers = data['scene_templates']['room']['layers']
    if target == 'actor':
        layers[1]['target_ref']['id'] = 'actor.missing'
    else:
        layers[2]['candidate_selector']['params']['role_id'] = 'role.missing'
    path.write_text(json.dumps(data), encoding='utf8')
    output = tmp_path / 'generated'
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, output)
    assert caught.value.code == 'MissingPackSceneReference'
    assert not output.exists()


def test_presentation_cannot_read_undeclared_resource(tmp_path):
    """即使表现Schema路径合法，仍只能引用resources中已冻结的图像。"""
    root, selection = new_fixture(tmp_path)
    path = root / 'base/presentation.json'
    data = json.loads(path.read_bytes())
    data['backgrounds']['city']['path'] = 'res://assets/art/undeclared.png'
    path.write_text(json.dumps(data), encoding='utf8')
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, tmp_path / 'generated')
    assert caught.value.code == 'UndeclaredPackResource'


def test_staged_header_contract_tamper_is_rejected(tmp_path, monkeypatch):
    """回读核验发现header与锁Schema身份不一致，不能只信写入前的Python对象。"""
    root, selection = new_fixture(tmp_path)
    validate = api()._validate_products
    def tamper_header(stage, merged, schema):
        """模拟临时代header被改，继续执行真实回读校验。"""
        path = stage / 'catalog.hmcatalog.json'
        header = json.loads(path.read_bytes())
        header['schema_sha256'] = '0' * 64
        path.write_bytes(api().canonical(header))
        validate(stage, merged, schema)
    monkeypatch.setattr(api(), '_validate_products', tamper_header)
    output = tmp_path / 'generated'
    with pytest.raises(PackError) as caught:
        api().compose_pack_set([root], selection, output)
    assert caught.value.code == 'ComposedArtifactMismatch'
    assert not output.exists()


def test_resource_only_pack_needs_no_fake_content_entry(tmp_path):
    """把真实表现和PNG分离成零content_entries资源包，内容按直接依赖明确引用。"""
    import shutil
    root, selection = new_fixture(tmp_path)
    paint = root / 'paint'
    paint.mkdir()
    shutil.copytree(root / 'base/assets', paint / 'assets')
    shutil.copyfile(root / 'base/presentation.json', paint / 'presentation.json')
    base_path = root / 'base/pack.json'
    base_manifest = json.loads(base_path.read_bytes())
    paint_manifest = {**base_manifest, 'id': 'paint', 'namespace': 'paint', 'content_entries': [], 'required_capabilities': []}
    (paint / 'pack.json').write_text(json.dumps(paint_manifest), encoding='utf8')
    base_manifest.update(resources=[], presentation_entries=[], dependencies=[{'id': 'paint', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}])
    base_path.write_text(json.dumps(base_manifest), encoding='utf8')
    content_path = root / 'base/content.json'
    content = json.loads(content_path.read_bytes())
    for place in content['places']:
        place['visual_tag'] = 'paint::' + place['visual_tag']
    content_path.write_text(json.dumps(content), encoding='utf8')
    selection['packs'].append({'id': 'paint', 'version': '1.0.0'})
    output = tmp_path / 'generated'
    api().compose_pack_set([root], selection, output)
    lock = json.loads((output / 'pack-set.lock.json').read_bytes())
    assert [pack['id'] for pack in lock['packs']] == ['paint', 'base', 'addon']
    presentation = json.loads((output / 'presentation.json').read_bytes())
    assert presentation['default_background'] == 'paint::city'
    assert (output / presentation['backgrounds']['paint::city']['path'].removeprefix('res://')).exists()
