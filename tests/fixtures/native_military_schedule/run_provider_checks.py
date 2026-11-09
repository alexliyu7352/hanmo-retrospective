"""对独立军政测试工程执行真实collector/缺库/native门，固定旧oracle且保存完整进程回执。"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
VERSION = '4.7.2.stable.official.ed1daf0bf'


def digest(path: Path) -> str:
    """核验本轮真实库、oracle及日志字节，路径名不代替身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], output: Path, label: str, environment: dict[str, str]) -> dict:
    """保留退出码、超时和完整输出；exit0但脚本报错或无汇总仍失败。"""
    started = time.monotonic()
    try:
        process = subprocess.run(command, env=environment, text=True, capture_output=True, timeout=180)
        text = process.stdout + process.stderr
        code = process.returncode
    except subprocess.TimeoutExpired as error:
        text = ''.join(value.decode() if isinstance(value, bytes) else (value or '') for value in (error.stdout, error.stderr))
        text += '\nPROBE_TIMEOUT: 180 seconds\n'
        code = 124
    log = output / (label + '.log')
    log.write_text(text)
    (output / (label + '.exit')).write_text(str(code) + '\n')
    (output / (label + '.command.json')).write_text(json.dumps(command, indent=2) + '\n')
    ok = code == 0 and 'FAILURES=0' in text and not any(marker in text for marker in ('SCRIPT ERROR:', 'ERROR:', 'FAIL:', 'ObjectDB instances were leaked'))
    return {'case': label, 'ok': ok, 'exit_code': code, 'elapsed_seconds': round(time.monotonic() - started, 3),
            'log': str(log), 'log_sha256': digest(log),
            'summary': [line for line in text.splitlines() if line.startswith('NATIVE_MILITARY_')]}


def stage_library(game: Path, library: Path, expected_hash: str) -> Path:
    """只在本门既有隔离工程建立内容寻址库路径，原so及旧扩展永不覆盖。"""
    actual = digest(library)
    if actual != expected_hash:
        raise ValueError('Frozen native library SHA-256 mismatch')
    folder = game / 'native_military_test' / actual
    folder.mkdir(parents=True, exist_ok=True)
    target = folder / 'libhanmo_world_bridge.so'
    if target.exists():
        if digest(target) != actual:
            raise ValueError('Existing staged native library identity mismatch')
    else:
        shutil.copy2(library, target)
    extension = folder / 'hanmo_world.gdextension'
    body = '[configuration]\nentry_symbol="hanmo_world_library_init"\ncompatibility_minimum="4.7.2"\nreloadable=false\n[libraries]\nlinux.debug.x86_64="res://' + str(target.relative_to(game)) + '"\n'
    if extension.exists() and extension.read_text() != body:
        raise ValueError('Existing extension configuration mismatch')
    if not extension.exists():
        extension.write_text(body)
    return extension


def main() -> int:
    """明确指定准备结果和模式；没有真实so时只允许collector或独立缺库证据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepared', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('collect', 'missing-library', 'native'), required=True)
    parser.add_argument('--group', choices=('oracle', 'atomic', 'all'), default='all')
    parser.add_argument('--build', type=Path)
    parser.add_argument('--library', type=Path)
    parser.add_argument('--library-sha256')
    args = parser.parse_args()
    prepared = args.prepared.resolve(strict=True)
    manifest = json.loads((prepared / 'preparation.json').read_text())
    game = Path(manifest['game']).resolve(strict=True)
    if game != prepared / 'game' or 'native_military_schedule_isolated=true' not in (game / 'project.godot').read_text():
        parser.error('Require this military oracle preparation output')
    output = args.output.resolve()
    if output.exists():
        parser.error('Evidence output must be new')
    engine = str(args.engine.resolve(strict=True))
    if subprocess.check_output([engine, '--version'], text=True).strip() != VERSION:
        parser.error('Require the pinned Godot version')
    if args.mode != 'collect' and (args.build is None or not args.build.is_file()):
        parser.error('Require the independently frozen expected build')
    if args.mode == 'missing-library' and any(game.rglob('*.gdextension')):
        parser.error('Missing-library evidence requires a fresh extension-free prepared project')
    if args.mode == 'native' and (args.library is None or not args.library_sha256):
        parser.error('Native evidence requires the real library and its independently supplied SHA-256')
    output.mkdir(parents=True)
    environment = dict(os.environ)
    for variable, name in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        target = output / name
        target.mkdir()
        environment[variable] = str(target)
    fixture = str(Path(manifest['dynamic_content']).resolve(strict=True))
    oracles = ROOT / 'tests/fixtures/native_military_schedule'
    paths = [oracles / 'old_oracle_a4fa824.json', oracles / 'old_return_oracle_a4fa824.json']
    before = {path.name: digest(path) for path in paths}
    common = ['--collect-only'] if args.mode == 'collect' else ['--build=' + str(args.build.resolve())]
    if args.mode == 'missing-library':
        common.append('--missing-library')
    if args.mode == 'native':
        extension = stage_library(game, args.library.resolve(strict=True), args.library_sha256)
        common += ['--native', '--extension=res://' + str(extension.relative_to(game))]
    jobs = []
    if args.group in ('oracle', 'all'):
        jobs += [('main', 'test_native_military_schedule.gd', ['--dynamic-content=' + fixture, '--oracle=' + str(paths[0]), '--native-evidence=' + str(output / 'main.json')]),
                 ('return', 'capture_return.gd', ['--oracle=' + str(paths[1]), '--native-evidence=' + str(output / 'return.json')])]
    if args.group in ('atomic', 'all') and args.mode != 'collect':
        jobs.append(('atomicity', 'check_atomicity.gd', ['--dynamic-content=' + fixture, '--oracle=' + str(paths[0]), '--evidence=' + str(output / 'atomicity.json')]))
    if not jobs:
        parser.error('Collector mode has no atomicity claim; choose oracle/all')
    results = [run([engine, '--headless', '--audio-driver', 'Dummy', '--path', str(game), '--script', 'res://tests/' + script, '--', *arguments, *common], output, label, environment) for label, script, arguments in jobs]
    unchanged = before == {path.name: digest(path) for path in paths}
    report = {'format': 'HM-MILITARY-PROVIDER-RUNNER/1', 'mode': args.mode, 'native_library_sha256': args.library_sha256 if args.mode == 'native' else None,
              'prepared_manifest_sha256': digest(prepared / 'preparation.json'), 'engine_version': VERSION,
              'expected_build_sha256': digest(args.build) if args.build else None,
              'old_oracle_sha256': before, 'old_oracle_unchanged': unchanged, 'results': results,
              'status': 'PASS' if unchanged and all(result['ok'] for result in results) else 'FAIL'}
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'mode': args.mode, 'report': str(output / 'report.json')}, ensure_ascii=False))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
