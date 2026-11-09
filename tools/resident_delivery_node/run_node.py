"""在冻结的实际Linux debug包上运行一次正常入口/存读节点；不修改包或旧银行。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


def digest(path):
    """逐块摘要原件，避免将大型嵌入PCK可执行文件整体读入内存。"""
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def inventory(root):
    """递归记录真实文件摘要，拒绝链接，原银行和包清单均按相对路径核对。"""
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'Symlink is outside this bounded node: {path}')
        if path.is_file():
            result[str(path.relative_to(root))] = digest(path)
    return result


def write(path, value):
    """JSON证据写入新目录；不覆盖上次节点结果。"""
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def main():
    """先核冻结身份再顺序启动两个新进程，最终独立核银行、包和真实退出码。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-executable-sha256', required=True)
    parser.add_argument('--expected-native-sha256', required=True)
    parser.add_argument('--editor-runner', type=Path)
    parser.add_argument('--expected-editor-sha256')
    args = parser.parse_args()
    package = args.package.resolve(strict=True)
    output = args.output.resolve()
    if package == output or package in output.parents or output in package.parents:
        raise SystemExit('Evidence/state must be separate from the immutable package.')
    build_path = package / 'BUILD.json'
    build = json.loads(build_path.read_text())
    before = inventory(package)
    expected = dict(build['files'])
    expected['BUILD.json'] = digest(build_path)
    if before != expected or before.get('hanmo.x86_64') != args.expected_executable_sha256 or before.get('libhanmo_world_bridge.so') != args.expected_native_sha256:
        raise SystemExit('The frozen complete package inventory does not match.')
    if build['format'] != 'HM-NATIVE-RESIDENT-LINUX-BUILD/1' or build['export_mode'] != 'debug' or build['packed_content']['status'] != 'PASS':
        raise SystemExit('Expected an actual inspected Linux debug export.')
    driver = Path(__file__).with_name('package_entry.gd').resolve(strict=True)
    runner = args.editor_runner.resolve(strict=True) if args.editor_runner else package / 'hanmo.x86_64'
    if args.editor_runner and (not args.expected_editor_sha256 or digest(runner) != args.expected_editor_sha256):
        raise SystemExit('The separate editor runner requires its exact frozen SHA256.')
    runner_kind = 'fixed_editor_loading_complete_exported_pck' if args.editor_runner else 'actual_exported_binary'
    output.mkdir(parents=True, exist_ok=False)
    state = output / 'state'
    env = dict(os.environ, HANMO_STATE_ROOT=str(state), GODOT_SILENCE_ROOT_WARNING='1')
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('TMPDIR', 'tmp')]:
        target = state / leaf
        target.mkdir(parents=True, exist_ok=False)
        env[key] = str(target)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    help_command = [str(runner), '--help']
    help_reply = subprocess.run(help_command, cwd=package, env=env, capture_output=True, timeout=20)
    (output / 'template-help.stdout.log').write_bytes(help_reply.stdout)
    (output / 'template-help.stderr.log').write_bytes(help_reply.stderr)
    help_text = re.sub(r'\x1b\[[0-9;]*m', '', help_reply.stdout.decode(errors='replace'))
    supports_script = any(line.strip().startswith(('-s, --script ', '--script ')) for line in help_text.splitlines())
    supports_pack = any(line.strip().startswith('--main-pack ') for line in help_text.splitlines())
    if help_reply.returncode or not supports_script or (args.editor_runner and not supports_pack):
        write(output / 'report.json', {'format': 'HM-RESIDENT-PACKAGE-ENTRY/1', 'ok': False,
              'status': 'BLOCKED_EXTERNAL_DRIVER_UNSUPPORTED', 'start_save_continue': 'NOT_EXECUTED',
              'help_command': help_command, 'help_exit_code': help_reply.returncode,
              'runner_kind': runner_kind, 'runner_sha256': digest(runner),
              'package_files': before, 'package_unchanged': inventory(package) == before,
              'source_commit': build['source_commit'], 'gui_pixels_accepted': False, 'gui_mouse_evidence': False})
        raise SystemExit('This exact export template does not support an external --script driver; no gameplay was attempted.')
    binding = {'package': str(package), 'source_commit': build['source_commit'],
               'build_sha256': before['BUILD.json'], 'package_files': before,
               'driver_sha256': digest(driver), 'wrapper_sha256': digest(Path(__file__)),
               'runner_kind': runner_kind, 'runner': str(runner), 'runner_sha256': digest(runner),
               'state_root': str(state), 'producer': str(output / 'produce'),
               'storage_profile': build['storage']['profile'],
               'scenario_id': build['content']['scenario_id'],
               'pack_set_digest': build['content']['pack_set_digest'],
               'player_actor_id': 'society::actor.apprentice',
               'appearance_choice_id': 'medium_square_jaw'}
    write(output / 'binding.json', binding)
    bank = state / 'data' / build['storage']['custom_user_dir_name'] / 'profiles' / build['storage']['profile'] / 'saves'
    stages = []
    saved_bank = None
    for phase in ['produce', 'consume']:
        phase_output = output / phase
        phase_output.mkdir()
        command = [str(runner), '--headless', '--max-fps', '60']
        if args.editor_runner:
            command += ['--path', str(package), '--main-pack', str(package / 'hanmo.x86_64')]
        command += ['--script', str(driver), '--',
                   str(output / 'binding.json'), phase, str(phase_output)]
        start = time.monotonic()
        timed_out = False
        with (phase_output / 'process.log').open('xb') as log:
            try:
                reply = subprocess.run(command, cwd=package, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
                code = reply.returncode
            except subprocess.TimeoutExpired:
                code = None
                timed_out = True
        result_path = phase_output / 'result.json'
        result = json.loads(result_path.read_text()) if result_path.exists() else {}
        stage = {'phase': phase, 'command': command, 'exit_code': code, 'timeout': timed_out,
                 'elapsed_seconds': round(time.monotonic() - start, 3), 'result': result,
                 'package_unchanged': inventory(package) == before}
        actual_bank = inventory(bank) if bank.is_dir() else {}
        stage['banks'] = actual_bank
        stage['ok'] = code == 0 and result.get('ok') is True and stage['package_unchanged'] and bool(actual_bank)
        if phase == 'produce':
            saved_bank = actual_bank
        else:
            stage['original_banks_unchanged'] = actual_bank == saved_bank
            stage['different_process'] = result.get('pid') != stages[0]['result'].get('pid')
            stage['ok'] = stage['ok'] and stage['original_banks_unchanged'] and stage['different_process']
        stages.append(stage)
        write(phase_output / 'process-result.json', stage)
        if not stage['ok']:
            break
    report = {'format': 'HM-RESIDENT-PACKAGE-ENTRY/1', 'ok': len(stages) == 2 and all(stage['ok'] for stage in stages),
              'runner_kind': runner_kind, 'runner': str(runner), 'runner_sha256': digest(runner),
              'source_commit': build['source_commit'], 'executable_sha256': before['hanmo.x86_64'],
              'native_sha256': before['libhanmo_world_bridge.so'], 'bank_directory': str(bank),
              'stages': stages, 'gui_pixels_accepted': False, 'gui_mouse_evidence': False}
    write(output / 'report.json', report)
    print(json.dumps({'ok': report['ok'], 'report': str(output / 'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
