"""复制既有沈蘅银行后运行本人档案真实Main只读门；来源、库、银行和失败日志均保留精确身份。"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time


EXPECTED_BANK_SHA256 = 'e52025590173c4258fde867bf72068d78a53db2957abdec5bcff5c08ec06b28e'
SOURCE_FILES = [
    'ui/main.gd', 'ui/native_resident_backend.gd', 'ui/screen_composer.gd',
    'ui/self_profile_adapter.gd', 'ui/self_profile_screen.gd',
    'ui/self_profile_skills.gd',
]


def helpers():
    """复用已验两人节点的摘要、配置和只写一次回执函数，不复制其新局或存档流程。"""
    path = Path(__file__).resolve().parent.parent / 'two_actors_main_node/run_node.py'
    spec = importlib.util.spec_from_file_location('self_profile_shared_runner', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_bindings(project, source_commit, shared):
    """逐个核查关键产品源码与所声明Git提交字节相等，禁止旧stage借新commit身份通过。"""
    root = Path(__file__).resolve().parents[2]
    resolved = subprocess.check_output(['git', 'rev-parse', '--verify', source_commit + '^{commit}'], cwd=root, text=True).strip()
    if resolved != source_commit:
        raise ValueError('The source commit must be the exact full SHA.')
    result = {}
    for name in SOURCE_FILES:
        expected = subprocess.check_output(['git', 'show', source_commit + ':game/' + name], cwd=root)
        actual = shared.digest(project / name)
        if hashlib.sha256(expected).hexdigest() != actual:
            raise ValueError('Stage source differs from declared commit: ' + name)
        result[name] = actual
    return result


def main():
    """只启动一个consume进程；原银行和世界输入只读，所有测试写入全新隔离目录。"""
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
    parser.add_argument('--original-bank', type=Path, required=True)
    parser.add_argument('--saved-world', type=Path, required=True)
    parser.add_argument('--profile-open-kind', required=True)
    args = parser.parse_args()
    shared = helpers()
    project = args.project.resolve(strict=True)
    output = args.output.resolve()
    editor = args.editor.resolve(strict=True)
    native = args.native_library.resolve(strict=True)
    original_bank = args.original_bank.resolve(strict=True)
    saved_world = args.saved_world.resolve(strict=True)
    if output == project or project in output.parents or output in project.parents:
        raise SystemExit('The evidence/state directory must be outside the source stage.')
    if output == original_bank.parent or output in original_bank.parents or original_bank.parent in output.parents:
        raise SystemExit('Use an independent output outside the original bank directory.')
    if original_bank.name != 'manual-1.B.hmcheckpoint' or shared.digest(original_bank) != EXPECTED_BANK_SHA256:
        raise SystemExit('Expected the unchanged approved Shen Heng bank e5202559...')
    if project not in native.parents or shared.digest(native) != args.expected_native_sha256:
        raise SystemExit('The native library must be the exact new library inside this stage.')
    extension = project / 'native_resident_bridge/hanmo_world_bridge.gdextension'
    selected_library = re.search(r'^linux\.debug\.x86_64\s*=\s*"res://([^"\n]+)"\s*$', extension.read_text(), re.M)
    if not selected_library or (project / selected_library.group(1)).resolve(strict=True) != native:
        raise SystemExit('The inspected library must be the one loaded by the active Linux debug extension.')
    if shared.digest(editor) != args.expected_editor_sha256:
        raise SystemExit('The fixed editor SHA256 does not match.')
    if not re.fullmatch(r'[a-z_]+', args.profile_open_kind):
        raise SystemExit('Use the actual profile menu command_kind.')
    if not args.catalog.startswith('res://') or '..' in args.catalog or not (project / args.catalog.removeprefix('res://')).is_file():
        raise SystemExit('The real installed catalog must exist inside the stage.')
    expected_world = json.loads(saved_world.read_text())
    actor_id = 'society::actor.study_swordswoman'
    if expected_world.get('player_actor_id') != actor_id:
        raise SystemExit('Expected the original Shen Heng world capture.')
    text = (project / 'project.godot').read_text()
    profile = shared.setting(text, 'config/storage_profile')
    custom = shared.setting(text, 'config/custom_user_dir_name')
    if not re.search(r'^config/use_custom_user_dir=true\s*$', text, re.M):
        raise SystemExit('The stage must use the product custom user directory.')
    for name in [profile, custom]:
        if not name or Path(name).name != name or name in ['.', '..']:
            raise SystemExit('Storage names must be single safe path components.')
    source_files = source_bindings(project, args.source_commit, shared)
    before = shared.inventory(project, skip_cache=True)
    original_inventory = shared.inventory(original_bank.parent)
    saved_world_sha = shared.digest(saved_world)
    output.mkdir(parents=True, exist_ok=False)
    private_editor = output / 'editor' / editor.name
    private_editor.parent.mkdir()
    try:
        os.link(editor, private_editor)
    except OSError:
        shutil.copy2(editor, private_editor)
    (private_editor.parent / '_sc_').touch()
    if shared.digest(private_editor) != args.expected_editor_sha256:
        raise SystemExit('The isolated editor copy differs from the fixed editor.')
    state = output / 'state'
    env = dict(os.environ, HANMO_STATE_ROOT=str(state), GODOT_SILENCE_ROOT_WARNING='1')
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('TMPDIR', 'tmp')]:
        folder = state / leaf
        folder.mkdir(parents=True, exist_ok=False)
        env[key] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    bank_directory = state / 'data' / custom / 'profiles' / profile / 'saves'
    bank_directory.mkdir(parents=True, exist_ok=False)
    shutil.copy2(original_bank, bank_directory / original_bank.name)
    bank_before = shared.inventory(bank_directory)
    driver = Path(__file__).with_name('main_entry.gd').resolve(strict=True)
    binding = {
        'runner_kind': 'fixed_editor_loading_source_stage',
        'source_commit': args.source_commit, 'source_files': source_files,
        'project': str(project), 'stage_files': before,
        'editor_sha256': shared.digest(editor), 'editor_source': str(editor),
        'isolated_editor': str(private_editor), 'native_library': str(native),
        'native_sha256': shared.digest(native), 'driver_sha256': shared.digest(driver),
        'wrapper_sha256': shared.digest(Path(__file__)),
        'shared_driver_sha256': shared.digest(driver.parent.parent / 'resident_delivery_node/package_entry.gd'),
        'formal_driver_sha256': shared.digest(driver.parent.parent / 'formal_appearance_main_node/main_entry.gd'),
        'shared_wrapper_sha256': shared.digest(driver.parent.parent / 'two_actors_main_node/run_node.py'),
        'state_root': str(state), 'storage_profile': profile,
        'catalog_path': args.catalog, 'pack_set_digest': args.pack_set_digest,
        'player_actor_id': actor_id, 'profile_open_kind': args.profile_open_kind,
        'original_bank': str(original_bank), 'original_bank_sha256': EXPECTED_BANK_SHA256,
        'saved_world': str(saved_world), 'saved_world_sha256': saved_world_sha,
    }
    shared.write(output / 'binding.json', binding)
    phase = output / 'consume'
    phase.mkdir()
    command = [str(private_editor), '--headless', '--max-fps', '60', '--path', str(project),
               '--script', str(driver), '--', str(output / 'binding.json'), 'consume', str(phase)]
    start = time.monotonic()
    timed_out = False
    with (phase / 'process.log').open('xb') as log:
        try:
            reply = subprocess.run(command, cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
            code = reply.returncode
        except subprocess.TimeoutExpired:
            code = None
            timed_out = True
    result_path = phase / 'result.json'
    result = json.loads(result_path.read_text()) if result_path.exists() else {}
    log_text = (phase / 'process.log').read_text(errors='replace')
    guards = {
        'stage_source_unchanged': shared.inventory(project, skip_cache=True) == before,
        'original_bank_directory_unchanged': shared.inventory(original_bank.parent) == original_inventory,
        'copied_bank_directory_unchanged': shared.inventory(bank_directory) == bank_before,
        'original_world_capture_unchanged': shared.digest(saved_world) == saved_world_sha,
        'no_script_errors': 'SCRIPT ERROR:' not in log_text and '\nERROR:' not in log_text,
    }
    report = {
        'format': 'HM-SELF-PROFILE-MAIN/1',
        'ok': code == 0 and result.get('ok') is True and all(guards.values()),
        'source_commit': args.source_commit, 'native_sha256': binding['native_sha256'],
        'editor_sha256': binding['editor_sha256'], 'command': command,
        'exit_code': code, 'timeout': timed_out,
        'elapsed_seconds': round(time.monotonic() - start, 3),
        'guards': guards, 'result': result, 'bank_directory': str(bank_directory),
        'gui_pixels_accepted': False, 'gui_mouse_evidence': False,
        'exported_package_test': False,
    }
    shared.write(output / 'report.json', report)
    print(json.dumps({'ok': report['ok'], 'report': str(output / 'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
