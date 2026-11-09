"""以独立stage与用户目录核对真实Main军令会话；旧作者输入、资源和原生库只读复用。"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

EXPECTED_INPUT = '074cb34a5a6618537b887c357e7926701f8aa19e47bcf144114c441f9fb3068c'
EXPECTED_NATIVE = '6d4d6d76a4cab4d0af7026d2df6f61385e885e41d9e5acfc628874c68d75ed0e'


def digest(path):
    """只记录实际文件字节身份，不能用来源名替代加载物。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """只创建新输出目录；从指定Git提交提取源码，银行由游戏真实菜单创建。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-stage', type=Path, required=True)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--mode', choices=['red', 'full'], default='red')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    reference = args.reference_stage.resolve(strict=True)
    source_input = args.input.resolve(strict=True)
    editor = args.editor.resolve(strict=True)
    output = args.output.resolve()
    if output.exists() or output == reference or reference in output.parents:
        raise ValueError('Use a fresh independent output directory.')
    if digest(source_input) != EXPECTED_INPUT:
        raise ValueError('The approved author battle input differs.')
    resolved = subprocess.check_output(['git', 'rev-parse', args.source_commit + '^{commit}'], cwd=repo, text=True).strip()
    if resolved != args.source_commit:
        raise ValueError('Use a full source commit SHA.')
    output.mkdir(parents=True)
    stage = output / 'stage' / 'game'
    shutil.copytree(reference, stage)
    names = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', args.source_commit, '--', 'game/ui', 'game/storage', 'game/core', 'game/main.tscn'], cwd=repo, text=True).splitlines()
    source_files = {}
    for name in names:
        if Path(name).suffix not in [".gd", ".tscn", ".tres"]:
            continue
        destination = output / 'stage' / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(subprocess.check_output(['git', 'show', args.source_commit + ':' + name], cwd=repo))
        source_files[name.removeprefix('game/')] = digest(destination)
    driver = repo / 'game/tests/test_military_session_main.gd'
    shutil.copy2(driver, stage / 'tests' / driver.name)
    config = (stage / 'project.godot').read_text()
    config = config.replace('[application]', '[application]\nconfig/storage_profile="player-alpha"\nconfig/use_custom_user_dir=true\nconfig/custom_user_dir_name="hanmo-f05"\nconfig/default_catalog="res://data/generated/march.hmcatalog.json"')
    config += '\n[development]\nnative_resident_world_enabled=true\n'
    (stage / 'project.godot').write_text(config)
    native = stage / 'native_resident_bridge/libhanmo_world_bridge.so'
    if digest(native) != EXPECTED_NATIVE:
        raise ValueError('The actual stage native library differs.')
    state = output / 'state'
    env = dict(os.environ, GODOT_SILENCE_ROOT_WARNING='1')
    for key, leaf in [('XDG_DATA_HOME','data'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state'),('TMPDIR','tmp')]:
        folder = state / leaf
        folder.mkdir(parents=True)
        env[key] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    isolated_editor = output / 'editor' / editor.name
    isolated_editor.parent.mkdir()
    os.link(editor, isolated_editor)
    (isolated_editor.parent / '_sc_').touch()
    binding = {
        'format':'HM-MILITARY-SESSION-MAIN/1', 'source_commit':resolved,
        'source_files':source_files,
        'input':str(source_input), 'input_sha256':digest(source_input),
        'native_sha256':digest(native), 'editor_sha256':digest(editor),
        'driver_sha256':digest(driver), 'wrapper_sha256':digest(Path(__file__)),
        'project_config_sha256':digest(stage / 'project.godot'),
        'extension_sha256':digest(stage / 'native_resident_bridge/hanmo_world_bridge.gdextension'),
        'reference_stage':str(reference),
        'state_root':str(state), 'mode':args.mode, 'stage':str(stage),
    }
    (output / 'binding.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2)+'\n')
    evidence = output / 'evidence'
    evidence.mkdir()
    command = [str(isolated_editor),'--headless','--max-fps','60','--path',str(stage),'--script','res://tests/test_military_session_main.gd','--',str(output / 'binding.json'),str(evidence)]
    started = time.monotonic()
    timed_out = False
    with (output / 'process.log').open('xb') as log:
        try:
            result = subprocess.run(command, env=env, cwd=stage, stdout=log, stderr=subprocess.STDOUT, timeout=240)
            exit_code = result.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
            exit_code = None
    result_path = evidence / 'result.json'
    result_json = json.loads(result_path.read_text()) if result_path.exists() else {}
    log_text = (output / 'process.log').read_text(errors='replace')
    guards = {'source_files_unchanged':all(digest(stage / name) == sha for name, sha in source_files.items()),'native_unchanged':digest(native) == EXPECTED_NATIVE,'input_unchanged':digest(source_input) == EXPECTED_INPUT,'no_script_errors':'SCRIPT ERROR:' not in log_text and '\nERROR:' not in log_text}
    banks = {str(path.relative_to(output)):digest(path) for path in state.rglob('*.hmcheckpoint')}
    report = {'ok':exit_code == 0 and result_json.get('ok') is True and all(guards.values()),'exit_code':exit_code,'timeout':timed_out,'guards':guards,'banks':banks,'elapsed_seconds':round(time.monotonic()-started,3),'result':result_json,'command':command,'native_sha256':digest(native),'input_unchanged':digest(source_input)==EXPECTED_INPUT,'gui_pixels_accepted':False,'gui_mouse_evidence':False}
    if report['ok'] and args.mode == 'full':
        cold = output / 'cold'
        cold.mkdir()
        cold_binding = dict(binding, mode='cold', saved_world_path=str(evidence / 'saved-world.json'))
        (output / 'cold-binding.json').write_text(json.dumps(cold_binding, ensure_ascii=False, indent=2)+'\n')
        cold_command = command[:-2] + [str(output / 'cold-binding.json'), str(cold)]
        with (cold / 'process.log').open('xb') as log:
            try:
                cold_reply = subprocess.run(cold_command, env=env, cwd=stage, stdout=log, stderr=subprocess.STDOUT, timeout=120)
                cold_code = cold_reply.returncode
            except subprocess.TimeoutExpired:
                cold_code = None
        cold_result = json.loads((cold / 'result.json').read_text()) if (cold / 'result.json').exists() else {}
        cold_log = (cold / 'process.log').read_text(errors='replace')
        report['cold'] = {'exit_code':cold_code,'result':cold_result,'command':cold_command}
        report['ok'] = cold_code == 0 and cold_result.get('ok') is True and 'SCRIPT ERROR:' not in cold_log and '\nERROR:' not in cold_log
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'ok':report['ok'],'report':str(output / 'report.json')}))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
