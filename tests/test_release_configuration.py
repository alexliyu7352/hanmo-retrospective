"""保证发布目录、版本与测试清单来自工程配置，而不是每版复制构建脚本。"""
import importlib.util
from pathlib import Path
import sys
import pytest
ROOT = Path(__file__).resolve().parents[1]


def helper():
    """读取唯一发布入口，缺失时测试明确失败。"""
    path = ROOT / 'tools/release.py'
    assert path.is_file(), '缺少通用发布入口'
    sys.path.insert(0, str(ROOT / 'tools'))
    spec = importlib.util.spec_from_file_location('release_configuration', path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def test_version_and_destination_are_derived(tmp_path):
    """修改工程版本应自动生成新的本地发布位置，不覆盖旧版。"""
    game = tmp_path / 'game'; game.mkdir()
    (game / 'project.godot').write_text('[application]\nconfig/version="0.7.3"\n')
    result = helper().release_config(tmp_path, tmp_path / 'storage')
    assert result['version'] == '0.7.3'
    assert result['folder'] == tmp_path / 'storage/builds/f0.7.3/hanmo-linux'
    (game / 'project.godot').write_text('[application]\nconfig/version="../../old"\n')
    with pytest.raises(ValueError): helper().release_config(tmp_path, tmp_path / 'storage')


def test_storage_profile_is_explicit(tmp_path):
    """新规则发行读取独立存档配置，不能默认覆盖旧版玩家槽。"""
    game=tmp_path/'game'; game.mkdir()
    path=game/'project.godot'
    path.write_text('[application]\nconfig/version="0.6.0"\nconfig/storage_profile="player-m01"\n')
    result=helper().release_config(tmp_path,tmp_path/'storage')
    assert result.get('save_profile')=='player-m01'
    path.write_text('[application]\nconfig/version="0.6.0"\nconfig/storage_profile="../old"\n')
    with pytest.raises(ValueError): helper().release_config(tmp_path,tmp_path/'storage')


def test_companion_checks_are_in_release_gate():
    """同伴、路遇与编辑器已实现后必须进入唯一发布门禁，不靠临时命令记通过。"""
    required = {'test_parties', 'test_road_contacts', 'test_encounters',
                'test_mobility_migration', 'test_mobility_integrity',
                'test_companion_scene', 'test_editor_companions', 'test_companion_reuse'}
    assert required <= set(helper().TESTS)


def test_script_profiles_do_not_share_saves():
    """不同内容的场景测试必须隔离槽位，同时旧场景仍用原作者内容回归。"""
    release = helper()
    assert callable(getattr(release, 'test_arguments', None)), '缺少独立测试配置'
    a = release.test_arguments('test_scene')
    b = release.test_arguments('test_companion_scene')
    assert next(x for x in a if x.startswith('--profile=')) != next(x for x in b if x.startswith('--profile='))
    assert '--content=res://data/generated/lived_city.hmcatalog.json' in a


def test_all_runtime_scripts_have_an_explicit_release_classification():
    """新增真实脚本必须加入门禁或注明独立验收原因，不能因静态列表过期而无声漏测。"""
    release = helper()
    scripts = {path.stem for path in (ROOT / 'game/tests').glob('test_*.gd')}
    excluded = getattr(release, 'EXCLUDED_TESTS', {})
    assert set(release.TESTS).isdisjoint(excluded)
    assert len(release.TESTS) == len(set(release.TESTS))
    assert set(release.TESTS) | set(excluded) == scripts
    assert set(excluded) == {'test_export_font', 'test_family_succession_screen',
                             'test_localized_file_dialog', 'test_military_camp_screen',
                             'test_visual_shell', 'test_dialog_lifecycle'}
    assert all(reason.strip() for reason in excluded.values())


@pytest.mark.parametrize('problem', ['unclassified', 'stale', 'duplicate', 'overlap'])
def test_release_preflight_rejects_incomplete_registry(tmp_path, monkeypatch, problem):
    """清单漏项、陈旧项、重复项和双重分类均须在启动引擎前失败。"""
    release = helper()
    assert callable(getattr(release, 'validate_test_registry', None)), '缺少测试清单门禁'
    folder = tmp_path / 'tests'; folder.mkdir()
    (folder / 'test_one.gd').write_text('extends SceneTree\n')
    monkeypatch.setattr(release, 'TESTS', ['test_one'])
    monkeypatch.setattr(release, 'EXCLUDED_TESTS', {})
    if problem == 'unclassified': (folder / 'test_new.gd').write_text('extends SceneTree\n')
    if problem == 'stale': release.TESTS.append('test_gone')
    if problem == 'duplicate': release.TESTS.append('test_one')
    if problem == 'overlap': release.EXCLUDED_TESTS['test_one'] = '需要图形输入'
    with pytest.raises(RuntimeError): release.validate_test_registry(tmp_path)


def test_release_exclusions_are_reported_as_not_run():
    """图形与字体专用脚本必须明确标为未运行，不能混入通过数。"""
    release = helper()
    assert callable(getattr(release, 'validate_test_registry', None)), '缺少测试清单门禁'
    result = release.validate_test_registry(ROOT / 'game')
    assert set(result) == set(release.EXCLUDED_TESTS)
    assert all(row['result'] == 'NOT_RUN' and row['reason'] for row in result.values())
    assert all('assertions' not in row for row in result.values())


def test_default_entry_receives_no_content_or_profile_overrides():
    """默认标题回归必须读取工程真实设置，不能被旧场景或测试槽命令行覆盖。"""
    assert helper().test_arguments('test_alpha_entry') == []


def test_military_world_requires_an_explicit_prepared_fixture(tmp_path):
    """军需全流程不接受空夹具；实际准备路径与独立测试槽一起传给引擎。"""
    release = helper()
    with pytest.raises(RuntimeError): release.test_arguments('test_military_world')
    fixture = tmp_path / 'military-world.hmcatalog.json'
    args = release.test_arguments('test_military_world', military_content=fixture)
    assert '--military-content=' + str(fixture) in args
    assert any(arg.startswith('--profile=tests-test-military-world-') for arg in args)


def test_release_prepares_real_military_supply_fixture(tmp_path):
    """从共享作者夹具经过真实校验准备异营军需目录，静态坏定义不能进入世界回归。"""
    import json
    release = helper()
    assert callable(getattr(release, 'prepare_military_content', None)), '缺少真实军需夹具准备'
    output = release.prepare_military_content(tmp_path)
    catalog = json.loads(output.read_text())['catalog']
    order = catalog['military_orders']['order.military.a']
    assert order['target_stronghold_id'] == 'stronghold.military.b'
    assert order['objective']['recipient_organization_id'] == 'org.military.b'
    assert catalog['garrisons']['garrison.military.a']['units'][0]['initial_soldiers'] == 8
    assert not any(actor.get('military_plan') for actor in catalog['actors'].values())
    assert (tmp_path / 'military-world.json').is_file()


def test_editor_reference_gate_runs_second_configuration_world():
    """作者引用门禁必须运行第二配置的真实世界，普通场景不注入专用开关。"""
    release = helper()
    args = release.test_arguments('test_editor_local_references')
    assert args.count('--verify-world') == 1
    assert any(arg.startswith('--profile=tests-test-editor-local-references-') for arg in args)
    assert args != release.test_arguments('test_editor_local_references')
    assert '--verify-world' not in release.test_arguments('test_scene')


def test_runtime_suite_checks_registry_before_starting_engine(tmp_path, monkeypatch):
    """发行入口自身必须执行清单门禁，不能只在另一个单元测试中发现遗漏。"""
    release = helper()
    folder = tmp_path / 'tests'; folder.mkdir()
    (folder / 'test_unlisted.gd').write_text('extends SceneTree\n')
    monkeypatch.setattr(release.build, 'GAME', tmp_path)
    monkeypatch.setattr(release, 'TESTS', [])
    monkeypatch.setattr(release, 'EXCLUDED_TESTS', {})
    with pytest.raises(RuntimeError, match='未分类.*test_unlisted'):
        release.run_suite('should-not-start')


def test_release_suite_reuses_unique_verification_runner(tmp_path, monkeypatch):
    """正式发布必须消费统一回执，不再另列源码、Python、Node 或引擎执行规则。"""
    release = helper()
    assert hasattr(release, 'verification'), '发布入口未接入统一验证器'
    calls = []
    def run(root, profile, output, engine):
        """只记录发布委托边界，假回执不足以宣称真实游戏通过。"""
        calls.append((root, profile, output, engine))
        return {'ok': False, 'format': 'HM-VERIFY/1', 'stages': {'godot_import': {'status': 'BLOCKED'}},
                'receipt': str(tmp_path / 'report.json')}
    monkeypatch.setattr(release.verification, 'run_verification', run)
    monkeypatch.setattr(release.verification, 'require_receipt', lambda *args, **kwargs: (_ for _ in ()).throw(RuntimeError('BLOCKED')))
    monkeypatch.setattr(release, 'LOGS', tmp_path / 'logs')
    with pytest.raises(RuntimeError, match='BLOCKED'): release.run_suite('unavailable')
    assert calls == [(release.ROOT, 'release', tmp_path / 'logs', 'unavailable')]



@pytest.mark.parametrize('platform,profile', [('linux', 'linux'), ('web', 'web'), ('all', 'release')])
def test_release_cli_selects_platform_profile(monkeypatch, platform, profile):
    """唯一 CLI 按请求平台显式路由，同一 profile 同时用于执行和消费回执。"""
    release = helper(); calls = []
    monkeypatch.setattr(release.build, 'find_engine', lambda: 'fixture-engine')
    monkeypatch.setattr(release, 'run_suite', lambda *args: calls.append(args) or {})
    assert release.main(['test', '--platform', platform]) == 0
    assert calls == [('fixture-engine', profile)]


def test_legacy_run_suite_default_remains_full_release(monkeypatch, tmp_path):
    """既有无 profile 的 run_suite 保持严格全平台 release；显式 Linux 路由另有自己的回执。"""
    import inspect
    release = helper()
    assert 'profile' in inspect.signature(release.run_suite).parameters, 'run_suite 缺平台参数'
    calls = []
    monkeypatch.setattr(release, 'validate_test_registry', lambda *args: {})
    monkeypatch.setattr(release.verification, 'run_verification', lambda root, profile, output, engine: calls.append(('run', profile)) or {'receipt': str(tmp_path / 'report.json')})
    monkeypatch.setattr(release.verification, 'require_receipt', lambda path, root, profile: calls.append(('require', profile)) or {})
    release.run_suite('fixture')
    release.run_suite('fixture', 'linux')
    assert calls == [('run', 'release'), ('require', 'release'), ('run', 'linux'), ('require', 'linux')]


@pytest.mark.parametrize('platform,profile', [('native', 'web'), ('web', 'linux'), ('native', 'architecture'), ('web', 'architecture'), ('native', None), ('web', None)])
def test_formal_export_rejects_wrong_or_missing_platform_receipt(tmp_path, monkeypatch, platform, profile):
    """正式导出须拒绝错平台、architecture 或无 HM 回执；不能调用字体检查/引擎来放行。"""
    from tests.test_portable_linux_release import configure_export
    release, commands = configure_export(tmp_path, monkeypatch, stage_content=False)
    monkeypatch.setattr(release.verification, 'require_receipt', lambda *args: pytest.fail('错平台不能消费回执'))
    monkeypatch.setattr(release, 'collect_content', lambda: pytest.fail('错平台不能进入内容边界'))
    assert not (tmp_path / 'game/data/generated').exists()
    report = {'engine': 'fixture-only'} if profile is None else {'engine': 'fixture-only', 'format': 'HM-VERIFY/1',
              'profile': profile, 'receipt': str(tmp_path / 'receipt.json')}
    with pytest.raises(RuntimeError, match='profile|回执'):
        if platform == 'native': release.export_native('fixture-engine', report)
        else: release.export_web('fixture-engine', report, tmp_path / 'web')
    assert not commands
    assert not release.FOLDER.exists()
    assert not (tmp_path / 'web').exists()
