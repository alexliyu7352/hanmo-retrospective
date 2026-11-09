"""独立复制实际行动银行，用固定编辑器正常继续，再以同生产静态库核对原生恢复。"""
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

NATIVE_SHA = '6d4d6d76a4cab4d0af7026d2df6f61385e885e41d9e5acfc628874c68d75ed0e'
PACK_DIGEST = '85073906b204952b66c9cf81a7963b5f39d6b75ad6a47de85f3112320f3914d7'
ACTOR = 'society::actor.study_swordswoman'


def digest(path: Path) -> str:
    """流式核对原文件，避免复制大world或将其展开进报告。"""
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def write(path: Path, value: object) -> None:
    """新证据写入独立目录；已有文件拒绝覆盖。"""
    with path.open('x') as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
        file.write('\n')


def inventory(root: Path, product: bool = False) -> dict:
    """保留完整银行清单或关键产品输入身份，缓存不冒充源码。"""
    result = {}
    for path in sorted(root.rglob('*')):
        if not path.is_file() or '.godot' in path.relative_to(root).parts:
            continue
        if product and path.suffix not in ('.gd', '.godot', '.gdextension', '.so', '.json', '.tscn'):
            continue
        result[str(path.relative_to(root))] = digest(path)
    return result


def run(command: list[str], log: Path, env: dict, cwd: Path, timeout: int) -> dict:
    """每个进程只启动一次，超时和失败原样落盘，绝不重放已接受命令。"""
    started = time.monotonic()
    with log.open('xb') as file:
        try:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=file, stderr=subprocess.STDOUT, timeout=timeout)
            code, timed_out = result.returncode, False
        except subprocess.TimeoutExpired:
            code, timed_out = None, True
    return {'command': command, 'exit_code': code, 'timeout': timed_out, 'elapsed_seconds': round(time.monotonic()-started, 3)}


def main() -> None:
    """终态交接后才复制原bank；所有Main与native证据只写全新隔离输出。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--producer-binding', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--native-oracle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    project, editor, oracle = (p.resolve(strict=True) for p in (args.project, args.editor, args.native_oracle))
    producer_binding = json.loads(args.producer_binding.read_text())
    manifest = json.loads(args.manifest.read_text())
    producer = args.manifest.resolve().parent
    original_directory = Path(manifest['bank_directory']).resolve(strict=True)
    output = args.output.resolve()
    if output.exists() or output == project or project in output.parents or output in project.parents or original_directory in output.parents:
        raise ValueError('Use a new output directory separate from project and original banks.')
    if manifest['actor_id'] != ACTOR or manifest['source']['tick'] <= 0:
        raise ValueError('Only the new real Shen action bank is accepted; tick-zero is not this node.')
    original_banks = inventory(original_directory)
    if original_banks != manifest['banks']:
        raise ValueError('Original complete bank directory no longer matches producer manifest.')
    bank_files = sorted(original_directory.glob('manual-1.*.hmcheckpoint'))
    if not bank_files:
        raise ValueError('No actual normal-menu manual-1 bank.')
    original_bank = max(bank_files, key=lambda p: json.loads(p.read_text())['generation'])
    native = project / 'native_resident_bridge/libhanmo_world_bridge.so'
    if digest(native) != NATIVE_SHA:
        raise ValueError('The real active native library differs from frozen native165.')
    config = (project/'project.godot').read_text()
    settings = {}
    for name in ['storage_profile', 'custom_user_dir_name', 'default_catalog']:
        match = re.search(r'^config/'+name+r'="([^"\n]+)"$', config, re.M)
        if not match:
            raise ValueError('Missing real product setting: '+name)
        settings[name] = match.group(1)
    if settings['storage_profile'] != 'qingchuan-active-person-check' or settings['custom_user_dir_name'] != 'hanmo-qingchuan-active-person-check':
        raise ValueError('Expected dedicated actual action profile.')
    before = inventory(project, product=True)
    originals = {str(path): digest(path) for path in [args.producer_binding, args.manifest, producer/'saved-world.json', producer/'saved-appearance.json']}
    output.mkdir(parents=True)
    phase = output/'consume'; phase.mkdir()
    state = output/'state'
    env = dict(os.environ, GODOT_SILENCE_ROOT_WARNING='1', HANMO_STATE_ROOT=str(state))
    for key, leaf in [('XDG_DATA_HOME','data'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state'),('TMPDIR','tmp')]:
        folder = state/leaf; folder.mkdir(parents=True); env[key] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    bank_directory = state/'data'/settings['custom_user_dir_name']/'profiles'/settings['storage_profile']/'saves'
    shutil.copytree(original_directory, bank_directory)
    private_editor = output/'editor'/editor.name; private_editor.parent.mkdir()
    os.link(editor, private_editor); (private_editor.parent/'_sc_').touch()
    driver = Path(__file__).with_name('consumer_main.gd').resolve(strict=True)
    binding = dict(producer_binding, runner_kind='fixed_editor_loading_source_stage', player_actor_id=ACTOR,
        pack_set_digest=PACK_DIGEST, storage_profile=settings['storage_profile'], state_root=str(state),
        catalog_path=settings['default_catalog'], producer=str(producer), original_bank=str(original_bank),
        boundary=manifest['boundary'], producer_pid=manifest['producer_pid'])
    binding['consumer_identity'] = {'driver_sha256':digest(driver), 'runner_sha256':digest(Path(__file__)),
        'oracle_sha256':digest(oracle), 'editor_sha256':digest(editor), 'native_sha256':digest(native),
        'original_bank_sha256':digest(original_bank), 'producer_files':originals, 'product_files':before}
    write(output/'binding.json', binding)
    command = [str(private_editor),'--headless','--max-fps','60','--path',str(project),'--script',str(driver),'--',str(output/'binding.json'),'consume',str(phase)]
    process = run(command, phase/'process.log', env, project, 300)
    result_path = phase/'result.json'
    result = json.loads(result_path.read_text()) if result_path.exists() else {}
    log = (phase/'process.log').read_text(errors='replace')
    guards = {'actual_new_process':result.get('pid') != manifest['producer_pid'],
        'original_banks_unchanged':inventory(original_directory)==original_banks,
        'copied_banks_unchanged':inventory(bank_directory)==original_banks,
        'producer_raw_unchanged':all(digest(Path(path))==sha for path,sha in originals.items()),
        'product_inputs_unchanged':inventory(project,product=True)==before,
        'no_script_errors':'SCRIPT ERROR:' not in log and '\nERROR:' not in log}
    native_result = {}
    if (phase/'bank-payload.hmc').exists():
        expanded = producer/'expanded-catalog.json'
        native_command = [str(oracle),str(expanded),str(phase/'bank-payload.hmc'),str(producer/'saved-world.json'),str(output/'native-report.json')]
        if (phase/'bank-continuation.hmc').exists(): native_command.append(str(phase/'bank-continuation.hmc'))
        native_result = run(native_command, output/'native-process.log', env, project, 120)
    report = {'format':'HM-QINGCHUAN-ACTIVE-CONSUMER/1', 'ok':process['exit_code']==0 and result.get('ok') is True and all(guards.values()) and native_result.get('exit_code')==0,
        'process':process,'guards':guards,'native':native_result,'result':result,
        'bank_directory':str(bank_directory),'native_sha256':NATIVE_SHA,'pack_set_digest':PACK_DIGEST,
        'gui_pixels_accepted':False,'gui_mouse_evidence':False,'exported_package_test':False}
    write(output/'report.json', report)
    print(json.dumps({'ok':report['ok'],'report':str(output/'report.json')},ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
