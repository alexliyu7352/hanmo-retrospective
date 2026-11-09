"""以真实临时源码验证依赖、元数据和发行边界；不把声明当作写屏障。"""
import importlib
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


def checker():
    """缺实现时明确红灯，避免导入失败掩盖验收目标。"""
    assert (ROOT / 'tools/check_architecture.py').is_file(), '缺少受控架构检查器'
    return importlib.import_module('tools.check_architecture').check_architecture


def fixture(tmp_path, edges=(), *, legacy=()):
    """建立最小真实核心图和一一文件归属，所有边均显式声明。"""
    files = {'game/core/a.gd': 'a', 'game/core/b.gd': 'b'}
    for name in files:
        path = tmp_path / name; path.parent.mkdir(parents=True, exist_ok=True)
        imports = '\n'.join('const X%d = preload("res://%s")' % (i, target.removeprefix('game/'))
                            for i, (source, target) in enumerate(edges) if source == name)
        path.write_text('extends RefCounted\n' + imports + '\n')
    contract = {'format': 'HM-MODULE-CONTRACTS/1', 'modules': [
        {'id': name, 'version': 'f10-architecture-1', 'requires': {}, 'optional_requires': {},
         'capabilities': [], 'owns': [], 'validators': []} for name in ['a', 'b']],
        'files': files, 'source_edges': [{'source': a, 'target': b} for a, b in edges],
        'legacy_edges': list(legacy), 'capability_modules': {}}
    path = tmp_path / 'contracts.json'; path.write_text(json.dumps(contract))
    return path, contract


def save(path, contract):
    """只改夹具合同，不触碰项目的可信声明。"""
    path.write_text(json.dumps(contract))


def test_legal_core_dag(tmp_path):
    """合法模块与文件DAG必须可接入。"""
    path, contract = fixture(tmp_path, [('game/core/a.gd', 'game/core/b.gd')])
    contract['modules'][0]['requires'] = {'b': 'f10-architecture-1'}; save(path, contract)
    assert checker()(tmp_path, path)['ok']


@pytest.mark.parametrize('target', ['game/ui/panel.gd', 'game/storage/store.gd', 'game/platform/linux.gd'])
def test_core_external_dependency_is_never_exempted(tmp_path, target):
    """声明或旧边也不能使core依赖表现、存储和平台。"""
    path, contract = fixture(tmp_path, [('game/core/a.gd', target)])
    p = tmp_path / target; p.parent.mkdir(parents=True); p.write_text('extends RefCounted\n')
    contract['files'][target] = 'b'; save(path, contract)
    assert not checker()(tmp_path, path)['ok']


def test_file_cycle_fails_even_when_every_edge_is_legacy(tmp_path):
    """准确兼容记录不豁免真实源码环。"""
    edges = [('game/core/a.gd', 'game/core/b.gd'), ('game/core/b.gd', 'game/core/a.gd')]
    legacy = [{'source': a, 'target': b, 'reason': '保留现有跨域查询', 'task': 'A1.7'} for a, b in edges]
    path, _ = fixture(tmp_path, edges, legacy=legacy)
    assert not checker()(tmp_path, path)['ok']


@pytest.mark.parametrize('mutation', ['undeclared', 'missing_file', 'unowned', 'stale_edge', 'dynamic', 'module_cycle'])
def test_uncontrolled_or_incomplete_graph_fails(tmp_path, mutation):
    """新增边、丢失文件、漏归属及动态加载都不能静默通过。"""
    path, contract = fixture(tmp_path)
    if mutation == 'undeclared':
        (tmp_path / 'game/core/a.gd').write_text('extends RefCounted\nconst B=load("res://core/b.gd")\n')
    elif mutation == 'missing_file':
        (tmp_path / 'game/core/b.gd').unlink()
    elif mutation == 'unowned':
        (tmp_path / 'game/core/c.gd').write_text('extends RefCounted\n')
    elif mutation == 'stale_edge':
        contract['source_edges'].append({'source': 'game/core/a.gd', 'target': 'game/core/b.gd'})
    elif mutation == 'dynamic':
        (tmp_path / 'game/core/a.gd').write_text('extends RefCounted\nfunc read(path):\n\treturn load(path)\n')
    else:
        contract['modules'][0]['requires'] = {'b': 'f10-architecture-1'}
        contract['modules'][1]['requires'] = {'a': 'f10-architecture-1'}
    save(path, contract)
    assert not checker()(tmp_path, path)['ok']


def test_comments_and_strings_are_not_imports(tmp_path):
    """注释和显示文字不能被误认成真实加载调用。"""
    path, _ = fixture(tmp_path)
    (tmp_path / 'game/core/a.gd').write_text('extends RefCounted\n# load("res://ui/x.gd")\nconst TEXT = \'preload("res://ui/x.gd")\'\n')
    assert checker()(tmp_path, path)['ok']


def test_project_contract_and_economy_boundary(tmp_path):
    """实际项目必须通过；经济重新依赖聚合Schema必须被检查器抓住。"""
    check = checker()
    assert check(ROOT, ROOT / 'game/data/module_contracts.json')['ok']
    import shutil
    shutil.copytree(ROOT / 'game/core', tmp_path / 'game/core')
    shutil.copytree(ROOT / 'game/data', tmp_path / 'game/data', ignore=shutil.ignore_patterns('generated'))
    path = tmp_path / 'game/core/economy_rules.gd'
    path.write_text(path.read_text() + '\nconst Bad=preload("res://core/state_schema.gd")\n')
    assert not check(tmp_path, tmp_path / 'game/data/module_contracts.json')['ok']


def test_module_contract_binds_receipt_and_package_identity():
    """模块合同改变后旧schema摘要失效，导出必须包含其真实字节。"""
    from tools.verification_report import schema_identity
    from tools import content_stage, verify
    assert 'game/data/module_contracts.json' in {x['path'] for x in schema_identity(ROOT)['files']}
    manifest = content_stage.read_manifest(ROOT / 'game/data/generated')
    rows = content_stage.package_files(ROOT, manifest)
    assert {'path': 'res://data/module_contracts.json', 'sha256': content_stage.digest(ROOT / 'game/data/module_contracts.json')} in rows
    assert 'architecture_dependencies' in verify.stage_plan(ROOT, ROOT / 'verification/fixture', 'godot', 'linux')
    assert 'test_module_registry' in verify.TESTS


@pytest.mark.parametrize('mutation', ['unknown_owner', 'duplicate_owner', 'unknown_validator', 'missing_alias', 'wrong_alias', 'no_capabilities'])
def test_project_metadata_mutations_fail(tmp_path, mutation):
    """现有描述被删改时，不能失去状态拥有权、真实引用或旧能力映射检查。"""
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    owner = next(m for m in contract['modules'] if m['owns'])
    if mutation == 'unknown_owner': owner['owns'][0]['owner'] = 'missing'
    elif mutation == 'duplicate_owner': owner['owns'].append(owner['owns'][0])
    elif mutation == 'unknown_validator':
        next(m for m in contract['modules'] if m['validators'])['validators'][0]['method'] = 'nonexistent'
    elif mutation == 'missing_alias': contract['capability_modules'].pop('world_factory')
    elif mutation == 'wrong_alias': contract['capability_modules']['world_factory'] = 'missing'
    else: contract['capability_modules'] = {}
    path = tmp_path / 'contract.json'; save(path, contract)
    assert not checker()(ROOT, path)['ok']


def test_current_discriminated_state_contract_matches_actual_kinds():
    """所有权使用当前实际kind，不能把未来猜测名称写成已存在协议。"""
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    claims = [c for m in contract['modules'] for c in m['owns']]
    actions = {c['key'] for c in claims if c['container'] == 'actions' and c['scope'] == 'kind'}
    assert actions == set('wait accept_offer report_offer deliver_message trade travel rescue service long_rest learn_skill practice_skill teach_skill estate_build family_adopt office_accept army_form army_resupply army_return'.split())
    facts = {c['key'] for c in claims if c['container'] == 'facts' and c['scope'] == 'kind'}
    assert facts == set('actor_creation personal_battle escort_arrival terrain_journey training estate_production estate_management estate_transfer estate_construction estate_funding estate_detachment estate_muster family_adoption_pending family_adoption family_adoption_failed family_designation family_succession long_rest office_resignation military_action military_muster military_supply military_casualties military_return military_battle military_contact military_occupation military_relief siege_material military_delivery campaign_arrival campaign_settlement'.split())


def test_script_inheritance_is_a_real_core_dependency(tmp_path):
    """脚本继承也不能绕过core依赖边界，即使没有preload调用。"""
    path, _ = fixture(tmp_path)
    (tmp_path / 'game/core/a.gd').write_text('extends "res://ui/panel.gd"\n')
    assert not checker()(tmp_path, path)['ok']


def test_module_contract_bytes_change_schema_identity(tmp_path):
    """通过真实文件改字节证明新合同使验证身份改变。"""
    from tools.verification_report import schema_identity
    path = tmp_path / 'game/data/module_contracts.json'; path.parent.mkdir(parents=True)
    path.write_text('{"version":1}')
    before = schema_identity(tmp_path)
    path.write_text('{"version":2}')
    assert schema_identity(tmp_path)['sha256'] != before['sha256']


PROJECT_CLASS_USES = [
    'extends ProjectPanel\n',
    'extends RefCounted\nstatic func marker():\n\treturn ProjectPanel.marker()\n',
    'extends RefCounted\nfunc echo(value: ProjectPanel) -> ProjectPanel:\n\treturn value\n',
    'extends RefCounted\nvar panels: Array[ProjectPanel] = []\n',
    'extends RefCounted\nfunc check(value):\n\treturn value is ProjectPanel\n',
    'extends RefCounted\nfunc convert(value):\n\treturn value as ProjectPanel\n',
    'extends RefCounted\nfunc pass_type(callback):\n\treturn callback.call(ProjectPanel)\n',
]


def project_class_fixture(tmp_path, code, *, folder='ui', declared=False):
    """项目类在真实源码中声明；不预先给检查器填充名称缓存。"""
    path, contract = fixture(tmp_path)
    panel = 'game/' + folder + '/panel.gd'
    source = tmp_path / panel; source.parent.mkdir(parents=True, exist_ok=True)
    source.write_text('class_name ProjectPanel\nextends RefCounted\nstatic func marker() -> String:\n\treturn "project implementation"\n')
    (tmp_path / 'game/core/a.gd').write_text(code)
    contract['files'][panel] = 'b'
    if declared:
        contract['source_edges'].append({'source': 'game/core/a.gd', 'target': panel})
        contract['modules'][0]['requires']['b'] = 'f10-architecture-1'
    save(path, contract)
    return path, contract, panel


@pytest.mark.parametrize('code', PROJECT_CLASS_USES)
@pytest.mark.parametrize('exempt', [False, True])
def test_project_class_outer_dependencies_cannot_bypass_guard(tmp_path, code, exempt):
    """继承、静态调用和参数/返回/容器类型都产生真边，声明或旧边也不得豁免外层。"""
    path, contract, panel = project_class_fixture(tmp_path, code, declared=exempt)
    if exempt:
        contract['legacy_edges'].append({'source': 'game/core/a.gd', 'target': panel,
                                         'reason': '测试不能绕过外层守卫', 'task': 'A1.7'})
        save(path, contract)
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert result['source_edges'] == 1
    assert any('core cannot depend' in error for error in result['errors'])


@pytest.mark.parametrize('code', PROJECT_CLASS_USES)
def test_declared_core_project_class_dependency_is_accepted(tmp_path, code):
    """合法核心项目类引用必须解析到精确文件，不能靠一概拒绝所有类型语法过关。"""
    path, _, _ = project_class_fixture(tmp_path, code, folder='core', declared=True)
    result = checker()(tmp_path, path)
    assert result['ok'], result
    assert result['source_edges'] == 1


def test_project_class_edge_participates_in_file_cycles(tmp_path):
    """具名类边和字面加载边组成的真实环不能被兼容声明藏住。"""
    path, contract, panel = project_class_fixture(tmp_path, 'extends ProjectPanel\n', folder='core', declared=True)
    source = tmp_path / panel
    source.write_text(source.read_text() + '\nconst A=preload("res://core/a.gd")\n')
    contract['source_edges'].append({'source': panel, 'target': 'game/core/a.gd'})
    contract['legacy_edges'] = [{**edge, 'reason': '测试环不允许豁免', 'task': 'A1.7'} for edge in contract['source_edges']]
    save(path, contract)
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert 'core source dependency cycle' in result['errors']


def test_undeclared_project_class_edge_fails(tmp_path):
    """没有声明的核心类引用也必须失败，不能只补外层路径黑名单。"""
    path, _, _ = project_class_fixture(tmp_path, 'extends ProjectPanel\n', folder='core')
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert any('undeclared source edge' in error for error in result['errors'])


def test_class_declarations_self_references_and_builtin_types_are_not_edges(tmp_path):
    """自己的类名、Godot内建类型与未使用的项目类声明不构成外部文件依赖。"""
    path, _ = fixture(tmp_path)
    (tmp_path / 'game/core/a.gd').write_text('class_name OwnClass\nextends RefCounted\nstatic func create() -> OwnClass:\n\treturn OwnClass.new()\n')
    result = checker()(tmp_path, path)
    assert result['ok'], result
    assert result['source_edges'] == 0


def test_comments_strings_and_qualified_member_names_are_not_global_classes(tmp_path):
    """字符串/注释中的类名和对象成员不解析成全局项目类；伪类声明不能进入符号表。"""
    code = ('extends RefCounted\n# ProjectPanel.marker()\nconst TEXT = "ProjectPanel"\n'
            'const DECL = "class_name ProjectPanel"\nfunc read(object):\n\treturn object.ProjectPanel\n')
    path, _, _ = project_class_fixture(tmp_path, code)
    result = checker()(tmp_path, path)
    assert result['ok'], result
    assert result['source_edges'] == 0


@pytest.mark.parametrize('code', [
    'extends RefCounted\nvar ProjectPanel = 1\n',
    'extends RefCounted\nconst ProjectPanel = 1\n',
    'extends RefCounted\nfunc read(ProjectPanel: int):\n\treturn ProjectPanel\n',
    'extends RefCounted\nfunc read():\n\tfor ProjectPanel in []:\n\t\tprint(ProjectPanel)\n',
    'extends RefCounted\nclass ProjectPanel:\n\textends RefCounted\n',
    'extends RefCounted\nvar callback = func(ProjectPanel): return ProjectPanel\n',
    'extends RefCounted\nenum Values { ProjectPanel, Other }\n',
])
def test_known_project_class_shadowing_fails_explicitly(tmp_path, code):
    """不建立完整词法作用域求值器；已知项目类同名绑定明确拒绝，而非误报成真实引用。"""
    path, _, _ = project_class_fixture(tmp_path, code)
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert any('ambiguous project class shadow' in error for error in result['errors'])
    assert not any('core cannot depend' in error for error in result['errors'])


def test_duplicate_project_class_declarations_fail_closed(tmp_path):
    """同名全局类不能由遍历顺序任选目标，也不能悄悄丢边。"""
    path, _, _ = project_class_fixture(tmp_path, 'extends ProjectPanel\n')
    (tmp_path / 'game/core/b.gd').write_text('class_name ProjectPanel\nextends RefCounted\n')
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert any('ambiguous project class declaration' in error for error in result['errors'])


LITERAL_PROJECT_CLASS_KEYS = [
    '{ProjectPanel = 1}',
    '{first = 0, ProjectPanel = 1}',
    '{\n\t\tProjectPanel # 单纯名称，不引用同名类型\n\t\t= 1\n\t}',
    '{outer = {ProjectPanel = 1}, array = [{ProjectPanel = 2}]}',
    '{text = "{ProjectPanel = ProjectPanel.marker()}", ProjectPanel = "}"}',
    '{ProjectPanel = [{inner = 1}, {"text": "ProjectPanel"}]}',
]


@pytest.mark.parametrize('expression', LITERAL_PROJECT_CLASS_KEYS)
def test_dictionary_stringname_keys_are_not_project_class_references(tmp_path, expression):
    """字典identifier=value位置是字面StringName键，嵌套/注释/字符串不应制造类依赖。"""
    code = 'extends RefCounted\nstatic func data() -> Dictionary:\n\treturn ' + expression + '\n'
    path, _, _ = project_class_fixture(tmp_path, code)
    result = checker()(tmp_path, path)
    assert result['ok'], result
    assert result['source_edges'] == 0


@pytest.mark.parametrize('expression', [
    '{ProjectPanel = ProjectPanel.marker()}',
    '{outer = {ProjectPanel = ProjectPanel}}',
    '{ProjectPanel: 1}',
    '{ProjectPanel.marker(): 1}',
    '{(ProjectPanel): 1}',
    '{ProjectPanel == null: 1}',
    '{ProjectPanel = [ProjectPanel]}',
    '{literal = {"nested": ProjectPanel.marker()}}',
])
def test_dictionary_values_and_expression_keys_keep_class_edges(tmp_path, expression):
    """只排除真正的identifier=键，值及冒号/括号/比较表达式键仍保留真实类依赖。"""
    code = 'extends RefCounted\nstatic func data() -> Dictionary:\n\treturn ' + expression + '\n'
    path, _, _ = project_class_fixture(tmp_path, code)
    result = checker()(tmp_path, path)
    assert not result['ok']
    assert result['source_edges'] == 1
    assert any('core cannot depend' in error for error in result['errors'])


def test_trade_adapter_dependency_boundary(tmp_path):
    """交易由固定组合间接接入Schema；精确门面兼容边不能豁免真实反向环。"""
    import shutil
    check = checker()
    adapter = 'game/core/domains/economy/trade_action.gd'
    runtime = 'game/core/runtime/action_runtime.gd'
    module = 'game/core/domains/economy/economy_module.gd'
    schema = 'game/core/state_schema.gd'
    assert (ROOT / adapter).is_file(), '缺少真实TradeAction'
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    assert check(ROOT, ROOT / 'game/data/module_contracts.json')['ok']
    assert contract['files'][adapter] == contract['files'][module] == 'economy'
    assert contract['files'][runtime] == 'runtime'
    builtin = 'game/core/runtime/builtin_modules.gd'
    compatibility = [e for e in contract['legacy_edges'] if e['source'] == schema and e['target'] == builtin]
    assert len(compatibility) == 1 and compatibility[0]['task'] == 'A1.5'
    assert not any(e['source'] == schema and e['target'] == adapter for e in contract['source_edges'])
    assert {e['target'] for e in contract['source_edges'] if e['source'] == schema} == {
        builtin, 'game/core/base/value_checks.gd', 'game/core/runtime/module_registry.gd',
        'game/core/runtime/world_validation.gd'}
    economy = next(m for m in contract['modules'] if m['id'] == 'economy')
    assert economy['requires']['runtime'] == 'f10-architecture-1'
    assert 'economy' not in next(m for m in contract['modules'] if m['id'] == 'runtime')['requires']
    claim = next(c for c in economy['owns'] if c['container'] == 'actions' and c['key'] == 'trade')
    assert claim['schema_refs'] == [{'path': adapter, 'method': 'validate'}]
    assert {e['target'] for e in contract['source_edges'] if e['source'] == runtime} == {
        'game/core/world_factory.gd', 'game/core/base/value_checks.gd'}
    assert {e['target'] for e in contract['source_edges'] if e['source'] == adapter} == {
        'game/core/economy_rules.gd', 'game/core/base/value_checks.gd'}
    assert {e['target'] for e in contract['source_edges'] if e['source'] == module} == {
        adapter, 'game/core/economy_rules.gd', 'game/core/runtime/module_registry.gd',
        'game/core/base/value_checks.gd', 'game/core/military_ownership.gd'}
    shutil.copytree(ROOT / 'game/core', tmp_path / 'game/core')
    shutil.copytree(ROOT / 'game/data', tmp_path / 'game/data', ignore=shutil.ignore_patterns('generated'))
    path = tmp_path / 'game/data/module_contracts.json'
    source = tmp_path / adapter; original = source.read_text()
    for target in [schema, 'game/core/runtime/builtin_modules.gd', 'game/core/world_kernel.gd']:
        mutated = json.loads(json.dumps(contract))
        edge = {'source': adapter, 'target': target}
        source.write_text(original + '\nconst Bad=preload("res://' + target.removeprefix('game/') + '")\n')
        mutated['source_edges'].append(edge)
        mutated['legacy_edges'].append({**edge, 'reason': '测试真实环不能获豁免', 'task': 'A1.5'})
        save(path, mutated)
        result = check(tmp_path, path)
        assert not result['ok'] and 'core source dependency cycle' in result['errors'], result
    source.write_text(original)
    mutated = json.loads(json.dumps(contract))
    mutated['legacy_edges'].remove(compatibility[0]); save(path, mutated)
    result = check(tmp_path, path)
    assert not result['ok'] and any('undeclared module dependency' in e and builtin in e for e in result['errors'])
    mutated['source_edges'].remove({'source': schema, 'target': builtin}); save(path, mutated)
    result = check(tmp_path, path)
    assert not result['ok'] and any('undeclared source edge' in e and builtin in e for e in result['errors'])
    save(path, contract)
    assert check(tmp_path, path)['ok']
    from tools import verify
    assert verify.TESTS.count('test_registered_trade_contracts') == 1


@pytest.mark.parametrize('mutation', ['state_abi', 'missing_stage', 'unknown_stage', 'string_order',
                                     'float_order', 'bool_order', 'negative_order', 'duplicate_order',
                                     'legacy_path', 'legacy_method', 'legacy_abi', 'legacy_duplicate',
                                     'legacy_owner', 'legacy_collection'])
def test_registered_validation_metadata_rejects_malformed_contract(tmp_path, mutation):
    """新二参状态次序和独立三参过渡引用都必须受到真实静态门禁检查。"""
    path, contract = fixture(tmp_path)
    source = tmp_path / 'game/core/a.gd'
    source.write_text('extends RefCounted\nstatic func state(s, d):\n\treturn {}\n'
                      'static func action(s, d, a):\n\treturn {}\n')
    module = contract['modules'][0]
    module['validators'] = [{'id': 'a.state', 'path': 'game/core/a.gd', 'method': 'state',
                             'signature': 'state,data', 'stage': 'before_actions', 'order': 0}]
    module['owns'] = [{'container': 'actions', 'scope': 'kind', 'key': 'work', 'owner': 'a',
                       'schema_refs': [{'path': 'game/core/a.gd', 'method': 'action'}]}]
    module['legacy_action_validators'] = [{'kind': 'work', 'path': 'game/core/a.gd', 'method': 'action',
                                         'signature': 'state,data,action'}]
    save(path, contract)
    assert checker()(tmp_path, path)['ok']
    state = module['validators'][0]; legacy = module['legacy_action_validators'][0]
    if mutation == 'state_abi': state['signature'] = 'state,catalog'
    elif mutation == 'missing_stage': state.pop('stage')
    elif mutation == 'unknown_stage': state['stage'] = 'between'
    elif mutation == 'string_order': state['order'] = '0'
    elif mutation == 'float_order': state['order'] = 0.0
    elif mutation == 'bool_order': state['order'] = True
    elif mutation == 'negative_order': state['order'] = -1
    elif mutation == 'duplicate_order': module['validators'].append({**state, 'id': 'second'})
    elif mutation == 'legacy_path': legacy['path'] = 'game/core/missing.gd'
    elif mutation == 'legacy_method': legacy['method'] = 'missing'
    elif mutation == 'legacy_abi': legacy['signature'] = 'state,data'
    elif mutation == 'legacy_duplicate': module['legacy_action_validators'].append(legacy.copy())
    elif mutation == 'legacy_owner': module['owns'] = []
    else: module['legacy_action_validators'] = {}
    save(path, contract)
    assert not checker()(tmp_path, path)['ok']


def test_time_queries_have_exact_base_dependencies():
    """共享历法仅归基础层；真实身份、工厂及家族调用声明精确依赖，不新增迁移豁免。"""
    contract=json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    target='game/core/base/time_rules.gd'
    assert contract['files'][target]=='base'
    sources={'game/core/actor_identity.gd','game/core/world_factory.gd','game/core/family_rules.gd'}
    assert {edge['source'] for edge in contract['source_edges'] if edge['target']==target}==sources
    assert not any(edge['source']==target for edge in contract['source_edges'])
    assert not any(target in (edge['source'],edge['target']) for edge in contract['legacy_edges'])
    modules={module['id']:module for module in contract['modules']}
    for owner in ('identity','family'):
        assert modules[owner]['requires']['base']==modules['base']['version']=='f10-architecture-1'
    assert checker()(ROOT, ROOT / 'game/data/module_contracts.json')['ok']


def test_runtime_behavior_queries_keep_identity_dependency_direction():
    """统一查询只向身份层读取；工厂不反向授权，旅行与军政声明真实依赖而不加豁免。"""
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    target = 'game/core/domains/identity/actor_behavior.gd'
    assert contract['files'][target] == 'identity'
    assert {edge['target'] for edge in contract['source_edges'] if edge['source'] == target} == {'game/core/actor_identity.gd'}
    assert {edge['source'] for edge in contract['source_edges'] if edge['target'] == target} == {
        'game/core/family_rules.gd', 'game/core/travel_schedule.gd',
        'game/core/military_schedule.gd', 'game/core/social_rules.gd', 'game/core/encounter_state.gd',
    }
    assert not any(target in (edge['source'], edge['target']) for edge in contract['legacy_edges'])
    modules = {module['id']: module for module in contract['modules']}
    for owner in ('family', 'travel', 'military', 'social'):
        assert modules[owner]['requires']['identity'] == modules['identity']['version']
    assert checker()(ROOT, ROOT / 'game/data/module_contracts.json')['ok']


def test_military_asset_adapter_contract_has_registered_production_boundary():
    """资产注册仅归既有military模块；规则保持唯一，跨域完成协调只有明确出口。"""
    check = checker()
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    assert check(ROOT, ROOT / 'game/data/module_contracts.json')['ok']
    action = 'game/core/domains/military/military_action.gd'
    module = 'game/core/domains/military/military_module.gd'
    assets = 'game/core/military_assets.gd'
    runtime = 'game/core/runtime/module_registry.gd'
    assert contract['files'][action] == contract['files'][module] == 'military'
    assert {e['target'] for e in contract['source_edges'] if e['source'] == action} == {assets}
    assert {e['target'] for e in contract['source_edges'] if e['source'] == module} == {assets, action, runtime, 'game/core/domains/military/campaign_module.gd', 'game/core/domains/military/military_combat_commands.gd', 'game/core/domains/military/siege_commands.gd'}
    military = next(m for m in contract['modules'] if m['id'] == 'military')
    assert military['requires']['runtime'] == 'f10-architecture-1'
    assert military['legacy_action_validators'] == []
    for claim in military['owns']:
        if claim['container'] == 'actions':
            assert claim['schema_refs'] == [{'path': assets, 'method': 'validate_action'}]
    coordination = 'game/core/domains/military/military_coordination.gd'
    assert contract['files'][coordination] == 'military'
    assert {e['target'] for e in contract['source_edges'] if e['source'] == coordination} == {
        assets, 'game/core/military_world.gd', 'game/core/campaign_rules.gd', 'game/core/travel_rules.gd', 'game/core/siege_world.gd'}
    assert {'source': 'game/core/runtime/builtin_modules.gd', 'target': module} in contract['source_edges']
    assert {'source': 'game/core/runtime/legacy_phases.gd', 'target': coordination} in contract['source_edges']
    assert not any(e['source'] == 'game/core/world_kernel.gd' and e['target'] == assets for e in contract['source_edges'])


def test_production_observer_dependency_boundary():
    """正式观察链只读写既有领域候选；新接点不能反向依赖UI、测试夹具或创造并列owner。"""
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    assert checker()(ROOT, ROOT / 'game/data/module_contracts.json')['ok']
    adapter = 'game/core/domains/interaction/interaction_observers.gd'
    assert contract['files'][adapter] == 'interaction'
    assert {edge['target'] for edge in contract['source_edges'] if edge['source'] == adapter} == {
        'game/core/event_rules.gd'}
    for source, target in [
        ('game/core/runtime/builtin_modules.gd', adapter),
        ('game/core/runtime/module_registry.gd', 'game/core/runtime/domain_observers.gd'),
        ('game/core/world_kernel.gd', 'game/core/runtime/domain_observers.gd'),
        ('game/core/world_kernel.gd', 'game/core/runtime/transaction_context.gd')]:
        assert {'source': source, 'target': target} in contract['source_edges']
        assert not any(edge['source'] == source and edge['target'] == target for edge in contract['legacy_edges'])
    assert contract['files']['game/core/runtime/domain_observers.gd'] == 'runtime'
    assert contract['files']['game/core/runtime/transaction_context.gd'] == 'runtime'


def test_military_combat_adapter_keeps_raw_rule_and_explicit_coordination_boundary():
    """会战注册只向原规则依赖；结果关闭后的围城刷新仍由明确协调出口串接。"""
    contract = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    adapter = 'game/core/domains/military/military_combat_commands.gd'
    coordination = 'game/core/domains/military/military_coordination.gd'
    assert contract['files'][adapter] == 'military'
    assert {edge['target'] for edge in contract['source_edges'] if edge['source'] == adapter} == {'game/core/military_combat.gd'}
    assert {'source': coordination, 'target': 'game/core/siege_world.gd'} in contract['source_edges']
    assert not any(adapter in (edge['source'], edge['target']) for edge in contract['legacy_edges'])
    kernel = (ROOT / 'game/core/world_kernel.gd').read_text()
    assert 'MilitaryCombat.apply(' not in kernel and 'MilitaryCombat.preview(' not in kernel
    builtin = (ROOT / 'game/core/runtime/builtin_modules.gd').read_text()
    runtime = (ROOT / 'game/core/runtime/runtime_commands.gd').read_text()
    assert 'MilitaryCombat.preview_step(' not in kernel
    assert 'descriptor.commands=RuntimeCommands.descriptor(preview_advance).commands' in builtin
    assert '{"kind":"advance","mode":"instant","preview":advance_preview,"apply":request_advance,"gate_id":"advance_before_actor"}' in runtime
    advance = builtin.split('static func preview_advance(', 1)[1]
    ordered = [
        'if kind!="advance": return {"ok":false,"code":"UnsupportedAction"}',
        'var military: Dictionary=MilitaryCombat.preview_step(state,data,state.tick+1)',
        'if not military.ok: return military',
        'var siege: Dictionary=SiegeWorld.preview_step(state,data)',
        'if not siege.ok: return siege',
        'if state.actions.is_empty() and military.battle_ids.is_empty() and siege.siege_ids.is_empty(): return {"ok":false,"code":"NoCurrentAction"}',
        'if action.actor_id==actor_id and action.kind=="travel" and not Combat.occupied(state,actor_id):',
        'var travel_check: Dictionary=TravelRules.can_step(state,data,action)',
        'if not travel_check.ok: return travel_check',
        'return {"ok":true,"duration":1}',
    ]
    assert all(advance.count(statement) == 1 for statement in ordered)
    assert [advance.index(statement) for statement in ordered] == sorted(advance.index(statement) for statement in ordered)
    preview_block = '\n'.join([
        '\tvar advance_command: Dictionary=_registry.commands.get(kind,{})',
        '\tif kind=="advance" and advance_command.get("owner")=="runtime" and advance_command.get("gate_id")=="advance_before_actor":',
        '\t\treturn advance_command.preview.call(state,data,actor_id,kind,params)',
    ])
    control_block = '\n'.join([
        '\t\tif applied.has("control"):',
        '\t\t\tif registered.owner!="runtime" or kind!="advance" or registered.gate_id!="advance_before_actor" or typeof(applied.ok)!=TYPE_BOOL or not applied.control is Dictionary or typeof(applied.control.get("advance_world"))!=TYPE_BOOL or applied!={"ok":true,"control":{"advance_world":true}}: return {"ok":false,"code":"InvalidCommandControl"}',
        '\t\t\treturn _advance(state)',
        '\t\tif registered.owner=="runtime" and kind=="advance": return {"ok":false,"code":"InvalidCommandControl"}',
    ])
    assert kernel.count(preview_block) == 1 and kernel.count(control_block) == 1
    assert kernel.index('applied=registered.apply.call(state,_catalog.data,actor_id,kind,params,offer)') < kernel.index(control_block)
    assert checker()(ROOT, ROOT / 'game/data/module_contracts.json')['ok']
