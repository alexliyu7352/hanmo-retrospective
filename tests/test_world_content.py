"""用真实集成内容检验地形与庄园作者边界，坏引用或私产不能等到游玩后才报错。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest

from tools.content.validation import validate_pack

ROOT = Path(__file__).resolve().parents[1]


def sample():
    """每次使用独立作者副本，避免测试改写已配置的真实庄园。"""
    return json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())


def test_world_pack_is_readonly_and_valid():
    """实际地形、管事、居民和私产可以通过原内容管线且校验没有副作用。"""
    pack = sample()
    before = deepcopy(pack)
    assert validate_pack(pack).ok
    assert pack == before


@pytest.mark.parametrize('case', ['cells', 'entry', 'water', 'owner', 'resident', 'manager', 'duty', 'material', 'level', 'family_wallet', 'family_service'])
def test_world_authoring_rejects_broken_semantics(case):
    """字段可解析仍须满足实际通行、居民在场、私产归属与单一分工的约束。"""
    pack = sample()
    estate = pack['estates'][0]
    if case == 'cells':
        pack['world_maps'][0]['cells'].pop()
    elif case == 'entry':
        pack['settlements'][0]['map_xy'] = [1000, 430]
    elif case == 'water':
        pack['settlements'][0]['map_xy'] = [500, 430]
        pack['world_maps'][0]['costs'] = {'foot': {'water': 10}}
    elif case == 'owner':
        next(w for w in pack['wallets'] if w['id'] == estate['wallet_id'])['owner'] = {'kind': 'organization', 'id': 'org.office'}
    elif case == 'resident':
        next(a for a in pack['actors'] if a['id'] == estate['resident_actor_ids'][1])['initial_place'] = 'west.street'
    elif case == 'manager':
        estate['manager_actor_id'] = 'actor.player'
    elif case == 'duty':
        estate['initial_duties'].append(deepcopy(estate['initial_duties'][0]))
    elif case == 'material':
        estate['construction']['material_item_id'] = next(i['id'] for i in pack['items'] if i['kind'] != 'commodity')
    elif case == 'level':
        estate['initial_facilities']['farm'] = estate['construction']['max_level'] + 1
    elif case == 'family_wallet':
        next(role for role in pack['roles'] if 'family' in role)['family']['wallet_id'] = 'wallet.office'
    else:
        next(role for role in pack['roles'] if 'family' in role).pop('family')
    before = deepcopy(pack)
    result = validate_pack(pack)
    assert not result.ok, case
    assert pack == before
    assert result.issues


def family_config(pack):
    """定位真实收养岗位及原型，测试不另造绕过作者边界的服务字典。"""
    role = next(row for row in pack['roles'] if 'family' in row)
    prototype = next(row for row in pack['actors'] if row['id'] == role['family']['prototype_id'])
    return role['family'], prototype


def family_travel_cases():
    """列出可选布尔开关及计划关联的有效、缺失、坏形状和坏引用输入。"""
    cases = []
    for name, enabled in [('default', None), ('false', False), ('true', True)]:
        pack = sample(); family, prototype = family_config(pack)
        if enabled is not None: family['enable_travel_plan'] = enabled
        if enabled:
            prototype['travel_plan'] = deepcopy(next(row['travel_plan'] for row in pack['actors'] if 'travel_plan' in row))
        cases.append((name, pack, True, ''))
    pack = sample(); family, _ = family_config(pack); family['enable_travel_plan'] = True
    cases.append(('missing-plan', pack, False, 'FAMILY_TRAVEL_PLAN'))
    for value in (None, 0, 1, 1.0, 'true', [], {}):
        pack = sample(); family, _ = family_config(pack); family['enable_travel_plan'] = value
        cases.append(('non-boolean-' + repr(value), pack, False, 'SCHEMA'))
    for name, plan, code in [
        ('empty-plan', {}, 'SCHEMA'),
        ('short-route', {'destinations': ['city.west'], 'dwell_ticks': 1, 'start_tick': 0}, 'SCHEMA'),
        ('bad-dwell', {'destinations': ['city.west', 'village.east'], 'dwell_ticks': 0, 'start_tick': 0}, 'SCHEMA'),
        ('bad-city', {'destinations': ['city.west', 'city.absent'], 'dwell_ticks': 1, 'start_tick': 0}, 'REFERENCE'),
    ]:
        pack = sample(); family, prototype = family_config(pack)
        family['enable_travel_plan'] = True; prototype['travel_plan'] = plan
        cases.append((name, pack, False, code))
    return cases


@pytest.mark.parametrize('name,pack,valid,code', family_travel_cases())
def test_family_travel_opt_in_authoring(name, pack, valid, code):
    """仅显式真值要求合法原型行旅；校验不得填入缺省开关或改写作者源。"""
    before = deepcopy(pack)
    report = validate_pack(pack)
    assert report.ok == valid, (name, report.issues)
    assert pack == before
    if not valid:
        assert any(issue.code == code for issue in report.issues), (name, report.issues)
        assert all(issue.path for issue in report.issues)


def test_dynamic_actor_fixture_uses_real_preparer(tmp_path):
    """三份独立作者目录经正式准备，原生产输入不变，计划与显式选择分别保留。"""
    from tests.dynamic_actor_content_fixture import prepare_dynamic_actor_fixture
    source = ROOT / 'content/foundation/alpha_life.json'; before = source.read_bytes()
    manifest = json.loads(prepare_dynamic_actor_fixture(tmp_path).read_text())
    assert set(manifest) == {'travel_enabled', 'travel_default', 'military'}
    for name, entry in manifest.items():
        prepared = json.loads(Path(entry['catalog']).read_text())
        assert prepared['catalog_sha256'] == entry['catalog_sha256']
        assert Path(entry['catalog']).parent == tmp_path
        assert validate_pack(json.loads(Path(entry['source']).read_text())).ok
        catalog = prepared['catalog']; family = catalog['roles'][entry['role_id']]['family']
        actor = catalog['actors'][entry['prototype_id']]
        if name == 'military':
            assert actor['military_plan']['office_id'] == 'office.military.a'
            assert 'travel_plan' not in actor and 'enable_travel_plan' not in family
        else:
            assert actor['travel_plan']['start_tick'] == 5 and actor['social']['interval_ticks'] == 1
            if name == 'travel_enabled': assert family['enable_travel_plan'] is True
            else: assert 'enable_travel_plan' not in family
    assert source.read_bytes() == before


def test_family_travel_authoring_matches_godot(tmp_path):
    """临时项目使用当前受信Schema和真实准备目录，跨端边界不依赖旧generated或手改摘要。"""
    import shutil
    import subprocess
    from tools import build
    from tools.content import validation
    from tests.dynamic_actor_content_fixture import prepare_dynamic_actor_fixture

    project = tmp_path / 'runtime'; (project / 'data/generated').mkdir(parents=True)
    shutil.copytree(ROOT / 'game/core', project / 'core')
    shutil.copy2(ROOT / 'schemas/content.schema.json', project / 'data/generated/content.schema.json')
    shutil.copy2(ROOT / 'game/data/rule_capabilities.json', project / 'data/rule_capabilities.json')
    (project / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Dynamic actor authoring fixture"\n', encoding='utf-8')
    manifest = json.loads(prepare_dynamic_actor_fixture(tmp_path / 'fixtures').read_text())
    payload = {'schema': validation.SCHEMA, 'cases': [], 'catalogs': [entry['catalog'] for entry in manifest.values()]}
    for name, pack, valid, _ in family_travel_cases():
        index = {table: {row['id']: row for row in pack.get(table, [])} for table in validation.TABLES}
        index.update(texts=pack['texts'], flags=pack['flags'], required_capabilities=pack['required_capabilities'])
        payload['cases'].append({'name': name, 'pack': pack, 'data': index, 'valid': valid})
    source = project / 'family-content.json'; source.write_text(json.dumps(payload, ensure_ascii=False), encoding='utf-8')
    script = project / 'check_family_content.gd'
    script.write_text('''## 临时作者边界测试只读取正式目录，坏字段先过结构门再进入跨表校验。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Shape=preload("res://core/content_shape.gd")
const World=preload("res://core/world_content.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 入口读测试载荷；缺文件或非法JSON不能当成零断言成功。
func _init() -> void:
    call_deferred("run_checks")

## 每项记录实际结果并保留错误上下文，不在失败时修复作者内容。
func check(value: bool, label: String) -> void:
    checks+=1
    if not value:
        failures+=1
        printerr("FAIL: ",label)

## 对相同Schema、引用及家事语义运行两端输入，并读取全部正式编译夹具。
func run_checks() -> void:
    var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(OS.get_cmdline_user_args()[0]))
    check(parsed.ok,"读取测试作者载荷")
    if not parsed.ok:
        quit(1); return
    var payload: Dictionary=parsed.value
    for path in payload.catalogs:
        var loaded: Dictionary=Catalog.read_prepared(path)
        check(loaded.ok,"正式Catalog载入独立调度夹具 "+str(loaded.get("code","")))
    for row in payload.cases:
        var before: Dictionary=row.data.duplicate(true)
        var authored: Dictionary=row.pack.duplicate(true)
        var checked: Dictionary=Shape.validate(row.pack,payload.schema,payload.schema,row.data)
        if checked.ok: checked=World.validate(row.data)
        check(checked.ok==row.valid,row.name+" "+str(checked))
        check(row.data==before and row.pack==authored,"只读校验 "+row.name)
        if not row.valid: check(not str(checked.get("path","")).is_empty(),"错误路径 "+row.name)
    print("FAMILY_CONTENT_CHECKS=",checks," FAILURES=",failures)
    quit(1 if failures else 0)
''', encoding='utf-8')
    imported = subprocess.run([build.find_engine(), '--headless', '--path', str(project), '--editor', '--quit'], capture_output=True, text=True, timeout=60)
    assert imported.returncode == 0 and 'SCRIPT ERROR' not in imported.stdout + imported.stderr, imported.stdout + imported.stderr
    result = subprocess.run([build.find_engine(), '--headless', '--path', str(project), '--script', str(script), '--', str(source)], capture_output=True, text=True, timeout=60)
    output = result.stdout + result.stderr
    assert result.returncode == 0 and 'FAMILY_CONTENT_CHECKS=' in output and 'FAILURES=0' in output and 'SCRIPT ERROR' not in output, output
