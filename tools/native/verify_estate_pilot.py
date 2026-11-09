#!/usr/bin/env python3
"""estate 试点原两门：新冷读比完整状态摘要，原六份 bank 输出仍逐字节硬核。

仅在本批完整编译/链接终态后执行。输入沿原 0745 两样本，不造 Field 正例，
不提升 current，不改旧 bank/成功日志。所有新进程与输出写入本次独立目录。
"""
import argparse
import datetime
import fcntl
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

BASE_SHA = '2356d0e3e1ba01806d15318df7bf182314c43a97840d5849f6550381bed011f9'
SPECS = {
    'market': ('complete-market-loader/growth-explicit-author-20261025T2234Z/complete.checked.json',
               'f56813be90b990845ae6786eb77339493374bab1c9259c2089917f2069ca1a33',
               'complete_market::scenario.complete_market', (0, 0), (2, 6), (3, 8)),
    'xiaohan': ('stages/n2-xiaohan-20261026T1902Z/xiaohan.checked.json',
                '28e04ee086400193573cb9b2a0b3c4c3ffa287d963074ca7f22b32e2e55a16c0',
                'xiaohan_dynamic::scenario.chuping_xiaohan_dynamic', (0, 0), (2, 2), (3, 3))}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def load(path):
    return json.loads(Path(path).read_text())


def raw(path):
    return dict(path=str(path), sha256=sha(path), bytes=Path(path).stat().st_size)


def stamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def write(path, value):
    pending = path.with_name(path.name + '.writing')
    pending.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    pending.replace(path)


def state_digest(path):
    """完整 JSON 状态规范化摘要：不删 ID/时刻/字段，不排序数组，不吞重复键。"""
    def object_value(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate state key: ' + key)
            result[key] = value
        return result
    def invalid_constant(value):
        raise ValueError('Non-finite state number: ' + value)
    state = json.loads(Path(path).read_text(), object_pairs_hook=object_value,
                       parse_constant=invalid_constant)
    encoded = json.dumps(state, sort_keys=True, ensure_ascii=False,
                         separators=(',', ':'), allow_nan=False).encode('utf-8')
    return hashlib.sha256(encoded).hexdigest()


def verify_build(run, link_path):
    """只消费本次完整终态；没有旧固定 TU 数、也不把 partial/blocked 当链接许可。"""
    complete_path = run / 'compile-complete.json'
    complete = load(complete_path)
    require(complete['stage'] == 'complete_compile_only', 'Full compile terminal required')
    batch = load(run / 'batch-inputs.json')
    for key in ('queue', 'selected', 'frozen_manifest', 'toolchain_manifest'):
        require(sha(batch[key]) == batch[key + '_sha256'], 'Batch input drift: ' + key)
    queue = load(batch['queue'])['queue']
    selected = load(batch['selected'])['selected']
    result_ref = complete['compile_results']
    require(sha(result_ref['path']) == result_ref['sha256'], 'Compile result drift')
    results = load(result_ref['path'])
    by_source = {row['source']: row for row in queue}
    require(len(by_source) == len(queue) == len(results) == complete['full_queue_count'],
            'Actual full queue/result count mismatch')
    require(len({r['source'] for r in results}) == len(results) and
            {r['source'] for r in results} == set(by_source), 'Full source coverage missing')
    for row in results:
        original = by_source[row['source']]
        require(row['exit_code'] == 0 and row.get('atomic_cache_update') is True and
                row['object'] == original['object'] and row['source_sha256'] == original['source_sha256'] and
                sha(row['source']) == row['source_sha256'], 'Unverified terminal source/object pair')
    for item in load(batch['frozen_manifest'])['files']:
        require(sha(item['path']) == item['sha256'], 'Frozen source/toolchain drift')
    require(len({r['source'] for r in selected}) == len(selected) and
            all(r['source'] in by_source and r['object'] == by_source[r['source']]['object']
                for r in selected), 'Selected link objects differ from full queue')
    response = complete['selected_objects']
    require(sha(response['path']) == response['sha256'], 'Selected response drift')
    link = load(link_path)
    require(link['exit_code'] == 0 and link['compile_complete_sha256'] == sha(complete_path) and
            link['selected_objects_sha256'] == response['sha256'], 'Link belongs to another compile batch')
    executable = Path(link['output'])
    require(sha(executable) == link['executable_sha256'], 'Linked executable changed')
    return executable, link['executable_sha256'], len(queue)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True, type=Path, help='Original hanmo-work root')
    parser.add_argument('--run', required=True, type=Path)
    parser.add_argument('--link-result', required=True, type=Path)
    args = parser.parse_args()
    workspace, run = args.workspace.resolve(), args.run.resolve()
    lock_path = workspace / 'complete-market-loader/shared-header-rebuild-20261025T2248Z/queue.lock'
    with lock_path.open('rb') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        executable, executable_sha, count = verify_build(run, args.link_result.resolve())
        baseline_path = workspace / 'complete-market-loader/h2-model-split-20261027T1231Z/baseline.json'
        require(sha(baseline_path) == BASE_SHA, 'Original six-bank manifest drift')
        baseline = load(baseline_path)
        for sample in baseline['samples'].values():
            for item in sample.values():
                require(sha(item['path']) == item['sha256'], 'Original bank changed')
        for spec in SPECS.values():
            require(sha(workspace / spec[0]) == spec[1], 'Original checked input changed')
        out = run / 'estate-pilot-acceptance'
        out.mkdir(exist_ok=False)
        evidence = dict(status='IN_PROGRESS', started_at=stamp(), executable=raw(executable),
                        actual_compile_count=count, baseline_manifest_sha256=BASE_SHA,
                        fresh={}, same_checkpoint={}, processes=[],
                        limits=['Original two catalogs do not select Field; no Field branch accepted.',
                                'State digest keeps all values; only JSON object order/format is ignored.',
                                'Six old bank outputs remain a separate byte-exact pilot requirement.'])
        report = out / 'verification.json'
        write(report, evidence)

        def process(mode, sample, destination):
            require(sha(executable) == executable_sha, 'Executable changed before gate')
            spec = SPECS[sample]
            command = [str(executable), mode, str(workspace / spec[0]), spec[2], str(destination)]
            log = out / (sample + '-' + destination.name + '-' + mode + '.log')
            started, clock = stamp(), time.monotonic()
            with log.open('wb') as stream:
                result = subprocess.run(command, cwd=workspace, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=600)
            evidence['processes'].append(dict(command=command, cwd=str(workspace),
                started_at=started, ended_at=stamp(), seconds=time.monotonic()-clock,
                exit_code=result.returncode, log=raw(log), executable_sha256=executable_sha))
            write(report, evidence)
            require(result.returncode == 0, 'Original gate failed: ' + sample + '/' + mode)
            require(sha(executable) == executable_sha, 'Executable changed during gate')

        for sample, spec in SPECS.items():
            destination = out / (sample + '-fresh')
            destination.mkdir()
            process('new', sample, destination)
            process('binary', sample, destination)
            day, restored = state_digest(destination / 'day.json'), state_digest(destination / 'restored.binary.json')
            require(day == restored, 'Fresh state digest mismatch: ' + sample)
            names = ['initial.json', 'day.json', 'restored.binary.json', 'continued.binary.json']
            moments = {}
            for name in names:
                state = load(destination / name)
                moments[name] = [state['tick'], state['revision']]
                require(state['scenario_id'] == spec[2], 'Wrong actual scenario')
            require([moments[n] for n in names] == [list(spec[3]), list(spec[4]), list(spec[4]), list(spec[5])],
                    'Original clock/revision changed')
            old_identity = workspace / 'complete-market-loader/n2-checkpoint-order-run-20261027T1144Z' / sample / 'actual.catalog-identity.json'
            require(state_digest(destination / 'actual.catalog-identity.json') == state_digest(old_identity),
                    'Actual catalog identity changed')
            fresh = dict(moments=moments, day_state_sha256=day, restored_state_sha256=restored,
                         state_digest_equal=True, files={n: raw(destination / n) for n in names +
                         ['day.checkpoint', 'day.receipts.json', 'continued.binary.receipts.json', 'actual.catalog-identity.json']})
            if sample == 'xiaohan':
                before = load(destination / 'new.role-dialogue-projection.json')
                after = load(destination / 'binary.role-dialogue-projection.json')
                require(before == after and before['ok'] is True and before['derived'] > 0,
                        'Actual role dialogue projection mismatch')
                fresh['role_dialogue_projection'] = dict(derived=before['derived'], same_after_restore=True,
                    new=raw(destination / 'new.role-dialogue-projection.json'),
                    cold=raw(destination / 'binary.role-dialogue-projection.json'))
            evidence['fresh'][sample] = fresh
            write(report, evidence)

        for sample in SPECS:
            destination = out / (sample + '-same-checkpoint')
            destination.mkdir()
            original = baseline['samples'][sample]
            shutil.copyfile(original['day.checkpoint']['path'], destination / 'day.checkpoint')
            require(sha(destination / 'day.checkpoint') == original['day.checkpoint']['sha256'],
                    'Old checkpoint copy differs')
            process('binary', sample, destination)
            comparisons = {}
            for name in ('restored.binary.json', 'continued.binary.json', 'continued.binary.receipts.json'):
                require((destination / name).read_bytes() == Path(original[name]['path']).read_bytes(),
                        'Old six-bank output changed: ' + sample + '/' + name)
                comparisons[name] = dict(before=original[name], after=raw(destination / name), bytes_equal=True)
            evidence['same_checkpoint'][sample] = dict(checkpoint=raw(destination / 'day.checkpoint'), comparisons=comparisons)
            write(report, evidence)
        require(sha(executable) == executable_sha, 'Final executable identity drift')
        evidence.update(status='PASS_TWO_ORIGINAL_GATES_STATE_DIGEST_AND_SIX_BANK_BYTES', finished_at=stamp())
        write(report, evidence)
        print(str(report), sha(report))


if __name__ == '__main__':
    main()
