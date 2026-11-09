"""运行沈蘅实际生活单进程生产者，并为独立消费者保留不可覆盖的原银行与来源身份。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha(path):
    """按原文件字节计算身份，证据和银行均不归一化。"""
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def files(root):
    """忽略Godot生成缓存，完整核对stage产品源及真实物理银行。"""
    result = {}
    for path in sorted(root.rglob('*')):
        if '.godot' not in path.relative_to(root).parts and path.is_file():
            result[str(path.relative_to(root))] = sha(path)
    return result


def write(path, value):
    """首次创建机器回执，拒绝覆盖先前运行或失败。"""
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def main():
    """核中央stage与固定引擎身份，隔离XDG运行，退出后冻结真实bank目录交付。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage-manifest', type=Path, required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest_path = args.stage_manifest.resolve(strict=True)
    manifest = json.loads(manifest_path.read_text())
    project = manifest_path.parent / 'game'
    editor = args.editor.resolve(strict=True)
    output = args.output.resolve()
    config = manifest['configuration']
    native = project / 'native_resident_bridge/libhanmo_world_bridge.so'
    if sha(editor) != config['engine']['sha256'] or sha(native) != config['native']['library_sha256']:
        raise SystemExit('Fixed engine or native identity differs from central stage manifest')
    if output == project or output in project.parents or project in output.parents:
        raise SystemExit('Evidence must be outside stage')
    driver = Path(__file__).with_name('producer.gd').resolve(strict=True)
    drivers = [driver, driver.parent.parent / 'two_actors_main_node/main_entry.gd',
               driver.parent.parent / 'formal_appearance_main_node/main_entry.gd',
               driver.parent.parent / 'resident_delivery_node/package_entry.gd', Path(__file__).resolve()]
    before = files(project)
    output.mkdir(parents=True, exist_ok=False)
    state = output / 'state'
    produce = output / 'produce'
    produce.mkdir()
    env = dict(os.environ, HANMO_STATE_ROOT=str(state), GODOT_SILENCE_ROOT_WARNING='1')
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('TMPDIR', 'tmp')]:
        folder = state / leaf
        folder.mkdir(parents=True)
        env[key] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    binding = {'runner_kind': 'fixed_editor_loading_source_stage', 'source_commit': manifest['source_commit'],
               'project': str(project), 'stage_files': before, 'stage_manifest': str(manifest_path),
               'stage_manifest_sha256': sha(manifest_path), 'editor': str(editor), 'editor_sha256': sha(editor),
               'native_library': str(native), 'native_sha256': sha(native),
               'native_source_commit': config['native']['source_commit'],
               'drivers': {str(path): sha(path) for path in drivers},
               'state_root': str(state), 'producer': str(produce),
               'storage_profile': config['storage']['profile'],
               'custom_user_dir_name': config['storage']['custom_user_dir_name'],
               'catalog_path': manifest['content']['catalog_path'],
               'pack_set_digest': config['content']['pack_set_digest'],
               'scenario_id': config['content']['scenario_id'],
               'player_actor_id': 'society::actor.study_swordswoman'}
    write(output / 'binding.json', binding)
    command = [str(editor), '--headless', '--max-fps', '60', '--path', str(project),
               '--script', str(driver), '--', str(output / 'binding.json'), 'produce', str(produce)]
    start = time.monotonic()
    timed_out = False
    with (produce / 'process.log').open('xb') as log:
        try:
            result = subprocess.run(command, cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = None
            timed_out = True
    result_path = produce / 'result.json'
    result = json.loads(result_path.read_text()) if result_path.exists() else {}
    bank = state / 'data' / config['storage']['custom_user_dir_name'] / 'profiles' / config['storage']['profile'] / 'saves'
    banks = files(bank) if bank.is_dir() else {}
    unchanged = files(project) == before
    report = {'format': 'HM-QINGCHUAN-ACTIVE-PRODUCER-PROCESS/1',
              'ok': code == 0 and result.get('ok') is True and unchanged and bool(banks),
              'command': command, 'exit_code': code, 'timeout': timed_out,
              'elapsed_seconds': round(time.monotonic() - start, 3), 'result': result,
              'stage_source_unchanged': unchanged, 'bank_directory': str(bank), 'banks': banks,
              'gui_pixels_accepted': False, 'gui_mouse_evidence': False, 'exported_package_test': False}
    write(output / 'report.json', report)
    print(json.dumps({'ok': report['ok'], 'exit_code': code, 'report': str(output / 'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
