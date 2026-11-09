"""单一任职Main节点启动器；只跑明确阶段，原生绑定、独立进程和物理银行来源先逐项验证。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time


def digest(path):
    """哈希原始字节，不接受路径等同于执行身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    """本阶段证据只写全新输出目录，原红与原银行均保留。"""
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')



def saved_release_proof(saved_case, saved_binding_path, current_binding, saved_driver_root):
    """失败生产者仍可提供真实manual2；终态、源绑定、原字节和银行必须一一可证。"""
    producer = saved_case / 'producer'
    manifest = json.loads((producer / 'save-manifest.json').read_text())
    result = json.loads((producer / 'result.json').read_text())
    process = json.loads((producer / 'process.json').read_text())
    old_binding = json.loads(saved_binding_path.read_text())
    if process.get('finished_unix', 0) <= 0 or process['finished_unix'] >= time.time():
        raise SystemExit('Original producer must already have terminated.')
    if process.get('exit_code') == 0 or result.get('ok'):
        raise SystemExit('This explicit recovery path is only for a preserved failed original producer.')
    for name, sha in process['artifacts'].items():
        if digest(producer / name) != sha:
            raise SystemExit('Original failed evidence changed: ' + name)
    if digest(saved_binding_path) != manifest['binding_sha256'] or process['binding_sha256'] != manifest['binding_sha256']:
        raise SystemExit('Original bank and producer binding disagree.')
    if old_binding['source_commit'] != manifest['source_commit'] or result['source_commit'] != manifest['source_commit']:
        raise SystemExit('Original source identity changed.')
    if manifest['case'] != 'short' or manifest.get('release', {}).get('slot') != 'manual-2':
        raise SystemExit('Expected the actual short-branch old-commitment release bank.')
    if old_binding['pack_set_digest'] != manifest['pack_set_digest'] or current_binding['pack_set_digest'] != manifest['pack_set_digest']:
        raise SystemExit('Saved release continuation must retain the original formal catalog.')
    if old_binding['native_extension']['sha256'] != current_binding['native_extension']['sha256']:
        raise SystemExit('This helper-only correction requires the unchanged actual native library.')
    if digest(producer / 'release-state.json') != manifest['release_world_sha256']:
        raise SystemExit('Original release-world evidence changed.')
    directory = Path(manifest['directory']).resolve(strict=True)
    if not directory.is_relative_to(saved_case) or directory.parts[-3:] != ('profiles', manifest['profile'], 'saves'):
        raise SystemExit('Original bank is outside its own saved profile.')
    banks = {path.name: digest(path) for path in directory.iterdir() if path.is_file()}
    if banks != manifest['banks']:
        raise SystemExit('Original physical release banks changed.')
    archived = {}
    for name, original in old_binding['drivers'].items():
        path = saved_driver_root / name
        if digest(path) != original['sha256']:
            raise SystemExit('Original driver bytes were not preserved exactly: ' + name)
        archived[name] = {'path': str(path), 'sha256': original['sha256']}
    return {'format': 'HM-NAMED-MEMBERSHIP-SAVED-RELEASE/1', 'saved_case_root': str(saved_case),
            'saved_binding_path': str(saved_binding_path), 'saved_binding_sha256': digest(saved_binding_path),
            'producer_process_sha256': digest(producer / 'process.json'), 'producer_result_sha256': digest(producer / 'result.json'),
            'producer_manifest_sha256': digest(producer / 'save-manifest.json'), 'original_source_commit': manifest['source_commit'],
            'original_exit_code': process['exit_code'], 'original_ok': result.get('ok', False),
            'original_finished_unix': process['finished_unix'], 'original_producer_pid': manifest['producer_pid'],
            'profile': manifest['profile'], 'directory': str(directory), 'banks': banks,
            'release_world_sha256': manifest['release_world_sha256'], 'pack_set_digest': manifest['pack_set_digest'],
            'xdg_data_home': process['xdg']['XDG_DATA_HOME'], 'preserved_drivers': archived}


def main():
    """生产和两个冷恢复阶段使用同一正式库；没有新绑定就拒绝启动，不自动重做前缀。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binding', type=Path, required=True)
    parser.add_argument('--case-root', type=Path, required=True)
    parser.add_argument('--case', choices=['funded', 'short', 'deferred', 'recipient_accept', 'recipient_decline'], required=True)
    parser.add_argument('--phase', choices=['produce', 'consume-pending', 'consume-release', 'continue-release-reference', 'continue-release-compare'], required=True)
    parser.add_argument('--saved-case-root', type=Path)
    parser.add_argument('--saved-binding', type=Path)
    parser.add_argument('--saved-driver-root', type=Path)
    parser.add_argument('--rendered', action='store_true')
    parser.add_argument('--timeout-seconds', type=int, default=600)
    args = parser.parse_args()
    continuing = args.phase in {'continue-release-reference', 'continue-release-compare'}
    if continuing != all(value is not None for value in [args.saved_case_root, args.saved_binding, args.saved_driver_root]):
        raise SystemExit('Saved-release phases require explicit original case, original binding and preserved driver directory.')
    if not continuing and any(value is not None for value in [args.saved_case_root, args.saved_binding, args.saved_driver_root]):
        raise SystemExit('Saved-source arguments require a saved-release continuation phase.')
    if continuing and args.case != 'short':
        raise SystemExit('Only actual short-branch release continuation is supported.')
    binding_path = args.binding.resolve(strict=True)
    binding = json.loads(binding_path.read_text())
    if (binding.get('native_artifact_status') != 'BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED'
            or len(binding.get('source_commit', '')) != 40
            or binding.get('source_commit') != binding.get('reviewed_source_commit')):
        raise SystemExit('Completed actual native build and exact reviewed source binding required.')
    if args.phase == 'consume-release' and args.case != 'short':
        raise SystemExit('Only the authored short branch has an old-commitment release bank.')
    records = binding['inputs'] + list(binding['drivers'].values())
    records += [binding[name] for name in ['native_extension', 'native_build_receipt', 'registration', 'descriptor']]
    records += binding.get('imported_resources', [])
    for record in records:
        path = Path(record['path'])
        if path.is_symlink() or digest(path) != record['sha256']:
            raise SystemExit('Actual bound input changed: ' + str(path))
    if digest(Path(binding['engine'])) != binding['engine_sha256']:
        raise SystemExit('Pinned Godot executable changed.')
    case = args.case_root.resolve()
    proof = {}
    saved_case = saved_binding = saved_drivers = None
    if continuing:
        saved_case = args.saved_case_root.resolve(strict=True)
        saved_binding = args.saved_binding.resolve(strict=True)
        saved_drivers = args.saved_driver_root.resolve(strict=True)
        if case == saved_case or case.is_relative_to(saved_case):
            raise SystemExit('Continuation output must be outside the preserved original case.')
        proof = saved_release_proof(saved_case, saved_binding, binding, saved_drivers)
    case.mkdir(parents=True, exist_ok=args.phase not in ['produce', 'continue-release-reference'])
    output_name = 'producer' if args.phase == 'produce' else 'release-reference' if args.phase == 'continue-release-reference' else 'release-comparison' if args.phase == 'continue-release-compare' else args.phase
    output = case / output_name
    output.mkdir(exist_ok=False)
    if args.phase == 'continue-release-compare':
        previous = case / 'release-reference'
        result = json.loads((previous / 'result.json').read_text())
        process = json.loads((previous / 'process.json').read_text())
        if not result.get('ok') or process['exit_code'] != 0 or process.get('finished_unix', 0) <= 0:
            raise SystemExit('A successful terminated saved-release reference is required.')
        if process['binding_sha256'] != digest(binding_path) or json.loads((previous / 'saved-source.json').read_text()) != proof:
            raise SystemExit('Comparison must use the identical new binding and original physical bank proof.')
        for name, value in process['artifacts'].items():
            if digest(previous / name) != value:
                raise SystemExit('Reference evidence changed: ' + name)
    if not continuing and args.phase != 'produce':
        previous = case / 'producer'
        result = json.loads((previous / 'result.json').read_text())
        process = json.loads((previous / 'process.json').read_text())
        manifest = json.loads((previous / 'save-manifest.json').read_text())
        if not result.get('ok') or process['exit_code'] != 0 or process.get('finished_unix', 0) <= 0:
            raise SystemExit('The original producer must have terminated successfully; preserve failed runs separately.')
        if process['binding_sha256'] != digest(binding_path) or manifest['case'] != args.case:
            raise SystemExit('Producer/consumer binding or case differs.')
        for name, value in process['artifacts'].items():
            if digest(previous / name) != value:
                raise SystemExit('Original producer evidence changed: ' + name)
        directory = Path(manifest['directory']).resolve(strict=True)
        if not directory.is_relative_to(case) or {p.name: digest(p) for p in directory.iterdir() if p.is_file()} != manifest['banks']:
            raise SystemExit('The original physical banks are not intact in this owned case.')
    env = dict(os.environ)
    xdg = {}
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        path = Path(proof['xdg_data_home']) if continuing and key == 'XDG_DATA_HOME' else case / 'xdg' / leaf
        path.mkdir(parents=True, exist_ok=True)
        xdg[key] = str(path)
        env[key] = str(path)
    command = [binding['engine']]
    if not args.rendered:
        command.append('--headless')
    command += ['--audio-driver', 'Dummy', '--path', binding['stage_game'], '--script',
                binding['drivers']['named_membership_main.gd']['path'], '--', '--binding=' + str(binding_path),
                '--case-root=' + str(case), '--case=' + args.case, '--phase=' + args.phase]
    if continuing:
        command += ['--saved-case-root=' + str(saved_case), '--saved-binding=' + str(saved_binding)]
        write(output / 'saved-source.json', proof)
    started = time.time()
    write(output / 'launch.json', {'command': command, 'started_unix': started, 'xdg': xdg})
    with (output / 'run.log').open('w') as log:
        try:
            code = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout_seconds).returncode
        except subprocess.TimeoutExpired:
            code = 124
    if code == 0 and ('SCRIPT ERROR:' in (output / 'run.log').read_text()
                      or not (output / 'result.json').is_file()
                      or not json.loads((output / 'result.json').read_text()).get('ok')):
        code = 1
    changed = [row['path'] for row in records if digest(Path(row['path'])) != row['sha256']]
    if changed:
        code = 2
    preserved = True
    if continuing:
        try:
            preserved = saved_release_proof(saved_case, saved_binding, binding, saved_drivers) == proof
        except (SystemExit, ValueError, KeyError, FileNotFoundError):
            preserved = False
        if not preserved:
            code = 2
    write(output / 'process.json', {'exit_code': code, 'phase': args.phase, 'case': args.case,
                                   'source_commit': binding['source_commit'], 'binding_sha256': digest(binding_path),
                                   'started_unix': started, 'finished_unix': time.time(), 'command': command,
                                   'xdg': xdg, 'changed_inputs': changed, 'original_saved_source_preserved': preserved,
                                   'saved_source': proof, 'scope': 'Only original saved-release continuation; original producer remains failed.' if continuing else 'Original actual Main route and independent continuation.',
                                   'artifacts': {path.name: digest(path) for path in output.iterdir() if path.is_file()}})
    print(json.dumps({'phase': args.phase, 'case': args.case, 'exit_code': code, 'output': str(output)}))
    print((output / 'run.log').read_text()[-3000:])
    return code


if __name__ == '__main__':
    raise SystemExit(main())
