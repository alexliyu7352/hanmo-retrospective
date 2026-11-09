"""运行独占标准Alpha的单次headless流程；先绑定实际源、正式安装代和Godot字节。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time


def digest(path):
    """按文件原字节绑定来源与结果，缺失输入不能用旧报告替代。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """每次只写全新case目录和私有XDG；原红与修后结果分别标注而不覆盖失败证据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binding', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', choices=['letter-first', 'accepted-letter', 'escort-first'], required=True)
    parser.add_argument('--expect-blocked', action='store_true')
    parser.add_argument('--timeout', type=int, default=300)
    args = parser.parse_args()
    binding = json.loads(args.binding.read_text())
    if binding['format'] != 'HM-ALPHA-LETTER-ESCORT-STANDARD/1' or binding['profile'] != 'standard-alpha-legacy':
        raise ValueError('Exact standard Alpha profile required.')
    game = Path(binding['stage_game'])
    root = game.parent
    for row in binding['inputs']:
        if digest(root / row['path']) != row['sha256']:
            raise ValueError('Bound source changed: ' + row['path'])
    if digest(Path(binding['engine'])) != binding['engine_sha256']:
        raise ValueError('Pinned engine changed.')
    for row in binding['content_stage']['files']:
        if digest(game / 'data/generated' / row['path']) != row['sha256']:
            raise ValueError('Standard content generation changed: ' + row['path'])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        directory = output / 'xdg' / leaf
        directory.mkdir(parents=True)
        env[key] = str(directory)
    driver = Path(__file__).with_name('alpha_letter_escort_main.gd').resolve(strict=True)
    command = [binding['engine'], '--headless', '--audio-driver', 'Dummy', '--path', str(game),
               '--script', str(driver), '--', '--output=' + str(output), '--case=' + args.case]
    if args.expect_blocked:
        command.append('--expect-blocked')
    started = time.time()
    launch = {'command': command, 'source_commit': binding['source_commit'],
              'binding_sha256': digest(args.binding), 'driver_sha256': digest(driver),
              'started_unix': started, 'evidence': 'HEADLESS_PRODUCTION_CONTROLS_NOT_GUI'}
    (output / 'launch.json').write_text(json.dumps(launch, ensure_ascii=False, indent=2) + '\n')
    try:
        reply = subprocess.run(command, env=env, capture_output=True, text=True, timeout=args.timeout)
        code, stdout, stderr = reply.returncode, reply.stdout, reply.stderr
    except subprocess.TimeoutExpired as exc:
        code = 124
        stdout = exc.stdout.decode() if isinstance(exc.stdout, bytes) else exc.stdout or ''
        stderr = exc.stderr.decode() if isinstance(exc.stderr, bytes) else exc.stderr or ''
    (output / 'stdout.log').write_text(stdout)
    (output / 'stderr.log').write_text(stderr)
    result = json.loads((output / 'result.json').read_text()) if (output / 'result.json').is_file() else {}
    passed = code == 0 and result.get('ok') is True and not re.search(r'(^|\n)(SCRIPT ERROR:|ERROR:)', stdout + stderr)
    receipt = dict(launch, finished_unix=time.time(), exit_code=code, ok=passed,
                   artifacts={path.name: digest(path) for path in output.iterdir() if path.is_file()})
    (output / 'process.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'ok': passed, 'output': str(output), 'exit_code': code}))
    raise SystemExit(0 if passed else code or 1)


if __name__ == '__main__':
    main()
