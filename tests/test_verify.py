"""验证统一门禁的身份、回执与进程语义；替身输出不能冒充真实引擎验收。"""
import importlib
import json
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


def verification_tool():
    """要求实际验证入口存在；未实现时以明确断言失败，而非导入异常掩盖红测试。"""
    assert (ROOT / 'tools/verify.py').is_file(), '缺少统一验证入口'
    return importlib.import_module('tools.verify')


@pytest.fixture
def repository(tmp_path):
    """创建真实 Git 夹具，身份检查读取提交和工作树字节，不替换 Git 实现。"""
    root = tmp_path / 'project'; root.mkdir()
    for name, text in [('tools/a.py', '"""职责。"""\n'), ('schemas/content.schema.json', '{}'),
                       ('content/foundation/author.json', '{"format":"HM-CONTENT"}'),
                       ('game/data/presentation.schema.json', '{}')]:
        path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(text)
    for args in [['init', '-q'], ['config', 'user.name', 'Runner Fixture'],
                 ['config', 'user.email', 'runner@example.invalid'], ['add', '.'], ['commit', '-qm', 'fixture']]:
        subprocess.run(['git', *args], cwd=root, check=True, capture_output=True)
    return root


def fake_runner(monkeypatch, verify):
    """只替换统一门禁的子进程边界，用于断言命令、状态和日志传播，不声称运行游戏。"""
    commands = []
    def run(command, *, timeout=180, log=None):
        """为工具语义夹具写实际日志，记录计划命令；引擎输出仅为夹具字符串。"""
        commands.append(command)
        text = verify.ENGINE_BUILD + '\n' if '--version' in command and 'node' not in command[0] else 'CHECKS=3 FAILURES=0\n'
        if log is not None:
            log.parent.mkdir(parents=True, exist_ok=True); log.write_text(text)
        return text
    monkeypatch.setattr(verify.build, 'run_checked', run)
    return commands


def test_verify_does_not_count_export_as_web_play(repository, tmp_path, monkeypatch):
    """执行器即使全部返回成功，导出与存储旁检也不能把实际 Web 输入阶段改成通过。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    checked_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repository, text=True).strip()
    assert report['stages']['web_actual_input']['status'] == 'NOT_RUN'
    assert report['stages']['web_actual_input']['reason']
    assert report['commit'] == checked_commit
    assert report['format'] == 'HM-VERIFY/1'
    assert report['catalog']['mode'] == 'legacy-single-directory'
    assert report['engine_build'] == verify.ENGINE_BUILD
    assert report['python_version']
    assert report['node_version']
    assert report['schema_sha256']
    assert report['artifacts'] == []


def test_cross_language_military_uses_python_fixture_entry(repository, tmp_path, monkeypatch):
    """军事坏包须走共享 Python 夹具生成与 Godot 入口，不能仅直接运行无载荷 GDScript。"""
    verify = verification_tool(); commands = fake_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    command = report['stages']['military_cross_language']['command']
    assert 'tests/test_military_content.py::test_godot_uses_prepared_author_definitions' in command
    assert command in commands
    assert report['stages']['python_pytest']['command'][1:4] == ['-m', 'pytest', 'tests']


def test_stale_or_missing_receipt_cannot_pass(repository, tmp_path, monkeypatch):
    """回执缺失、源树变更或日志被改动均不能再用于放行；旧证据本身必须保留。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    destination = tmp_path / 'receipts'
    with pytest.raises(RuntimeError): verify.require_receipt(destination / 'missing.json', repository, 'architecture')
    report = verify.run_verification(repository, 'architecture', destination, sys.executable)
    receipt = Path(report['receipt'])
    verify.require_receipt(receipt, repository, 'architecture')
    before = receipt.read_bytes()
    (repository / 'tools/a.py').write_text('"""改变职责。"""\n')
    with pytest.raises(RuntimeError, match='身份'): verify.require_receipt(receipt, repository, 'architecture')
    assert receipt.read_bytes() == before


def test_dirty_tree_changes_report_identity(repository):
    """同提交的暂存、未暂存和新增业务文件都进入差异摘要；证据目录不自污染身份。"""
    verify = verification_tool()
    before = verify.source_identity(repository)
    (repository / 'tools/a.py').write_text('"""新职责。"""\n')
    dirty = verify.source_identity(repository)
    assert dirty['commit'] == before['commit']
    assert dirty['dirty_diff_sha256'] != before['dirty_diff_sha256']
    subprocess.run(['git', 'add', 'tools/a.py'], cwd=repository, check=True)
    assert verify.source_identity(repository)['dirty_diff_sha256'] == dirty['dirty_diff_sha256']
    (repository / 'tools/new.py').write_text('"""新增职责。"""\n')
    newer = verify.source_identity(repository)
    assert newer['dirty_diff_sha256'] != dirty['dirty_diff_sha256']
    evidence = repository / 'verification'; evidence.mkdir(); (evidence / 'receipt.json').write_text('{}')
    assert verify.source_identity(repository) == newer


def test_runner_failure_is_not_counted_as_pass(repository, tmp_path, monkeypatch):
    """进程失败保留确切退出码与输出，必需阶段失败时汇总不能放行。"""
    verify = verification_tool(); normal = fake_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """只让 Node 存储单测按指定退出码失败，其余返回工具夹具。"""
        if '--test' in command:
            log.parent.mkdir(parents=True, exist_ok=True); log.write_text('fixture-failure')
            raise RuntimeError(f'Command failed with status 17: {command}\nfixture-failure')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    required_stage_failed = report['stages']['node_storage']['status'] == 'FAIL'
    assert not report['ok'] if required_stage_failed else True
    assert required_stage_failed
    assert report['stages']['node_storage']['exit_code'] == 17
    assert Path(report['stages']['node_storage']['log']).read_text() == 'fixture-failure'
    assert verify.report_exit_code(report) == 1


def test_missing_engine_blocks_required_stages(repository, tmp_path, monkeypatch):
    """明确引擎不存在时不查询系统替代引擎，所有依赖阶段保留阻断原因。"""
    verify = verification_tool(); commands = fake_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', str(tmp_path / 'absent-godot'))
    assert report['stages']['godot_import']['status'] == 'BLOCKED'
    assert not report['ok']
    assert report['engine_build'] is None
    assert verify.report_exit_code(report) == 2
    assert not any('res://tests/' in ' '.join(command) for command in commands)


def test_rerun_creates_new_receipt_and_does_not_overwrite(repository, tmp_path, monkeypatch):
    """复跑生成独立目录与原子回执，不能覆盖同一父目录中的旧证据。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    first = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    old = Path(first['receipt']).read_bytes()
    second = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert first['receipt'] != second['receipt']
    assert Path(first['receipt']).read_bytes() == old
    assert json.loads(Path(second['receipt']).read_text())['run_id'] == second['run_id']


def test_zero_exit_script_error_fails_and_keeps_log(repository, tmp_path, monkeypatch):
    """真实退出零但打印 SCRIPT ERROR 的进程也必须失败，日志与实际退出码均保留。"""
    verify = verification_tool()
    specification = {'command': [sys.executable, '-c', 'print("SCRIPT ERROR: fixture-only")'],
                     'scope': '进程错误语义夹具', 'timeout': 10, 'required': True}
    result = verify.run_stage('fixture', specification, tmp_path)
    assert result['status'] == 'FAIL'
    assert result['exit_code'] == 0
    assert 'SCRIPT ERROR: fixture-only' in Path(result['log']).read_text()


def test_changed_or_missing_log_rejects_receipt(repository, tmp_path, monkeypatch):
    """回执自身宣称通过也不能覆盖日志摘要不匹配或实际日志被删的事实。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    log = Path(report['stages']['node_storage']['log']); log.write_text('改变后的日志')
    with pytest.raises(RuntimeError, match='日志'): verify.require_receipt(Path(report['receipt']), repository, 'architecture')
    log.unlink()
    with pytest.raises(RuntimeError, match='日志'): verify.require_receipt(Path(report['receipt']), repository, 'architecture')


def test_staged_deletion_changes_identity(repository):
    """已暂存删除也须进入差异摘要，不能因工作树与索引同时缺文件漏记删除。"""
    verify = verification_tool(); before = verify.source_identity(repository)
    subprocess.run(['git', 'rm', 'tools/a.py'], cwd=repository, check=True, capture_output=True)
    after = verify.source_identity(repository)
    assert after['dirty_diff_sha256'] != before['dirty_diff_sha256']
    assert any(row['path'] == 'tools/a.py' and row['sha256'] is None for row in after['dirty_paths'])


def test_skipped_pytest_is_not_pass(tmp_path):
    """pytest 即使退出零，只要必需用例被跳过，全量阶段就不能变为 PASS。"""
    verify = verification_tool()
    junit = tmp_path / 'result.xml'
    junit.write_text('<testsuites><testsuite><testcase classname="tests.content.test_contract" name="needed"><skipped/></testcase></testsuite></testsuites>')
    row = {'status': 'PASS', 'exit_code': 0, 'reason': ''}
    verify.pytest_dependencies(row, junit, True, 'fixed-editor')
    assert row['status'] == 'NOT_RUN'


def test_engine_mismatch_does_not_run_godot(repository, tmp_path, monkeypatch):
    """不同引擎构建只能记录身份阻断，不能用另一版本导入或执行游戏。"""
    verify = verification_tool(); commands = fake_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """为版本查询返回故意不匹配的构建，其余沿用语义夹具。"""
        if command == [sys.executable, '--version']:
            log.write_text('4.6.stable.fixture\n'); commands.append(command); return '4.6.stable.fixture\n'
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert report['engine_build'] == '4.6.stable.fixture'
    assert report['stages']['engine_version']['status'] == 'BLOCKED'
    assert report['stages']['godot_import']['status'] == 'BLOCKED'
    assert not any('--headless' in command for command in commands)


def test_export_artifact_receipt_never_marks_actual_web_play(repository, tmp_path, monkeypatch):
    """派生构建回执绑定真实文件字节与导出日志，Web 实际输入仍独立 NOT_RUN。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'release', tmp_path / 'receipts', sys.executable)
    artifact = tmp_path / 'fixture.html'; artifact.write_text('仅执行器构建夹具')
    log = tmp_path / 'export.log'; log.write_text('fixture-export-only')
    assert hasattr(verify, 'record_build_artifacts'), '缺少构建摘要回执'
    derived = verify.record_build_artifacts(report, repository, [artifact], 'web', ['fixture-export'], log,
                                           tmp_path / 'receipts')
    assert derived['stages']['web_export']['status'] == 'PASS'
    assert derived['stages']['web_actual_input']['status'] == 'NOT_RUN'
    assert derived['artifacts'][0]['bytes'] == artifact.stat().st_size
    assert derived['receipt'] != report['receipt']
    verify.require_receipt(Path(derived['receipt']), repository, 'release')
    artifact.write_text('变化后的产物')
    with pytest.raises(RuntimeError, match='产物'): verify.require_receipt(Path(derived['receipt']), repository, 'release')


def test_changed_engine_file_invalidates_receipt(repository, tmp_path, monkeypatch):
    """固定引擎同路径的二进制变化也使回执陈旧，不能仅复用先前版本字符串。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    engine = tmp_path / 'fixture-editor'; engine.write_text('仅执行器夹具'); engine.chmod(0o755)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', str(engine))
    engine.write_text('变化后的执行器夹具')
    with pytest.raises(RuntimeError, match='引擎.*身份'): verify.require_receipt(Path(report['receipt']), repository, 'architecture')


def test_timeout_retains_partial_process_log(tmp_path):
    """超时也保留进程已输出字节；未知退出码不能伪记为零或丢掉首个错误。"""
    verify = verification_tool()
    specification = {'command': [sys.executable, '-c', 'import time; print("partial-fixture", flush=True); time.sleep(5)'],
                     'scope': '超时语义夹具', 'timeout': 1, 'required': True}
    result = verify.run_stage('timeout-fixture', specification, tmp_path)
    assert result['status'] == 'FAIL'
    assert result['exit_code'] is None
    assert Path(result['log']).read_text().startswith('partial-fixture\n')


@pytest.mark.parametrize('change', ['changed', 'deleted', 'added', 'schema'])
def test_prepared_runtime_catalog_change_invalidates_receipt(repository, tmp_path, monkeypatch, change):
    """运行目录中的 catalog、复制 Schema 和完整文件集合都必须绑定，不能借忽略目录绕过。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    folder = repository / 'game/data/generated'; folder.mkdir(parents=True)
    catalog = folder / 'alpha_life.hmcatalog.json'; catalog.write_text('{"fixture":"runtime"}')
    schema = folder / 'content.schema.json'; schema.write_text('{}')
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    receipt = Path(report['receipt']); original = receipt.read_bytes()
    if change == 'changed': catalog.write_text('{"fixture":"changed"}')
    elif change == 'deleted': catalog.unlink()
    elif change == 'added': (folder / 'extra.hmcatalog.json').write_text('{}')
    else: schema.write_text('{"changed":true}')
    with pytest.raises(RuntimeError, match='运行内容'): verify.require_receipt(receipt, repository, 'architecture')
    assert receipt.read_bytes() == original


@pytest.mark.parametrize('location', ['game/assets/fonts/cjk.ttc', 'game/assets/fonts/cjk.tres', 'build/font/FONT-LICENSE.txt'])
def test_prepared_font_change_invalidates_receipt(repository, tmp_path, monkeypatch, location):
    """引擎读取的字体、资源配置与导出许可同属准备依赖，改变字节后旧回执不能放行。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    font = repository / location; font.parent.mkdir(parents=True, exist_ok=True); font.write_bytes(b'fixture-only')
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    receipt = Path(report['receipt']); original = receipt.read_bytes()
    font.write_bytes(b'changed-fixture')
    with pytest.raises(RuntimeError, match='字体'): verify.require_receipt(receipt, repository, 'architecture')
    assert receipt.read_bytes() == original


def test_generated_runtime_change_during_verification_fails(repository, tmp_path, monkeypatch):
    """prepare 后到引擎验证结束之间的生成输入变化必须成为必需失败，不能只在结尾采新摘要。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    folder = repository / 'game/data/generated'; folder.mkdir(parents=True)
    catalog = folder / 'alpha_life.hmcatalog.json'; catalog.write_text('before-engine')
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """仅在军事跨语言进程边界改变忽略文件，模拟外部运行输入漂移。"""
        if 'tests/test_military_content.py::test_godot_uses_prepared_author_definitions' in command:
            catalog.write_text('after-engine')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert not report['ok']
    assert verify.report_exit_code(report) == 1
    assert report['stages']['runtime_inputs_unchanged']['status'] == 'FAIL'


@pytest.mark.parametrize('folder', ['config', 'logs', 'cache', 'tmp', 'backups'])
@pytest.mark.parametrize('filename', ['validator.py', 'rules.bin'])
def test_nested_business_paths_change_dirty_identity(repository, folder, filename):
    """普通目录名在业务树中不是缓存白名单，新增和暂存的 Python/二进制都进入身份。"""
    verify = verification_tool(); before = verify.source_identity(repository)
    path = repository / 'tools/content' / folder / filename; path.parent.mkdir(parents=True)
    path.write_bytes(b'first-source-bytes')
    added = verify.source_identity(repository)
    assert added['dirty_diff_sha256'] != before['dirty_diff_sha256']
    subprocess.run(['git', 'add', str(path.relative_to(repository))], cwd=repository, check=True)
    assert verify.source_identity(repository)['dirty_diff_sha256'] == added['dirty_diff_sha256']
    path.write_bytes(b'changed-source-bytes')
    assert verify.source_identity(repository)['dirty_diff_sha256'] != added['dirty_diff_sha256']


def test_root_evidence_and_local_state_do_not_pollute_identity(repository):
    """仅真正根目录证据与本地缓存被排除，复跑写回执不会不断改变源码差异摘要。"""
    verify = verification_tool(); before = verify.source_identity(repository)
    for folder in ['verification', '.superpowers', 'config', 'logs', 'cache', 'tmp', 'backups']:
        path = repository / folder / 'evidence.bin'; path.parent.mkdir(); path.write_bytes(b'local-state')
    assert verify.source_identity(repository) == before


@pytest.mark.parametrize('source_error', [False, True])
def test_node_missing_static_stage_uses_structured_status(repository, tmp_path, monkeypatch, source_error):
    """只有明确缺 Node 才阻断；同时存在真实源码错误时退出一，不能借依赖缺失遮蔽。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """模拟静态工具返回真实结构化依赖分类，其他命令只测试聚合语义。"""
        if command == ['node', '--version']:
            try:
                raise FileNotFoundError('node')
            except FileNotFoundError as exc:
                raise RuntimeError('Command could not finish: node') from exc
        if 'tools/check_source.py' in command[1] and command[-1] == 'web':
            receipt = Path(command[command.index('--output') + 1])
            result = {'ok': False, 'status': 'FAIL' if source_error else 'BLOCKED',
                      'errors': ['fixture syntax error'] if source_error else [],
                      'blocked': ['node executable unavailable']}
            receipt.write_text(json.dumps(result)); log.write_text(json.dumps(result))
            code = 1 if source_error else 2
            raise RuntimeError(f'Command failed with status {code}: {command}')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert report['stages']['source_web_static']['status'] == ('FAIL' if source_error else 'BLOCKED')
    assert verify.report_exit_code(report) == (1 if source_error else 2)


@pytest.mark.parametrize('change_raw_font', [False, True])
def test_first_font_import_freezes_only_new_sidecar_after_import(repository, tmp_path, monkeypatch, change_raw_font):
    """首次合法导入可新增元数据；原始字体在导入期间漂移仍失败，稳定后 sidecar 不可篡改。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    folder = repository / 'game/assets/fonts'; folder.mkdir(parents=True)
    font = folder / 'cjk.ttc'; font.write_bytes(b'raw-font-fixture')
    (folder / 'cjk.tres').write_text('仅采样边界夹具')
    sidecar = folder / 'cjk.ttc.import'
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """在 Godot 导入进程边界首次生成 sidecar，按用例选择是否同时篡改稳定原文件。"""
        if '--editor' in command:
            sidecar.write_text('[remap]\nimporter="font_data_dynamic"\n')
            if change_raw_font: font.write_bytes(b'changed-during-import')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert report['stages']['runtime_inputs_unchanged']['status'] == ('FAIL' if change_raw_font else 'PASS')
    assert report['ok'] is not change_raw_font
    if change_raw_font:
        assert verify.report_exit_code(report) == 1
    else:
        receipt = Path(report['receipt']); before = receipt.read_bytes()
        verify.require_receipt(receipt, repository, 'architecture')
        sidecar.write_text('[params]\nantialiasing=0\n')
        with pytest.raises(RuntimeError, match='字体导入'): verify.require_receipt(receipt, repository, 'architecture')
        assert receipt.read_bytes() == before


def test_font_import_metadata_drift_after_import_fails_run(repository, tmp_path, monkeypatch):
    """导入成功冻结后的元数据也是运行输入，后续验证期间变化必须阻断当轮回执。"""
    verify = verification_tool(); fake_runner(monkeypatch, verify)
    folder = repository / 'game/assets/fonts'; folder.mkdir(parents=True)
    sidecar = folder / 'cjk.ttc.import'; sidecar.write_text('initial-import-settings')
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """只在跨语言阶段改变已经稳定的 sidecar，不能归入合法首次导入初始化。"""
        if 'tests/test_military_content.py::test_godot_uses_prepared_author_definitions' in command:
            sidecar.write_text('changed-after-import')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'architecture', tmp_path / 'receipts', sys.executable)
    assert not report['ok']
    assert report['stages']['runtime_inputs_unchanged']['status'] == 'FAIL'



def platform_runner(monkeypatch, verify):
    """先确认新平台接口已经实现，再记录真实执行器边界；避免未知 profile 异常冒充断言红灯。"""
    import inspect
    assert 'profile' in inspect.signature(verify.stage_plan).parameters, 'stage_plan 缺少平台 profile'
    return fake_runner(monkeypatch, verify)


def test_linux_profile_defers_browser_and_node_without_passing_them(repository, tmp_path, monkeypatch):
    """Linux 明列 Web 延期且不调用 Node 或浏览器；共享套件精确忽略唯一 Web 文件。"""
    verify = verification_tool(); commands = platform_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'linux', tmp_path / 'receipts', sys.executable)
    for name in ['source_web_static', 'node_version', 'node_storage', 'python_web_storage']:
        row = report['stages'][name]
        assert row['status'] == 'NOT_RUN' and row['required'] is False and row['reason']
        assert row['command'] and 'assertions' not in row and row['command'] not in commands
    assert report['ok'] and verify.report_exit_code(report) == 0
    assert report['node_version'] is None
    assert not any(command[0] == 'node' for command in commands)
    shared = report['stages']['python_pytest']['command']
    assert [arg for arg in shared if arg.startswith('--ignore')] == ['--ignore=tests/test_web_storage.py']
    assert not any(arg == '-k' for arg in shared)
    assert '--scope' in report['stages']['source_static']['command']
    assert report['stages']['source_static']['command'][-1] == 'shared'
    verify.require_receipt(Path(report['receipt']), repository, 'linux')


@pytest.mark.parametrize('target', ['source_static', 'godot_test_rules', 'godot_test_storage'])
def test_linux_shared_failure_still_blocks(repository, tmp_path, monkeypatch, target):
    """共享源码、规则或原生存档失败仍拒绝 Linux 放行，Web 延期不遮蔽失败。"""
    verify = verification_tool(); platform_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """仅使指定共享门进程失败，保持其他门禁边界输出不变。"""
        if log.stem == target:
            log.write_text('fixture actual shared failure')
            raise RuntimeError(f'Command failed with status 1: {command}')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'linux', tmp_path / 'receipts', sys.executable)
    assert report['stages'][target]['status'] == 'FAIL'
    assert verify.report_exit_code(report) == 1 and not report['ok']


@pytest.mark.parametrize('profile', ['web', 'architecture', 'release'])
@pytest.mark.parametrize('failure', ['browser_assertion', 'browser_dependency', 'node_dependency'])
def test_web_profile_requires_browser_storage(repository, tmp_path, monkeypatch, profile, failure):
    """Web 与旧全平台 profile 强制保留浏览器门；真实断言失败和明确缺依赖分别退出一、二。"""
    verify = verification_tool(); platform_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """在真实执行器边界提供对应 JUnit，依赖分类不能吞掉断言失败。"""
        if failure == 'node_dependency' and command == ['node', '--version']:
            try: raise FileNotFoundError('node missing fixture')
            except FileNotFoundError as exc: raise RuntimeError('missing node') from exc
        if log.stem == 'python_web_storage' and failure != 'node_dependency':
            xml = Path(next(arg.split('=', 1)[1] for arg in command if arg.startswith('--junitxml=')))
            detail = '<error message="Executable doesn\'t exist at fixture"/>' if failure == 'browser_dependency' else '<failure message="AssertionError: bad stored bytes"/>'
            xml.write_text('<testsuites><testsuite><testcase classname="tests.test_web_storage" name="test_adapter_is_available">' + detail + '</testcase></testsuite></testsuites>')
            log.write_text(failure)
            raise RuntimeError(f'Command failed with status 1: {command}')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, profile, tmp_path / 'receipts', sys.executable)
    web = report['stages']['python_web_storage']
    assert web['required'] and web['command'][1:4] == ['-m', 'pytest', 'tests/test_web_storage.py']
    assert web['command'][-1].endswith('/python-web-storage.xml')
    assert report['stages']['python_pytest']['command'][-1].endswith('/python-pytest.xml')
    assert verify.report_exit_code(report) == (1 if failure == 'browser_assertion' else 2)
    with pytest.raises(RuntimeError): verify.require_receipt(Path(report['receipt']), repository, profile)


def test_receipt_rejects_wrong_platform_or_removed_required_stage(repository, tmp_path, monkeypatch):
    """Linux 回执不能消费为 Web，移除必需项或篡改 required 标记同样拒绝，原回执保留。"""
    verify = verification_tool(); platform_runner(monkeypatch, verify)
    report = verify.run_verification(repository, 'linux', tmp_path / 'receipts', sys.executable)
    receipt = Path(report['receipt']); before = receipt.read_bytes()
    with pytest.raises(RuntimeError, match='profile'): verify.require_receipt(receipt, repository, 'web')
    for mutation in ['remove', 'flag']:
        changed = json.loads(before)
        if mutation == 'remove': changed['required_stages'].remove('godot_test_storage')
        else: changed['stages']['godot_test_storage']['required'] = False
        tampered = receipt.parent / (mutation + '.json'); tampered.write_text(json.dumps(changed))
        with pytest.raises(RuntimeError, match='必需'): verify.require_receipt(tampered, repository, 'linux')
    assert receipt.read_bytes() == before


@pytest.mark.parametrize('profile,platform_name', [('linux', 'web'), ('web', 'native'), ('architecture', 'native'), ('architecture', 'web')])
def test_build_receipt_rejects_cross_platform_profile(repository, tmp_path, monkeypatch, profile, platform_name):
    """构建证据只能来自对应平台或正式全平台回执，架构检查不能替代正式发行 profile。"""
    verify = verification_tool(); platform_runner(monkeypatch, verify)
    report = verify.run_verification(repository, profile, tmp_path / 'receipts', sys.executable)
    artifact = tmp_path / 'artifact'; artifact.write_bytes(b'fixture')
    log = tmp_path / 'export.log'; log.write_text('fixture')
    with pytest.raises(RuntimeError, match='profile'):
        verify.record_build_artifacts(report, repository, [artifact], platform_name, ['fixture'], log, tmp_path / 'receipts')



def test_native_profile_module_collects_without_playwright():
    """原生配置模块真实导入不依赖浏览器库，普通完整 pytest 仍单独保留全部 Web 用例。"""
    command = [sys.executable, '-c', "import builtins, runpy; original=builtins.__import__; "
               "builtins.__import__=lambda name,*args,**kwargs: (_ for _ in ()).throw(ImportError('browser denied fixture')) "
               "if name.startswith('playwright') else original(name,*args,**kwargs); "
               "loaded=runpy.run_path('tests/test_storage_profiles.py'); "
               "assert callable(loaded['test_godot_forwards_release_profile_and_preserves_native_paths'])"]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize('case', ['unknown_collection', 'assertion', 'native_engine_missing'])
def test_pytest_dependency_split_preserves_unknown_or_native_failures(tmp_path, case):
    """新原生 classname 的明确缺引擎才阻断；未知收集与真实断言失败保持 FAIL。"""
    verify = verification_tool(); junit = tmp_path / 'result.xml'
    classname = 'tests.test_storage_profiles' if case == 'native_engine_missing' else 'tests.unknown'
    name = 'test_godot_forwards_release_profile_and_preserves_native_paths' if case == 'native_engine_missing' else 'unknown'
    detail = 'Godot executable not found' if case == 'native_engine_missing' else 'ModuleNotFoundError: unknown' if case == 'unknown_collection' else 'AssertionError'
    junit.write_text('<testsuites><testsuite><testcase classname="' + classname + '" name="' + name + '"><error message="' + detail + '"/></testcase></testsuite></testsuites>')
    row = {'status': 'FAIL', 'exit_code': 1, 'reason': ''}
    verify.pytest_dependencies(row, junit, False, 'missing-engine')
    assert row['status'] == ('BLOCKED' if case == 'native_engine_missing' else 'FAIL')


def test_character_presentation_scripts_use_independent_arguments(tmp_path):
    """独立表现脚本不能收到世界槽参数；共享候选仍由专用跨语言测试显式传入。"""
    verify = verification_tool()
    plan = verify.stage_plan(ROOT, tmp_path, 'fixed-godot', 'linux')
    for name in ('test_feature_bindings', 'test_character_presentation', 'test_character_presentation_edges', 'test_character_battle_identity'):
        assert verify.TESTS.count(name) == 1
        assert verify.test_arguments(name) == []
        command = plan['godot_' + name]['command']
        assert command[-1] == f'res://tests/{name}.gd'
        assert '--profile' not in ' '.join(command)


@pytest.mark.parametrize('name', ['test_registered_military_contracts', 'test_registered_military', 'test_registered_campaign'])
def test_registered_military_contract_uses_real_prepared_fixture(tmp_path, name):
    """新增合同只登记一次并使用与军事世界相同的真实作者目录；缺夹具必须明确失败。"""
    verify = verification_tool()
    assert verify.TESTS.count(name) == 1
    with pytest.raises(RuntimeError, match='军事全流程测试必须提供'):
        verify.test_arguments(name)
    content = tmp_path / 'military-world.hmcatalog.json'
    arguments = verify.test_arguments(name, military_content=content)
    assert '--military-content=' + str(content) in arguments
    assert len([arg for arg in arguments if arg.startswith('--military-content=')]) == 1
    plan = verify.stage_plan(ROOT, tmp_path, 'fixed-godot', 'linux')
    command = plan['godot_' + name]['command']
    assert 'res://tests/' + name + '.gd' in command
    world = plan['godot_test_military_world']['command']
    assert [arg for arg in command if arg.startswith('--military-content=')] == [
        arg for arg in world if arg.startswith('--military-content=')]


def test_production_observer_matrix_is_registered_without_profile_arguments(tmp_path):
    """正式Kernel故障矩阵唯一执行，旧动态插缝预演已退役，不被当成生产验证。"""
    verify = verification_tool()
    name = 'test_observer_integration'
    assert verify.TESTS.count(name) == 1
    assert verify.test_arguments(name) == []
    assert 'test_observer_integration_staging' not in verify.TESTS
    assert not (ROOT / 'game/tests/test_observer_integration_staging.gd').exists()
    plan = verify.stage_plan(ROOT, tmp_path, 'fixed-godot', 'linux')
    assert plan['godot_' + name]['command'][-1] == f'res://tests/{name}.gd'


def test_combined_early_gate_test_inputs_are_explicit(tmp_path):
    """个人两门使用可追溯旧Kernel，Journey必须取得当前军事目录，不能静默跳过分支。"""
    verify = verification_tool()
    legacy = '--legacy-kernel=res://tests/fixtures/journey_94e1477/world_kernel.gd'
    for name in ['test_registered_personal', 'test_registered_personal_early_guards']:
        assert legacy in verify.test_arguments(name)
    assert (ROOT / 'game/tests/fixtures/journey_94e1477/world_kernel.gd').is_file()
    with pytest.raises(RuntimeError, match='军事'):
        verify.test_arguments('test_journey_registration')
    military = tmp_path / 'military.hmcatalog.json'
    assert '--military-content=' + str(military) in verify.test_arguments('test_journey_registration', military_content=military)
    assert {'test_early_gate_integration', 'test_registered_personal', 'test_registered_personal_contracts',
            'test_registered_personal_early_guards', 'test_journey_registration', 'test_registered_rest'} <= set(verify.TESTS)


def test_journey_counter_accepts_only_explicit_comparison_field():
    """Journey原日志中比较数独立于断言数；零断言、失败或未知间隔字段仍不能通过。"""
    verify = verification_tool()
    assert verify.assertion_count('JOURNEY_REGISTRATION_CHECKS=380 COMPARED_COMMANDS=47 FAILURES=0') == 380
    for output in ['CHECKS=0 COMPARED_COMMANDS=47 FAILURES=0',
                   'CHECKS=380 COMPARED_COMMANDS=47 FAILURES=1',
                   'CHECKS=380 UNVERIFIED=47 FAILURES=0']:
        with pytest.raises(RuntimeError, match='assertion'):
            verify.assertion_count(output)



def test_core_and_startup_gates_use_explicit_aggregate_inputs(tmp_path):
    """启动锚与核心门唯一登记，旧源固定随工程保存，实际汇总由现有严格解析器消费。"""
    verify = verification_tool()
    name = 'test_registered_core_commands'
    assert verify.TESTS.count(name) == 1
    assert verify.TESTS.count('test_builtin_command_contract') == 1
    assert verify.TESTS.count('test_builtin_phase_contract') == 1
    argument = '--legacy-kernel=res://tests/fixtures/core_5a02a9e/world_kernel.gd'
    for command_test in ['test_registered_core_commands', 'test_registered_knowledge', 'test_registered_movement', 'test_registered_dialogue']:
        assert verify.TESTS.count(command_test) == 1
        assert verify.test_arguments(command_test).count(argument) == 1
    plan = verify.stage_plan(ROOT, tmp_path, 'fixed-godot', 'linux')
    assert plan['godot_' + name]['command'].count(argument) == 1
    assert verify.assertion_count('REGISTERED_CORE_COMMANDS_CHECKS=188 COMPARED_COMMANDS=17 FAILURES=0') == 188
    assert verify.assertion_count('BUILTIN_COMMAND_CONTRACT_CHECKS=163 FAILURES=0') == 163
    for output in ['REGISTERED_CORE_COMMANDS_CHECKS=188 COMPARED_COMMANDS=17 FAILURES=1',
                   'BUILTIN_COMMAND_CONTRACT_CHECKS=0 FAILURES=0']:
        with pytest.raises(RuntimeError, match='assertion'):
            verify.assertion_count(output)


def test_core_frozen_source_identity_matches_declared_original():
    """固定5a02a9e两份原文件只重定向旧Builtin引用，不能把当前源码当旧回归基线。"""
    import hashlib
    import json
    folder = ROOT / 'game/tests/fixtures/core_5a02a9e'
    record = json.loads((folder / 'provenance.json').read_text())
    assert record['commit'] == '5a02a9edf92d1a1408a91e0484eeb323a1d7f688'
    expected = {'builtin_modules.gd': '784434128e6c4098dc71ff7752608455a8389e9fe3a186867313a8e6bb664e9c',
                'world_kernel.gd': '94339d760005bf9844f5d05056c4f21d487bed050e0bb34a8ee72968450c14ff'}
    for name, original_sha256 in expected.items():
        payload = (folder / name).read_bytes()
        assert hashlib.sha256(payload).hexdigest() == record['sources'][name]['fixture_sha256']
        original = payload.replace(b'res://tests/fixtures/core_5a02a9e/builtin_modules.gd', b'res://core/runtime/builtin_modules.gd') if name == 'world_kernel.gd' else payload
        assert hashlib.sha256(original).hexdigest() == original_sha256 == record['sources'][name]['original_sha256']



def test_second_military_lifecycle_uses_its_own_prepared_fixture(tmp_path):
    """新增全链只能取得专用占城夹具；旧delivery输入和其他军事脚本参数必须保持。"""
    verify = verification_tool(); name = 'test_second_military_lifecycle'
    assert verify.TESTS.count(name) == 1
    delivery = tmp_path / 'military-world.hmcatalog.json'
    lifecycle = tmp_path / 'military-lifecycle.hmcatalog.json'
    with pytest.raises(RuntimeError, match='专用占城'):
        verify.test_arguments(name, military_content=delivery)
    arguments = verify.test_arguments(name, military_content=delivery, lifecycle_content=lifecycle)
    assert '--military-content=' + str(lifecycle) in arguments
    assert '--military-content=' + str(delivery) not in arguments
    assert len([arg for arg in arguments if arg.startswith('--military-content=')]) == 1
    plan = verify.stage_plan(ROOT, tmp_path, 'fixed-godot', 'linux')
    prepared = plan['military_lifecycle_fixture']['command']
    assert prepared[1:3] == ['-m', 'tests.military_content_fixture']
    assert prepared[-1] == '--lifecycle' and str(tmp_path / 'lifecycle-fixtures') in prepared
    assert '--lifecycle' not in plan['military_fixture']['command']
    assert '--military-content=' + str(tmp_path / 'lifecycle-fixtures/military-lifecycle.hmcatalog.json') in plan['godot_' + name]['command']
    assert '--military-content=' + str(tmp_path / 'fixtures/military-world.hmcatalog.json') in plan['godot_test_registered_military']['command']
    assert list(plan).index('military_lifecycle_fixture') < list(plan).index('godot_' + name)


def test_second_military_lifecycle_blocks_on_its_own_preparation_failure(repository, tmp_path, monkeypatch):
    """专用prepare失败必须阻断新Godot脚本，不能用旧军需夹具成功冒充新链输入。"""
    verify = verification_tool(); commands = fake_runner(monkeypatch, verify)
    original = verify.build.run_checked
    def run(command, *, timeout=180, log=None):
        """仅注入专用正式准备阶段故障，其他工具边界保留原成功替身。"""
        if '--lifecycle' in command:
            raise RuntimeError('InjectedLifecyclePreparationFailure')
        return original(command, timeout=timeout, log=log)
    monkeypatch.setattr(verify.build, 'run_checked', run)
    report = verify.run_verification(repository, 'linux', tmp_path / 'receipts', sys.executable)
    assert report['stages']['military_lifecycle_fixture']['status'] == 'FAIL'
    assert report['stages']['military_fixture']['status'] == 'PASS'
    row = report['stages']['godot_test_second_military_lifecycle']
    assert row['status'] == 'BLOCKED' and 'military_lifecycle_fixture' in row['reason']
    assert row['command'] not in commands



def test_journey_oracle_freezes_its_own_historical_builtin():
    """旧Kernel必须连同原Builtin隔离，不能让新注册表污染等待、推进或对白的旧源对照。"""
    import hashlib
    folder = ROOT / 'game/tests/fixtures/journey_94e1477'
    record = json.loads((folder / 'provenance.json').read_text())
    kernel = (folder / 'world_kernel.gd').read_bytes()
    original = kernel.replace(b'res://tests/fixtures/journey_94e1477/builtin_modules.gd', b'res://core/runtime/builtin_modules.gd')
    assert hashlib.sha256(original).hexdigest() == 'e948560aa723f652ef1f7c3ff9be673b3d06d7b1d461efc034708f008b16e405' == record['sha256']
    assert hashlib.sha256(kernel).hexdigest() == record['fixture_sha256']
    assert hashlib.sha256((folder / 'builtin_modules.gd').read_bytes()).hexdigest() == '1c8755814a14465dc38a7a113c66b95902c74e77db24bb63a793cba7ca792fd8' == record['builtin_source_sha256']
    assert b'preload("res://core/runtime/builtin_modules.gd")' not in kernel


@pytest.mark.parametrize('name', ['test_appearance_picker', 'test_editor_appearance',
                                 'test_character_auxiliary_screens', 'test_scene_screen_integration',
                                 'test_scene_draw_order', 'test_visual_entrypoints'])
def test_visual_foundation_scripts_have_unique_self_prepared_registration(name):
    """新表现门自行准备隔离场景与存储，不可注入旧lived_city参数替换其真实入口。"""
    verify = verification_tool()
    assert verify.TESTS.count(name) == 1
    assert name not in verify.EXCLUDED_TESTS
    assert verify.test_arguments(name) == []
    assert (ROOT / 'game/tests' / (name + '.gd')).is_file()


def test_vertical_slice_requires_its_dedicated_preparation_receipt():
    """专用纵切片必须走双作者准备与独立进程回执，不作为缺输入的普通脚本启动。"""
    verify = verification_tool()
    name = 'test_presentation_vertical_slice'
    assert name not in verify.TESTS
    assert 'tools/verify_presentation_slice.py' in verify.EXCLUDED_TESTS[name]
    assert 'BLOCKED' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT / 'game')


def test_real_art_slice_requires_prepared_project_and_output():
    """真实美术候选脚本要求独立作者工程与证据目录，不得裸启动或误报原生视觉通过。"""
    verify = verification_tool()
    name = 'test_vertical_slice_real_art'
    assert name not in verify.TESTS
    assert 'HANMO_REAL_ART_PROJECT/OUTPUT' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT / 'game')


def test_font_scale_keeps_isolated_profile_and_layout_requires_preparation():
    """字号用现有独立开发profile，真实布局样板另需双配置准备环境，不混成普通无参门。"""
    verify = verification_tool()
    assert verify.TESTS.count('test_ui_font_scale') == 1
    args = verify.test_arguments('test_ui_font_scale')
    assert any(arg.startswith('--profile=tests-test-ui-font-scale-') for arg in args)
    assert 'test_vertical_slice_layout' not in verify.TESTS
    assert 'HANMO_REAL_ART_PROJECT/HANMO_LAYOUT_OUTPUT' in verify.EXCLUDED_TESTS['test_vertical_slice_layout']
    verify.validate_test_registry(ROOT / 'game')


@pytest.mark.parametrize('name,output', [
    ('test_vertical_slice_representatives', 'HANMO_REPRESENTATIVE_OUTPUT'),
    ('test_vertical_slice_age_boundary', 'HANMO_AGE_OUTPUT'),
])
def test_representative_slices_require_explicit_preparation(name, output):
    """代表图与正常年龄门只接受专用准备输入，聚合不得隐式重复720半日轨迹。"""
    verify = verification_tool()
    assert name not in verify.TESTS
    reason = verify.EXCLUDED_TESTS[name]
    assert '--representatives' in reason and 'HANMO_REAL_ART_PROJECT' in reason
    assert output in reason
    verify.validate_test_registry(ROOT / 'game')


def test_exact_content_resolver_uses_only_its_pure_fixture():
    """精确内容resolver测试不新建世界，不能被通用旧内容参数改变其纯合同输入。"""
    verify = verification_tool()
    assert verify.TESTS.count('test_pack_set_restore') == 1
    assert verify.test_arguments('test_pack_set_restore') == []
    verify.validate_test_registry(ROOT / 'game')


@pytest.mark.parametrize('name', ['test_storage_metadata', 'test_storage_slot_inspection', 'test_scenario_factory_contract', 'test_scenario_snapshot'])
def test_a2_pure_contract_scripts_keep_their_own_fixtures(name):
    """元信息和纯快照各读有身份的专用fixture，不强加旧游戏scenario或存储profile。"""
    verify = verification_tool()
    assert verify.TESTS.count(name) == 1 and verify.test_arguments(name) == []
    verify.validate_test_registry(ROOT / 'game')


def test_scenario_kernel_requires_formal_fixture_and_pure_gates_are_registered():
    """双年代新局必须绑定专用准备目录，纯身份/历法/v2门不接旧默认内容参数。"""
    verify=verification_tool()
    with pytest.raises(RuntimeError,match='双年代'):
        verify.test_arguments('test_scenario_kernel_startup')
    target=Path('/tmp/formal-scenario/two-eras.hmcatalog.json')
    assert verify.test_arguments('test_scenario_kernel_startup',scenario_content=target)==['--','--scenario-content='+str(target)]
    for name in ['test_save_v2_encoding','test_scenario_active_identity','test_scenario_calendar']:
        assert verify.TESTS.count(name)==1 and verify.test_arguments(name)==[]
    plan=verify.stage_plan(ROOT,Path('/tmp/scenario-plan'),'/fixed/godot','linux')
    assert plan['scenario_fixture']['required']
    assert plan['scenario_fixture']['command'][1:3]==['-m','tests.scenario_content_fixture']
    assert '--scenario-content=/tmp/scenario-plan/scenario-fixtures/two-eras.hmcatalog.json' in plan['godot_test_scenario_kernel_startup']['command']


def test_installed_and_goods_gates_require_isolated_real_inputs():
    """安装篡改门只准独立工程，显式初货门的两份目录由唯一准备阶段提供。"""
    verify=verification_tool()
    assert 'test_installed_content' not in verify.TESTS
    assert '独立Godot工程' in verify.EXCLUDED_TESTS['test_installed_content']
    for name in ['test_scenario_menu','test_save_v2_decoding']:
        assert verify.TESTS.count(name)==1 and verify.test_arguments(name)==[]
    with pytest.raises(RuntimeError,match='个人初货'):
        verify.test_arguments('test_scenario_initial_goods')
    folder=Path('/tmp/scenario-goods')
    assert verify.test_arguments('test_scenario_initial_goods',goods_folder=folder)==['--','--goods-content=/tmp/scenario-goods/goods-a.hmcatalog.json','--goods-content=/tmp/scenario-goods/goods-b.hmcatalog.json']


def test_ticket_and_restore_menu_have_distinct_fixture_boundaries():
    """纯票据门无内容参数，恢复UI必须使用独立的实际安装工程。"""
    verify=verification_tool()
    assert verify.TESTS.count('test_storage_callback_epoch')==1
    assert verify.test_arguments('test_storage_callback_epoch')==[]
    assert 'test_restore_menu' not in verify.TESTS
    assert 'prepare_restore_menu_test.py' in verify.EXCLUDED_TESTS['test_restore_menu']
    verify.validate_test_registry(ROOT/'game')


def test_personal_load_draft_requires_prepared_isolated_project():
    """真实Main个人草案读档门依赖正式安装夹具，不能在无内容共享工程裸跑。"""
    verify=verification_tool()
    name='test_personal_load_draft'
    assert name not in verify.TESTS
    assert 'personal_load/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '隔离profile' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_slot_metadata_and_menu_have_separate_runtime_boundaries():
    """文件元信息门独立生成银行，真实选槽必须使用正式双包隔离工程。"""
    verify=verification_tool()
    assert verify.TESTS.count('test_slot_file_metadata')==1
    assert verify.test_arguments('test_slot_file_metadata')==[]
    assert 'test_slot_selection_menu' not in verify.TESTS
    assert 'prepare_fixture.py' in verify.EXCLUDED_TESTS['test_slot_selection_menu']
    verify.validate_test_registry(ROOT/'game')


def test_military_receipt_history_reuses_prepared_combat_fixture_contract():
    """历史收据门复用原军事夹具与正式lived_city目录，不引入另一个内容准备入口。"""
    verify=verification_tool()
    name='test_military_receipt_history'
    assert verify.TESTS.count(name)==1
    assert name not in verify.EXCLUDED_TESTS
    args=verify.test_arguments(name)
    assert '--content=res://data/generated/lived_city.hmcatalog.json' in args
    assert '--scenario=scenario.lived_city' in args
    verify.validate_test_registry(ROOT/'game')


def test_siege_strategy_contract_requires_safe_departure_fixture():
    """战略额外字段门复用正式围城作者小工程，不能裸跑或冒称整编闭合。"""
    verify=verification_tool()
    name='test_siege_strategy_contract'
    assert name not in verify.TESTS
    assert 'siege_safe_departure/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '不共享裸跑' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_ordinary_skill_routes_requires_preserved_baseline_fixture():
    """十技艺覆盖门必须携带原1.2作者及独立内容准备，不能误注册为普通无参脚本。"""
    verify=verification_tool()
    name='test_ordinary_skill_routes'
    assert name not in verify.TESTS
    assert 'ordinary_skill_routes/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '原1.2作者' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_military_condition_history_uses_training_prepared_input():
    """空活动投影的条件折叠门沿原训练夹具运行，不登记新世界生产者或另造输入。"""
    verify=verification_tool()
    name='test_military_condition_history'
    assert verify.TESTS.count(name)==1 and name not in verify.EXCLUDED_TESTS
    assert '--content=res://data/generated/lived_city.hmcatalog.json' in verify.test_arguments(name)
    verify.validate_test_registry(ROOT/'game')


def test_siege_integration_contract_stays_isolated_and_inactive():
    """整编合同叶只用真实围城准备输入；专用登记不能暗示接受或移交已经接线。"""
    verify=verification_tool()
    name='test_siege_integration_contract'
    assert name not in verify.TESTS
    assert 'siege_safe_departure/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '未激活' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_military_march_source_and_kernel_keep_distinct_input_boundaries():
    """行军来源沿原军事准备夹具；真实Kernel门必须单独组合新锁且不可误入共享裸跑。"""
    verify=verification_tool()
    source='test_military_march_conditions'
    kernel='test_military_march_kernel'
    assert verify.TESTS.count(source)==1 and source not in verify.EXCLUDED_TESTS
    assert '--content=res://data/generated/lived_city.hmcatalog.json' in verify.test_arguments(source)
    assert kernel not in verify.TESTS
    assert 'military_march_kernel/prepare_fixture.py' in verify.EXCLUDED_TESTS[kernel]
    assert '不裸跑' in verify.EXCLUDED_TESTS[kernel]
    verify.validate_test_registry(ROOT/'game')


def test_personal_result_consistency_requires_prepared_fixture():
    """个人终态一致性门沿正式tiny世界输入，不把单门改成共享裸跑或完整存档迁移。"""
    verify=verification_tool()
    name='test_personal_result_consistency'
    assert name not in verify.TESTS
    assert 'personal_load/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '不共享裸跑' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_world_command_context_requires_isolated_main_fixture():
    """M0门面必须用正式tiny世界和真实Main节点，不能把纯投影冒充生产调用接线。"""
    verify=verification_tool()
    name='test_world_command_context'
    assert name not in verify.TESTS
    assert 'procurement prepare/stage' in verify.EXCLUDED_TESTS[name]
    assert 'Main节点' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_study_progress_and_exact_version_gates_are_registered():
    """修业终态成长与纯规则身份各有唯一局部门，不扩完整存读或小游戏窗口。"""
    verify=verification_tool()
    for name in ['test_skill_study_progress','test_cultivation_rule_versions']:
        assert verify.TESTS.count(name)==1 and name not in verify.EXCLUDED_TESTS
    verify.validate_test_registry(ROOT/'game')


def test_siege_food_sources_require_their_actual_delivery_fixture():
    """真实围城交粮与非粮门使用专用作者/实际Kernel，不以纯收据形状冒充供给完成。"""
    verify=verification_tool()
    name='test_siege_supply_sources'
    assert name not in verify.TESTS
    assert 'siege_supply_sources/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_study_failure_messages_remain_an_isolated_main_gate():
    """资格说明门加载真实Main但不进树，不准误注册为GUI验收或共享普通脚本。"""
    verify=verification_tool()
    name='test_study_failure_messages'
    assert name not in verify.TESTS
    assert 'Main/UI' in verify.EXCLUDED_TESTS[name]
    assert '不共享裸跑' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_personal_defeat_delay_requires_actual_travel_fixture():
    """败北两半日门必须真实旅程与同行，不能把纯hold列表当作完整世界已推进。"""
    verify=verification_tool()
    name='test_personal_defeat_delay'
    assert name not in verify.TESTS
    assert 'personal_load' in verify.EXCLUDED_TESTS[name]
    assert '两半日travel hold' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_m0_topic_context_requires_actual_guarded_main_fixture():
    """话题门面只在正式Main节点夹具核默认玩家与旧按钮守卫，不代GUI或存读验收。"""
    verify=verification_tool()
    name='test_m0_topic_command_context'
    assert name not in verify.TESTS
    assert 'procurement' in verify.EXCLUDED_TESTS[name]
    assert 'epoch/revision/preview' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_personal_stamina_authored_requires_versioned_fixture():
    """回气作者门须保留旧四场与新场独立锁，不能在通用内容目录裸跑。"""
    verify=verification_tool()
    name='test_personal_stamina_authored'
    assert name not in verify.TESTS
    assert 'personal_stamina_authored/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '旧四场初态等价' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_character_visible_projection_is_pure_registered_gate():
    """公开外观门复用现有manifest与固定oracle，无需资源解码或世界准备参数。"""
    verify=verification_tool()
    assert 'test_character_visible_projection' in verify.TESTS
    assert verify.test_arguments('test_character_visible_projection') == []
    verify.validate_test_registry(ROOT/'game')


def test_siege_occupation_requires_explicit_author_variant():
    """接纳占用门使用同组织援军的独立作者锁，不混用原补粮作者身份。"""
    verify=verification_tool()
    name='test_siege_start_occupation'
    assert name not in verify.TESTS
    assert 'siege_supply_sources/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert 'occupation' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_m0_projection_requires_fixed_source_oracle():
    """迁位等价门只从核验过的旧源生成临时oracle，不把两新入口互比当保旧。"""
    verify=verification_tool()
    name='test_m0_projection_compatibility'
    assert name not in verify.TESTS
    assert 'm0_projection/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '固定旧源码SHA' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_character_input_callers_uses_pure_manifest_fixture():
    """人物真实caller门使用已有manifest及离树Main，不伪装完整世界或纹理验收。"""
    verify=verification_tool()
    assert 'test_character_input_callers' in verify.TESTS
    assert verify.test_arguments('test_character_input_callers') == []
    verify.validate_test_registry(ROOT/'game')


def test_siege_join_entry_requires_actual_player_variants():
    """援军入口门实际改变作者玩家身份，不能以NPC命令成功代替玩家页面可达。"""
    verify=verification_tool()
    name='test_siege_join_entry'
    assert name not in verify.TESTS
    assert 'join-player-foreign' in verify.EXCLUDED_TESTS[name]
    assert 'navigation/Button' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_hidden_person_token_uses_pure_manifest_fixture():
    """入树取图时机门复用原manifest，不启动Main完整生命周期或默认存储。"""
    verify=verification_tool()
    assert 'test_person_token_hidden_setup' in verify.TESTS
    assert verify.test_arguments('test_person_token_hidden_setup') == []
    verify.validate_test_registry(ROOT/'game')


def test_m0_scene_context_requires_live_host_fixture():
    """场景请求门必须真实入树Host及Layered候选，不能用宽松替身冒充生命周期校验。"""
    verify=verification_tool()
    name='test_m0_scene_command_context'
    assert name not in verify.TESTS
    assert 'm0_scene_context/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert 'SceneHost/Layered' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_kernel_character_input_requires_actual_caller_fixture():
    """有限人物门实际比旧conversation结果与快照次数，不把新接口存在当保旧。"""
    verify=verification_tool()
    name='test_kernel_character_input'
    assert name not in verify.TESTS
    assert 'kernel_character_input/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_legacy_backend_requires_explicit_input_and_old_oracle():
    """后端事务门可重建正式输入且只读原Kernel oracle，不依赖遗失的scratch pin。"""
    verify=verification_tool()
    name='test_legacy_backend_ownership'
    assert name not in verify.TESTS
    for token in ['--content/--scenario','--oracle','--ownership-required']:
        assert token in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_legacy_asset_projection_requires_real_ownership_sources():
    """资产只读门以真实受托货物和预留来源验证，不能用计算余额冒充第二权威账。"""
    verify=verification_tool()
    name='test_legacy_asset_projection'
    assert name not in verify.TESTS
    assert 'legacy_asset_projection/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert 'task产权' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_legacy_activity_projection_requires_real_runtime_sources():
    """行动职务门以真实任免和多人行动证明来源，不能把记录存在解释成当前权限。"""
    verify=verification_tool()
    name='test_legacy_activity_projection'
    assert name not in verify.TESTS
    assert 'legacy_activity_projection/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_scene_view_requires_real_host_boundary_fixture():
    """有限场景门复用真实Host和两caller，不能仅以background代替人物与旧按钮边界。"""
    verify=verification_tool()
    name='test_kernel_scene_view'
    assert name not in verify.TESTS
    assert 'kernel_scene_view/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_journal_text_requires_existing_text_oracle():
    """手记门只读旧实际文字oracle，不由新实现重录期望或扩新知识语义。"""
    verify=verification_tool()
    name='test_kernel_journal_text'
    assert name not in verify.TESTS
    assert '--content' in verify.EXCLUDED_TESTS[name] and '--oracle' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_trade_sheet_is_registered_finite_ui_gate():
    """货单主门使用固定旧菜单oracle，正式new_game相邻段由显式参数另行记录。"""
    verify=verification_tool()
    assert 'test_trade_sheet_view' in verify.TESTS
    assert verify.test_arguments('test_trade_sheet_view') == []
    verify.validate_test_registry(ROOT/'game')


def test_map_view_requires_fixed_canvas_oracle():
    """地图有限门只比固定旧Canvas与真实caller，像素GUI仍独立。"""
    verify=verification_tool()
    assert 'test_kernel_map_view' not in verify.TESTS
    assert 'kernel_map_view/prepare_fixture.py' in verify.EXCLUDED_TESTS['test_kernel_map_view']
    verify.validate_test_registry(ROOT/'game')


def test_backend_lifecycle_requires_original_public_oracle():
    """生命周期迁位门须原7652完整输出与真实prepared输入，不自行重录新期待。"""
    verify=verification_tool()
    reason=verify.EXCLUDED_TESTS['test_legacy_backend_lifecycle']
    assert '--content' in reason and '--oracle' in reason and '--ownership-required' in reason
    verify.validate_test_registry(ROOT/'game')


def test_hud_view_requires_original_branch_and_text_oracle():
    """HUD门固定原个人战选择及文字控件，不借视图授权行动或屏蔽零体力。"""
    verify=verification_tool()
    assert 'test_kernel_hud_view' not in verify.TESTS
    assert 'kernel_hud_view/prepare_fixture.py' in verify.EXCLUDED_TESTS['test_kernel_hud_view']
    verify.validate_test_registry(ROOT/'game')


def test_character_summary_is_registered_control_oracle_gate():
    """成长门用固定旧输出摘要和现有小清单，不依赖未提交oracle或新图片。"""
    verify=verification_tool()
    assert 'test_character_summary' in verify.TESTS
    assert verify.test_arguments('test_character_summary') == []
    verify.validate_test_registry(ROOT/'game')


def test_backend_coordination_requires_real_scheduled_source_and_oracle():
    """协调门必须使用实际旅人/军政作者及旧完整短链，不以空调度或伪回调代替。"""
    verify=verification_tool()
    reason=verify.EXCLUDED_TESTS['test_legacy_backend_coordination']
    assert '--schedule-content' in reason and '--oracle' in reason and '--backend-required' in reason
    verify.validate_test_registry(ROOT/'game')


def test_bargain_source_uses_frozen_caller_oracle_without_new_content():
    """议价来源门保原同帧只读报价时序，不把全链Session快照算成本片收益。"""
    verify=verification_tool()
    assert 'test_bargain_source_view' in verify.TESTS
    assert verify.test_arguments('test_bargain_source_view') == []
    verify.validate_test_registry(ROOT/'game')


def test_backend_queries_and_action_context_keep_explicit_inputs():
    """公共查询与Main时序分别使用原oracle，不把边界替身当作世界推进。"""
    verify=verification_tool()
    assert '--oracle' in verify.EXCLUDED_TESTS['test_legacy_backend_queries']
    assert 'kernel_action_context/prepare_fixture.py' in verify.EXCLUDED_TESTS['test_kernel_action_context']
    verify.validate_test_registry(ROOT/'game')


def test_family_estate_views_use_local_owner_fixture_and_page_oracle():
    """家事庄务门复用原owner小输入及冻结页面oracle，不替代完整继任/存读。"""
    verify=verification_tool()
    assert 'test_family_estate_views' in verify.TESTS
    assert verify.test_arguments('test_family_estate_views') == []
    verify.validate_test_registry(ROOT/'game')


def test_personal_battle_view_requires_original_node_oracle():
    """个人战有限输入只在原节点oracle与真实会话夹具中验证，不默认运行旧存读。"""
    verify=verification_tool()
    name='test_kernel_personal_battle_view'
    assert name not in verify.TESTS
    assert 'kernel_personal_battle_view/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_native_bridge_requires_explicit_isolated_load():
    """桥接门须本地精确库和隔离加载，编辑器首发现阻碍不能被默认门遮盖。"""
    verify=verification_tool()
    name='test_native_inventory_bridge'
    assert name not in verify.TESTS
    reason=verify.EXCLUDED_TESTS[name]
    assert 'native_inventory_bridge/prepare_fixture.py' in reason
    assert '默认OFF' in reason and 'BLOCKED' in reason
    verify.validate_test_registry(ROOT/'game')


def test_scene_journey_sidebar_keeps_original_main_oracle():
    """侧栏门使用原真实Main控件和明确三作者输入，不把无路遇fixture当接触已验。"""
    verify=verification_tool()
    name='test_scene_journey_sidebar'
    assert name not in verify.TESTS
    reason=verify.EXCLUDED_TESTS[name]
    assert 'scene_journey_sidebar/prepare_fixture.py' in reason and '--oracle' in reason
    verify.validate_test_registry(ROOT/'game')


def test_military_camp_and_battle_views_keep_their_fixture_scopes():
    """营务本地owner门与野战专门原绘图oracle分开登记，不默认裸跑临时oracle路径。"""
    verify=verification_tool()
    assert 'test_military_camp_views' in verify.TESTS
    assert verify.test_arguments('test_military_camp_views') == []
    name='test_military_battle_presentation_input'
    assert name not in verify.TESTS
    assert 'military_battle_presentation_input/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_study_activity_input_uses_explicit_real_source_oracle():
    """学习身份门须正式课程和原票据oracle，不将协议替身当真实收费或改冻结版本。"""
    verify=verification_tool()
    name='test_study_activity_input'
    assert name not in verify.TESTS
    assert 'study_activity_input/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_shell_routing_keeps_explicit_isolated_scope():
    """外壳在途门明确隔离原路由与缓存输入，不能把部分叶通过当整壳验收。"""
    verify=verification_tool()
    name='test_presentation_shell_routing'
    assert name not in verify.TESTS
    assert '货单缓存oracle' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')


def test_siege_page_keeps_real_lifecycle_and_original_oracle_scope():
    """围城有限读必须绑定真实存活页前提与旧报价顺序，不能把新身份当提交权限。"""
    verify=verification_tool()
    name='test_kernel_siege_page'
    assert name not in verify.TESTS
    assert 'kernel_siege_page/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    verify.validate_test_registry(ROOT/'game')
