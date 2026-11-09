"""在固定编辑器与正式source stage上执行两进程Main组合创建/物理存读；不改产品设置或旧银行。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


def digest(path):
    """逐块记录原始源码、图像、固定引擎和新原生库身份。"""
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def inventory(root, skip_cache=False):
    """完整列出真实文件；仅引擎生成缓存可排除，链接不作为源文件证明。"""
    result = {}
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if skip_cache and '.godot' in relative.parts:
            continue
        if path.is_symlink():
            raise ValueError(f'Unexpected symlink: {path}')
        if path.is_file():
            result[str(relative)] = digest(path)
    return result


def write(path, value):
    """每份机器回执首次写入；失败和旧回执不会被后续运行覆盖。"""
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def setting(text, key):
    """读取已写入source stage的确切字符串配置，不在测试中替换发行profile。"""
    match = re.search(r'^' + re.escape(key) + r'="([^"\n]+)"\s*$', text, re.M)
    if not match:
        raise ValueError(f'Missing fixed project setting: {key}')
    return match.group(1)


def main():
    """在新隔离银行顺序启动producer/consumer，核退出码、不同PID、原世界字节和银行未变。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--expected-editor-sha256', required=True)
    parser.add_argument('--native-library', type=Path, required=True)
    parser.add_argument('--expected-native-sha256', required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--catalog', required=True)
    parser.add_argument('--pack-set-digest', required=True)
    parser.add_argument('--scenario', default='society::scenario.qingchuan_spring_reserve')
    parser.add_argument('--actor', default='society::actor.apprentice')
    args = parser.parse_args()
    project = args.project.resolve(strict=True)
    output = args.output.resolve()
    editor = args.editor.resolve(strict=True)
    native = args.native_library.resolve(strict=True)
    if output == project or project in output.parents or output in project.parents:
        raise SystemExit('Evidence and isolated state must be outside the source stage.')
    if project not in native.parents or digest(native) != args.expected_native_sha256:
        raise SystemExit('The native library is not the exact expected library inside this stage.')
    if digest(editor) != args.expected_editor_sha256:
        raise SystemExit('The fixed editor SHA256 does not match.')
    if not args.catalog.startswith('res://') or '..' in args.catalog or not (project / args.catalog.removeprefix('res://')).is_file():
        raise SystemExit('The real installed catalog must exist under the selected project.')
    text = (project / 'project.godot').read_text()
    profile = setting(text, 'config/storage_profile')
    custom = setting(text, 'config/custom_user_dir_name')
    if not re.search(r'^config/use_custom_user_dir=true\s*$', text, re.M):
        raise SystemExit('Expected the product custom user directory.')
    driver = Path(__file__).with_name('main_entry.gd').resolve(strict=True)
    shared_driver = driver.parent.parent / 'resident_delivery_node/package_entry.gd'
    before = inventory(project, skip_cache=True)
    output.mkdir(parents=True, exist_ok=False)
    state = output / 'state'
    env = dict(os.environ, HANMO_STATE_ROOT=str(state), GODOT_SILENCE_ROOT_WARNING='1')
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('TMPDIR', 'tmp')]:
        folder = state / leaf
        folder.mkdir(parents=True, exist_ok=False)
        env[key] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    binding = {'runner_kind': 'fixed_editor_loading_source_stage', 'source_commit': args.source_commit,
               'project': str(project), 'stage_files': before, 'editor_sha256': digest(editor),
               'native_library': str(native), 'native_sha256': digest(native),
               'driver_sha256': digest(driver), 'shared_driver_sha256': digest(shared_driver),
               'wrapper_sha256': digest(Path(__file__)), 'state_root': str(state),
               'producer': str(output / 'produce'), 'storage_profile': profile,
               'catalog_path': args.catalog, 'pack_set_digest': args.pack_set_digest,
               'scenario_id': args.scenario, 'player_actor_id': args.actor}
    write(output / 'binding.json', binding)
    bank = state / 'data' / custom / 'profiles' / profile / 'saves'
    stages = []
    saved_bank = None
    for phase in ['produce', 'consume']:
        folder = output / phase
        folder.mkdir()
        command = [str(editor), '--headless', '--max-fps', '60', '--path', str(project),
                   '--script', str(driver), '--', str(output / 'binding.json'), phase, str(folder)]
        start = time.monotonic()
        timed_out = False
        with (folder / 'process.log').open('xb') as log:
            try:
                reply = subprocess.run(command, cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
                code = reply.returncode
            except subprocess.TimeoutExpired:
                code = None
                timed_out = True
        result_path = folder / 'result.json'
        result = json.loads(result_path.read_text()) if result_path.exists() else {}
        actual_bank = inventory(bank) if bank.is_dir() else {}
        unchanged = inventory(project, skip_cache=True) == before
        stage = {'phase': phase, 'command': command, 'exit_code': code, 'timeout': timed_out,
                 'elapsed_seconds': round(time.monotonic() - start, 3), 'result': result,
                 'stage_source_unchanged': unchanged, 'banks': actual_bank,
                 'ok': code == 0 and result.get('ok') is True and unchanged and bool(actual_bank)}
        if phase == 'produce':
            saved_bank = actual_bank
        else:
            stage['original_banks_unchanged'] = actual_bank == saved_bank
            stage['different_process'] = result.get('pid') != stages[0]['result'].get('pid')
            stage['ok'] = stage['ok'] and stage['original_banks_unchanged'] and stage['different_process']
        stages.append(stage)
        write(folder / 'process-result.json', stage)
        if not stage['ok']:
            break
    report = {'format': 'HM-FORMAL-APPEARANCE-MAIN/1', 'ok': len(stages) == 2 and all(stage['ok'] for stage in stages),
              'source_commit': args.source_commit, 'editor_sha256': binding['editor_sha256'],
              'native_sha256': binding['native_sha256'], 'pack_set_digest': args.pack_set_digest,
              'bank_directory': str(bank), 'stages': stages,
              'gui_pixels_accepted': False, 'gui_mouse_evidence': False, 'exported_package_test': False}
    write(output / 'report.json', report)
    print(json.dumps({'ok': report['ok'], 'report': str(output / 'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
