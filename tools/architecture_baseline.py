#!/usr/bin/env python3
"""采集实际Godot命令/完整保存/同机性能基线；Python只绑定证据和比较，不结算玩法。"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import threading
import time
import uuid

try:
    from tools import build, verify
    from tools.verification_report import (catalog_identity, file_set_identity, runtime_inputs_identity,
                                           schema_identity, sha256_file, source_identity, write_report)
except ModuleNotFoundError:
    import build
    import verify
    from verification_report import (catalog_identity, file_set_identity, runtime_inputs_identity,
                                     schema_identity, sha256_file, source_identity, write_report)

ROOT = Path(__file__).resolve().parents[1]
ENGINE_BUILD = verify.ENGINE_BUILD
REQUIRED_CASE_IDS = ('cultivation', 'escort', 'family_succession', 'military_siege',
                     'military_supply', 'personal_duel', 'procurement_courier')
TRACE_RUNS = ('first', 'second', 'restored')


def normalize_state(state: dict) -> dict:
    """只递归排序字典键并复制容器；数组次序和所有字段/标量保持不变。"""
    return {key: normalize_value(state[key]) for key in sorted(state)}


def normalize_value(value):
    """复制证据JSON值而不解释世界规则，不修改传入容器或随机数串。"""
    if isinstance(value, dict): return normalize_state(value)
    if isinstance(value, list): return [normalize_value(row) for row in value]
    return value


def state_digest(state: dict) -> str:
    """对完整证据JSON比较求摘要；不能过滤revision、短缓存、资产或永久事实。"""
    text = json.dumps(normalize_state(state), ensure_ascii=False, separators=(',', ':'), allow_nan=False)
    return hashlib.sha256(text.encode()).hexdigest()


def validate_trace(trace: dict, folder: Path | None = None, case: dict | None = None) -> None:
    """校验实际命令失败零副作用和稳定节点完整恢复；缺/错数据不能转成PASS。"""
    if trace.get('format') != 'HM-ARCH-TRACE/1' or not trace.get('ok'):
        raise ValueError('实际轨迹未完成：' + str(trace.get('reason', '格式错误')))
    for step in trace.get('steps', []):
        if not step['result'].get('ok') and step['before_sha256'] != step['state_sha256']:
            raise ValueError('失败命令有完整状态副作用：' + step['command']['kind'])
    if not trace.get('steps') or not trace.get('checkpoints'):
        raise ValueError('轨迹缺少实际命令或稳定节点')
    if case is not None:
        labels = [row['label'] for row in case['commands'] if row['op'] == 'checkpoint']
        if (trace.get('case_id') != case['case_id'] or trace.get('commands') != case['commands'] or
                [node['label'] for node in trace['checkpoints']] != labels):
            raise ValueError('轨迹实际输入或节点覆盖不完整')
    for node in trace.get('checkpoints', []):
        if node['state_sha256'] != node['after_restore_sha256']:
            raise ValueError('稳定节点完整恢复不等价：' + node['label'])
        if folder is not None: validate_save(folder, node)


def validate_save(folder: Path, node: dict) -> None:
    """稳定节点只能引用当前证据目录内的真实文件，缺失或字节变化立即拒绝。"""
    name = node.get('save', '')
    if not name or Path(name).name != name or sha256_file(folder / name) != node.get('save_sha256'):
        raise ValueError('节点存档字节已变化或引用非法：' + name)


def validate_reuse_identity(prior: dict, current: dict) -> None:
    """只复用完整冻结身份一致的独立完成阶段，不信旧顶层或个别PASS标签。"""
    if prior.get('source_frozen') is not True or prior.get('execution_frozen') is not True:
        raise ValueError('待复用收集的完整执行身份未成功冻结')
    if prior.get('execution_identity') != current:
        raise ValueError('待复用阶段的完整执行身份已变化')


def summarize_samples(samples: list[int]) -> dict:
    """保留全部原始微秒样本，只报告样本中位数和范围，不宣称总体尾部。"""
    if not samples: raise ValueError('统计必须有实际样本')
    ordered = sorted(samples)
    middle = len(ordered) // 2
    median = ordered[middle] if len(ordered) % 2 else (ordered[middle - 1] + ordered[middle]) / 2
    return {'samples': samples, 'median': median, 'minimum': ordered[0], 'maximum': ordered[-1],
            'n': len(samples), 'unit': 'microseconds'}


def performance_matrix() -> dict:
    """明确root裁定的分层矩阵，长期负载只能来自真实耐久终点。"""
    return {'window_ticks': 720, 'repetitions': 5, 'warmups': 1, 'endurance_ticks': 7200,
            'endurance_repetitions': 1, 'endurance_workloads': ['alpha_life', 'second_military'],
            'checkpoint_interval': 720, 'continuation_ticks': 30,
            'aged_input': 'alpha_life.endurance.final_save',
            'statistics': 'raw/median/range；五样本不提供总体p95保证',
            'limits': ['单seed', '单次耐久时延', '不声明整段7200两次重放等价', '不覆盖tick14400',
                       '被动推进不是活跃战争/完整人生玩法压力证明']}


def run_logged(command: list[str], cwd: Path, log: Path, timeout: int) -> dict:
    """单一wait4持有者负责真实RSS、终止及回收；中断也保留明确非PASS回执。"""
    log.parent.mkdir(parents=True, exist_ok=True)
    began = time.monotonic()
    process = None; usage = None; error = None
    old_handlers = {}
    def interrupt(signum, _frame) -> None:
        """把可捕获终止信号转换为同一清理路径；不吞掉父中断。"""
        raise InterruptedError('父进程收到终止信号：' + str(signum))
    if threading.current_thread() is threading.main_thread():
        for signum in (signal.SIGINT, signal.SIGTERM):
            old_handlers[signum] = signal.signal(signum, interrupt)
    result = {'command': command, 'log': str(log), 'status': 'NOT_RUN', 'exit_code': None,
              'timed_out': False, 'interrupted': False}
    try:
        with log.open('x', encoding='utf-8') as stream:
            write_report(log.with_suffix('.started.json'), {**result, 'status': 'STARTING'})
            process = subprocess.Popen(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            result['pid'] = process.pid
            while True:
                pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid:
                    process.returncode = os.waitstatus_to_exitcode(status); break
                if time.monotonic() - began > timeout:
                    result['timed_out'] = True; raise TimeoutError('子进程超时')
                time.sleep(.05)
    except BaseException as exc:
        error = exc; result['reason'] = str(exc) or type(exc).__name__
        result['interrupted'] = isinstance(exc, (KeyboardInterrupt, SystemExit, InterruptedError))
    finally:
        if process is not None and process.returncode is None:
            try: os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            deadline = time.monotonic() + 2
            while True:
                pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid: break
                if time.monotonic() >= deadline:
                    try: os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                    pid, status, usage = os.wait4(process.pid, 0); break
                time.sleep(.05)
            process.returncode = os.waitstatus_to_exitcode(status)
        result.update(exit_code=process.returncode if process else None,
                      elapsed_seconds=time.monotonic() - began,
                      peak_rss_kib=usage.ru_maxrss if usage else None,
                      peak_rss_scope='Linux wait4真实子进程高水位，含引擎启动与采集辅助',
                      status='INTERRUPTED' if result['interrupted'] else
                             'PASS' if error is None and process and process.returncode == 0 else 'FAIL')
        if log.is_file(): result['log_sha256'] = sha256_file(log)
        for signum, handler in old_handlers.items(): signal.signal(signum, handler)
        write_report(log.with_suffix('.result.json'), result)
    if error is not None and not isinstance(error, TimeoutError): raise error
    return result


def engine_identity(engine: str, output: Path) -> dict:
    """只使用明确指定的固定入口，缺失/错误版本不搜索系统替代品。"""
    path = Path(engine)
    if not path.is_file() or not os.access(path, os.X_OK):
        return {'status': 'BLOCKED', 'path': engine, 'reason': '固定引擎入口不存在或不可执行'}
    try:
        result = run_logged([engine, '--version'], ROOT, output / 'engine.log', 30)
        version = Path(result['log']).read_text().strip()
    except InterruptedError:
        raise
    except OSError as exc:
        return {'status': 'BLOCKED', 'path': engine, 'reason': str(exc)}
    return {'status': 'PASS' if version == ENGINE_BUILD and result['exit_code'] == 0 else 'BLOCKED',
            'path': engine, 'build': version, 'sha256': sha256_file(path), 'evidence': result,
            'reason': '' if version == ENGINE_BUILD else '版本不符，要求' + ENGINE_BUILD}


def performance_summary(data: dict) -> dict:
    """保留引擎原始性能样本并汇总同起点实跑，不用Python预估代替测量。"""
    validate_performance(data)
    samples = data['samples']
    summary = {'load': summarize_samples([row['load_us'] for row in samples]),
               'save': summarize_samples([row['save_us'] for row in samples]),
               'command_10': summarize_samples([sum(row['command_10_us']) for row in samples]),
               'advance': {}}
    for length in samples[0]['advances']:
        values = [row['advances'][length] for row in data['warmups'] + samples]
        if len({row['state_sha256'] for row in values}) != 1:
            raise ValueError('同输入性能推进产生不同完整状态：' + length)
        summary['advance'][length] = {'end_to_end': summarize_samples([row['advances'][length]['elapsed_us'] for row in samples]),
                                      'kernel_submit': summarize_samples([row['advances'][length]['kernel_us'] for row in samples])}
    return summary


def validate_performance(data: dict, folder: Path | None = None) -> None:
    """核验裁定矩阵的小型结构和真实日期，不把空推进或短探针算作完整门。"""
    if (not data.get('ok') or len(data.get('samples', [])) != 5 or len(data.get('warmups', [])) != 1 or
            [row.get('repetition') for row in data['samples']] != list(range(5)) or
            data['warmups'][0].get('repetition') != -1):
        raise ValueError('性能矩阵实际样本数/序号不足')
    start = data.get('initial_counts', {}).get('tick')
    if start not in (0, 7200): raise ValueError('性能矩阵起点日期非法')
    if not complete_digest(data.get('initial_state_sha256')): raise ValueError('性能初态完整摘要缺失')
    if folder is not None: validate_save(folder, data['initial_save'])
    for row in data['warmups'] + data['samples']:
        if (row.get('load_ok') is not True or row.get('save_ok') is not True or
                not row.get('command_result', {}).get('ok') or len(row.get('command_10_us', [])) != 10):
            raise ValueError('性能样本缺成功操作或十命令')
        first = data['warmups'][0]
        if (row.get('command_result') != first.get('command_result') or
                row.get('command_state_sha256') != first.get('command_state_sha256')):
            raise ValueError('性能样本同输入命令完整状态不等价')
        if set(row.get('advances', {})) != {'720'}: raise ValueError('性能矩阵缺少准确720推进窗')
        advance = row['advances']['720']
        if not complete_digest(advance.get('state_sha256')) or not complete_digest(row.get('command_state_sha256')):
            raise ValueError('性能样本完整状态摘要缺失')
        if (advance.get('ok') is not True or advance.get('start_tick') != start or
                advance.get('end_tick') != start + 720 or advance.get('counts', {}).get('tick') != start + 720 or
                advance.get('submit_count', 0) < 720):
            raise ValueError('性能推进日期或成功标记错误')
        for value in [row.get('load_us'), row.get('save_us'), *row['command_10_us'],
                      advance.get('elapsed_us'), advance.get('kernel_us')]:
            if not isinstance(value, (int, float)) or value < 0: raise ValueError('性能样本计时非法')
        if folder is not None: validate_save(folder, row)


def complete_digest(value) -> bool:
    """完整状态摘要必须是实际SHA256字符串，None相等不能代表确定性证据。"""
    return isinstance(value, str) and len(value) == 64 and all(char in '0123456789abcdef' for char in value)


def refresh_performance_status(report: dict) -> None:
    """只有全部五个独立阶段完成才PASS；已失败立即FAIL，其余保持NOT_RUN。"""
    performance = report['performance']
    rows = [*performance.get('endurance', {}).values(), *performance.get('workloads', {}).values()]
    performance['status'] = ('FAIL' if any(row['status'] in {'FAIL', 'BLOCKED', 'INTERRUPTED'} for row in rows) else
                             'PASS' if len(rows) == 5 and all(row['status'] == 'PASS' for row in rows) else 'NOT_RUN')


def validate_endurance(data: dict, folder: Path) -> None:
    """连续7200必须含全部年度节点与独立短窗，缺样本或中断分段不能准入。"""
    matrix = performance_matrix(); nodes = data.get('checkpoints', []); segments = data.get('segments', [])
    if (not data.get('ok') or data.get('target_ticks') != 7200 or data.get('interval') != 720 or
            data.get('continuation_ticks') != 30 or [node.get('tick') for node in nodes] != list(range(0, 7201, 720)) or
            len(segments) != 10): raise ValueError('耐久矩阵或年度节点不完整')
    for index, segment in enumerate(segments):
        if (segment.get('ok') is not True or segment.get('start_tick') != index * 720 or
                segment.get('end_tick') != (index + 1) * 720 or segment.get('submit_count', 0) < 720):
            raise ValueError('耐久连续推进日期不完整')
    for node in nodes:
        compared = node.get('continuation', {})
        if any(not complete_digest(value) for value in
               [node.get('state_sha256'), node.get('after_decode_sha256'),
                compared.get('uninterrupted_sha256'), compared.get('restored_sha256')]):
            raise ValueError('耐久节点或短窗完整状态摘要缺失/畸形')
        if (node.get('ok') is not True or node.get('state_sha256') != node.get('after_decode_sha256') or
                compared.get('ok') is not True or compared.get('ticks') != matrix['continuation_ticks'] or
                compared.get('uninterrupted_sha256') != compared.get('restored_sha256')):
            raise ValueError('耐久节点或短窗完整状态不等价')
        validate_save(folder, node)
    if (not complete_digest(data.get('final_state_sha256')) or data.get('final_save') != nodes[-1]['save'] or
            data.get('final_state_sha256') != nodes[-1]['state_sha256']):
        raise ValueError('耐久真实完整终点不符')


def admit_evidence(evidence: dict, kind: str, case: dict | None = None, *, reuse: bool = False) -> dict:
    """从日志、退出码、持久数据及节点字节重新准入，复用不信旧摘要或PASS。"""
    log = Path(evidence['log']); folder = log.parent
    if (evidence.get('exit_code') != 0 or evidence.get('timed_out') or evidence.get('interrupted') or
            sha256_file(log) != evidence.get('log_sha256')): raise ValueError('阶段退出失败或日志字节变化')
    if reuse and evidence.get('status') != 'PASS': raise ValueError('待复用独立阶段未完成PASS')
    evidence['assertions'] = verify.assertion_count(log.read_text())
    path = folder / (kind + '.json'); actual = sha256_file(path)
    key = 'trace_sha256' if kind == 'trace' else 'data_sha256'
    if reuse and evidence.get(key) != actual: raise ValueError('待复用阶段数据字节变化')
    data = json.loads(path.read_text())
    if kind == 'trace':
        validate_trace(data, folder, case)
        evidence.update(steps=len(data['steps']), checkpoints=len(data['checkpoints']))
    elif kind == 'endurance': validate_endurance(data, folder)
    else:
        validate_performance(data, folder); evidence['summary'] = performance_summary(data)
    evidence.update(status='PASS', **{key: actual})
    if kind != 'trace': evidence['data'] = data
    return data


def current_identity(root: Path, engine: str, cases: Path, selected: list[str] | None,
                     military: Path, environment: dict) -> dict:
    """复用现有身份助手覆盖源码之外的实际引擎、准备输入和相对军事夹具字节。"""
    inputs = sorted(path for path in cases.glob('*.json') if not selected or path.stem in selected)
    fixture_paths = [military, military.with_name('military-world.json')]
    if not all(path.is_file() for path in fixture_paths): raise ValueError('准备军事输入缺失')
    return {'source': source_identity(root), 'case_inputs': file_set_identity(cases, inputs),
            'engine': {'build': ENGINE_BUILD, 'sha256': sha256_file(Path(engine)),
                       'executable': os.access(engine, os.X_OK)},
            'catalog': catalog_identity(root), 'schemas': schema_identity(root),
            'runtime_inputs': runtime_inputs_identity(root),
            'military_fixture': file_set_identity(military.parent, fixture_paths), 'environment': environment}


def collect_baseline(root: Path, engine: str, cases: Path, output: Path, *, performance: bool = True,
                     selected_cases: list[str] | None = None, trace_receipt: Path | None = None,
                     reuse_receipt: Path | None = None, reference: Path | None = None, timeout: int = 86400) -> dict:
    """采集原有矩阵并允许复用身份/字节核验的独立完成阶段；从不续接未完成计时。"""
    root = root.resolve(); cases = cases.resolve(); output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = output / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-' + uuid.uuid4().hex[:8]); run.mkdir()
    report = {'format': 'HM-ARCH-BASELINE/1', 'baseline_commit': '89d68fd5b6d100525de35c8fedf0734d10050937',
              'original_discovery_commit': '9b4ce120666d98dae5eeec6b79adad1ae4b33c9d',
              'status': 'BLOCKED', 'ok': False, 'receipt': str(run / 'baseline.json'),
              'source': source_identity(root), 'cases': {}, 'stages': {}, 'source_frozen': False,
              'execution_frozen': False, 'performance': {'status': 'NOT_RUN'},
              'environment': {'system': platform.platform(), 'machine': platform.machine(), 'python': sys.version,
                  'cpu_count': os.cpu_count(), 'cpu': Path('/proc/cpuinfo').read_text().split('model name\t: ')[-1].split('\n')[0],
                  'memory': Path('/proc/meminfo').read_text().splitlines()[0]},
              'historical_saves': {'status': 'GAP', 'acceptance': 'ARCH09',
                  'reason': '真实F0.5发行原档字节未在授权恢复根找到；当前状态降ruleset不是历史黄金档',
                  'expected_sha256': '014ec72df268ab8202d4cea83ca4d37a2d1731448f7e1464f8e185f683cb1d17',
                  'provenance': ['verification/f051/legacy-save.log', 'verification/m01/old-file.log']}}
    military = None; failed = False; active_log = None; active_stage = None; case_data = {}
    def check_identity() -> None:
        """每个消费/复用边界及收集末尾重新核验完整实际输入。"""
        report['identity_after'] = current_identity(root, engine, cases, selected_cases, military, report['environment'])
        if report['identity_after'] != report['execution_identity']: raise ValueError('收集执行身份中途变化')
    def stage(kind: str, name: str, command: list[str], prior: dict | None = None, case: dict | None = None) -> tuple[dict, dict]:
        """单阶段只绑定独立完成产物；旧产物保持原生产者，失败保留当前目录。"""
        nonlocal active_log, active_stage
        check_identity(); active_stage = kind + ':' + name
        if prior is not None:
            evidence = json.loads(json.dumps(prior)); data = admit_evidence(evidence, kind, case, reuse=True)
            evidence.setdefault('producer', {'receipt': str(reuse_path), 'execution_identity': previous['execution_identity']})
            if evidence['producer']['execution_identity'] != report['execution_identity']:
                raise ValueError('复用阶段原生产身份不符')
            evidence['reused_from'] = {'receipt': str(reuse_path), 'sha256': sha256_file(reuse_path)}
        else:
            active_log = Path(next(arg[6:] for arg in command if arg.startswith('--out='))) / 'run.log'
            print('ARCH_STAGE_START ' + active_stage, flush=True)
            evidence = run_logged(command, root, active_log, timeout)
            evidence['producer'] = {'receipt': report['receipt'], 'execution_identity': report['execution_identity']}
            try: data = admit_evidence(evidence, kind, case)
            except (OSError, ValueError, RuntimeError, KeyError, TypeError) as exc:
                evidence.update(status='FAIL', reason=str(exc)); data = {}
        report['stages'][active_stage] = evidence
        check_identity(); active_log = None; active_stage = None
        return evidence, data
    try:
        report['engine'] = engine_identity(engine, run)
        if report['engine']['status'] != 'PASS': return report
        build.stage_content(root)
        inputs = sorted(path for path in cases.glob('*.json') if not selected_cases or path.stem in selected_cases)
        if not inputs: raise ValueError('缺少实际命令输入')
        case_data = {path.stem: json.loads(path.read_text()) for path in inputs}
        if any(case.get('case_id') != name for name, case in case_data.items()): raise ValueError('命令输入case_id不符')
        military = verify.prepare_military_content(run / 'fixtures')
        identity = current_identity(root, engine, cases, selected_cases, military, report['environment'])
        report['execution_identity'] = identity
        if identity['source'] != report['source']: raise ValueError('准备期间源码身份变化，拒绝建立冲突执行身份')
        for key in ('catalog', 'schemas', 'runtime_inputs'): report[key] = identity[key]
        report['case_inputs'] = {'root': str(cases), **identity['case_inputs']}
        report['military_fixture'] = {'root': str(military.parent), **identity['military_fixture']}
        canonical = root / 'tests/fixtures/architecture/commands'
        expected = [canonical / (name + '.json') for name in REQUIRED_CASE_IDS]
        report['required_case_inputs'] = file_set_identity(canonical, expected)
        report['required_case_ids'] = list(REQUIRED_CASE_IDS)
        report['complete_case_scope'] = (all(path.is_file() for path in expected) and
            set(case_data) == set(REQUIRED_CASE_IDS) and identity['case_inputs'] == report['required_case_inputs'])
        report['stages'] = {'trace:' + name + ':' + label: {'status': 'NOT_RUN'}
                            for name in case_data for label in TRACE_RUNS}
        if performance:
            report['matrix'] = performance_matrix()
            report['performance'] = {'status': 'NOT_RUN',
                'endurance': {name: {'status': 'NOT_RUN'} for name in ('alpha_life', 'second_military')},
                'workloads': {name: {'status': 'NOT_RUN'} for name in ('alpha_life', 'second_military', 'long_running')},
                'scope': '计时保留完整校验；只复用原独立完成阶段，不续接中断7200计时'}
            report['stages'].update({kind + ':' + name: {'status': 'NOT_RUN'} for kind, names in
                [('endurance', report['performance']['endurance']), ('performance', report['performance']['workloads'])]
                for name in names})
        check_identity(); write_report(run / 'start.json', report)
        if trace_receipt and reuse_receipt: raise ValueError('不能同时指定两种复用回执')
        reuse_path = trace_receipt or reuse_receipt; previous = {}
        if reuse_path is not None:
            previous = json.loads(reuse_path.read_text()); check_identity(); validate_reuse_identity(previous, identity)
            if trace_receipt and set(previous.get('cases', {})) != set(case_data): raise ValueError('复用轨迹用例覆盖不完整')
            report['reused_receipt'] = {'path': str(reuse_path), 'sha256': sha256_file(reuse_path)}
        for path in inputs:
            name = path.stem; case = case_data[name]; prior_case = previous.get('cases', {}).get(name)
            reuse_case = prior_case is not None and prior_case.get('status') == 'PASS'
            if trace_receipt and not reuse_case: raise ValueError('待复用轨迹未完成：' + name)
            if reuse_case:
                runs = prior_case.get('runs', [])
                if (len(runs) != 3 or [row.get('run_name', Path(row['log']).parent.name) for row in runs] != list(TRACE_RUNS)):
                    raise ValueError('复用轨迹缺少first/second/restored完整三遍')
                if any('--restore-nodes=' + str(index == 2).lower() not in row.get('command', [])
                       for index, row in enumerate(runs)):
                    raise ValueError('复用轨迹三遍实际恢复参数不符')
            traces = []; evidence_runs = []
            for index, label in enumerate(TRACE_RUNS):
                folder = run / 'traces' / name / label
                command = [engine, '--headless', '--path', str(root / 'game'), '--script', 'res://tests/architecture_trace.gd',
                           '--', '--case=' + str(path), '--out=' + str(folder), '--military-content=' + str(military),
                           '--restore-nodes=' + str(label == 'restored').lower(), '--collector-log=run.log']
                evidence, data = stage('trace', name + ':' + label, command, runs[index] if reuse_case else None, case)
                evidence['run_name'] = label; evidence_runs.append(evidence)
                if evidence['status'] == 'PASS': traces.append(data)
            equal = len(traces) == 3 and traces[0] == traces[1] == traces[2]
            digest = state_digest(traces[0]) if traces else None
            if reuse_case and (not equal or prior_case.get('trace_digest') != digest): raise ValueError('复用三遍完整轨迹或摘要不符')
            report['cases'][name] = {'status': 'PASS' if equal else 'FAIL', 'runs': evidence_runs,
                                    'full_trace_equal': equal, 'trace_digest': digest}
            failed |= not equal
            write_report(run / ('progress-trace-' + name + '.json'), report)
        if performance:
            contents = [('alpha_life', 'res://data/generated/alpha_life.hmcatalog.json'), ('second_military', str(military))]
            for name, content in contents:
                command = [engine, '--headless', '--path', str(root / 'game'), '--script', 'res://tests/architecture_trace.gd',
                           '--', '--mode=endurance', '--out=' + str(run / 'endurance' / name), '--content=' + content,
                           '--ticks=7200', '--interval=720', '--continuation-ticks=30', '--collector-log=run.log']
                prior = previous.get('performance', {}).get('endurance', {}).get(name) if reuse_receipt else None
                evidence, _ = stage('endurance', name, command, prior if prior and prior.get('status') == 'PASS' else None)
                report['performance']['endurance'][name] = evidence; failed |= evidence['status'] != 'PASS'
                refresh_performance_status(report)
                write_report(run / ('progress-endurance-' + name + '.json'), report)
            alpha = report['performance']['endurance']['alpha_life']
            aged = Path(alpha['log']).parent / alpha['data']['final_save'] if alpha['status'] == 'PASS' else None
            for name, content, initial in [(name, content, None) for name, content in contents] + [('long_running', contents[0][1], aged)]:
                if name == 'long_running' and initial is None:
                    evidence = {'status': 'BLOCKED', 'reason': '真实alpha7200终点缺失'}
                else:
                    command = [engine, '--headless', '--path', str(root / 'game'), '--script', 'res://tests/architecture_trace.gd',
                               '--', '--mode=performance', '--out=' + str(run / 'performance' / name), '--content=' + content,
                               '--lengths=720', '--repetitions=5', '--collector-log=run.log']
                    if initial: command.append('--initial-save=' + str(initial))
                    prior = previous.get('performance', {}).get('workloads', {}).get(name) if reuse_receipt else None
                    evidence, data = stage('performance', name, command, prior if prior and prior.get('status') == 'PASS' else None)
                    if evidence['status'] == 'PASS' and data['initial_counts']['tick'] != (7200 if initial else 0):
                        evidence.update(status='FAIL', reason='性能负载起点日期不符')
                    if initial and evidence['status'] == 'PASS':
                        if (data['initial_state_sha256'] != alpha['data']['final_state_sha256'] or
                                sha256_file(initial) != data.get('initial_save_source_sha256')):
                            evidence.update(status='FAIL', reason='长期负载没有复用真实耐久完整终点')
                        evidence['aged_source'] = {'save': str(initial), 'sha256': sha256_file(initial)}
                report['stages']['performance:' + name] = evidence
                report['performance']['workloads'][name] = evidence; failed |= evidence['status'] != 'PASS'
                refresh_performance_status(report)
                write_report(run / ('progress-performance-' + name + '.json'), report)
        if reference is not None:
            old = json.loads(reference.read_text()); report['reference'] = {'path': str(reference), 'sha256': sha256_file(reference), 'cases': {}}
            for name, case in report['cases'].items():
                equal = name in old['cases'] and case['trace_digest'] == old['cases'][name]['trace_digest']
                report['reference']['cases'][name] = {'full_trace_equal': equal}; failed |= not equal
        check_identity()
        report['status'] = 'FAIL' if failed else 'PASS' if performance and report['complete_case_scope'] else 'INCOMPLETE'
    except (KeyboardInterrupt, SystemExit, InterruptedError):
        report.update(status='INTERRUPTED', reason='父采集器中断；已终止并回收当前孩子，完成独立阶段可核验复用')
        if active_log and active_log.with_suffix('.result.json').is_file():
            report['stages'][active_stage] = json.loads(active_log.with_suffix('.result.json').read_text())
        raise
    except (OSError, ValueError, RuntimeError, KeyError, TypeError) as exc:
        report.update(status='FAIL', reason=str(exc))
    finally:
        for name, evidence in report['stages'].items():
            if evidence['status'] != 'PASS': continue
            kind, case_id, *_label = name.split(':')
            try: admit_evidence(evidence, kind, case_data.get(case_id) if kind == 'trace' else None, reuse=True)
            except (OSError, ValueError, RuntimeError, KeyError, TypeError) as exc:
                evidence.update(status='FAIL', reason='完成时证据复核失败：' + str(exc))
                if report['status'] != 'INTERRUPTED': report.update(status='FAIL', reason=evidence['reason'])
        for case in report['cases'].values():
            if any(row['status'] != 'PASS' for row in case['runs']): case['status'] = 'FAIL'
        refresh_performance_status(report)
        report['source_after'] = source_identity(root); report['source_frozen'] = report['source_after'] == report['source']
        if military is not None and 'execution_identity' in report:
            try:
                check_identity(); report['execution_frozen'] = True
            except (OSError, ValueError):
                report['execution_frozen'] = False
            if not report['execution_frozen'] or not report['source_frozen']:
                if report['status'] != 'INTERRUPTED':
                    report.update(status='FAIL', reason='最终源码身份已变化' if not report['source_frozen'] else '最终完整执行身份已变化')
                for evidence in report['stages'].values():
                    if evidence['status'] == 'PASS': evidence.update(status='FAIL', reason='源码/完整执行身份未冻结，证据无效')
                for case in report['cases'].values(): case['status'] = 'FAIL'
                if performance and report['performance'].get('endurance'): report['performance']['status'] = 'FAIL'
        report['ok'] = report['status'] == 'PASS' and report['execution_frozen'] and report['source_frozen']
        report['coverage_note'] = '当前Linux基线；ARCH09历史原档缺口、Web及长跨度排除仍保留'
        write_report(run / 'baseline.json', report)
    return report


def main(argv: list[str] | None = None) -> int:
    """统一采集入口允许短探针，但短/少样本不能冒充完整性能门。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', default=os.environ.get('GODOT_BIN', ''))
    parser.add_argument('--cases', type=Path, default=ROOT / 'tests/fixtures/architecture/commands')
    parser.add_argument('--output', type=Path, default=ROOT / 'verification/architecture-a01')
    parser.add_argument('--trace-only', action='store_true')
    parser.add_argument('--case', action='append')
    parser.add_argument('--trace-receipt', type=Path, help='复用同一冻结源树的三次完整轨迹')
    parser.add_argument('--reuse-receipt', type=Path, help='复用相同完整冻结身份下已完成独立阶段；未完成阶段重跑')
    parser.add_argument('--reference', type=Path, help='与既有基线完整轨迹摘要比较，不丢状态字段')
    args = parser.parse_args(argv)
    report = collect_baseline(ROOT, args.engine, args.cases, args.output,
                              performance=not args.trace_only, selected_cases=args.case,
                              trace_receipt=args.trace_receipt, reuse_receipt=args.reuse_receipt, reference=args.reference)
    print(json.dumps({'status': report['status'], 'receipt': report['receipt']}, ensure_ascii=False))
    return 0 if report['ok'] else 2 if report['status'] in {'BLOCKED', 'INCOMPLETE'} else 1


if __name__ == '__main__':
    raise SystemExit(main())
