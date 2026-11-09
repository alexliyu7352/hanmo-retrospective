"""中央增量编译入口：原全量选择/MF 闭包接 N1，不采用源码、不链接、不改 current。

默认只核证并列计划。只有 --execute 才调用指定 SHA 的 N1 编译器调度。
新全 queue 必须来自本批真实 CMake 命令，不能用旧命令掩盖 sections 等 flag 变化。
只有全 queue 的成功对象/依赖都核齐才写 compile-complete.json；它不代表守门通过。
失败/等待/排空保留部分进度，无已核原调用终态的中断不得盲目 --resume-from。
"""
from pathlib import Path
import argparse
import datetime
import fcntl
import hashlib
import importlib.util
import json
import os
import shlex
import sys
import time

sys.dont_write_bytecode = True


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def load(path):
    return json.loads(Path(path).read_text())


def stamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def save(path, value):
    # 本运行目录独占；原子更新本次进度，绝不写上次银行或运行回执。
    path = Path(path)
    pending = path.with_name(path.name + '.writing')
    pending.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    pending.replace(path)


def identity(path):
    path = Path(path).resolve()
    return dict(path=str(path), sha256=sha(path))


def index(rows, key):
    result = {row[key]: row for row in rows}
    require(len(result) == len(rows), 'Duplicate ' + key)
    return result


def logical_sources(rows, root):
    """只接受真实根内一一相对路径；不以 basename 或字符串替换伪造同一 TU。"""
    result = {}
    for row in rows:
        source = Path(row['source']).resolve()
        require(source.is_relative_to(root), 'Source outside declared root: ' + str(source))
        relative = source.relative_to(root).as_posix()
        require(relative not in result, 'Duplicate logical source: ' + relative)
        result[relative] = row
    return result


def verify_source_relocation(path, queue, selected, old_queue, old_selected, old_results):
    """工作树迁移仅授予全编及旧 RSS 查找，不改旧 batch/MF/对象成功身份。"""
    request = load(path)
    require(request.get('schema_version') == 1 and
            request.get('reason') == 'isolated_worktree_full_rebuild',
            'Explicit full-rebuild source relocation required')
    roots = []
    for key in ('old_source_root', 'new_source_root'):
        raw = Path(request[key])
        require(raw.is_absolute() and raw.is_dir(), 'Absolute existing source root required')
        roots.append(raw.resolve())
    old_root, new_root = roots
    require(not old_root.is_relative_to(new_root) and not new_root.is_relative_to(old_root),
            'Source roots must be distinct and non-overlapping')
    evidence = {}
    for key in ('old_baseline_reference', 'new_worktree_receipt'):
        item = request[key]
        require(Path(item['path']).is_absolute() and sha(item['path']) == item['sha256'],
                'Relocation evidence drift: ' + key)
        evidence[key] = identity(item['path'])
    receipt = load(evidence['new_worktree_receipt']['path'])
    require(Path(receipt['path']).resolve() == new_root and
            receipt.get('ordinary_independent_files_no_hardlinks') is True and
            receipt.get('commit') == request['new_baseline_commit'] and
            receipt.get('tree') == request['new_baseline_tree'],
            'New worktree is not the identified independent local checkpoint')
    baseline = load(evidence['old_baseline_reference']['path'])
    baseline_sources = index(baseline['files'], 'path')
    old_external = [row for row in old_queue if not Path(row['source']).is_relative_to(old_root)]
    external = [row for row in queue if not Path(row['source']).is_relative_to(new_root)]
    locked_flecs = None
    if external or request.get('locked_flecs_source'):
        # 生产 CMake 唯一外部 C TU：只按原依赖锁准入这一个文件，不泛放外部根。
        item = request.get('locked_flecs_source')
        require(isinstance(item, dict), 'External source requires the explicit Flecs dependency lock')
        lock_path = new_root / 'native/dependencies.lock.json'
        require(Path(item['dependency_lock']['path']).resolve() == lock_path and
                sha(lock_path) == item['dependency_lock']['sha256'],
                'Flecs dependency lock identity differs')
        library = load(lock_path)['libraries']['flecs']
        root = Path(item['root'])
        require(root.is_absolute() and root.is_dir(), 'Existing absolute locked Flecs root required')
        source = (root.resolve() / 'distr/flecs.c').resolve()
        require(Path(item['source']).resolve() == source and
                library['acquisition_state'] == 'verified' and
                sha(source) == library['files']['distr/flecs.c'] == item['sha256'],
                'Locked Flecs source bytes differ')
        require(len(external) == 1 and external[0]['source'] == str(source) and
                external[0]['source_sha256'] == item['sha256'],
                'Only the single locked Flecs C source may be outside the new root')
        locked_flecs = dict(source=str(source), sha256=item['sha256'],
                            dependency_lock=identity(lock_path))
    # 后续独立工作树的旧生产队列也可能含同一锁定 Flecs；仅此单个 C 源不作根迁移。
    # 旧实际 argv/历史不改写，必须逐字匹配本轮依赖锁及旧成功记录，绝不泛放外部根。
    if old_external:
        require(locked_flecs is not None and len(old_external) == 1 and
                old_external[0]['source'] == locked_flecs['source'] and
                old_external[0]['source_sha256'] == locked_flecs['sha256'],
                'Old external queue must contain only the same locked Flecs C source')
        old_flecs_results = [row for row in old_results if row['source'] == locked_flecs['source']]
        require(len(old_flecs_results) == 1 and
                old_flecs_results[0]['source_sha256'] == locked_flecs['sha256'],
                'Old locked Flecs successful source evidence differs')
        def external_compile_input(row):
            command = list(row['command'])
            require(command.count('-o') == 1, 'Locked Flecs output argument must be unique')
            output = command.index('-o')
            require(output + 1 < len(command), 'Locked Flecs output argument missing')
            return command[:output] + command[output + 2:]
        require(external_compile_input(old_external[0]) == external_compile_input(external[0]),
                'Locked Flecs compile flags changed during source relocation')
    old = logical_sources([row for row in old_queue if row not in old_external], old_root)
    current = logical_sources([row for row in queue if row not in external], new_root)
    prior_selected = logical_sources(old_selected, old_root)
    current_selected = logical_sources(selected, new_root)
    require(set(old) <= set(current), 'Source relocation must preserve every old logical TU')
    require(set(prior_selected) <= set(current_selected),
            'Source relocation must preserve every old logical resident selection')
    results = index(old_results, 'source')
    # 源路径迁移不是旧缓存清理授权。新输出不得覆盖旧对象或实际 MF/日志证据；
    # 若今后另获同路径滚动替换授权，应以独立精确清单接入，不能在此默许。
    protected_outputs = set()
    for row in old_results:
        protected_outputs.add(str(Path(row['object']).resolve()))
        protected_outputs.add(str(dependency_path(row).resolve()))
        for key in ('log', 'time_log'):
            if row.get(key):
                protected_outputs.add(str(Path(row[key]).resolve()))
    for row in queue:
        targets = {str(Path(row['object']).resolve()), str(Path(row['object'] + '.d').resolve())}
        require(not targets & protected_outputs,
                'Source relocation cannot overwrite an old object/MF/log: ' + row['object'])
    aliases = {}
    for relative, row in old.items():
        require(relative in baseline_sources, 'Old source lacks approved baseline: ' + relative)
        frozen = baseline_sources[relative]
        require(Path(frozen['frozen_source']).resolve() == Path(row['source']).resolve() and
                frozen['sha256'] == results[row['source']]['source_sha256'] and
                sha(frozen['frozen_source']) == frozen['sha256'],
                'Original baseline/source evidence differs: ' + relative)
        aliases[current[relative]['source']] = row['source']
    for row in queue:
        # CMake 必须以新工作树为项目输入；外部锁定工具链路径不作迁移。
        require(all(str(old_root) not in arg for arg in row['command']),
                'New compile command still names the old source tree: ' + row['source'])
    return dict(old_source_root=str(old_root), new_source_root=str(new_root),
                new_baseline_commit=request['new_baseline_commit'],
                new_baseline_tree=request['new_baseline_tree'], evidence=evidence,
                history_sources=aliases, locked_flecs_source=locked_flecs,
                cache_reuse_allowed=False)


def manifest_files(path, check_current):
    rows = load(path)['files']
    result = {}
    for row in rows:
        p = Path(row['path'])
        require(p.is_absolute(), 'Absolute frozen path required: ' + str(p))
        key = str(p.resolve())
        require(key not in result, 'Duplicate frozen path: ' + key)
        require(len(row['sha256']) == 64, 'Invalid frozen SHA: ' + key)
        if check_current:
            require(sha(p) == row['sha256'], 'Frozen input drift: ' + key)
        result[key] = row['sha256']
    return result


def dependency_path(row):
    if row.get('dependency_file'):
        return Path(row['dependency_file'])
    command = row['command']
    require(command.count('-MF') == 1, 'Original successful TU lacks MF: ' + row['source'])
    path = Path(command[command.index('-MF') + 1])
    return path if path.is_absolute() else Path(row['cwd']) / path


def dependency_inputs(path, cwd):
    # GNU make dependency output: continuation/escaped blanks are parsed as
    # tokens, not substring matches (a.hpp must not accidentally match ba.hpp).
    value = Path(path).read_text().replace('\\\n', ' ')
    first = value.splitlines()[0] if value.splitlines() else ''
    escaped = False
    colon = None
    for i, char in enumerate(first):
        if char == ':' and not escaped:
            colon = i
            break
        escaped = char == '\\' and not escaped
        if char != '\\':
            escaped = False
    require(colon is not None, 'Malformed original MF: ' + str(path))
    lexer = shlex.shlex(first[colon + 1:], posix=True)
    lexer.whitespace_split = True
    lexer.commenters = ''
    tokens = list(lexer)
    require(tokens, 'Empty original MF: ' + str(path))
    return {str((Path(token) if Path(token).is_absolute() else Path(cwd) / token).resolve())
            for token in tokens}


def normalized_queue(rows):
    out = []
    for row in rows:
        for key in ('source', 'object', 'cwd'):
            require(Path(row[key]).is_absolute(), 'Absolute queue ' + key + ' required')
        source, obj, cwd = (str(Path(row[k]).resolve()) for k in ('source', 'object', 'cwd'))
        cmd = list(row['command'])
        require(isinstance(cmd, list) and all(isinstance(x, str) for x in cmd),
                'Real argv required, not a shell string: ' + source)
        require(cmd.count('-c') == cmd.count('-o') == 1, 'Exactly one compile input/output required')
        c_input = Path(cmd[cmd.index('-c') + 1])
        c_input = c_input if c_input.is_absolute() else Path(cwd) / c_input
        require(str(c_input.resolve()) == source, 'Compile command source mismatch: ' + source)
        cmd[cmd.index('-c') + 1] = source
        require(len(row['source_sha256']) == 64 and sha(source) == row['source_sha256'],
                'Queue source drift: ' + source)
        require(Path(obj).parent.is_dir(), 'Create approved object parent before scheduling: ' + obj)
        require(Path(cwd).is_dir(), 'Missing original command cwd: ' + cwd)
        out.append(dict(source=source, object=obj, cwd=cwd, source_sha256=row['source_sha256'],
                        command=cmd, role=row.get('role', 'resident')))
    index(out, 'source')
    index(out, 'object')
    return out


def verify_cmake_commands(queue, compile_commands, command_key):
    # 只认生成的真实 CMake command/arguments；对象缓存路径可换，编译语义不可手填。
    candidates = {}
    for item in load(compile_commands):
        cwd = str(Path(item['directory']).resolve())
        source = Path(item['file'])
        source = str((source if source.is_absolute() else Path(cwd) / source).resolve())
        command = list(item['arguments']) if 'arguments' in item else shlex.split(item['command'])
        require(command.count('-c') == 1, 'Non-compile CMake row: ' + source)
        actual_source = Path(command[command.index('-c') + 1])
        actual_source = actual_source if actual_source.is_absolute() else Path(cwd) / actual_source
        require(str(actual_source.resolve()) == source, 'CMake file/argv mismatch: ' + source)
        command[command.index('-c') + 1] = source
        candidates.setdefault(source, set()).add((cwd, command_key(command)))
    for row in queue:
        require((row['cwd'], command_key(row['command'])) in candidates.get(row['source'], set()),
                'Queue does not match actual CMake compile command: ' + row['source'])


def verify_result(row, expected_source=None):
    require(row['exit_code'] == 0 and row.get('atomic_cache_update'),
            'Not a successful atomic TU: ' + row['source'])
    require(sha(row['object']) == row['object_sha256'], 'Object cache drift: ' + row['object'])
    dep = dependency_path(row)
    require(dep.is_file(), 'Missing actual original MF: ' + str(dep))
    if row.get('dependency_sha256'):
        require(sha(dep) == row['dependency_sha256'], 'MF cache drift: ' + str(dep))
    if expected_source:
        require(row['source_sha256'] == expected_source['source_sha256'], 'Terminal source identity mismatch')
        require(row['object'] == expected_source['object'], 'Terminal object selection mismatch')
    return dep


def make_plan(queue, selected, headers, old_headers, old_results, command_key,
              toolchain_matches, source_relocation=None):
    """保留原 MF 精确闭包；新增头/工具链无旧证明时保守全编，不能漏潜在 include。"""
    by_source = index(old_results, 'source')
    by_queue = index(queue, 'source')
    index(selected, 'source')
    index(selected, 'object')
    for row in selected:
        require(row['source'] in by_queue and by_queue[row['source']]['object'] == row['object'],
                'Selected link object outside actual full queue: ' + row['source'])
    changed = {p for p in headers.keys() | old_headers.keys() if headers.get(p) != old_headers.get(p)}
    added = set(headers) - set(old_headers)
    if source_relocation is not None:
        # 不把旧绝对路径改写成新成功结果；所有实际新 queue 都必须重新编译。
        reason = 'explicit_worktree_migration_full_rebuild'
        return dict(rebuild=list(queue), reused=[],
                    evidence=[dict(source=r['source'], reasons=[reason], old_mf=None,
                                   affected_headers=[]) for r in queue],
                    changed_frozen_inputs=sorted(changed), added_frozen_inputs=sorted(added),
                    global_reasons=[reason], source_relocation=source_relocation,
                    total_queue_count=len(queue), compile_count=len(queue),
                    reused_count=0, selected_count=len(selected))
    global_reasons = ([] if toolchain_matches else ['toolchain_identity_changed_or_unrecorded'])
    if added:
        # Newly appearing __has_include targets cannot be discovered from an old MF.
        global_reasons.append('new_frozen_inputs_require_full_include_closure')
    rebuild, reused, evidence = [], [], []
    for row in queue:
        old = by_source.get(row['source'])
        reasons, affected = list(global_reasons), []
        dep_proof = None
        if old is None:
            reasons.append('new_registered_translation_unit')
        else:
            require(old['exit_code'] == 0 and old.get('atomic_cache_update'),
                    'Previous full selection is not terminal success: ' + row['source'])
            if row['source_sha256'] != old['source_sha256']:
                reasons.append('source_changed')
            if row['cwd'] != old['cwd'] or command_key(row['command']) != command_key(old['command']):
                reasons.append('compiler_flags_or_cwd_changed')
            if row['object'] != old['object']:
                reasons.append('object_destination_changed')
            dep = dependency_path(old)
            if not dep.is_file():
                reasons.append('missing_original_dependency_evidence')
            elif old.get('dependency_sha256') and sha(dep) != old['dependency_sha256']:
                reasons.append('changed_original_dependency_evidence')
            else:
                dependencies = dependency_inputs(dep, old['cwd'])
                require(row['source'] in dependencies, 'Original MF omits its source: ' + str(dep))
                affected = sorted(dependencies & changed)
                if affected:
                    reasons.append('actual_mf_header_dependency_changed')
                dep_proof = dict(path=str(dep), sha256=sha(dep), dependency_count=len(dependencies))
            obj = Path(row['object'])
            if not obj.is_file() or sha(obj) != old.get('object_sha256'):
                reasons.append('missing_or_changed_object_cache')
        if reasons:
            rebuild.append(row)
        else:
            verify_result(old, row)
            reused.append(row['source'])
        evidence.append(dict(source=row['source'], reasons=reasons, old_mf=dep_proof,
                             affected_headers=affected))
    return dict(rebuild=rebuild, reused=reused, evidence=evidence,
                changed_frozen_inputs=sorted(changed), added_frozen_inputs=sorted(added),
                global_reasons=global_reasons, total_queue_count=len(queue),
                compile_count=len(rebuild), reused_count=len(reused), selected_count=len(selected))


def repair_plan(prior, queue, selected, headers, frozen_path, toolchain_hash, command_key):
    """修红只接已排空的 partial；原成功对象按真实 MF 重判，绝不伪装完整旧批。"""
    prior = Path(prior).resolve()
    batch = load(prior / 'batch-inputs.json')
    terminal = load(prior / 'compile/result.json')
    require(terminal['status'] != 'complete' and terminal.get('ended_at') and
            load(prior / 'compile/active-calls.json') == [], 'Repair requires a drained terminal batch')
    for key in ('queue', 'selected', 'frozen_manifest', 'toolchain_manifest'):
        require(sha(batch[key]) == batch[key + '_sha256'], 'Repair original metadata drift: ' + key)
    require(batch['toolchain_manifest_sha256'] == toolchain_hash, 'Repair toolchain change requires full rebuild')
    old_queue = index(load(batch['queue'])['queue'], 'source')
    require(set(old_queue) == {r['source'] for r in queue}, 'Repair cannot change the actual full queue')
    require({(r['source'], r['object']) for r in load(batch['selected'])['selected']} ==
            {(r['source'], r['object']) for r in selected}, 'Repair selected objects differ')
    for row in queue:
        old = old_queue[row['source']]
        require(row['object'] == old['object'] and row['cwd'] == old['cwd'] and
                command_key(row['command']) == command_key(old['command']), 'Repair compile semantics differ')
    old_headers = manifest_files(batch['frozen_manifest'], False)
    # 仅两代 input 目录下已核的 JSON 元数据不作为新增 C++ include 目标。
    # 实际项目/生成头仍全在原 make_plan 的新增头保守全编与 MF 改头判据中。
    input_roots = (Path(batch['frozen_manifest']).parent, Path(frozen_path).parent)
    metadata = {p for p in set(headers) | set(old_headers) if Path(p).suffix == '.json' and
                any(Path(p).parent == root for root in input_roots)}
    successful = load(prior / 'compile-partial-results.json')
    candidates = index(successful, 'source')
    require(set(candidates) <= set(old_queue), 'Repair result outside original queue')
    for row in successful:
        verify_result(row)
        require(command_key(row['command']) == command_key(old_queue[row['source']]['command']),
                'Repair successful command differs')
    plan = make_plan(queue, selected, {p: h for p, h in headers.items() if p not in metadata},
                     {p: h for p, h in old_headers.items() if p not in metadata}, successful,
                     command_key, True)
    plan.update(repair_from=str(prior), repair_evidence=[identity(prior / name) for name in
                ('batch-inputs.json', 'compile/result.json', 'compile-partial-results.json')])
    return plan, candidates


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ('queue', 'selected', 'compile-commands', 'frozen-manifest', 'toolchain-manifest', 'previous-run',
                'run', 'scheduler', 'scheduler-sha256', 'policy', 'time-tool', 'lock'):
        p.add_argument('--' + arg, required=True)
    p.add_argument('--rss-history', action='append', default=[])
    p.add_argument('--source-relocation', help='Explicit independently verified worktree migration; always full rebuild')
    p.add_argument('--resume-from')
    p.add_argument('--repair-from', help='Explicit terminal partial batch; replan changed source/headers using original MF')
    p.add_argument('--execute', action='store_true')
    args = p.parse_args()
    require(not (args.resume_from and args.repair_from), 'Choose exact resume or explicit repair, not both')
    paths = {key: Path(getattr(args, key)).resolve() for key in
             ('queue', 'selected', 'compile_commands', 'frozen_manifest', 'toolchain_manifest', 'previous_run',
              'run', 'scheduler', 'policy', 'time_tool', 'lock')}
    require(sha(paths['scheduler']) == args.scheduler_sha256, 'Scheduler source identity mismatch')
    spec = importlib.util.spec_from_file_location('actual_n1_scheduler', paths['scheduler'])
    scheduler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(scheduler)
    # Same original central lock covers planning, cache validation and N1 updates.
    with paths['lock'].open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        require(not paths['run'].exists(), 'Never overwrite an original run: ' + str(paths['run']))
        require(paths['run'].parent.is_dir(), 'Approved run parent must already exist')
        queue = normalized_queue(load(paths['queue'])['queue'])
        verify_cmake_commands(queue, paths['compile_commands'], scheduler.command_key)
        selected = load(paths['selected'])['selected']
        headers = manifest_files(paths['frozen_manifest'], True)
        toolchain = manifest_files(paths['toolchain_manifest'], True)
        require(toolchain and all(headers.get(k) == v for k, v in toolchain.items()),
                'Actual toolchain files must also be frozen for every N1 launch/finish')
        previous_batch_path = paths['previous_run'] / 'batch-inputs.json'
        old_batch = load(previous_batch_path)
        for key in ('queue', 'selected', 'frozen_manifest'):
            require(sha(old_batch[key]) == old_batch[key + '_sha256'], 'Previous batch metadata drift: ' + key)
        old_queue = load(old_batch['queue'])['queue']
        old_selected = load(old_batch['selected'])['selected']
        if not args.source_relocation:
            require(set(index(old_queue, 'source')) <= set(index(queue, 'source')),
                    'Silent old TU removal refused; this batch only supports preserved/added original sources')
            require(set(index(old_selected, 'source')) <= set(index(selected, 'source')),
                    'Silent old resident selection removal refused; explicit removal protocol required')
        old_results_path = paths['previous_run'] / 'compile-results.json'
        old_results = load(old_results_path)
        require(set(index(old_results, 'source')) == set(index(old_queue, 'source')),
                'Previous full queue/result coverage mismatch')
        require(all(r['exit_code'] == 0 and r.get('atomic_cache_update') for r in old_results),
                'Previous batch must have complete successful compile results')
        relocation = None
        if args.source_relocation:
            relocation = verify_source_relocation(Path(args.source_relocation).resolve(), queue, selected,
                                                  old_queue, old_selected, old_results)
        old_headers = manifest_files(old_batch['frozen_manifest'], False)
        provenance = {k: identity(paths[k]) for k in
                      ('queue', 'selected', 'compile_commands', 'frozen_manifest', 'toolchain_manifest', 'scheduler', 'policy', 'time_tool')}
        provenance.update(previous_batch=identity(previous_batch_path), previous_results=identity(old_results_path))
        if relocation:
            provenance['source_relocation'] = identity(args.source_relocation)
            provenance.update({'relocation_' + k: v for k, v in relocation['evidence'].items()})
        for i, path in enumerate(args.rss_history):
            provenance['rss_history_' + str(i)] = identity(path)
        old_compile = None
        if args.resume_from:
            resume = Path(args.resume_from).resolve()
            prior = load(resume / 'batch-inputs.json')
            require(prior['input_provenance'] == provenance, 'Resume input identities differ')
            require((resume / 'compile' / 'result.json').is_file(),
                    'Original scheduler has no terminal result; inspect actual call, do not replay')
            require(load(resume / 'compile' / 'result.json')['status'] != 'complete',
                    'Already complete; reuse original terminal evidence instead of replaying')
            plan = load(resume / 'dependency-closure.json')
            require(sha(resume / 'dependency-closure.json') == prior['dependency_closure_sha256'],
                    'Resume closure drift')
            old_compile = resume / 'compile'
        else:
            plan = make_plan(queue, selected, headers, old_headers, old_results, scheduler.command_key,
                             old_batch.get('toolchain_manifest_sha256') == sha(paths['toolchain_manifest']), relocation)
        old_by_source = index(old_results, 'source')
        if args.repair_from:
            require(relocation is not None, 'Repair requires the original explicit worktree provenance')
            plan, old_by_source = repair_plan(args.repair_from, queue, selected, headers,
                paths['frozen_manifest'], sha(paths['toolchain_manifest']), scheduler.command_key)
            for i, item in enumerate(plan['repair_evidence']):
                provenance['repair_' + str(i)] = item
        by_source = index(queue, 'source')
        for source in plan['reused']:
            verify_result(old_by_source[source], by_source[source])
        paths['run'].mkdir()
        save(paths['run'] / 'dependency-closure.json', plan)
        batch = dict(started_at=stamp(), input_provenance=provenance,
                     queue=str(paths['queue']), queue_sha256=sha(paths['queue']),
                     selected=str(paths['selected']), selected_sha256=sha(paths['selected']),
                     frozen_manifest=str(paths['frozen_manifest']), frozen_manifest_sha256=sha(paths['frozen_manifest']),
                     toolchain_manifest=str(paths['toolchain_manifest']), toolchain_manifest_sha256=sha(paths['toolchain_manifest']),
                     dependency_closure_sha256=sha(paths['run'] / 'dependency-closure.json'),
                     total_queue_count=len(queue), full_selected_count=len(selected),
                     compile_count=plan['compile_count'], reused_count=plan['reused_count'],
                     scheduler_policy=('N1 verified RSS packing <=MemTotal-2GiB; unknown/world_codec exclusive; max8; ordinary physical floor2GiB+256MiB' if load(paths['policy']).get('measured_rss_packing') else 'N1 actual RSS; >=2GiB/unknown exclusive; light max8; ordinary available reserve2GiB; standing rule only when explicitly frozen'),
                     standing_exclusive_memory_rule=load(paths['policy']).get('standing_exclusive_memory_rule'),
                     adopts_sources=False, links=False, promotes_current=False)
        save(paths['run'] / 'batch-inputs.json', batch)
        if not args.execute:
            save(paths['run'] / 'progress.json', dict(stage='planned_not_executed', compile_count=plan['compile_count'],
                                                     reused_count=plan['reused_count']))
            print(json.dumps(dict(status='planned_not_executed', compile_count=plan['compile_count'],
                                  reused_count=plan['reused_count'], run=str(paths['run']))))
            return 0
        history = list(old_results)
        for path in args.rss_history:
            rows = load(path)
            history.extend(rows if isinstance(rows, list) else rows['results'])
        started = time.monotonic()
        result = scheduler.compile_queue(plan['rebuild'], paths['run'] / 'compile', paths['frozen_manifest'],
                                         paths['time_tool'], load(paths['policy']), history=history,
                                         prior_run=old_compile, lock_handle=lock,
                                         history_sources={r['source']: relocation['history_sources'][r['source']]
                                             for r in plan['rebuild'] if r['source'] in relocation['history_sources']}
                                             if relocation else None)
        # Revalidate metadata/toolchain as well as all selected source/header bytes.
        for item in provenance.values():
            require(sha(item['path']) == item['sha256'], 'Batch input changed during compile: ' + item['path'])
        manifest_files(paths['frozen_manifest'], True)
        manifest_files(paths['toolchain_manifest'], True)
        for row in queue:
            require(sha(row['source']) == row['source_sha256'], 'Terminal source drift: ' + row['source'])
        completed = {source: old_by_source[source] for source in plan['reused']}
        completed.update({r['source']: r for r in result['results'] if r['exit_code'] == 0})
        save(paths['run'] / 'compile-partial-results.json', [completed[r['source']] for r in queue if r['source'] in completed])
        summary = dict(stage=result['status'], compile_wall_seconds=time.monotonic() - started,
                       successful_count=len(completed), full_queue_count=len(queue), pending=result['pending'],
                       actual_attempt_count=len(result['attempts']), baseline_seconds=9028.928972,
                       baseline_scope='0745 full batch incl link+two old gates, not directly comparable to compile-only or different TU sets')
        if result['status'] != 'complete':
            save(paths['run'] / 'progress.json', summary)
            print(json.dumps(summary))
            return 1
        require(set(completed) == set(by_source), 'Terminal full compile coverage missing')
        terminal = []
        for row in queue:
            actual = completed[row['source']]
            dep = verify_result(actual, row)
            if relocation:
                old_root = Path(relocation['old_source_root'])
                require(not any(Path(p).is_relative_to(old_root)
                                for p in dependency_inputs(dep, row['cwd'])),
                        'New object used an old worktree dependency: ' + row['source'])
            require(scheduler.command_key(actual['command']) == scheduler.command_key(row['command']),
                    'Terminal command mismatch: ' + row['source'])
            actual = dict(actual, dependency_file=str(dep), dependency_sha256=sha(dep))
            terminal.append(actual)
        save(paths['run'] / 'compile-results.json', terminal)
        # Response file exists only at terminal full success, never when blocked.
        (paths['run'] / 'selected-objects.rsp').write_text(''.join(shlex.quote(r['object']) + '\n' for r in selected))
        summary.update(stage='complete_compile_only', compile_results=identity(paths['run'] / 'compile-results.json'),
                       selected_objects=identity(paths['run'] / 'selected-objects.rsp'), tests_run=False)
        save(paths['run'] / 'compile-complete.json', summary)
        save(paths['run'] / 'progress.json', summary)
        print(json.dumps(summary))
        return 0


if __name__ == '__main__':
    sys.exit(main())
