"""冻结源码后只启动一次真实Main；复制既有规划银行并保存只读播放原始证据。"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

BANK_SHA = '8b31f1e52156a83a46c97dbdbad21841716dfa17b355e2a57ad19f97624b6fb2'
NATIVE_SHA = '6d4d6d76a4cab4d0af7026d2df6f61385e885e41d9e5acfc628874c68d75ed0e'
EDITOR_SHA = '8d106cbe6144c2dc7e881d61d2429c1a8a76e6b22ef48bd5e48dcf934953f71e'
PACK = '85073906b204952b66c9cf81a7963b5f39d6b75ad6a47de85f3112320f3914d7'


def digest(path: Path) -> str:
    """原字节流式校验，不通过解析重排制造相等。"""
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def write(path: Path, data: object) -> None:
    """独立证据只创建一次；失败运行不覆盖以便保留原红。"""
    with path.open('x') as handle:
        json.dump(data, handle, ensure_ascii=False, indent=2)
        handle.write('\n')


def inventory(root: Path) -> dict[str, str]:
    """记录项目所有实际输入，Godot导入缓存不混作源码身份。"""
    return {str(path.relative_to(root)): digest(path) for path in sorted(root.rglob('*'))
            if path.is_file() and '.godot' not in path.relative_to(root).parts}


def main() -> None:
    """正式freeze与库哈希一致才运行；不构建native、不改源银行、不启动GUI。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['playback','cold_reload'], default='playback')
    parser.add_argument('--project', required=True, type=Path)
    parser.add_argument('--editor', required=True, type=Path)
    parser.add_argument('--bank', required=True, type=Path)
    parser.add_argument('--saved-world', required=True, type=Path)
    parser.add_argument('--source-freeze', required=True, type=Path)
    parser.add_argument('--controls', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    project, editor, bank, saved = (p.resolve(strict=True) for p in
                                    [args.project, args.editor, args.bank, args.saved_world])
    freeze = json.loads(args.source_freeze.read_text())
    controls = json.loads(args.controls.read_text())
    before = inventory(project)
    if freeze.get('product_files') != before or not freeze.get('source_commit'):
        raise ValueError('Explicit frozen product_files and source_commit are required.')
    if digest(bank) != BANK_SHA or digest(editor) != EDITOR_SHA or digest(project/'native_resident_bridge/libhanmo_world_bridge.so') != NATIVE_SHA:
        raise ValueError('Bank, fixed editor or native165 identity differs.')
    if set(controls) != {'pause', 'play', 'speed_1', 'speed_2', 'skip'}:
        raise ValueError('Controls must be the confirmed production command-kind/target pairs.')
    for pair in controls.values():
        if not isinstance(pair, list) or len(pair) != 2 or not all(isinstance(s, str) for s in pair):
            raise ValueError('A control pair is [command_kind, target_id].')
    output = args.output.resolve()
    if output.exists() or output == project or project in output.parents or output in project.parents or bank.parent in output.parents:
        raise ValueError('Output must be new and separate from the product and source banks.')
    config = (project/'project.godot').read_text()
    settings = {}
    for key in ['storage_profile', 'custom_user_dir_name', 'default_catalog']:
        found = re.search(r'^config/'+key+r'="([^"\n]+)"$', config, re.M)
        if not found:
            raise ValueError('Missing product setting '+key)
        settings[key] = found.group(1)
    if settings['storage_profile'] != 'qingchuan-active-person-check' or settings['custom_user_dir_name'] != 'hanmo-qingchuan-active-person-check':
        raise ValueError('Use the existing dedicated bank profile without reconfiguring production.')
    original_banks = inventory(bank.parent)
    saved_sha = digest(saved)
    output.mkdir(parents=True)
    phase = output/'consume'; phase.mkdir()
    state = output/'state'
    env = dict(os.environ, GODOT_SILENCE_ROOT_WARNING='1', HANMO_STATE_ROOT=str(state))
    for key, leaf in [('XDG_DATA_HOME','data'), ('XDG_CONFIG_HOME','config'), ('XDG_CACHE_HOME','cache'), ('XDG_STATE_HOME','state'), ('TMPDIR','tmp')]:
        directory = state/leaf; directory.mkdir(parents=True); env[key] = str(directory)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    banks = state/'data'/settings['custom_user_dir_name']/'profiles'/settings['storage_profile']/'saves'
    shutil.copytree(bank.parent, banks)
    isolated_editor = output/'editor'/editor.name; isolated_editor.parent.mkdir()
    os.link(editor, isolated_editor); (isolated_editor.parent/'_sc_').touch()
    driver = Path(__file__).with_name('main_entry.gd').resolve(strict=True)
    inherited = [driver, driver.with_name('source_boundaries.gd'), driver.parent.parent/'formal_appearance_main_node/main_entry.gd', driver.parent.parent/'resident_delivery_node/package_entry.gd']
    binding = {'runner_kind':'fixed_editor_loading_source_stage', 'storage_profile':settings['storage_profile'],
               'state_root':str(state), 'catalog_path':settings['default_catalog'], 'pack_set_digest':PACK,
               'player_actor_id':'society::actor.study_swordswoman', 'saved_world':str(saved),
               'original_bank':str(bank), 'mode':args.mode, 'controls':controls, 'source_commit':freeze['source_commit'],
               'driver_files':{str(p):digest(p) for p in inherited}, 'product_files':before,
               'editor_sha256':EDITOR_SHA, 'native_sha256':NATIVE_SHA, 'bank_sha256':BANK_SHA}
    write(output/'binding.json', binding)
    command = [str(isolated_editor), '--headless', '--max-fps', '60', '--path', str(project), '--script', str(driver), '--', str(output/'binding.json'), 'consume', str(phase)]
    started = time.monotonic()
    with (phase/'process.log').open('xb') as log:
        try:
            process = subprocess.run(command, cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300)
            code, timed_out = process.returncode, False
        except subprocess.TimeoutExpired:
            code, timed_out = None, True
    result = json.loads((phase/'result.json').read_text()) if (phase/'result.json').exists() else {}
    log_text = (phase/'process.log').read_text(errors='replace')
    guards = {'source_banks_unchanged':inventory(bank.parent)==original_banks,
              'copied_banks_unchanged':inventory(banks)==original_banks, 'saved_world_unchanged':digest(saved)==saved_sha,
              'frozen_product_unchanged':inventory(project)==before,
              'drivers_unchanged':all(digest(Path(p))==sha for p,sha in binding['driver_files'].items()),
              'no_script_errors':'SCRIPT ERROR:' not in log_text and '\nERROR:' not in log_text}
    report = {'format':'HM-PERSONAL-PLAYBACK-MAIN/1', 'ok':code==0 and result.get('ok') is True and all(guards.values()),
              'source_commit':freeze['source_commit'], 'mode':args.mode, 'process':{'command':command, 'exit_code':code, 'timeout':timed_out, 'elapsed_seconds':round(time.monotonic()-started,3)},
              'guards':guards, 'result':result, 'gui_pixels_accepted':False, 'gui_mouse_evidence':False,
              'exported_package_test':False, 'native_rebuilt':False}
    write(output/'report.json', report)
    print(json.dumps({'ok':report['ok'], 'report':str(output/'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
