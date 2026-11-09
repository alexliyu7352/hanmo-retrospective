"""Compile-only N1 successor; caller owns selection, adoption, link and thin gates.

Queue rows: absolute source/object/cwd, source_sha256, command argv.
Policy: soft_budget_bytes, soft_budget_evidence {path,sha256} (NOT a quota),
unknown_peak_bytes (conservative exclusive reservation), optional timeout_seconds.
History is original GNU-time result rows; changed source/flags/headers use conservative estimates, not proof of current compilation.
A blocked/drained/failed result is NOT permission to link. Resume into a NEW run_dir.
"""
from pathlib import Path
import datetime
import fcntl
import hashlib
import json
import math
import os
import re
import resource
import shutil
import signal
import subprocess
import time

GIB = 1024 ** 3
HEAVY = 2 * GIB
RESERVE = 2 * GIB
MAX_WORKERS = 8
DISK_LOW_WATERMARK = 1_000_000_000
BOUNDED_DRIVER_RESERVE = 256 * 1024 ** 2
BOUNDED_MONITOR_GUARD = 256 * 1024 ** 2


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def stamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def command_key(command):
    # Only output/dependency destinations vary between original N1 attempts.
    out = []
    i = 0
    while i < len(command):
        if command[i] in ('-o', '-MF', '-MT', '-MQ'):
            require(i + 1 < len(command), 'Malformed compiler arguments')
            i += 2
        elif command[i] in ('-MMD', '-MD'):
            i += 1
        else:
            out.append(command[i])
            i += 1
    return hashlib.sha256(json.dumps(out).encode()).hexdigest()


def memory_snapshot():
    """Read real visible cgroup hierarchy, not CPU count or host free alone."""
    info = {line.split(':')[0]: int(line.split()[1]) * 1024
            for line in Path('/proc/meminfo').read_text().splitlines()
            if line.startswith(('MemAvailable:', 'MemTotal:'))}
    groups = [line.split(':', 2) for line in Path('/proc/self/cgroup').read_text().splitlines()]
    limits = []
    for line in Path('/proc/self/mountinfo').read_text().splitlines():
        left, right = line.split(' - ', 1)
        fields, tail = left.split(), right.split()
        version = 2 if tail[0] == 'cgroup2' else 1 if tail[0] == 'cgroup' and 'memory' in tail[2].split(',') else 0
        if not version:
            continue
        matches = [g[2] for g in groups if (version == 2 and g[0] == '0') or
                   (version == 1 and 'memory' in g[1].split(','))]
        for group in matches:
            root, mount = fields[3], Path(fields[4])
            # Cgroup namespaces may expose '/' while mountinfo retains its host root.
            relative = group[len(root):].lstrip('/') if group == root or group.startswith(root.rstrip('/') + '/') else None
            if group == '/':
                relative = ''
            require(relative is not None, 'Cannot resolve current memory cgroup')
            current = mount / relative
            while True:
                limit_file = current / ('memory.max' if version == 2 else 'memory.limit_in_bytes')
                used_file = current / ('memory.current' if version == 2 else 'memory.usage_in_bytes')
                require(limit_file.is_file() and used_file.is_file(), 'Memory cgroup evidence unavailable')
                raw = limit_file.read_text().strip()
                used = int(used_file.read_text().strip())
                limit = None if raw == 'max' else int(raw)
                limits.append(dict(path=str(current), version=version, limit_bytes=limit, used_bytes=used))
                if current == mount:
                    break
                current = current.parent
    finite = [x for x in limits if x['limit_bytes'] is not None]
    return dict(cgroup_limit_status='visible' if limits else 'unknown_no_mount',
                mem_available_bytes=info['MemAvailable'], host_total_bytes=info['MemTotal'],
                cgroups=limits,
                visible_limit_bytes=min([x['limit_bytes'] for x in finite]) if finite else None,
                visible_available_bytes=min([info['MemAvailable']] +
                    [max(0, x['limit_bytes'] - x['used_bytes']) for x in finite]))


def gnu_elapsed_seconds(text):
    """从原 GNU time 取耗时；无该字段时不伪造测量。"""
    for line in text.splitlines():
        if line.strip().startswith('Elapsed (wall clock) time') and ': ' in line:
            try:
                parts = [float(x) for x in line.rsplit(': ', 1)[1].split(':')]
                if len(parts) not in (2, 3) or any(not math.isfinite(x) or x < 0 for x in parts):
                    return None
                value = sum(x * 60 ** i for i, x in enumerate(reversed(parts)))
                return value if value > 0 else None
            except ValueError:
                return None
    return None


def budget_for(row, history, policy, frozen_hash, historical_source=None):
    """Historical peaks are estimates, never same-generation compile evidence.

    Use maximum successful peak for this exact source path, even after source,
    flags or frozen headers change. Same identity: ceil(1.25*peak)+64MiB;
    any mismatch: ceil(1.5*peak)+128MiB. Classify padded >=2GiB as exclusive.
    A recorded killed/timeout attempt is a LOWER bound, forces exclusive, and
    cannot prove a source light. Unknown => explicit conservative reservation.
    """
    # 只为显式工作树迁移查旧实测；真实 queue/argv/result 始终使用新源路径。
    # 旧记录不改 source/command，迁移永远按不同代估算，不能变成缓存复用证据。
    history_source = historical_source or row['source']
    require(Path(history_source).is_absolute(), 'Absolute historical source required')
    proof, lower_bounds, resource_stops = [], [], []
    for old in history:
        if old.get('source') not in {history_source, row['source']}:
            continue
        command = old.get('original_command', old.get('command', []))
        if not command or '-c' not in command or not old.get('source_sha256'):
            continue
        if command.index('-c') + 1 >= len(command):
            continue
        original_input = Path(command[command.index('-c') + 1])
        if not original_input.is_absolute():
            if not old.get('cwd'):
                continue
            original_input = Path(old['cwd']) / original_input
        if original_input.resolve() != Path(old['source']).resolve():
            continue
        stop_kinds = [key for key in ('packed_memory_guard_stop', 'bounded_memory_guard_stop',
                      'owned_group_remaining_after_leader_exit') if old.get(key)]
        stopped = bool(stop_kinds) and old.get('exit_code') not in (None, 0)
        stop_frozen = old.get('frozen_manifest_sha256')
        if stopped and isinstance(stop_frozen, str) and len(stop_frozen) == 64:
            # 具名停止即使 GNU time 未写完仍需隔离；不造 RSS，不把裸 125 叫 OOM。
            resource_stops.append(dict(source=old['source'], source_sha256=old['source_sha256'],
                original_command=command, frozen_manifest_sha256=stop_frozen,
                exit_code=old['exit_code'], stop_kinds=stop_kinds,
                time_log=old.get('time_log'), numeric_RSS_not_inferred=True))
        log = Path(old.get('time_log', ''))
        if not log.is_file():
            continue
        timing_text = log.read_text(errors='replace')
        elapsed = gnu_elapsed_seconds(timing_text)
        match = re.search(r'Maximum resident set size \(kbytes\):\s*(\d+)', timing_text)
        if not match or int(match[1]) != old.get('maximum_resident_set_kib') or int(match[1]) <= 0:
            continue
        frozen = old.get('frozen_manifest_sha256')
        origin = None
        # Original N1 results keep the frozen identity in their run inputs.
        # Preserve that recorded historical identity; do not hash today's heads
        # and pretend they were the headers used by the earlier compiler.
        if not frozen:
            inputs = log.parent / 'batch-inputs.json'
            if inputs.is_file():
                recorded = json.loads(inputs.read_text())
                frozen = recorded.get('frozen_manifest_sha256')
                origin = dict(path=str(inputs), sha256=sha(inputs), frozen_manifest=recorded.get('frozen_manifest'))
        if not isinstance(frozen, str) or len(frozen) != 64:
            continue
        mismatch = dict(source_path=old['source'] != row['source'],
                        source=old['source_sha256'] != row['source_sha256'],
                        command=command_key(command) != command_key(row['command']),
                        frozen_headers=frozen != frozen_hash)
        entry = dict(peak_bytes=int(match[1])*1024, time_log=str(log), time_log_sha256=sha(log),
                     original_command=command, source_sha256=old['source_sha256'],
                     frozen_manifest_sha256=frozen, frozen_origin=origin, mismatch=mismatch,
                     exit_code=old.get('exit_code'), actual_source=row['source'],
                     historical_source=old['source'],
                     elapsed_seconds=elapsed if elapsed is not None else old.get('seconds') if
                         isinstance(old.get('seconds'), (int, float)) and math.isfinite(old['seconds']) and old['seconds'] > 0 else None,
                     elapsed_basis='GNU_time_wall_seconds' if elapsed is not None else 'original_scheduler_wall_seconds_or_missing')
        if stopped or old.get('oom_or_signal_evidence') or old.get('timeout_seconds') or old.get('exit_code') in (-9, -15, 124, 137, 143):
            lower_bounds.append(entry)
        elif old.get('exit_code') == 0:
            proof.append(entry)
    if not proof:
        lower = max([0] + [x['peak_bytes'] for x in lower_bounds])
        return dict(bytes=max(policy['unknown_peak_bytes'], (lower*3+1)//2+128*1024**2 if lower else 0),
                    exclusive=True, known=False, reason='No reliable successful historical peak; exclusive estimate',
                    evidence=[], failed_peak_lower_bounds=lower_bounds, resource_stop_evidence=resource_stops, prediction_is_upper_bound=False)
    peak = max(x['peak_bytes'] for x in proof)
    mismatch = any(any(x['mismatch'].values()) for x in proof)
    allowance = (peak*3+1)//2+128*1024**2 if mismatch else (peak*5+3)//4+64*1024**2
    if lower_bounds:
        allowance = max(allowance, policy['unknown_peak_bytes'],
                        (max(x['peak_bytes'] for x in lower_bounds)*3+1)//2+128*1024**2)
    if resource_stops:
        allowance = max(allowance, policy['unknown_peak_bytes'])
    return dict(bytes=allowance, exclusive=bool(lower_bounds) or bool(resource_stops) or allowance >= HEAVY,
                known=True, measured_peak_bytes=peak, evidence=proof,
                measured_elapsed_seconds=max([x['elapsed_seconds'] for x in proof if x['elapsed_seconds'] is not None], default=None),
                failed_peak_lower_bounds=lower_bounds, resource_stop_evidence=resource_stops, historical_mismatch=mismatch,
                estimate_rule='max(historical_padded, unknown_reservation, verified_failure_lower_bound_padding)' if resource_stops else '1.5x+128MiB' if mismatch else '1.25x+64MiB',
                prediction_is_upper_bound=False)


def admission(budget, active_budgets, snapshot, policy):
    if len(active_budgets) >= MAX_WORKERS:
        return False, 'worker_limit'
    if active_budgets and (budget['exclusive'] or any(x['exclusive'] for x in active_budgets)):
        return False, 'exclusive'
    total = sum(x['bytes'] for x in active_budgets) + budget['bytes']
    # Soft cap governs LIGHT parallel work only. Heavy/unknown retain their
    # full estimate, run alone, and must fit actual available memory + reserve.
    ceiling = policy['soft_budget_bytes'] if not budget['exclusive'] else None
    if snapshot['visible_limit_bytes'] is not None:
        cgroup_ceiling = max(0, snapshot['visible_limit_bytes'] - RESERVE)
        ceiling = cgroup_ceiling if ceiling is None else min(ceiling, cgroup_ceiling)
    # Conservatively reserve active full peaks even if RSS is already included in
    # memory.current. This may underfill, but never treats headroom as extra quota.
    if ceiling is not None and total > ceiling:
        return False, 'soft_budget_or_visible_cgroup_limit'
    if total + RESERVE > snapshot['visible_available_bytes']:
        return False, 'available_memory'
    return True, 'admitted'


def disk_admission(free_bytes, active_bytes, next_bytes, floor_bytes):
    """实际剩余空间须先保用户指定 1 GB，再容纳已派发与下一原子对象。"""
    require(floor_bytes >= DISK_LOW_WATERMARK, 'Disk floor must preserve at least 1000000000 bytes')
    needed = floor_bytes + active_bytes + next_bytes
    return free_bytes >= needed, needed


def bounded_heavy_limit(budget, active_budgets, snapshot, policy):
    """只对旧估算不适配的已知重 TU 提供独占正式尝试，不改原峰值证据。"""
    if not policy.get('allow_bounded_heavy_attempt') or active_budgets or not budget['known'] or not budget['exclusive']:
        return None
    ok, reason = admission(budget, [], snapshot, policy)
    if ok or reason not in ('available_memory', 'soft_budget_or_visible_cgroup_limit'):
        return None
    available = snapshot['visible_available_bytes']
    cap = available - RESERVE - BOUNDED_DRIVER_RESERVE - BOUNDED_MONITOR_GUARD
    page = os.sysconf('SC_PAGE_SIZE')
    cap = cap // page * page
    if cap < HEAVY:
        return None
    return dict(kind='inherited_per_process_RLIMIT_AS', as_limit_bytes=cap,
                driver_reserve_bytes=BOUNDED_DRIVER_RESERVE,
                monitor_guard_bytes=BOUNDED_MONITOR_GUARD, headroom_bytes=RESERVE,
                admission_available_bytes=available, original_estimate_bytes=budget['bytes'],
                group_rss_stop_bytes=cap + BOUNDED_DRIVER_RESERVE,
                group_hard_limit=False, cgroup_limit_status=snapshot['cgroup_limit_status'],
                automatic_retry=False)



def validate_standing_rule(policy, headers):
    """仅接受本用户具名常设规则；不是可配置任意限额的通用逃逸口。"""
    rule = policy.get('standing_exclusive_memory_rule')
    if rule is None:
        return
    require(isinstance(rule, dict) and set(rule) == {'rule_id', 'world_codec_source', 'authorization'},
            'Invalid standing memory rule shape')
    require(rule['rule_id'] == 'user_20261029_memtotal_minus_2gib_available_1gib',
            'Unknown standing memory rule')
    require(policy.get('timeout_seconds') == 1800,
            'Standing rule preserves the approved 1800 second timeout')
    source = Path(rule['world_codec_source'])
    require(source.is_absolute() and source.resolve() == source and
            source.as_posix().endswith('/native/world/src/resident/kernel/world_codec.cpp'),
            'Standing world codec requires its unique canonical project path')
    proof = rule['authorization']
    require(isinstance(proof, dict) and set(proof) == {'path', 'sha256'} and
            Path(proof['path']).is_absolute(), 'Explicit standing authorization required')
    locked = {x['path']: x['sha256'] for x in headers}
    require(locked.get(str(source)) == sha(source) and
            locked.get(proof['path']) == proof['sha256'] == sha(proof['path']),
            'Standing source and authorization must be actual frozen inputs')
    authority = json.loads(Path(proof['path']).read_text())
    require(authority.get('user_slack_ts') == '1793290848.945159' and
            authority.get('standing_rule_recorded') is True and authority.get('user_text') and
            authority.get('physical_stop_available_bytes') == GIB and
            authority.get('as_formula') == 'actual_MemTotal_bytes - 2147483648',
            'Standing authorization does not match the actual user decision')


def standing_memory_limit(budget, snapshot, policy, reason):
    """独占继承自身 AS；不把进程 AS 冒作整组 RSS/cgroup 硬配额。"""
    if snapshot['visible_available_bytes'] < GIB:
        return None
    cap = snapshot['host_total_bytes'] - RESERVE
    if cap < HEAVY:
        return None
    return dict(kind='standing_user_inherited_per_process_RLIMIT_AS',
                as_limit_bytes=cap, formula='actual_MemTotal_bytes - 2147483648',
                mem_total_bytes=snapshot['host_total_bytes'],
                admission_available_bytes=snapshot['mem_available_bytes'],
                admission_visible_available_bytes=snapshot['visible_available_bytes'],
                physical_stop_available_bytes=GIB,
                driver_reserve_bytes=BOUNDED_DRIVER_RESERVE,
                monitor_guard_bytes=BOUNDED_MONITOR_GUARD,
                original_estimate_bytes=budget['bytes'],
                group_rss_stop_bytes=cap + BOUNDED_DRIVER_RESERVE,
                group_hard_limit=False, effective_exclusive=True,
                cgroup_limit_status=snapshot['cgroup_limit_status'], automatic_retry=False,
                standing_reason=reason,
                authorization=policy['standing_exclusive_memory_rule']['authorization'])


def runtime_tool_inputs(policy, headers):
    """独立锁工具与用户回执，不把新 Python 当成新增 C++ include 而全量失效。"""
    if not policy.get('measured_rss_packing'):
        return headers
    tools = policy.get('execution_tool_lock')
    require(isinstance(tools, dict) and set(tools) == {'scheduler', 'runner'}, 'Exact execution tool lock required')
    require(Path(tools['scheduler']['path']).resolve() == Path(__file__).resolve(), 'Scheduler lock names another source')
    require(policy.get('standing_exclusive_memory_rule'), 'Packing preserves the standing exclusive rule')
    items = list(tools.values()) + [policy['measured_rss_packing']['authorization'],
                                   policy['standing_exclusive_memory_rule']['authorization']]
    locked = {x['path']: x['sha256'] for x in headers}
    result = list(headers)
    for item in items:
        require(set(item) == {'path', 'sha256'} and Path(item['path']).is_absolute(), 'Invalid tool/authorization lock')
        require(sha(item['path']) == item['sha256'], 'Execution tool or authorization drift')
        require(item['path'] not in locked or locked[item['path']] == item['sha256'], 'Conflicting runtime input lock')
        if item['path'] not in locked:
            result.append(dict(item))
            locked[item['path']] = item['sha256']
    return result


def validate_packing_rule(policy, headers):
    """具名批准只改预算装箱；不开放作者配置或任意保护线。"""
    rule = policy.get('measured_rss_packing')
    if rule is None:
        return
    require(set(rule) == {'rule_id', 'authorization'} and
            rule['rule_id'] == 'user_20261029_verified_rss_packing', 'Unknown packing rule')
    proof = rule['authorization']
    locked = {x['path']: x['sha256'] for x in headers}
    require(locked.get(proof['path']) == proof['sha256'] == sha(proof['path']), 'Packing authorization not frozen')
    authority = json.loads(Path(proof['path']).read_text())
    require(authority.get('user_slack_ts') == '1793302239.976639' and authority.get('user_text') and
            authority.get('packing_basis') == 'verified_previous_success_GNU_time_max_RSS' and
            authority.get('cap_formula') == 'actual_MemTotal_bytes - 2147483648', 'Packing decision identity differs')
    require(policy.get('max_workers') == MAX_WORKERS and policy.get('reserve_bytes') == RESERVE and
            policy.get('timeout_seconds') == 1800 and policy.get('keep_going_compile_errors') is True,
            'Packing may not change the worker, reserve, timeout, or error policy')


def packing_budget(budget):
    """保留原证据与 padded estimate；装箱只用已验 GNU time 成功峰，失败仍是下界。"""
    if not budget['known'] or budget.get('failed_peak_lower_bounds') or budget.get('resource_stop_evidence'):
        return dict(budget, exclusive=True)
    peak = budget.get('measured_peak_bytes')
    require(isinstance(peak, int) and peak > 0, 'Verified packing RSS missing')
    return dict(budget, bytes=peak, exclusive=False, original_padded_bytes=budget['bytes'],
                packing_basis='maximum_verified_success_GNU_time_RSS', prediction_is_upper_bound=False)


def packing_order_key(index, budget):
    """峰值优先、已记录耗时次之、原序最后；不改 TU 身份或命令。"""
    known = budget['known'] and not budget.get('failed_peak_lower_bounds') and not budget.get('resource_stop_evidence')
    seconds = budget.get('measured_elapsed_seconds')
    duration = seconds if isinstance(seconds, (int, float)) and math.isfinite(seconds) and seconds > 0 else float('inf')
    return (0 if known else 1, budget.get('measured_peak_bytes', budget['bytes']), duration, index)


def active_admission_budgets(active, policy):
    """扣减仅取当前 owned group RSS，不用曾经峰值；组不可证时不释放工位。"""
    if not policy.get('measured_rss_packing'):
        return [item['budget'] for item in active.values()]
    values = []
    for pid, item in active.items():
        group = owned_group_memory(pid)
        require(not group['errors'], 'Cannot verify active packing group')
        require(group['members'] or item['proc'].poll() is not None, 'Active packing group disappeared')
        values.append(dict(item['budget'], observed_group_rss_bytes=group['rss_bytes']))
    return values


def packed_admission(budget, active_budgets, snapshot, policy):
    """真实峰值总和受 total−2GiB 约束，未来组增长另保 driver 与实时物理余量。"""
    if len(active_budgets) >= MAX_WORKERS:
        return False, 'worker_limit'
    if active_budgets and (budget['exclusive'] or any(x['exclusive'] for x in active_budgets)):
        return False, 'exclusive'
    cap = snapshot['host_total_bytes'] - RESERVE
    if snapshot['visible_limit_bytes'] is not None:
        cap = min(cap, snapshot['visible_limit_bytes'] - RESERVE)
    if sum(x['bytes'] for x in active_budgets) + budget['bytes'] > cap:
        return False, 'packing_total_cap'
    # GNU time 是单进程最大 RSS，不是组峰和；每工位仍另保原 256MiB driver 窗。
    future = budget['bytes'] + BOUNDED_DRIVER_RESERVE
    for item in active_budgets:
        current = item.get('observed_group_rss_bytes')
        require(isinstance(current, int) and current >= 0, 'Current owned RSS required for packing')
        future += max(0, item['bytes'] + BOUNDED_DRIVER_RESERVE - current)
    if future + RESERVE + BOUNDED_MONITOR_GUARD > snapshot['visible_available_bytes']:
        return False, 'available_memory'
    return True, 'measured_rss_pack'


def packed_memory_guard(snapshot, groups, active):
    """并行普通组保原 2GiB+256MiB 物理线；不可把 standing 的 1GiB 用到此处。"""
    return (snapshot['visible_available_bytes'] < RESERVE + BOUNDED_MONITOR_GUARD or
            any(group['errors'] or (not group['members'] and active[pid]['proc'].poll() is None)
                for pid, group in groups.items()))


def packed_group_is_live(item, group):
    """已返回且组排空的真正成功，不因同时另一组触线被重写成失败。"""
    return item['proc'].poll() is None or bool(group['members']) or bool(group['errors'])


def stop_packed_active(active, groups, observation):
    """只标记并停止触线时仍在途的具名拥有组，已排空项留原收集路径。"""
    for pid, item in active.items():
        if packed_group_is_live(item, groups[pid]):
            item['result']['packed_memory_guard_stop'] = observation
            stop_owned(item['proc'])


def dispatch_admission(row, budget, active_budgets, snapshot, policy):
    """候选及哈希复核后派发共用；原历史预算只读，有效独占单独传给工位。"""
    rule = policy.get('standing_exclusive_memory_rule')
    world_codec = bool(rule and row['source'] == rule['world_codec_source'])
    gate_budget = dict(budget, exclusive=True) if world_codec else budget
    if world_codec:
        if active_budgets:
            return False, 'exclusive', None, gate_budget
        limit = standing_memory_limit(budget, snapshot, policy, 'world_codec_always_exclusive')
        return bool(limit), 'standing_world_codec' if limit else 'standing_memory_floor', limit, gate_budget
    if policy.get('measured_rss_packing'):
        gate_budget = packing_budget(budget)
        ok, reason = packed_admission(gate_budget, active_budgets, snapshot, policy)
    else:
        ok, reason = admission(budget, active_budgets, snapshot, policy)
    if (rule and not ok and not active_budgets and budget['known'] and budget['exclusive'] and
            reason in ('available_memory', 'soft_budget_or_visible_cgroup_limit', 'packing_total_cap')):
        limit = standing_memory_limit(budget, snapshot, policy, 'known_heavy_default_estimate_does_not_fit')
        return bool(limit), 'standing_nonfitting_heavy' if limit else 'standing_memory_floor', limit, dict(gate_budget, exclusive=True)
    limit = None if ok else bounded_heavy_limit(budget, active_budgets, snapshot, policy)
    return bool(ok or limit), 'bounded_heavy_formal_attempt' if limit else reason, limit, dict(gate_budget, exclusive=True) if limit else gate_budget


def limited_memory_guard(limit, snapshot, group):
    """常设重档只改其自身保护线；其余原 bounded 规则仍是 2 GiB 加提前余量。"""
    return (not group['members'] or bool(group['errors']) or
            snapshot['visible_available_bytes'] < limit.get('physical_stop_available_bytes',
                                                            RESERVE + BOUNDED_MONITOR_GUARD) or
            group['rss_bytes'] > limit['group_rss_stop_bytes'])


def child_address_limit(cap):
    """仅在本调度器即将 exec 的子进程中降低自身限额；不改父进程或系统。"""
    resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def owned_group_memory(pgid):
    """只统计本次 start_new_session 的进程组；RSS 之和保守含共享页重复。"""
    members, errors = [], []
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            fields = (path / 'stat').read_text().rpartition(')')[2].split()
            if int(fields[2]) != pgid or int(fields[3]) != pgid or fields[0] in ('Z', 'X'):
                continue
        except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError, IndexError):
            continue
        try:
            values = {line.split(':')[0]: int(line.split()[1]) * 1024
                      for line in (path / 'status').read_text().splitlines()
                      if line.startswith(('VmRSS:', 'VmSize:'))}
            members.append(dict(pid=int(path.name), rss_bytes=values.get('VmRSS', 0),
                                virtual_bytes=values.get('VmSize', 0)))
        except (FileNotFoundError, ProcessLookupError):
            continue
        except (PermissionError, ValueError, IndexError) as error:
            errors.append(dict(pid=int(path.name), error=str(error)))
    return dict(members=members, rss_bytes=sum(x['rss_bytes'] for x in members), errors=errors)


def stop_owned(proc):
    """只停本次独立组；组长退出不代表子进程排空，排空前不释放工位。"""
    group = owned_group_memory(proc.pid)
    require(not group['errors'], 'Cannot verify owned group before stop')
    if group['members']:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline, killed = time.monotonic() + 5, False
    while True:
        group = owned_group_memory(proc.pid)
        require(not group['errors'], 'Cannot verify owned group drain')
        if not group['members']:
            break
        if time.monotonic() >= deadline:
            require(not killed, 'Owned group did not drain; no further dispatch permitted')
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            killed, deadline = True, time.monotonic() + 5
        time.sleep(0.05)
    proc.wait(timeout=5)


def observe_wait(records, source, reason, generation, observation):
    """等待诊断按源/原因/预算代聚合；PID 轮换不再复制整份预算证据。"""
    key = (source, reason, generation)
    previous = records.get(key)
    if previous is None:
        records[key] = dict(source=source, reason=reason, budget_generation=generation,
                            observations=1, suppressed_observations=0,
                            first_at=observation['at'], last_at=observation['at'],
                            latest=observation)
        return True
    previous['observations'] += 1
    previous['suppressed_observations'] += 1
    previous['last_at'] = observation['at']
    previous['latest'] = observation
    return False


def ordinary_compile_failure(code, result, output, resource_evidence):
    """只把已排空的普通编译诊断归入收错；资源、信号、超时或工具异常仍停。"""
    return code == 1 and not resource_evidence and not any(result.get(key) for key in
        ('timeout_seconds', 'disk_low_watermark_stop', 'bounded_memory_guard_stop', 'packed_memory_guard_stop',
         'owned_group_remaining_after_leader_exit')) and 'internal compiler error' not in output.lower()


def compile_queue(queue, run_dir, frozen_manifest, time_tool, policy,
                  history=(), prior_run=None, lock_handle=None,
                  history_sources=None):
    """Return status complete/failed/drained/blocked. No linker or live source writes.

    frozen_manifest is {files:[{path,sha256}]}; its exact hash participates in
    resume identity. Prior successful object AND dependency hashes are checked.
    Caller passes its already-held original queue.lock handle, or holds no lock
    and supplies policy['lock_path'] for this function to acquire.
    """
    # caller 只可传已核一一相对路径映射；此参数仅用于预算，不修改编译输入。
    history_sources = dict(history_sources or {})
    require(set(history_sources) <= {r['source'] for r in queue},
            'History mapping contains a source outside the actual compile queue')
    require(len(set(history_sources.values())) == len(history_sources) and
            all(Path(k).is_absolute() and Path(v).is_absolute()
                for k, v in history_sources.items()), 'Invalid historical source mapping')
    evidence = policy['soft_budget_evidence']
    require(sha(evidence['path']) == evidence['sha256'], 'Soft budget evidence drift')
    profile = json.loads(Path(evidence['path']).read_text())
    require(profile['exit_code'] == 0 and 0 < policy['soft_budget_bytes'] <= profile['peak_rss_kib'] * 1024, 'Soft budget must not exceed approved successful profile peak')
    require(policy.get('timeout_seconds', 600) > 0, 'Positive timeout required')
    require(policy['unknown_peak_bytes'] >= HEAVY, 'Unknown peak must reserve >=2 GiB exclusive')
    disk_floor = policy.get('minimum_free_disk_bytes', DISK_LOW_WATERMARK)
    require(disk_floor >= DISK_LOW_WATERMARK, 'User disk low-watermark cannot be reduced')
    out = Path(run_dir).resolve()
    out.mkdir(exist_ok=False)
    manifest = Path(frozen_manifest).resolve()
    frozen_hash = sha(manifest)
    headers = json.loads(manifest.read_text())['files']
    headers = runtime_tool_inputs(policy, headers)
    validate_packing_rule(policy, headers)
    validate_standing_rule(policy, headers)
    time_tool = Path(time_tool).resolve()
    require(time_tool.is_file() and os.access(time_tool, os.X_OK), 'GNU time executable required')
    owned_lock = lock_handle is None
    lock = open(policy['lock_path'], 'a') if owned_lock else lock_handle
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def save(name, value):
        temporary = out / (name + '.writing')
        temporary.write_text(json.dumps(value, indent=2) + '\n')
        temporary.replace(out / name)

    def check_frozen():
        require(sha(manifest) == frozen_hash, 'Frozen manifest drift')
        for item in headers:
            require(sha(item['path']) == item['sha256'], 'Frozen input drift: ' + item['path'])
        for row in queue:
            require(sha(row['source']) == row['source_sha256'], 'Selected source drift: ' + row['source'])

    def event(name, value, terminal=False, reserved_bytes=0):
        encoded = json.dumps(value) + '\n'
        if not terminal and shutil.disk_usage(out).free < disk_floor + reserved_bytes + len(encoded.encode()):
            return False
        with (out / name).open('a') as stream:
            stream.write(encoded)
        return True

    identity = hashlib.sha256(json.dumps(dict(queue=queue, frozen=frozen_hash,
                                             history_sources=history_sources, policy=policy), sort_keys=True).encode()).hexdigest()
    active, attempts, final = {}, [], {}
    pending = list(enumerate(queue))
    retry_counts = {}
    waiting_records = {}
    disk_stop_recorded = False
    try:
        require(len({r['source'] for r in queue}) == len(queue) and
                len({r['object'] for r in queue}) == len(queue), 'Duplicate queue source/object')
        for row in queue:
            require(all(Path(row[k]).is_absolute() for k in ('source', 'object', 'cwd')), 'Absolute queue paths required')
            if policy.get('standing_exclusive_memory_rule') and Path(row['source']).name == 'world_codec.cpp':
                require(row['source'] == policy['standing_exclusive_memory_rule']['world_codec_source'],
                        'Another same-named world codec is not covered by the standing rule')
            require(Path(row['object']).parent.is_dir(), 'Caller must create approved object parent before scheduling: ' + str(Path(row['object']).parent))
            cmd = row['command']
            require(cmd.count('-c') == 1 and cmd.count('-o') == 1 and
                    Path(cmd[cmd.index('-c') + 1]).resolve() == Path(row['source']), 'Original compile command required')
        check_frozen()
        if prior_run:
            previous = Path(prior_run)
            require(json.loads((previous / 'inputs.json').read_text())['identity'] == identity, 'Resume identity mismatch')
            terminal = json.loads((previous / 'result.json').read_text())
            for old in terminal['results']:
                if old['exit_code'] == 0 and old.get('atomic_cache_update'):
                    require(sha(old['object']) == old['object_sha256'] and sha(old['dependency_file']) == old['dependency_sha256'], 'Resume cache drift')
                    final[old['source']] = old
            # Known terminal failures are not silently retried under a new run ID.
            for old in terminal['results']:
                if old['exit_code'] != 0:
                    final[old['source']] = old
            pending = [(i, r) for i, r in pending if r['source'] not in final]
        budgets = {r['source']: budget_for(r, history, policy, frozen_hash,
                                          history_sources.get(r['source'])) for r in queue}
        if policy.get('measured_rss_packing'):
            pending.sort(key=lambda pair: packing_order_key(pair[0], budgets[pair[1]['source']]))
        save('inputs.json', dict(identity=identity, frozen_manifest=str(manifest), frozen_sha256=frozen_hash,
                                 history_sources=history_sources,
                                policy=policy, budgets=budgets, queue=queue, dispatch_order=[r['source'] for _, r in pending],
                                runtime_tool_inputs=policy.get('execution_tool_lock'), initial_memory=memory_snapshot()))
        stop_reason = 'failed' if any(x['exit_code'] != 0 for x in final.values()) else None
        while pending or active:
            # 运行中首次观察到低于 1 GB，停止新派发并停止本调度器拥有的编译。
            # 未完成临时对象/日志保留；它们不是 OOM 或代码失败，不自动重试或链接。
            free_now = shutil.disk_usage(out).free
            if (free_now < disk_floor or stop_reason == 'disk_low_watermark') and not disk_stop_recorded:
                stop_reason = 'disk_low_watermark'
                disk_stop_recorded = True
                observation = dict(at=stamp(), free_bytes=free_now, floor_bytes=disk_floor,
                                   active_owned_pids=sorted(active), pending_count=len(pending))
                event('disk-low-watermark.jsonl', observation, terminal=True)
                print(json.dumps(dict(status='disk_low_watermark', **observation)), flush=True)
                for pid, item in active.items():
                    if item['proc'].poll() is None:
                        item['result']['disk_low_watermark_stop'] = observation
                        stop_owned(item['proc'])
            if policy.get('measured_rss_packing') and active:
                # 普通装箱也监测真实物理线；独占 standing 才能沿其原 1 GiB 线。
                snapshot = memory_snapshot()
                groups = {pid: owned_group_memory(pid) for pid in active}
                for pid, item in active.items():
                    group = groups[pid]
                    item['result']['observed_peak_group_rss_bytes'] = max(item['result'].get('observed_peak_group_rss_bytes', 0), group['rss_bytes'])
                    item['result']['minimum_observed_available_bytes'] = min(item['result'].get('minimum_observed_available_bytes', snapshot['visible_available_bytes']), snapshot['visible_available_bytes'])
                regular = [item for pid, item in active.items() if not item.get('bounded_limit') and packed_group_is_live(item, groups[pid])]
                if regular and packed_memory_guard(snapshot, groups, active):
                    stop_reason = 'packed_memory_stop'
                    observation = dict(at=stamp(), memory=snapshot, owned_groups=groups, physical_floor_bytes=RESERVE+BOUNDED_MONITOR_GUARD)
                    stop_packed_active(active, groups, observation)
            for pid, item in list(active.items()):
                proc = item['proc']
                limited = item.get('bounded_limit')
                if proc.poll() is None and limited:
                    snapshot = memory_snapshot()
                    group = owned_group_memory(pid)
                    item['result']['bounded_observed_peak_group_rss'] = max(
                        item['result'].get('bounded_observed_peak_group_rss', 0), group['rss_bytes'])
                    item['result']['minimum_observed_available_bytes'] = min(
                        item['result'].get('minimum_observed_available_bytes', snapshot['visible_available_bytes']),
                        snapshot['visible_available_bytes'])
                    item['result']['minimum_observed_mem_available_bytes'] = min(
                        item['result'].get('minimum_observed_mem_available_bytes', snapshot['mem_available_bytes']),
                        snapshot['mem_available_bytes'])
                    if proc.poll() is None and limited_memory_guard(limited, snapshot, group):
                        item['result']['bounded_memory_guard_stop'] = dict(at=stamp(), memory=snapshot, owned_group=group)
                        stop_reason = 'bounded_memory_stop'
                        stop_owned(proc)
                if proc.poll() is None and time.monotonic() - item['started'] > policy.get('timeout_seconds', 600):
                    item['result']['timeout_seconds'] = policy.get('timeout_seconds', 600)
                    stop_owned(proc)
                if proc.poll() is None:
                    continue
                remaining = owned_group_memory(pid)
                require(not remaining['errors'], 'Cannot verify terminal owned group')
                if remaining['members']:
                    item['result']['owned_group_remaining_after_leader_exit'] = remaining
                    stop_reason = 'owned_group_stop'
                    stop_owned(proc)
                item['handle'].close()
                result = item['result']
                code = 125 if result.get('bounded_memory_guard_stop') or result.get('packed_memory_guard_stop') or result.get('owned_group_remaining_after_leader_exit') else 124 if result.get('timeout_seconds') else proc.returncode
                text = item['log'].read_text(errors='replace')
                timing = item['timing'].read_text(errors='replace') if item['timing'].exists() else ''
                match = re.search(r'Maximum resident set size \(kbytes\):\s*(\d+)', timing)
                result.update(exit_code=code, ended_at=stamp(), seconds=time.monotonic() - item['started'],
                              maximum_resident_set_kib=int(match[1]) if match else None, atomic_cache_update=False)
                check_frozen()
                killed = code < 0 or code in (137, 143) or any(x in text.lower() for x in ('killed signal', 'out of memory', 'cannot allocate memory'))
                disk_stopped = bool(result.get('disk_low_watermark_stop'))
                result['oom_or_signal_evidence'] = killed and not disk_stopped and not result.get('bounded_memory_guard_stop') and not result.get('packed_memory_guard_stop')
                ordinary_error = ordinary_compile_failure(code, result, text, killed)
                if code != 0:
                    result['failure_class'] = 'ordinary_compile_error' if ordinary_error else 'resource_or_tool_failure'
                if code == 0:
                    require(item['temp'].is_file() and item['dep'].is_file() and match and int(match[1]) > 0, 'Successful TU missing object/dep/RSS evidence')
                    dependency_cache = Path(result['object'] + '.d')
                    dependency_pending = Path(str(dependency_cache) + '.' + out.name + '.pending')
                    require(not dependency_pending.exists(), 'Dependency pending already exists')
                    shutil.copyfile(item['dep'], dependency_pending)
                    item['temp'].replace(result['object'])
                    dependency_pending.replace(dependency_cache)
                    result['dependency_cache_update'] = True
                    result.update(atomic_cache_update=True, object_sha256=sha(result['object']),
                                  dependency_file=str(item['dep']), dependency_sha256=sha(item['dep']))
                retry = not policy.get('keep_going_compile_errors') and not limited and not disk_stopped and not result.get('packed_memory_guard_stop') and not result.get('owned_group_remaining_after_leader_exit') and code != 0 and (killed or result.get('timeout_seconds')) and retry_counts.get(result['source'], 0) == 0
                if disk_stopped and code != 0:
                    pending.append((item['index'], item['row']))
                    result['unfinished_object_preserved'] = str(item['temp'])
                elif retry:
                    retry_counts[result['source']] = 1
                    failed_lower_bound = (result['maximum_resident_set_kib'] or 0) * 1024
                    retry_reservation = (failed_lower_bound * 3 + 1) // 2 + 128 * 1024 ** 2 if failed_lower_bound else 0
                    budgets[result['source']] = dict(bytes=max(budgets[result['source']]['bytes'], policy['unknown_peak_bytes'], retry_reservation), exclusive=True, known=False, reason='One isolated retry after terminal resource/timeout failure', failed_peak_lower_bound_bytes=failed_lower_bound, failed_time_log=result['time_log'], estimate_rule='max(old, unknown, failed_RSS*1.5+128MiB)', prediction_is_upper_bound=False)
                    pending.insert(0, (item['index'], item['row']))
                else:
                    final[result['source']] = result
                    if code != 0 and not (policy.get('keep_going_compile_errors') and ordinary_error) and stop_reason not in ('disk_low_watermark', 'bounded_memory_stop', 'owned_group_stop', 'packed_memory_stop'):
                        stop_reason = 'failed'
                result['continued_after_compile_error'] = bool(policy.get('keep_going_compile_errors') and ordinary_error)
                result['retry_scheduled'] = bool(retry)
                attempts.append(result)
                event('compile-results.jsonl', result, terminal=True)
                del active[pid]
            save('active-calls.json', [v['result'] for v in active.values()])
            if (out / 'drain-request.json').exists() and not stop_reason:
                stop_reason = 'drained'
            while pending and not stop_reason:
                chosen = None
                chosen_limit = None
                snapshot = memory_snapshot()
                # Do not let an oversized exclusive TU at the head block other
                # fitting work. The deferred TU remains pending, never shrunk.
                for pending_index, (index, row) in enumerate(pending):
                    budget = budgets[row['source']]
                    ok, reason, limit, gate_budget = dispatch_admission(
                        row, budget, active_admission_budgets(active, policy), snapshot, policy)
                    disk_ok, disk_needed = disk_admission(
                        shutil.disk_usage(Path(row['object']).parent).free,
                        sum(v['disk_budget'] for v in active.values()),
                        max(64 * 1024 ** 2, 2 * Path(row['object']).stat().st_size if Path(row['object']).exists() else 0), disk_floor)
                    if not disk_ok:
                        ok, reason = False, 'atomic_object_disk_budget'
                    now = stamp()
                    first_wait = False if ok else observe_wait(waiting_records, row['source'], reason,
                        retry_counts.get(row['source'], 0), dict(at=now, active_count=len(active),
                            visible_available_bytes=snapshot['visible_available_bytes'],
                            free_disk_bytes=shutil.disk_usage(out).free))
                    if first_wait:
                        logged = event('admissions.jsonl', dict(at=now, source=row['source'], budget=budget, memory=snapshot,
                                                   active=len(active), admitted=ok, reason=reason, disk_required=disk_needed,
                                                   minimum_free_disk_bytes=disk_floor,
                                                   bounded_limit=limit, budget_generation=retry_counts.get(row['source'], 0),
                                                   estimated_active_plus_next_bytes=sum(v['budget']['bytes'] for v in active.values())+gate_budget['bytes'],
                                                   soft_budget_bytes=policy['soft_budget_bytes'], soft_budget_applies=not gate_budget['exclusive'] and not bool(policy.get('measured_rss_packing')), effective_exclusive=gate_budget['exclusive'], soft_budget_is_quota=False, packing_cap_bytes=snapshot['host_total_bytes']-RESERVE if policy.get('measured_rss_packing') else None),
                                                   reserved_bytes=sum(v['disk_budget'] for v in active.values()))
                        if not logged:
                            stop_reason = 'disk_low_watermark'
                            break
                    if ok:
                        chosen = pending_index
                        chosen_limit = limit
                        break
                if chosen is None:
                    if not active and not stop_reason:
                        stop_reason = 'blocked'
                    break
                check_frozen()
                # 完整哈希检查后再取实时预算，之前的候选判断不是实际准入证据。
                active_budgets = active_admission_budgets(active, policy)
                snapshot = memory_snapshot()
                ok, reason, chosen_limit, gate_budget = dispatch_admission(
                    row, budget, active_budgets, snapshot, policy)
                disk_reservation = sum(v['disk_budget'] for v in active.values()) + max(
                    64 * 1024 ** 2, 2 * Path(row['object']).stat().st_size if Path(row['object']).exists() else 0)
                disk_ok, disk_needed = disk_admission(shutil.disk_usage(out).free, 0, disk_reservation, disk_floor)
                if not disk_ok:
                    stop_reason = 'disk_low_watermark'
                    break
                if not ok:
                    if not active:
                        stop_reason = 'blocked'
                    break
                if not event('admissions.jsonl', dict(at=stamp(), source=row['source'], budget=budget,
                        memory=snapshot, active=len(active), admitted=True, reason=reason,
                        bounded_limit=chosen_limit, disk_required=disk_needed,
                        minimum_free_disk_bytes=disk_floor, budget_generation=retry_counts.get(row['source'], 0),
                        estimated_active_plus_next_bytes=sum(x['bytes'] for x in active_budgets)+gate_budget['bytes'],
                        soft_budget_bytes=policy['soft_budget_bytes'], soft_budget_applies=not gate_budget['exclusive'] and not bool(policy.get('measured_rss_packing')), effective_exclusive=gate_budget['exclusive'],
                        soft_budget_is_quota=False, packing_cap_bytes=snapshot['host_total_bytes']-RESERVE if policy.get('measured_rss_packing') else None), reserved_bytes=disk_reservation):
                    stop_reason = 'disk_low_watermark'
                    break
                attempt = 1 + sum(x['source'] == row['source'] for x in attempts)
                name = f"{index:04d}-{Path(row['source']).name}-attempt{attempt}"
                temp = Path(row['object'] + '.' + out.name + f'-attempt{attempt}.pending.o')
                dep, log, timing = [out / (name + suffix) for suffix in ('.o.d', '.log', '.time.txt')]
                require(not any(p.exists() for p in (temp, dep, log, timing)), 'Attempt path already exists; no replay')
                command = list(row['command'])
                command[command.index('-o') + 1] = str(temp)
                for flag, value in (('-MF', str(dep)), ('-MT', row['object'])):
                    if flag in command:
                        command[command.index(flag) + 1] = value
                    else:
                        command.extend([flag, value])
                if '-MMD' not in command and '-MD' not in command:
                    command.append('-MMD')
                wrapped = [str(time_tool), '-v', '-o', str(timing), '--'] + command
                handle = log.open('x')
                try:
                    proc = subprocess.Popen(wrapped, cwd=row['cwd'], stdout=handle, stderr=subprocess.STDOUT,
                                            start_new_session=True, env=dict(os.environ, LC_ALL='C', CCACHE_DISABLE='1'),
                                            preexec_fn=(lambda cap=chosen_limit['as_limit_bytes']: child_address_limit(cap)) if chosen_limit else None)
                except BaseException:
                    handle.close()
                    raise
                result = dict(source=row['source'], object=row['object'], source_sha256=row['source_sha256'],
                              command=command, original_command=row['command'], time_command=wrapped, cwd=row['cwd'],
                              frozen_manifest_sha256=frozen_hash, pid=proc.pid, attempt=attempt, started_at=stamp(), log=str(log), time_log=str(timing), budget=budget,
                              resource_limit_wrapper=chosen_limit, effective_exclusive=gate_budget['exclusive'], packing_budget=gate_budget if policy.get('measured_rss_packing') else None)
                if chosen_limit:
                    result.update(minimum_observed_available_bytes=snapshot['visible_available_bytes'],
                                  minimum_observed_mem_available_bytes=snapshot['mem_available_bytes'],
                                  bounded_observed_peak_group_rss=0)
                active[proc.pid] = dict(proc=proc, handle=handle, row=row, index=index, budget=gate_budget,
                                        temp=temp, dep=dep, log=log, timing=timing, started=time.monotonic(), result=result, bounded_limit=chosen_limit,
                                        disk_budget=max(64 * 1024 ** 2, 2 * Path(row['object']).stat().st_size if Path(row['object']).exists() else 0))
                pending.pop(chosen)
                save('active-calls.json', [v['result'] for v in active.values()])
            if stop_reason and not active:
                break
            if active:
                time.sleep(0.05 if any(v.get('bounded_limit') for v in active.values()) else 0.2)
        check_frozen()
        status = stop_reason or ('failed' if any(x['exit_code'] != 0 for x in final.values()) else 'complete')
        result = dict(status=status, identity=identity, results=list(final.values()), attempts=attempts,
                      pending=[r['source'] for _, r in pending], ended_at=stamp())
        result['waiting_diagnostics'] = dict(path=str(out / 'waiting-summary.json'),
            distinct_groups=len(waiting_records), suppressed=sum(x['suppressed_observations'] for x in waiting_records.values()))
        save('result.json', result)
        return result
    finally:
        # On interruption stop only children owned here; never retry an unknown live call.
        for item in active.values():
            stop_owned(item['proc'])
            item['handle'].close()
        if active:
            save('interrupted-calls.json', dict(at=stamp(), calls=[dict(v['result'], terminal_exit_code=v['proc'].returncode, atomic_cache_update=False) for v in active.values()], note='Stopped only owned children. Inspect evidence before any new run; no automatic replay.'))
        save('waiting-summary.json', dict(scope='Aggregated wait diagnostics, not per-observation admission proof',
             groups=list(waiting_records.values())))
        if owned_lock:
            lock.close()
