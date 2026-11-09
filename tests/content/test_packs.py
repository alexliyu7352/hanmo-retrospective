"""以真实文件快照验证包发现和确定依赖解析；不启动游戏或修改作者源。"""
import hashlib
import importlib
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


def api():
    """先断言生产模块存在，再加载真实发现与解析入口。"""
    assert (ROOT / 'tools/content/packs.py').is_file(), '包发现与解析尚未实现'
    return importlib.import_module('tools.content.packs')


def manifest(identifier='base', **updates):
    """构造最小合法资源空包；每项测试只改被检查的声明。"""
    result = {'format': 'HM-PACK', 'format_version': 1, 'id': identifier,
              'version': '1.0.0', 'namespace': identifier.replace('.', '_'),
              'rules_api': {'id': 'hanmo.rules', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'},
              'dependencies': [], 'conflicts': [], 'required_capabilities': [],
              'content_entries': [], 'presentation_entries': [], 'resources': [],
              'author': '汉末测试作者', 'license': 'CC0-1.0'}
    result.update(updates)
    return result


def write_pack(root, value, files=None):
    """在隔离目录写入完整声明，返回可用于发现的包根。"""
    root.mkdir(parents=True, exist_ok=True)
    (root / 'pack.json').write_text(json.dumps(value, ensure_ascii=False), encoding='utf8')
    for name, raw in (files or {}).items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    return root


def runtime():
    """提供调用端受信规则版本和能力；不得由作者manifest构造可用能力。"""
    return {'rules_api': {'id': 'hanmo.rules', 'version': '1.0.0'}, 'capabilities': ['multi_actor']}


def test_explicit_selection_and_dependency_first_order(tmp_path):
    """输入与依赖顺序倒置也保持依赖在前，且未选包不进入结果。"""
    dep = {'id': 'base', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}
    write_pack(tmp_path / 'base', manifest())
    write_pack(tmp_path / 'addon', manifest('addon', dependencies=[dep]))
    write_pack(tmp_path / 'unused', manifest('unused'))
    packs = api().discover_packs([tmp_path])
    selection = [{'id': 'addon', 'version': '1.0.0'}, {'id': 'base', 'version': '1.0.0'}]
    result = api().resolve_pack_set(packs[::-1], selection, runtime())
    assert [p['manifest']['id'] for p in result['packs']] == ['base', 'addon']
    assert result['runtime_verified'] is False
    assert result == api().resolve_pack_set(packs, selection[::-1], runtime())


def resolve(tmp_path, values, selected=None, target=None):
    """用真实发现记录调用解析，默认逐个显式选择测试输入。"""
    roots = [write_pack(tmp_path / str(n), value) for n, value in enumerate(values)]
    if selected is None:
        selected = [{'id': value['id'], 'version': value['version']} for value in values]
    return api().resolve_pack_set(api().discover_packs(roots), selected, target or runtime())


def expect_code(code):
    """只捕获生产PackError并核对稳定码，不用宽异常掩盖程序缺陷。"""
    return pytest.raises(importlib.import_module('tools.content.pack_errors').PackError,
                         match='.') if code is None else _ExpectedPackError(code)


class _ExpectedPackError:
    """在保留失败上下文的同时检查真实错误码。"""

    def __init__(self, code):
        """记录本次唯一预期错误，不接受普通程序异常。"""
        self.code = code
        self.context = pytest.raises(importlib.import_module('tools.content.pack_errors').PackError)

    def __enter__(self):
        """进入pytest原生异常检查作用域。"""
        return self.context.__enter__()

    def __exit__(self, *args):
        """先验证异常类型，再核对精确错误码。"""
        result = self.context.__exit__(*args)
        assert self.context.excinfo.value.code == self.code
        return result


def dependency(identifier, minimum='1.0.0', maximum='2.0.0'):
    """构造半开版本范围，避免测试在不同位置暗用不同默认值。"""
    return {'id': identifier, 'min_version': minimum, 'max_version_exclusive': maximum}


def test_unselected_dependency_is_not_automatically_added(tmp_path):
    """磁盘上存在依赖也不能绕过显式选择约束。"""
    values = [manifest(), manifest('addon', dependencies=[dependency('base')])]
    with expect_code('MissingPackDependency'):
        resolve(tmp_path, values, [{'id': 'addon', 'version': '1.0.0'}])


def test_wrong_dependency_version_is_actionable(tmp_path):
    """选定版本与依赖范围冲突时列出依赖者和所选版本。"""
    values = [manifest(version='2.0.0'), manifest('addon', dependencies=[dependency('base')])]
    with expect_code('PackDependencyVersionMismatch') as caught:
        resolve(tmp_path, values)
    assert caught.value.details['selected_version'] == '2.0.0'
    assert caught.value.details['id'] == 'addon'


@pytest.mark.parametrize('selected,code', [
    ([{'id': 'missing', 'version': '1.0.0'}], 'MissingPack'),
    ([{'id': 'base', 'version': '9.0.0'}], 'PackVersionMismatch'),
    ([{'id': 'base', 'version': '1.0.0'}, {'id': 'base', 'version': '2.0.0'}], 'PackVersionConflict'),
    ([], 'InvalidPackSelection'),
    ([{'id': 'base', 'version': 'latest'}], 'InvalidPackSelection'),
    ([{'id': 'base', 'version': '1.0.0', 'optional': True}], 'InvalidPackSelection'),
])
def test_selection_is_exact_and_single_version(tmp_path, selected, code):
    """未知、浮动、多版本或额外选择字段都不被静默纠正。"""
    with expect_code(code):
        resolve(tmp_path, [manifest()], selected)


def test_dependency_cycle_fails_before_compilation(tmp_path):
    """有环的所选包不会生成部分排序结果。"""
    values = [manifest('first', dependencies=[dependency('second')]),
              manifest('second', dependencies=[dependency('first')])]
    with expect_code('PackDependencyCycle') as caught:
        resolve(tmp_path, values)
    assert caught.value.details['packs'] == ['first', 'second']


def test_self_dependency_is_a_cycle(tmp_path):
    """自引用不因已经发现自身而被当作满足。"""
    with expect_code('PackDependencyCycle'):
        resolve(tmp_path, [manifest(dependencies=[dependency('base')])])


def test_conflicts_are_checked_in_both_directions(tmp_path):
    """单方冲突声明足以拒绝同选，不要求两包互相重复声明。"""
    with expect_code('PackConflict'):
        resolve(tmp_path, [manifest(), manifest('addon', conflicts=[dependency('base')])])
    resolve(tmp_path / 'outside', [manifest(version='2.0.0'), manifest('addon', conflicts=[dependency('base')])])


def test_namespace_is_unique_only_in_selected_set(tmp_path):
    """未选包的同namespace不妨碍独立内容集，所选重名明确拒绝。"""
    values = [manifest(), manifest('addon', namespace='base')]
    with expect_code('PackNamespaceConflict'):
        resolve(tmp_path, values)
    assert len(resolve(tmp_path, values, [{'id': 'base', 'version': '1.0.0'}])['packs']) == 1


def test_topological_layers_have_fixed_order(tmp_path):
    """同层先排序，下一层不能因字母更小插到当前层之前。"""
    values = [manifest('a_child', dependencies=[dependency('b_parent')]),
              manifest('z_independent'), manifest('b_parent')]
    result = resolve(tmp_path, values)
    assert [item['manifest']['id'] for item in result['packs']] == ['b_parent', 'z_independent', 'a_child']


def test_versions_compare_integer_components(tmp_path):
    """1.10.0应大于1.9.0，范围上界仍排除相等版本。"""
    result = resolve(tmp_path, [manifest(version='1.10.0'), manifest('addon', dependencies=[dependency('base', '1.9.0', '1.11.0')])])
    assert result['packs'][0]['manifest']['version'] == '1.10.0'


def test_versioned_author_directories_preserve_exact_old_selection(tmp_path):
    """同包不同显式版本可并存；新作者目录不能替换旧选择或同时混入同一内容集。"""
    for folder, version in [('qingchuan_study', '1.2.0'), ('qingchuan_study_1_3', '1.3.0')]:
        write_pack(tmp_path / folder, manifest('qingchuan.study', version=version, namespace='qingstudy'))
    discovered=api().discover_packs([tmp_path])
    assert [(row['manifest']['id'], row['manifest']['version']) for row in discovered] == [
        ('qingchuan.study', '1.2.0'), ('qingchuan.study', '1.3.0')]
    for version in ['1.2.0', '1.3.0']:
        selected=api().resolve_pack_set(discovered, [{'id':'qingchuan.study', 'version':version}], runtime())
        assert len(selected['packs']) == 1
        assert selected['packs'][0]['manifest']['version'] == version
    with expect_code('PackVersionConflict'):
        api().resolve_pack_set(discovered, [{'id':'qingchuan.study', 'version':version} for version in ['1.2.0','1.3.0']], runtime())


def test_qingchuan_new_author_version_preserves_original_inputs():
    """新1.3只增加独立作者目录；核原1.2文本字节与美术声明，不冒称缺失原图已组合。"""
    old=ROOT/'content/packs/authored/qingchuan_study'
    new=ROOT/'content/packs/authored/qingchuan_study_1_3'
    original={
        'README.md':'8b36d2da703967ae57bc26bab4ee7a59898ca57cdb1627691884f17e58512845',
        'content.json':'13204c8e07240490d3134e44119a132c0eb2b435f9589bd23cb22f80fb33926e',
        'pack.json':'0188db21234cfee7a7d528644c8b4ae0e0cd303050a9753bce81ede6ee1b8822',
        'presentation.json':'8c538a48aa7433ed364935673c25ffe95eb0d0013599409bd2b971f802223a3e',
        'assets/art/project/provenance.json':'e8076868440ae4ff8104972f8145680bdfa843546467f1d0c031c6094cf6155c',
    }
    for name, digest in original.items():
        assert hashlib.sha256((old/name).read_bytes()).hexdigest()==digest
    prior=json.loads((old/'pack.json').read_text())
    current=json.loads((new/'pack.json').read_text())
    assert prior['version']=='1.2.0' and current['version']=='1.3.0'
    assert {**current, 'version':'1.2.0'}==prior
    source=json.loads((new/'content.json').read_text())
    assert source['id']==current['id']=='qingchuan.study' and source['version']=='1.3.0'
    for name in ['presentation.json','assets/art/project/provenance.json']:
        assert (old/name).read_bytes()==(new/name).read_bytes()
    for version in ['1.2.0','1.3.0']:
        selection=json.loads((ROOT/('content/packs/selections/qingchuan-study-'+version+'.json')).read_text())
        assert selection['packs']==[{'id':'qingchuan.study','version':version}]


@pytest.mark.parametrize('version', ['01.0.0', '1.0', '1.0.0-dev', '1.0.0+build', '1.-1.0', '1.0.0\n'])
def test_versions_reject_ambiguous_spelling(tmp_path, version):
    """版本身份不能包含前导零、预发行或未约定元信息。"""
    with expect_code('InvalidPackManifest'):
        resolve(tmp_path, [manifest(version=version)])


def test_bad_range_and_duplicate_requirement_rejected(tmp_path):
    """空范围和同ID重复依赖不通过首轮结构语义检查。"""
    with expect_code('InvalidVersionRange'):
        resolve(tmp_path, [manifest(dependencies=[dependency('base', '1.0.0', '1.0.0')])])
    with expect_code('DuplicatePackRequirement'):
        resolve(tmp_path, [manifest(dependencies=[dependency('other'), dependency('other', '1.1.0')])])


@pytest.mark.parametrize('changes,code', [
    ({'required_capabilities': ['unimplemented']}, 'UnsupportedPackCapability'),
    ({'rules_api': dependency('other.rules')}, 'IncompatibleRulesApi'),
    ({'rules_api': dependency('hanmo.rules', '1.1.0', '2.0.0')}, 'IncompatibleRulesApi'),
    ({'rules_api': dependency('hanmo.rules', '0.0.0', '1.0.0')}, 'IncompatibleRulesApi'),
])
def test_runtime_is_not_derived_from_manifest(tmp_path, changes, code):
    """作者声明不能注册规则能力或改变实际规则API版本。"""
    with expect_code(code):
        resolve(tmp_path, [manifest(**changes)])


def test_runtime_loader_uses_project_declarations():
    """可用能力来自既有受信合同，pack_runtime只负责API身份。"""
    actual = api().load_pack_runtime()
    assert actual['rules_api'] == {'id': 'hanmo.rules', 'version': '1.0.0'}
    assert 'multi_actor' in actual['capabilities']
    assert len(actual['capabilities']) == len(set(actual['capabilities']))


def test_same_manifest_copy_deduplicates_but_different_bytes_conflict(tmp_path):
    """相同副本合一；相同解析内容但不同原始字节仍是身份冲突。"""
    a = write_pack(tmp_path / 'a', manifest())
    b = write_pack(tmp_path / 'b', manifest())
    assert len(api().discover_packs([b, a, a / 'pack.json'])) == 1
    (b / 'pack.json').write_bytes((b / 'pack.json').read_bytes() + b'\n')
    with expect_code('PackIdentityConflict'):
        api().discover_packs([a, b])


def test_same_version_changed_entry_bytes_conflict(tmp_path):
    """原manifest字节相同也不能隐藏内容入口变化。"""
    value = manifest(content_entries=['data/content.json'])
    a = write_pack(tmp_path / 'a', value, {'data/content.json': b'{"texts":{}}'})
    b = write_pack(tmp_path / 'b', value, {'data/content.json': b'{"texts":{}}\n'})
    with expect_code('PackIdentityConflict'):
        api().discover_packs([a, b])


def test_presentation_entry_is_in_content_hash(tmp_path):
    """表现入口也纳入声明输入摘要，防止同版只换表现配置。"""
    a = write_pack(tmp_path / 'a', manifest(presentation_entries=['art.json']), {'art.json': b'{}'})
    first = api().discover_packs([a])[0]
    (a / 'art.json').write_bytes(b'{}\n')
    second = api().discover_packs([a])[0]
    assert first['manifest_sha256'] == second['manifest_sha256']
    assert first['content_sha256'] != second['content_sha256']
    assert first['files']['art.json'] == b'{}'


@pytest.mark.parametrize('raw', [b'{"format":"HM-PACK","format":"other"}', b'{"n":NaN}', b'{"n":1e999}', b'[]', b'{"x":"\\ud800"}', b'\xff'])
def test_manifest_uses_shared_strict_json_reader(tmp_path, raw):
    """重复键、非有限数、非对象和坏UTF-8在Schema前拒绝。"""
    tmp_path.joinpath('pack.json').write_bytes(raw)
    with expect_code('InvalidPackJson'):
        api().discover_packs([tmp_path])


def test_json_entry_uses_shared_bounds(tmp_path):
    """入口深度和2MiB字节限制不因新包入口被绕开。"""
    root = write_pack(tmp_path, manifest(content_entries=['content.json']), {'content.json': b'{"x":' * 34 + b'0' + b'}' * 34})
    with expect_code('InvalidPackJson'):
        api().discover_packs([root])
    (root / 'content.json').write_bytes(b' ' * (2 * 1024 * 1024 + 1))
    with expect_code('InvalidPackJson'):
        api().discover_packs([root])


@pytest.mark.parametrize('path', ['../outside.json', '/tmp/outside.json', 'C:/outside.json', 'nested/../entry.json', './entry.json', 'nested//entry.json', 'nested\\entry.json'])
def test_paths_reject_aliases_and_escape(tmp_path, path):
    """路径规则跨平台一致，不等文件存在才检查声明逃逸。"""
    with expect_code('InvalidPackManifest'):
        api().discover_packs([write_pack(tmp_path, manifest(content_entries=[path]))])


def test_symlinks_are_rejected_in_entry_and_discovery_parent(tmp_path):
    """包内和发现根祖先的链接都不能经resolve掩盖。"""
    outside = write_pack(tmp_path / 'outside', manifest())
    linked = tmp_path / 'linked'
    linked.symlink_to(outside, target_is_directory=True)
    with expect_code('InvalidPackPath'):
        api().discover_packs([linked / 'pack.json'])
    root = write_pack(tmp_path / 'pack', manifest(content_entries=['entry.json']))
    (root / 'entry.json').symlink_to(outside / 'pack.json')
    with expect_code('InvalidPackPath'):
        api().discover_packs([root])


@pytest.mark.parametrize('name', ['mod.gd', 'mod.pck', 'mod.zip', 'mod.dll', 'scene.tscn'])
def test_undeclared_executable_or_archive_member_is_rejected(tmp_path, name):
    """未列入资源表的脚本或嵌套归档也不能混入导入目录。"""
    root = write_pack(tmp_path, manifest(), {name: b'payload'})
    with expect_code('UnsupportedPackResource'):
        api().discover_packs([root])


def resource(path='art/icon.png', raw=b'png bytes', **changes):
    """资源测试只验证路径与原字节摘要，解码属于现有表现校验层。"""
    result = {'path': path, 'kind': 'image', 'required': True,
              'sha256': hashlib.sha256(raw).hexdigest(), 'license': 'CC0-1.0'}
    result.update(changes)
    return result


def test_resources_are_frozen_and_hash_checked(tmp_path):
    """发现后改文件不改变已冻结快照，下一次发现须拒绝摘要失配。"""
    root = write_pack(tmp_path, manifest(resources=[resource()]), {'art/icon.png': b'png bytes'})
    before = api().discover_packs([root])[0]
    assert before['files']['art/icon.png'] == b'png bytes'
    (root / 'art/icon.png').write_bytes(b'changed')
    assert before['files']['art/icon.png'] == b'png bytes'
    with expect_code('PackResourceDigestMismatch'):
        api().discover_packs([root])


def test_optional_missing_resources_are_explicit_and_affect_hash(tmp_path):
    """可选缺件必须有清单，恢复文件后资源摘要会改变。"""
    root = write_pack(tmp_path, manifest(resources=[resource(required=False)]))
    missing = api().discover_packs([root])[0]
    assert missing['missing_resources'] == ['art/icon.png']
    assert 'art/icon.png' in missing['file_paths']
    assert 'art/icon.png' not in missing['files']
    (root / 'art').mkdir()
    (root / 'art/icon.png').write_bytes(b'png bytes')
    restored = api().discover_packs([root])[0]
    assert restored['resources_sha256'] != missing['resources_sha256']


def test_missing_required_and_wrong_kind_are_errors(tmp_path):
    """必需缺件和类型扩展不符都不能作为纯资源空包通过。"""
    with expect_code('MissingPackResource'):
        api().discover_packs([write_pack(tmp_path / 'missing', manifest(resources=[resource()]))])
    with expect_code('UnsupportedPackResource'):
        api().discover_packs([write_pack(tmp_path / 'kind', manifest(resources=[resource(kind='font')]))])


def test_duplicate_path_across_entry_scopes_is_rejected(tmp_path):
    """内容入口、表现入口和资源不能争用同一规范路径。"""
    root = write_pack(tmp_path, manifest(content_entries=['entry.json'], presentation_entries=['entry.json']), {'entry.json': b'{}'})
    with expect_code('DuplicatePackPath'):
        api().discover_packs([root])


def test_pack_schema_rejects_unknown_fields_at_every_object(tmp_path):
    """manifest、依赖与资源嵌套对象都保持封闭声明协议。"""
    for number, changes in enumerate([{'script': 'run.gd'}, {'dependencies': [{**dependency('other'), 'optional': True}]}, {'resources': [{**resource(), 'execute': True}]}]):
        with expect_code('InvalidPackManifest'):
            api().discover_packs([write_pack(tmp_path / str(number), manifest(**changes))])


def test_catalog_lock_has_no_digest_cycle():
    """锁只记录数据摘要，禁止自摘要、路径和时间戳污染精确身份。"""
    from tools.content.validation import StrictValidator
    from tools.content.io import canonical
    locked = {'format': 'HM-PACK-SET', 'format_version': 1,
              'packs': [{'id': 'base', 'namespace': 'base', 'version': '1.0.0',
                         'manifest_sha256': 'a' * 64, 'content_sha256': 'b' * 64, 'resources_sha256': 'c' * 64}],
              'rules_api': {'id': 'hanmo.rules', 'version': '1.0.0'},
              'rule_capabilities_sha256': 'd' * 64, 'schema_sha256': 'e' * 64,
              'catalog_sha256': hashlib.sha256(canonical({'texts': {}})).hexdigest(),
              'presentation_sha256': 'f' * 64}
    validator = StrictValidator(api().PACK_SET_SCHEMA)
    assert validator.is_valid(locked)
    for key in ('digest', 'pack_set_digest', 'root', 'timestamp'):
        assert not validator.is_valid({**locked, key: 'value'})
    assert canonical(locked).endswith(b'\n')


def test_legacy_fixed_mapping_preserves_author_bytes_and_ids(tmp_path):
    """五个旧源固定映射且保持原ID，不解释作者../../逃逸入口。"""
    for name, source in api().LEGACY_SOURCES.items():
        original = source.read_bytes()
        source_data = json.loads(original)
        value = manifest(source_data['id'], version=source_data['version'], namespace=name, legacy_source=name)
        root = write_pack(tmp_path / name, value)
        record = api().discover_packs([root])[0]
        logical = 'content/foundation/' + name + '.json'
        assert record['files'][logical] == original
        assert json.loads(record['files'][logical])['actors'][0]['id'] == source_data['actors'][0]['id']
        assert record['resource_paths'], '旧表现资源也须冻结以保护同版本身份'
        assert source.read_bytes() == original


def test_legacy_branch_rejects_arbitrary_source_and_normal_entries(tmp_path):
    """受信旧源适配不能被扩写为任意宿主文件读取。"""
    for number, updates in enumerate([{'legacy_source': '../outside'}, {'legacy_source': 'procurement', 'content_entries': ['entry.json']}]):
        with expect_code('InvalidPackManifest'):
            api().discover_packs([write_pack(tmp_path / str(number), manifest(**updates))])


def test_legacy_imports_require_selected_direct_dependency(tmp_path):
    """合法旧引用显式导入；省略依赖或映射别名都须失败。"""
    old = manifest('foundation.procurement', version='0.3.1', namespace='old', legacy_source='procurement')
    addon = manifest('addon', dependencies=[dependency(old['id'], '0.3.0', '0.4.0')], legacy_imports={'old::actor.player': 'actor.player'})
    assert resolve(tmp_path, [old, addon])['packs'][0]['manifest']['id'] == old['id']
    with expect_code('InvalidLegacyImport'):
        resolve(tmp_path / 'missing', [old, {**addon, 'dependencies': []}])
    with expect_code('InvalidLegacyImport'):
        resolve(tmp_path / 'alias', [old, {**addon, 'legacy_imports': {'old::alias': 'actor.player'}}])


def test_multiple_legacy_packs_are_not_implicitly_merged(tmp_path):
    """旧同名实体不能通过选两包被重命名或后写覆盖。"""
    first = manifest('foundation.procurement', version='0.3.1', namespace='one', legacy_source='procurement')
    second = manifest('foundation.two_stories', version='0.3.1', namespace='two', legacy_source='two_stories')
    with expect_code('LegacyPackConflict'):
        resolve(tmp_path, [first, second])


def test_declared_resource_size_and_directory_total_are_bounded(tmp_path):
    """稀疏文件验证真实64MiB读取上限和256MiB目录预检，不装入超限总包。"""
    root = write_pack(tmp_path / 'resource', manifest(resources=[resource(path='large.png')]))
    with (root / 'large.png').open('wb') as stream:
        stream.truncate(64 * 1024 * 1024 + 1)
    with expect_code('PackResourceLimit'):
        api().discover_packs([root])
    total = write_pack(tmp_path / 'total', manifest())
    with (total / 'unused.bin').open('wb') as stream:
        stream.truncate(256 * 1024 * 1024 + 1)
    with expect_code('PackTotalLimit'):
        api().discover_packs([total])


def test_directory_member_limit_precedes_entry_loading(tmp_path):
    """4096成员上限检查目录实际成员，不能仅统计声明数组。"""
    root = write_pack(tmp_path, manifest())
    for index in range(4096):
        (root / ('note-' + str(index) + '.txt')).touch()
    with expect_code('PackMemberLimit'):
        api().discover_packs([root])


def test_discovery_rejects_zip_and_reports_missing_roots(tmp_path):
    """发现入口不会擅自解压ZIP或将不存在目录当成合法空集。"""
    archive = tmp_path / 'pack.zip'
    archive.write_bytes(b'not an archive')
    for path in (archive, tmp_path / 'missing'):
        with expect_code('InvalidPackRoot'):
            api().discover_packs([path])


def test_legacy_identity_cannot_be_relabelled(tmp_path):
    """固定旧源不能冒充另一个包或版本以绕过精确恢复身份。"""
    with expect_code('LegacyIdentityMismatch'):
        api().discover_packs([write_pack(tmp_path, manifest(legacy_source='procurement'))])


def test_resolve_does_not_mutate_snapshot_or_selection(tmp_path):
    """解析只排序返回视图，不改冻结源、manifest语义数组或选择数组。"""
    from copy import deepcopy
    root = write_pack(tmp_path, manifest(content_entries=['entry.json']), {'entry.json': b'{"steps":["second","first"]}'})
    snapshots = api().discover_packs([root])
    selected = [{'id': 'base', 'version': '1.0.0'}]
    before = deepcopy((snapshots, selected))
    api().resolve_pack_set(snapshots, selected, runtime())
    assert (snapshots, selected) == before


def test_checked_in_manifest_fixture_relocates_without_identity_changes(tmp_path):
    """持久fixture能在不同根发现并保持所有锁摘要字段相同。"""
    import shutil
    fixture = ROOT / 'tests/fixtures/content_packs/manifest_contract'
    shutil.copytree(fixture, tmp_path / 'relocated')
    original = api().discover_packs([fixture])
    relocated = api().discover_packs([tmp_path / 'relocated'])
    assert len(original) == len(relocated) == 2
    for left, right in zip(original, relocated):
        for field in ('manifest_sha256', 'content_sha256', 'resources_sha256'):
            assert left[field] == right[field]
    selected = [{'id': pack['manifest']['id'], 'version': pack['manifest']['version']} for pack in original]
    assert [pack['manifest']['id'] for pack in api().resolve_pack_set(original, selected[::-1], runtime())['packs']] == ['fixture.base', 'fixture.addon']


@pytest.mark.parametrize('field,valid', [('id', 'a' * 80), ('namespace', 'a' * 32)])
def test_identity_full_length_boundaries_remain_valid(tmp_path, field, valid):
    """满80字符包ID与满32字符namespace仍合法，不靠缩短上限修复换行。"""
    value = manifest(**{field: valid})
    result = resolve(tmp_path, [value])
    assert result['packs'][0]['manifest'][field] == valid
    with expect_code('InvalidPackManifest'):
        resolve(tmp_path / 'too-long', [{**value, field: valid + 'a'}])


@pytest.mark.parametrize('field,value', [
    ('id', 'base\n'), ('id', 'a' * 80 + '\n'),
    ('namespace', 'base\n'), ('namespace', 'a' * 32 + '\n'),
    ('id', 'base\r\n'), ('namespace', 'base\u2028'),
])
def test_manifest_identity_rejects_trailing_line_terminators(tmp_path, field, value):
    """Schema必须匹配整个字符串，不能利用Python的$尾换行例外进入解析器。"""
    with expect_code('InvalidPackManifest'):
        resolve(tmp_path, [manifest(**{field: value})])


@pytest.mark.parametrize('field,value', [('id', 'base\n'), ('id', 'a' * 80 + '\n'), ('version', '1.0.0\n')])
def test_selection_identity_rejects_trailing_newline(tmp_path, field, value):
    """选择文件与manifest采用同一整串身份合同，坏选择不能命中已有包。"""
    selected = [{'id': 'base', 'version': '1.0.0', field: value}]
    with expect_code('InvalidPackSelection'):
        resolve(tmp_path, [manifest()], selected)


@pytest.mark.parametrize('field,value', [
    ('id', 'base\n'), ('id', 'a' * 80 + '\n'),
    ('namespace', 'base\n'), ('namespace', 'a' * 32 + '\n'),
    ('version', '1.0.0\n'), ('manifest_sha256', 'a' * 64 + '\n'),
])
def test_lock_identity_rejects_trailing_newline(field, value):
    """锁的包身份及摘要同样绝对终止，与GDScript读取边界一致。"""
    from tools.content.validation import StrictValidator
    schema = {**api().PACK_SET_SCHEMA, 'oneOf': [{'$ref': '#/$defs/locked_pack'}]}
    locked = {'id': 'base', 'namespace': 'base', 'version': '1.0.0',
              'manifest_sha256': 'a' * 64, 'content_sha256': 'b' * 64, 'resources_sha256': 'c' * 64}
    assert StrictValidator(schema).is_valid(locked)
    assert not StrictValidator(schema).is_valid({**locked, field: value})
