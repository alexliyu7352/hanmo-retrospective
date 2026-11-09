"""三类有限继续的单一Main节点启动器；绑定原始物理银行和中央受审库后才允许运行。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time


def digest(path):
    """只认实际字节摘要，不以文件名或旧身份代替新库。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    """输出仅写本阶段新目录，保全失败证据和原物理银行。"""
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def main():
    """明确一类行动的生产、自动恢复或手动对照阶段；消费者必须晚于真实生产者退出。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binding', type=Path, required=True)
    parser.add_argument('--case-root', type=Path, required=True)
    parser.add_argument('--phase', choices=['produce', 'consume-auto', 'consume-manual'], required=True)
    parser.add_argument('--case', choices=['travel', 'rest', 'study'], required=True)
    parser.add_argument('--rendered', action='store_true')
    parser.add_argument('--timeout-seconds', type=int, default=300)
    args = parser.parse_args()
    binding_path = args.binding.resolve(strict=True)
    binding = json.loads(binding_path.read_text())
    if (binding.get('native_artifact_status') != 'BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED'
            or len(binding.get('source_commit', '')) != 40
            or binding.get('source_commit') != binding.get('reviewed_source_commit')):
        raise SystemExit('An actual library bound to exact reviewed source is required.')
    receipt = json.loads(Path(binding['native_build_receipt']['path']).read_text())
    if len(binding.get('native_source_commit', '')) != 40 or receipt.get('source_commit') != binding['native_source_commit'] or receipt.get('build_exit') != 0:
        raise SystemExit('The native library must retain its exact successful build-source identity.')
    records = binding['inputs'] + list(binding['drivers'].values())
    records += [binding[name] for name in ['native_extension', 'native_build_receipt', 'registration', 'descriptor']]
    records += binding.get('imported_resources', [])
    for item in records:
        path = Path(item['path'])
        if path.is_symlink() or not path.is_file() or digest(path) != item['sha256']:
            raise SystemExit('Bound input changed: ' + str(path))
    if digest(Path(binding['engine'])) != binding['engine_sha256']:
        raise SystemExit('Pinned Godot executable changed.')
    case = args.case_root.resolve()
    if args.phase == 'produce':
        case.mkdir(parents=True, exist_ok=False)
    else:
        previous = case / 'producer'
        if args.phase == 'consume-manual':
            automatic = json.loads((case / 'consume-auto/process.json').read_text())
            if automatic.get('finished_unix', 0) <= 0 or automatic['finished_unix'] >= time.time():
                raise SystemExit('The automatic consumer must already have terminated; its failure does not skip the independent manual path.')
        result = json.loads((previous / 'result.json').read_text())
        process = json.loads((previous / 'process.json').read_text())
        manifest = json.loads((previous / 'save-manifest.json').read_text())
        if not result.get('ok') or process.get('ok') is not True or process.get('exit_code') != 0 or process.get('finished_unix', 0) <= 0 or process['finished_unix'] >= time.time():
            raise SystemExit('Original producer must already have terminated successfully.')
        if manifest['case'] != args.case:
            raise SystemExit('The original physical bank belongs to another action case.')
        if process['binding_sha256'] != digest(binding_path) or manifest['binding_sha256'] != digest(binding_path):
            raise SystemExit('Producer and consumer bindings differ.')
        for name, value in process['artifacts'].items():
            if digest(previous / name) != value:
                raise SystemExit('Original producer evidence changed: ' + name)
        directory = Path(manifest['directory']).resolve(strict=True)
        if not directory.is_relative_to(case) or directory.parts[-3:] != ('profiles', manifest['profile'], 'saves'):
            raise SystemExit('Physical bank is outside its owned profile.')
        if {path.name: digest(path) for path in directory.iterdir() if path.is_file()} != manifest['banks']:
            raise SystemExit('Original physical banks changed.')
    output = case / ('producer' if args.phase == 'produce' else args.phase)
    output.mkdir(exist_ok=False)
    env = dict(os.environ)
    xdg = {}
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        path = case / 'xdg' / leaf
        path.mkdir(parents=True, exist_ok=True)
        xdg[key] = env[key] = str(path)
    command = [binding['engine']]
    if not args.rendered:
        command.append('--headless')
    command += ['--audio-driver', 'Dummy', '--path', binding['stage_game'], '--script',
                binding['drivers']['activity_continuation_main.gd']['path'], '--',
                '--native-resident', '--binding=' + str(binding_path), '--case-root=' + str(case), '--phase=' + args.phase, '--case=' + args.case]
    started = time.time()
    write(output / 'launch.json', {'started_unix': started, 'phase': args.phase, 'command': command,
                                   'binding_sha256': digest(binding_path), 'xdg': xdg})
    timeout = False
    try:
        reply = subprocess.run(command, env=env, capture_output=True, text=True, timeout=args.timeout_seconds)
        code, stdout, stderr = reply.returncode, reply.stdout, reply.stderr
    except subprocess.TimeoutExpired as error:
        timeout = True
        code = 124
        stdout = error.stdout.decode() if isinstance(error.stdout, bytes) else error.stdout or ''
        stderr = error.stderr.decode() if isinstance(error.stderr, bytes) else error.stderr or ''
    (output / 'stdout.log').write_text(stdout)
    (output / 'stderr.log').write_text(stderr)
    result_path = output / 'result.json'
    result = json.loads(result_path.read_text()) if result_path.is_file() else {}
    passed = code == 0 and result.get('ok') is True and 'SCRIPT ERROR' not in stdout + stderr and 'Parse Error' not in stdout + stderr
    write(output / 'process.json', {'exit_code': code, 'ok': passed, 'timed_out': timeout,
                                   'started_unix': started, 'finished_unix': time.time(),
                                   'binding_sha256': digest(binding_path), 'xdg': xdg,
                                   'artifacts': {path.name: digest(path) for path in output.iterdir() if path.is_file()}})
    print(json.dumps({'phase': args.phase, 'ok': passed, 'output': str(output)}))
    raise SystemExit(0 if passed else code or 1)


if __name__ == '__main__':
    main()
