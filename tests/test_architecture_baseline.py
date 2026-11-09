"""冻结采集工具的无损比较、引擎门与回执边界，游戏行为仍交给实际 Godot。"""
import importlib
from copy import deepcopy
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import signal

import pytest

ROOT = Path(__file__).resolve().parents[1]


def baseline_tool():
    """缺实现时产生明确的首个红断言，避免导入错误伪装为行为失败。"""
    assert (ROOT / 'tools/architecture_baseline.py').is_file(), '缺少实际 Kernel 基线采集器'
    return importlib.import_module('tools.architecture_baseline')


def test_trace_preserves_rng_ids_arrays_and_assets():
    """只有字典键允许排序，随机串、实例身份、金额和数组次序不能删除或改写。"""
    tool = baseline_tool()
    state = {'rng': {'world': {'state': '18446744073709551615', 'seed': '-1'}},
             'id': 'actor:07', 'wallet': {'balance': 800}, 'array': ['b', 'a'],
             'revision': 9, 'request_sequences': {'actor:07': 4},
             'results': {'actor:07:4': {'id': 'result:08'}}, 'history': [{'id': 'result:08'}]}
    before = deepcopy(state)
    normalized = tool.normalize_state(state)
    assert state == before
    assert normalized == before and normalized['array'] == ['b', 'a']
    assert list(normalized) == sorted(state)
    assert normalized is not state and normalized['wallet'] is not state['wallet']


@pytest.mark.parametrize('mutation', ['balance', 'rng', 'array', 'revision', 'history'])
def test_full_snapshot_mutations_are_detected(mutation):
    """比较器必须识别钱柜、随机流、数组与回执历史的任何真实差异。"""
    tool = baseline_tool()
    first = {'wallet': {'balance': 800}, 'rng': {'state': '001'}, 'array': ['b', 'a'],
             'revision': 1, 'history': [{'id': 'result:1'}]}
    other = deepcopy(first)
    if mutation == 'balance': other['wallet']['balance'] += 1
    if mutation == 'rng': other['rng']['state'] = '002'
    if mutation == 'array': other['array'].reverse()
    if mutation == 'revision': other['revision'] += 1
    if mutation == 'history': other['history'].clear()
    assert tool.state_digest(first) != tool.state_digest(other)


def test_injected_failed_command_preserves_full_snapshot():
    """采集器验证实际引擎回报的失败前后完整摘要，不能只检查余额。"""
    tool = baseline_tool()
    row = {'command': {'kind': 'bad'}, 'result': {'ok': False, 'code': 'InvalidCommand'},
           'before_sha256': 'a' * 64, 'state_sha256': 'b' * 64}
    with pytest.raises(ValueError, match='失败.*副作用'):
        tool.validate_trace({'format': 'HM-ARCH-TRACE/1', 'steps': [row], 'ok': True})


@pytest.mark.parametrize('wrong', [False, True])
def test_missing_or_wrong_engine_is_blocked(tmp_path, wrong):
    """缺失或版本错误的实际可执行入口都不能产生 PASS 或执行规则轨迹。"""
    tool = baseline_tool()
    engine = tmp_path / 'engine'
    if wrong:
        engine.write_text('#!/bin/sh\nprintf "4.6.stable.official.wrong\\n"\n')
        engine.chmod(0o755)
    output = tmp_path / 'baseline'
    report = tool.collect_baseline(ROOT, str(engine), ROOT / 'tests/fixtures/architecture/commands', output)
    assert not report['ok'] and report['status'] == 'BLOCKED'
    assert report['engine']['status'] == 'BLOCKED'
    assert not list(output.rglob('trace.json'))


def test_performance_summary_keeps_raw_samples():
    """实测统计保留五个原值和单位，不隐藏实际测量样本。"""
    tool = baseline_tool()
    summary = tool.summarize_samples([4, 1, 5, 3, 2])
    assert summary['samples'] == [4, 1, 5, 3, 2] and summary['unit'] == 'microseconds'


def test_matrix_has_single_endurance_and_real_aged_input():
    """裁定矩阵保留三负载五次720窗，长期负载必须复用真实7200终点而非改tick。"""
    tool = baseline_tool()
    assert hasattr(tool, 'performance_matrix'), '缺少已裁定的分层性能矩阵'
    matrix = tool.performance_matrix()
    assert matrix['window_ticks'] == 720 and matrix['repetitions'] == 5 and matrix['warmups'] == 1
    assert matrix['endurance_ticks'] == 7200 and matrix['endurance_repetitions'] == 1
    assert matrix['endurance_workloads'] == ['alpha_life', 'second_military']
    assert matrix['aged_input'] == 'alpha_life.endurance.final_save'
    assert matrix['continuation_ticks'] == 30


def test_summary_reports_observed_range_without_population_tail_claim():
    """五样本只报告原值、样本中位数和范围，不把最大值包装为总体尾部保证。"""
    tool = baseline_tool()
    summary = tool.summarize_samples([4, 1, 5, 3, 2])
    assert summary.get('median') == 3 and summary.get('minimum') == 1 and summary.get('maximum') == 5
    assert summary['n'] == 5 and 'p95' not in summary


def complete_trace(folder, case_id='cultivation'):
    """构造小型完整证据字节，只用于门禁测试，不冒充实际游戏轨迹。"""
    tool = baseline_tool()
    folder.mkdir(parents=True, exist_ok=True)
    save = folder / 'node.hmsave'; save.write_text('controlled full save bytes')
    return {'format': 'HM-ARCH-TRACE/1', 'ok': True, 'case_id': case_id,
            'commands': [{'op': 'checkpoint', 'label': 'node'}],
            'steps': [{'command': {'kind': 'wait'}, 'result': {'ok': True},
                       'before_sha256': 'a' * 64, 'state_sha256': 'b' * 64}],
            'checkpoints': [{'label': 'node', 'save': 'node.hmsave',
                             'save_sha256': tool.sha256_file(save), 'state_sha256': 'b' * 64,
                             'after_restore_sha256': 'b' * 64}]}


@pytest.mark.parametrize('damage', ['missing', 'changed'])
def test_trace_admission_checks_persisted_save_bytes(tmp_path, damage):
    """删除或更改真实节点文件必须拒绝准入，不能只信内存摘要。"""
    tool = baseline_tool(); trace = complete_trace(tmp_path)
    if damage == 'missing': (tmp_path / 'node.hmsave').unlink()
    else: (tmp_path / 'node.hmsave').write_text('corrupted')
    with pytest.raises((ValueError, OSError), match='存档|save|No such'):
        tool.validate_trace(trace, tmp_path)


@pytest.mark.parametrize('missing', ['steps', 'checkpoints'])
def test_trace_admission_rejects_empty_required_rows(tmp_path, missing):
    """ok标记不能替代实际命令和稳定节点覆盖。"""
    tool = baseline_tool(); trace = complete_trace(tmp_path); trace[missing] = []
    with pytest.raises(ValueError, match='轨迹|节点|命令'):
        tool.validate_trace(trace)


def execution_identity():
    """提供可独立变更的执行输入身份，保留运行目录之外的相对字节清单。"""
    return {'source': {'commit': 'source'}, 'case_inputs': {'sha256': 'cases', 'files': []},
            'engine': {'build': 'fixed', 'sha256': 'engine'}, 'catalog': {'sha256': 'catalog'},
            'schemas': {'sha256': 'schemas'}, 'runtime_inputs': {'prepared_catalogs': 'prepared'},
            'military_fixture': {'sha256': 'fixture', 'files': []}, 'environment': {'machine': 'test'}}


@pytest.mark.parametrize('changed', ['engine', 'runtime_inputs', 'military_fixture', 'source', 'case_inputs'])
def test_reuse_requires_complete_frozen_execution_identity(changed):
    """固定版本不同字节或任一准备输入变化均不得复用旧阶段。"""
    tool = baseline_tool()
    assert hasattr(tool, 'validate_reuse_identity'), '缺少完整执行身份复用门'
    current = execution_identity(); prior = {'source_frozen': True, 'execution_frozen': True,
                                          'execution_identity': deepcopy(current)}
    prior['execution_identity'][changed] = {'changed': True}
    with pytest.raises(ValueError, match='身份'):
        tool.validate_reuse_identity(prior, current)


def test_reuse_rejects_unfrozen_prior_collection():
    """失败收集中个别PASS轨迹也不能绕过全身份冻结失败。"""
    tool = baseline_tool()
    assert hasattr(tool, 'validate_reuse_identity'), '缺少完整执行身份复用门'
    identity = execution_identity()
    with pytest.raises(ValueError, match='冻结'):
        tool.validate_reuse_identity({'source_frozen': False, 'execution_identity': identity}, identity)


def matrix_data(start=0):
    """受控的小数据模拟完整矩阵形状，不测量或重算游戏。"""
    sample = {'repetition': 0, 'load_us': 1, 'save_us': 2, 'load_ok': True, 'save_ok': True,
              'command_10_us': [1] * 10, 'command_result': {'ok': True},
              'command_state_sha256': 'c' * 64,
              'advances': {'720': {'ok': True, 'start_tick': start, 'end_tick': start + 720,
                                  'elapsed_us': 3, 'kernel_us': 2, 'submit_count': 720,
                                  'counts': {'tick': start + 720}, 'state_sha256': 'd' * 64}}}
    return {'ok': True, 'initial_counts': {'tick': start}, 'initial_state_sha256': 'a' * 64,
            'warmups': [{**deepcopy(sample), 'repetition': -1}],
            'samples': [{**deepcopy(sample), 'repetition': n} for n in range(5)]}


@pytest.mark.parametrize('damage', ['empty', 'missing', 'wrong_window', 'wrong_tick', 'nine_commands',
                                  'load_failed', 'save_failed', 'advance_failed', 'command_state_diff'])
def test_performance_summary_enforces_complete_matrix_rows(damage):
    """五行标签必须含真实720窗、十命令和成功操作，空推进不能通过。"""
    tool = baseline_tool(); data = matrix_data(); row = data['samples'][0]
    if damage == 'empty': row['advances'] = {}
    if damage == 'missing': del row['advances']
    if damage == 'wrong_window': row['advances']['30'] = row['advances'].pop('720')
    if damage == 'wrong_tick': row['advances']['720']['end_tick'] -= 1
    if damage == 'nine_commands': row['command_10_us'].pop()
    if damage == 'load_failed': row['load_ok'] = False
    if damage == 'save_failed': row['save_ok'] = False
    if damage == 'advance_failed': row['advances']['720']['ok'] = False
    if damage == 'command_state_diff': row['command_state_sha256'] = 'e' * 64
    with pytest.raises(ValueError, match='矩阵|样本|推进|命令|操作'):
        tool.performance_summary(data)


def test_short_process_timeout_is_reaped_with_explicit_receipt(tmp_path):
    """短实际进程超时必须终止并由同一wait4持有者回收，不能丢失退出证据。"""
    tool = baseline_tool()
    evidence = tool.run_logged([sys.executable, '-c', 'import time; time.sleep(30)'], tmp_path,
                               tmp_path / 'run.log', .05)
    assert evidence['timed_out'] and evidence['exit_code'] < 0
    assert evidence.get('status') == 'FAIL', '超时缺少明确非PASS阶段回执'
    with pytest.raises(ChildProcessError): os.waitpid(evidence['pid'], os.WNOHANG)
    receipt = json.loads((tmp_path / 'run.result.json').read_text())
    assert receipt['timed_out'] and receipt['status'] == 'FAIL'


def test_parent_interruption_reaps_child_and_preserves_log(tmp_path, monkeypatch):
    """父采集器中断仍须保留日志、明确中断回执并回收实际孩子。"""
    tool = baseline_tool(); original_sleep = tool.time.sleep; raised = False
    def interrupt_once(seconds):
        """只模拟一次父层中断，其后清理仍使用真实等待。"""
        nonlocal raised
        if not raised: raised = True; raise KeyboardInterrupt()
        original_sleep(seconds)
    monkeypatch.setattr(tool.time, 'sleep', interrupt_once)
    with pytest.raises(KeyboardInterrupt):
        tool.run_logged([sys.executable, '-c', 'import time; time.sleep(30)'], tmp_path,
                        tmp_path / 'run.log', 30)
    assert (tmp_path / 'run.log').exists()
    assert (tmp_path / 'run.result.json').exists(), '中断未写阶段回执'
    receipt = json.loads((tmp_path / 'run.result.json').read_text())
    assert receipt['interrupted'] and receipt['status'] == 'INTERRUPTED'
    with pytest.raises(ChildProcessError): os.waitpid(receipt['pid'], os.WNOHANG)


def test_sigterm_parent_leaves_child_reaped_receipt(tmp_path):
    """真实SIGTERM不能绕过finally留下长期孩子，短进程验证实际信号路径。"""
    script = tmp_path / 'parent.py'
    script.write_text('import sys\nfrom pathlib import Path\nsys.path.insert(0, ' + repr(str(ROOT)) + ')\n'
        'from tools.architecture_baseline import run_logged\n'
        'run_logged([sys.executable, "-c", "import time; time.sleep(30)"], Path(' + repr(str(tmp_path)) +
        '), Path(' + repr(str(tmp_path / 'run.log')) + '), 30)\n')
    parent = subprocess.Popen([sys.executable, str(script)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 5
    while not (tmp_path / 'run.started.json').exists() and time.monotonic() < deadline: time.sleep(.01)
    assert (tmp_path / 'run.started.json').exists()
    # 启动回执在发射孩子前写入，稍后发信号确保实际孩子已存在。
    time.sleep(.1); parent.send_signal(signal.SIGTERM)
    parent.communicate(timeout=5)
    assert (tmp_path / 'run.result.json').exists(), 'SIGTERM未留下回收回执'
    evidence = json.loads((tmp_path / 'run.result.json').read_text())
    assert evidence['status'] == 'INTERRUPTED' and evidence['exit_code'] < 0
    with pytest.raises(ProcessLookupError): os.kill(evidence['pid'], 0)


def test_interrupted_engine_probe_is_not_mislabeled_blocked(controlled_collection, monkeypatch):
    """引擎探针的父终止也写中断收集回执，不能因OSError继承关系误分类。"""
    tool, root, engine, commands, output = controlled_collection
    def interrupted_probe(*_args):
        """模拟已被运行器回收的探针中断。"""
        raise InterruptedError('received SIGTERM')
    monkeypatch.setattr(tool, 'run_logged', interrupted_probe)
    with pytest.raises(InterruptedError): tool.collect_baseline(root, str(engine), commands, output)
    assert json.loads(next(output.glob('*/baseline.json')).read_text())['status'] == 'INTERRUPTED'


@pytest.mark.parametrize('missing', ['initial_digest', 'advance_digest', 'command_digest'])
def test_performance_missing_complete_state_digest_is_rejected(missing):
    """缺失完整状态摘要不能用None相等制造确定性。"""
    tool = baseline_tool(); data = matrix_data()
    if missing == 'initial_digest': del data['initial_state_sha256']
    for row in data['warmups'] + data['samples']:
        if missing == 'advance_digest': del row['advances']['720']['state_sha256']
        if missing == 'command_digest': del row['command_state_sha256']
    with pytest.raises(ValueError, match='摘要|状态|样本'):
        tool.performance_summary(data)


@pytest.fixture
def controlled_collection(tmp_path, monkeypatch):
    """临时Git源树和短实际子进程仅产生受控门禁夹具，无游戏验收含义。"""
    tool = baseline_tool(); root = tmp_path / 'repo'; root.mkdir()
    subprocess.run(['git', 'init', '-q', str(root)], check=True)
    subprocess.run(['git', '-C', str(root), '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                    'commit', '--allow-empty', '-qm', 'controlled input'], check=True)
    commands = root / 'tests/fixtures/architecture/commands'; commands.mkdir(parents=True)
    for name in ('cultivation', 'escort', 'family_succession', 'military_siege', 'military_supply',
                 'personal_duel', 'procurement_courier'):
        (commands / (name + '.json')).write_text(json.dumps({'case_id': name, 'scenario_id': 'controlled',
            'commands': [{'op': 'submit', 'kind': 'wait'}, {'op': 'checkpoint', 'label': 'node'}]}))
    runtime = root / 'game/data/generated'; runtime.mkdir(parents=True)
    (runtime / 'alpha_life.hmcatalog.json').write_text('controlled catalog')
    monkeypatch.setattr(tool.build, 'stage_content', lambda _root: None)
    def fixture(folder):
        """准备稳定相对路径与字节，目录本身每次不同。"""
        folder.mkdir(parents=True)
        (folder / 'military-world.json').write_text('controlled military author')
        catalog = folder / 'military-world.hmcatalog.json'; catalog.write_text('controlled military catalog')
        return catalog
    monkeypatch.setattr(tool.verify, 'prepare_military_content', fixture)
    engine = tmp_path / 'engine'
    engine.write_text('#!' + sys.executable + '\n' + '''
import hashlib, json, pathlib, sys
if '--version' in sys.argv:
    print(''' + repr(tool.ENGINE_BUILD) + '''); raise SystemExit()
args = dict(arg[2:].split('=', 1) for arg in sys.argv if arg.startswith('--') and '=' in arg)
folder = pathlib.Path(args['out']); mode = args.get('mode', 'trace')
def saved(name):
    path = folder / name; path.write_text('controlled complete save bytes: '+name)
    return {'save': name, 'save_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
if mode == 'trace':
    case = json.loads(pathlib.Path(args['case']).read_text())
    data = {'format': 'HM-ARCH-TRACE/1', 'ok': True, 'case_id': case['case_id'], 'commands': case['commands'],
            'steps': [{'command': {'kind': 'wait'}, 'result': {'ok': True}, 'before_sha256': 'a'*64,
                       'state_sha256': 'b'*64}],
            'checkpoints': [dict(saved('node.hmsave'), label='node', state_sha256='b'*64,
                                 after_restore_sha256='b'*64)]}
elif mode == 'endurance':
    nodes = [dict(saved('tick-'+str(tick)+'.hmsave'), ok=True, tick=tick, state_sha256='a'*64,
                  after_decode_sha256='a'*64, continuation={'ok': True, 'ticks': 30,
                  'uninterrupted_sha256': 'b'*64, 'restored_sha256': 'b'*64}) for tick in range(0,7201,720)]
    data = {'ok': True, 'target_ticks': 7200, 'interval': 720, 'continuation_ticks': 30, 'checkpoints': nodes,
            'segments': [{'ok': True, 'start_tick': tick, 'end_tick': tick+720,
                          'elapsed_us': 1, 'kernel_us': 1, 'submit_count': 720} for tick in range(0,7200,720)],
            'elapsed_us': 10, 'kernel_us': 10, 'final_save': nodes[-1]['save'], 'final_state_sha256': 'a'*64}
else:
    start = 7200 if 'initial-save' in args else 0
    def sample(rep):
        return dict(saved('saved-'+str(rep)+'.hmsave'), repetition=rep, load_us=1, save_us=1,
                    load_ok=True, save_ok=True, command_10_us=[1]*10, command_result={'ok': True},
                    command_state_sha256='c'*64, advances={'720': {'ok': True, 'start_tick': start,
                    'end_tick': start+720, 'counts': {'tick': start+720}, 'submit_count': 720,
                    'elapsed_us': 1, 'kernel_us': 1, 'state_sha256': 'd'*64}})
    data = {'ok': True, 'initial_counts': {'tick': start}, 'initial_state_sha256': 'a'*64,
            'initial_save': saved('initial.hmsave'), 'warmups': [sample(-1)],
            'samples': [sample(rep) for rep in range(5)],
            'initial_save_source_sha256': hashlib.sha256(pathlib.Path(args['initial-save']).read_bytes()).hexdigest()
                                         if 'initial-save' in args else ''}
(folder / (mode+'.json')).write_text(json.dumps(data))
print('ARCHITECTURE_TRACE_CHECKS=1 FAILURES=0')
''')
    engine.chmod(0o755)
    return tool, root, engine, commands, tmp_path / 'evidence'


def test_one_case_custom_directory_is_incomplete(controlled_collection, tmp_path):
    """自定义目录只有一项时即使性能全部成功仍不能代表七用例全门。"""
    tool, root, engine, commands, output = controlled_collection
    subset = tmp_path / 'one-case'; subset.mkdir()
    (subset / 'cultivation.json').write_bytes((commands / 'cultivation.json').read_bytes())
    report = tool.collect_baseline(root, str(engine), subset, output)
    assert report['status'] == 'INCOMPLETE' and not report['complete_case_scope']


@pytest.mark.parametrize('changed', ['source', 'engine', 'runtime', 'military'])
def test_collection_detects_input_changes_between_children(controlled_collection, monkeypatch, changed):
    """阶段完成边界再核验真实输入字节，不能把不同输入混入同一PASS。"""
    tool, root, engine, commands, output = controlled_collection; run_logged = tool.run_logged; modified = False
    def mutate_after_child(command, cwd, log, timeout):
        """首个实际轨迹退出后修改一项真实消费输入。"""
        nonlocal modified
        result = run_logged(command, cwd, log, timeout)
        if '--version' not in command and not modified:
            modified = True
            target = {'source': root / 'source.txt', 'engine': engine,
                      'runtime': root / 'game/data/generated/alpha_life.hmcatalog.json',
                      'military': log.parents[3] / 'fixtures/military-world.hmcatalog.json'}[changed]
            with target.open('a') as stream: stream.write('\nchanged')
        return result
    monkeypatch.setattr(tool, 'run_logged', mutate_after_child)
    report = tool.collect_baseline(root, str(engine), commands, output, performance=False)
    assert report['status'] == 'FAIL' and not report.get('execution_frozen', True)


@pytest.mark.parametrize('damage', ['missing_restored', 'incomplete_cases', 'wrong_digest', 'wrong_assertions',
                                  'wrong_restore_flag', 'failed_run'])
def test_reused_traces_are_revalidated_as_full_triplets(controlled_collection, damage):
    """旧PASS标签不能掩盖缺失恢复遍、用例或摘要与断言失败。"""
    tool, root, engine, commands, output = controlled_collection
    prior = tool.collect_baseline(root, str(engine), commands, output, performance=False)
    if damage == 'missing_restored': prior['cases']['cultivation']['runs'].pop()
    if damage == 'incomplete_cases': del prior['cases']['cultivation']
    if damage == 'wrong_digest': prior['cases']['cultivation']['trace_digest'] = 'wrong'
    if damage == 'failed_run': prior['cases']['cultivation']['runs'][0]['status'] = 'FAIL'
    if damage == 'wrong_restore_flag':
        evidence = prior['cases']['cultivation']['runs'][2]
        evidence['command'] = [arg.replace('--restore-nodes=true', '--restore-nodes=false') for arg in evidence['command']]
    if damage == 'wrong_assertions':
        evidence = prior['cases']['cultivation']['runs'][0]
        Path(evidence['log']).write_text('ARCHITECTURE_TRACE_CHECKS=1 FAILURES=1\n')
        evidence['log_sha256'] = tool.sha256_file(Path(evidence['log']))
    receipt = Path(prior['receipt']).with_name('controlled-damaged.json')
    receipt.write_text(json.dumps(prior))
    report = tool.collect_baseline(root, str(engine), commands, output, performance=False, trace_receipt=receipt)
    assert report['status'] == 'FAIL' and not report['ok']


def test_completed_stages_reuse_after_honest_interruption(controlled_collection, monkeypatch):
    """中断只重跑未完成独立阶段，原连续耐久及生产身份保持原样。"""
    tool, root, engine, commands, output = controlled_collection; run_logged = tool.run_logged
    def interrupt_second(command, cwd, log, timeout):
        """第二耐久启动前模拟父中断，已完成第一耐久保留。"""
        if '--mode=endurance' in command and log.parent.name == 'second_military': raise KeyboardInterrupt()
        return run_logged(command, cwd, log, timeout)
    monkeypatch.setattr(tool, 'run_logged', interrupt_second)
    with pytest.raises(KeyboardInterrupt): tool.collect_baseline(root, str(engine), commands, output)
    receipt = next(output.glob('*/baseline.json')); prior = json.loads(receipt.read_text())
    assert prior['status'] == 'INTERRUPTED' and prior['execution_frozen']
    assert prior['performance']['status'] == 'NOT_RUN'
    assert prior['performance']['endurance']['alpha_life']['status'] == 'PASS'
    assert prior['performance']['endurance']['second_military']['status'] == 'NOT_RUN'
    started = json.loads(receipt.with_name('start.json').read_text())
    assert started['execution_identity'] == prior['execution_identity']
    assert all(stage['status'] == 'NOT_RUN' for stage in started['stages'].values())
    calls = []
    def record_new_children(command, cwd, log, timeout):
        """观察新实际子进程，复用阶段不得再次发射。"""
        calls.append(command); return run_logged(command, cwd, log, timeout)
    monkeypatch.setattr(tool, 'run_logged', record_new_children)
    report = tool.collect_baseline(root, str(engine), commands, output, reuse_receipt=receipt)
    assert report['status'] == 'PASS'
    assert len(calls) == 5  # 版本核验、第二耐久、三个短窗。
    reused = report['performance']['endurance']['alpha_life']
    assert reused['producer']['receipt'] == str(receipt)
    assert reused['producer']['execution_identity'] == prior['execution_identity']


def test_completed_checkpoint_bytes_revalidated_at_collection_end(controlled_collection, monkeypatch):
    """较早完成节点在后续阶段被更改时，最终收集不能保留其PASS。"""
    tool, root, engine, commands, output = controlled_collection; run_logged = tool.run_logged
    def damage_earlier_save(command, cwd, log, timeout):
        """第二用例运行后破坏第一用例已准入节点，模拟外部文件变更。"""
        result = run_logged(command, cwd, log, timeout)
        if log.parent.name == 'first' and log.parent.parent.name == 'escort':
            earlier = log.parents[3] / 'traces/cultivation/first/node.hmsave'
            earlier.write_text('changed after first case admission')
        return result
    monkeypatch.setattr(tool, 'run_logged', damage_earlier_save)
    report = tool.collect_baseline(root, str(engine), commands, output, performance=False)
    assert report['status'] == 'FAIL' and report['cases']['cultivation']['status'] == 'FAIL'


def test_partial_performance_failure_is_not_hidden_by_not_run(controlled_collection, monkeypatch):
    """待运行矩阵保持NOT_RUN，但已完成失败要立刻反映在性能聚合状态。"""
    tool, root, engine, commands, output = controlled_collection; run_logged = tool.run_logged
    def failed_assertion(command, cwd, log, timeout):
        """真实短子进程完成后改为显式失败断言，测试准入及中间状态。"""
        result = run_logged(command, cwd, log, timeout)
        if '--mode=endurance' in command and log.parent.name == 'alpha_life':
            log.write_text('ARCHITECTURE_TRACE_CHECKS=1 FAILURES=1\n')
            result['log_sha256'] = tool.sha256_file(log)
        return result
    monkeypatch.setattr(tool, 'run_logged', failed_assertion)
    report = tool.collect_baseline(root, str(engine), commands, output)
    progress = Path(report['receipt']).with_name('progress-endurance-alpha_life.json')
    assert json.loads(progress.read_text())['performance']['status'] == 'FAIL'


@pytest.mark.parametrize('preparation', ['runtime', 'military'])
def test_preparation_source_drift_is_rejected_before_stage_start(controlled_collection, monkeypatch, preparation):
    """准备期间真实源码变化不能建立冲突身份或发射后续昂贵阶段。"""
    tool, root, engine, commands, output = controlled_collection
    if preparation == 'runtime':
        def changed_source(_root):
            """准备运行输入时修改真实Git源文件，后续保持稳定。"""
            (root / 'source.txt').write_text('changed during runtime preparation')
        monkeypatch.setattr(tool.build, 'stage_content', changed_source)
    else:
        prepare = tool.verify.prepare_military_content
        def changed_military_source(folder):
            """军事准备完成时修改真实Git源文件，后续保持稳定。"""
            result = prepare(folder)
            (root / 'source.txt').write_text('changed during military preparation')
            return result
        monkeypatch.setattr(tool.verify, 'prepare_military_content', changed_military_source)
    report = tool.collect_baseline(root, str(engine), commands, output)
    assert report['status'] == 'FAIL' and not report['ok'] and not report['source_frozen']
    assert '源码' in report['reason']
    assert not Path(report['receipt']).with_name('start.json').exists()
    assert not list(output.rglob('trace.json')) and not list(output.rglob('endurance.json'))
    assert not any(row['status'] == 'PASS' for row in report['stages'].values())


def complete_endurance(folder):
    """只构造门禁所需完整摘要/文件形状，不执行或测量游戏耐久。"""
    tool = baseline_tool(); nodes = []
    for tick in range(0, 7201, 720):
        path = folder / ('tick-' + str(tick) + '.hmsave'); path.write_text('controlled save ' + str(tick))
        nodes.append({'ok': True, 'tick': tick, 'save': path.name, 'save_sha256': tool.sha256_file(path),
                      'state_sha256': 'a' * 64, 'after_decode_sha256': 'a' * 64,
                      'continuation': {'ok': True, 'ticks': 30, 'uninterrupted_sha256': 'b' * 64,
                                       'restored_sha256': 'b' * 64}})
    return {'ok': True, 'target_ticks': 7200, 'interval': 720, 'continuation_ticks': 30,
            'checkpoints': nodes,
            'segments': [{'ok': True, 'start_tick': tick, 'end_tick': tick + 720, 'submit_count': 720}
                         for tick in range(0, 7200, 720)],
            'final_save': nodes[-1]['save'], 'final_state_sha256': 'a' * 64}


@pytest.mark.parametrize('pair', ['node', 'continuation', 'terminal'])
@pytest.mark.parametrize('damage', ['missing', None, 'malformed'])
def test_endurance_requires_every_complete_digest_pair(tmp_path, pair, damage):
    """双摘要缺失或同一畸形值也不能作为相等，尤其是非末年节点和短窗。"""
    tool = baseline_tool(); data = complete_endurance(tmp_path)
    if pair == 'node': target = data['checkpoints'][1]; keys = ('state_sha256', 'after_decode_sha256')
    elif pair == 'continuation':
        target = data['checkpoints'][1]['continuation']; keys = ('uninterrupted_sha256', 'restored_sha256')
    else:
        target = data; keys = ('final_state_sha256',)
        if damage != 'missing':
            data['checkpoints'][-1]['state_sha256'] = damage
            data['checkpoints'][-1]['after_decode_sha256'] = damage
    for key in keys:
        if damage == 'missing': target.pop(key)
        else: target[key] = damage
    with pytest.raises(ValueError, match='摘要|状态|终点'):
        tool.validate_endurance(data, tmp_path)


def test_complete_endurance_digests_remain_admissible(tmp_path):
    """完整真实形状的控制数据仍通过，严格化不拒绝合法证据。"""
    baseline_tool().validate_endurance(complete_endurance(tmp_path), tmp_path)
