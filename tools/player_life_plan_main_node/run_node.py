"""生活安排的单一Main节点启动器；绑定原始物理银行和中央受审库后才允许运行。"""
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



def validate_ui_continuation(path, producer_path, consumer_path, producer, consumer):
    """只准已显式复核的同银行UI读屏障关系；原生、资源和其它生产文件须精确不变。"""
    if path.is_symlink() or not path.is_file():
        raise SystemExit('Reviewed UI continuation must be a regular JSON file.')
    transition = json.loads(path.read_text())
    fields = {'format', 'producer_binding_sha256', 'consumer_binding_sha256',
              'producer_source_commit', 'consumer_source_commit', 'reviewed_source_commit', 'source_files'}
    if not isinstance(transition, dict) or set(transition) != fields or transition.get('format') != 'HM-QINGCHUAN-REVIEWED-UI-CONTINUATION/1':
        raise SystemExit('Wrong reviewed UI continuation contract.')
    if (transition['producer_binding_sha256'] != digest(producer_path)
            or transition['consumer_binding_sha256'] != digest(consumer_path)
            or transition['producer_source_commit'] != producer.get('source_commit')
            or transition['consumer_source_commit'] != consumer.get('source_commit')
            or transition['reviewed_source_commit'] != consumer.get('reviewed_source_commit')
            or transition['reviewed_source_commit'] != transition['consumer_source_commit']
            or producer.get('source_commit') != producer.get('reviewed_source_commit')
            or transition['producer_source_commit'] == transition['consumer_source_commit']):
        raise SystemExit('Reviewed UI continuation does not bind these exact reviewed sources and bindings.')
    allowed = 'game/ui/resident_page_requests.gd'
    old_files, new_files = producer.get('source_files', {}), consumer.get('source_files', {})
    if (not old_files or set(old_files) != set(new_files)
            or {name for name in old_files if old_files[name] != new_files[name]} != {allowed}
            or transition['source_files'] != {allowed: {'before_sha256': old_files.get(allowed),
                                                       'after_sha256': new_files.get(allowed)}}):
        raise SystemExit('Reviewed UI continuation permits only the exact resident_page_requests.gd change.')
    for key in ['stage_game', 'engine', 'engine_sha256', 'native_production_files', 'native_source_commit',
                'cases', 'chapter_profile', 'pack_set_digest', 'catalog_path']:
        if producer.get(key) != consumer.get(key):
            raise SystemExit('Reviewed UI continuation changed an immutable product input: ' + key)
    if producer.get('chapter_profile') != 'qingchuan_region':
        raise SystemExit('Reviewed UI continuation is regional only.')
    for key in ['native_extension', 'native_build_receipt', 'registration', 'descriptor']:
        if producer[key]['sha256'] != consumer[key]['sha256']:
            raise SystemExit('Reviewed UI continuation changed an immutable artifact: ' + key)
    # 四个既有stage测试助手仍按独立driver身份更新；不能借driver标签伪装产品文件。
    helper_changes = {}
    helper_names = ['test_dynamic_world_player_flow.gd', 'test_regional_life_player_flow.gd',
                    'test_study_segments_player_flow.gd', 'test_qingchuan_region_player_flow.gd']
    for name in helper_names:
        expected_path = str(Path(consumer['stage_game']) / 'tests' / name)
        old_driver = producer.get('drivers', {}).get(name, {})
        new_driver = consumer.get('drivers', {}).get(name, {})
        if old_driver.get('path') != expected_path or new_driver.get('path') != expected_path:
            raise SystemExit('Reviewed UI continuation requires each original helper identity: ' + name)
        if old_driver.get('sha256') != new_driver.get('sha256'):
            helper_changes[expected_path] = {'before_sha256': old_driver['sha256'], 'after_sha256': new_driver['sha256']}
    # stage完整路径集合保持；除单一产品文件和明确绑定的旧测试助手，其它文件均相同。
    allowed_path = str(Path(consumer['stage_game']) / allowed.removeprefix('game/'))
    for key in ['inputs', 'imported_resources']:
        old = {item['path']: item['sha256'] for item in producer.get(key, [])}
        new = {item['path']: item['sha256'] for item in consumer.get(key, [])}
        if len(old) != len(producer.get(key, [])) or len(new) != len(consumer.get(key, [])) or set(old) != set(new):
            raise SystemExit('Reviewed UI continuation changed or duplicated bound input paths: ' + key)
        differences = {name for name in old if old[name] != new[name]}
        expected = {allowed_path} | set(helper_changes) if key == 'inputs' else set()
        if (differences != expected or (key == 'inputs' and
                (old[allowed_path] != old_files[allowed] or new[allowed_path] != new_files[allowed]))):
            raise SystemExit('Reviewed UI continuation changed another bound file: ' + key)
        if key == 'inputs':
            for name, change in helper_changes.items():
                if old.get(name) != change['before_sha256'] or new.get(name) != change['after_sha256']:
                    raise SystemExit('Reviewed UI continuation helper bytes differ from their driver bindings.')
    return {'path': str(path.resolve()), 'sha256': digest(path), 'transition': transition, 'helper_input_changes': helper_changes}

def main():
    """明确一类行动的生产、自动恢复或手动对照阶段；消费者必须晚于真实生产者退出。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binding', type=Path, required=True)
    parser.add_argument('--case-root', type=Path, required=True)
    parser.add_argument('--phase', choices=['produce', 'consume-auto', 'consume-manual'], required=True)
    parser.add_argument('--chapter-profile', choices=['player_life', 'qingchuan_region'], default='player_life')
    parser.add_argument('--case', choices=['funded', 'busy', 'fulfill', 'course_first'], required=True)
    parser.add_argument('--rendered', action='store_true')
    parser.add_argument('--producer-binding', type=Path)
    parser.add_argument('--reviewed-ui-continuation', type=Path)
    parser.add_argument('--attempt', default='')
    parser.add_argument('--automatic-output', default='consume-auto')
    parser.add_argument('--timeout-seconds', type=int, default=300)
    args = parser.parse_args()
    allowed_cases = ['funded', 'busy'] if args.chapter_profile == 'player_life' else ['fulfill', 'course_first']
    if args.case not in allowed_cases:
        raise SystemExit('Case does not belong to the selected chapter profile.')
    if args.chapter_profile == 'qingchuan_region' and args.phase == 'consume-manual':
        raise SystemExit('Regional recovery evidence uses one fresh-title consumer, not the old comparison matrix.')
    if args.reviewed_ui_continuation and (args.chapter_profile != 'qingchuan_region'
            or args.phase != 'consume-auto' or args.case != 'course_first'
            or args.producer_binding is None or not args.attempt):
        raise SystemExit('Reviewed UI continuation requires an explicit named regional course consumer and original producer binding.')
    driver_name = 'player_life_plan_main.gd' if args.chapter_profile == 'player_life' else 'qingchuan_region_main.gd'
    binding_path = args.binding.resolve(strict=True)
    binding = json.loads(binding_path.read_text())
    if (binding.get('native_artifact_status') != 'BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED'
            or len(binding.get('source_commit', '')) != 40
            or binding.get('source_commit') != binding.get('reviewed_source_commit')):
        raise SystemExit('An actual library bound to exact reviewed source is required.')
    if binding.get('chapter_profile', 'player_life') != args.chapter_profile:
        raise SystemExit('Bound chapter profile differs from the requested profile.')
    receipt = json.loads(Path(binding['native_build_receipt']['path']).read_text())
    if args.chapter_profile == 'qingchuan_region':
        from bootstrap_recovery_stage import validate_recovery_receipt
        validate_recovery_receipt(receipt, binding['native_source_commit'], binding['native_extension']['sha256'], binding['engine_sha256'])
    elif len(binding.get('native_source_commit', '')) != 40 or receipt.get('source_commit') != binding['native_source_commit'] or receipt.get('build_exit') != 0:
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
    if args.attempt and (not args.attempt.isidentifier() or args.phase == 'produce'):
        raise SystemExit('Only a named consumer attempt may use a separate output directory.')
    producer_binding_path = (args.producer_binding or binding_path).resolve(strict=True)
    producer_binding = json.loads(producer_binding_path.read_text())
    ui_continuation = None
    if args.reviewed_ui_continuation:
        ui_continuation = validate_ui_continuation(args.reviewed_ui_continuation.resolve(strict=True),
            producer_binding_path, binding_path, producer_binding, binding)
    if producer_binding_path != binding_path:
        for key in ([] if ui_continuation else ['source_files']) + [ 'native_production_files', 'native_source_commit', 'engine_sha256', 'cases', 'chapter_profile']:
            if producer_binding.get(key) != binding.get(key):
                raise SystemExit('Consumer helper change cannot substitute different product inputs: ' + key)
        if producer_binding['native_extension']['sha256'] != binding['native_extension']['sha256']:
            raise SystemExit('Consumer helper change cannot substitute another native artifact.')
    if Path(args.automatic_output).name != args.automatic_output or not args.automatic_output.startswith('consume-auto') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in args.automatic_output):
        raise SystemExit('Automatic comparison output must be a sibling directory.')
    case = args.case_root.resolve()
    if args.phase == 'produce':
        case.mkdir(parents=True, exist_ok=False)
    else:
        previous = case / 'producer'
        if args.phase == 'consume-manual':
            automatic = json.loads((case / args.automatic_output / 'process.json').read_text())
            if automatic.get('finished_unix', 0) <= 0 or automatic['finished_unix'] >= time.time():
                raise SystemExit('The automatic consumer must already have terminated; its failure does not skip the independent manual path.')
        result = json.loads((previous / 'result.json').read_text())
        process = json.loads((previous / 'process.json').read_text())
        manifest = json.loads((previous / 'save-manifest.json').read_text())
        if not result.get('ok') or process.get('ok') is not True or process.get('exit_code') != 0 or process.get('finished_unix', 0) <= 0 or process['finished_unix'] >= time.time():
            raise SystemExit('Original producer must already have terminated successfully.')
        if manifest.get('chapter_profile', 'player_life') != args.chapter_profile:
            raise SystemExit('The physical bank belongs to another chapter profile.')
        if manifest['case'] != args.case:
            raise SystemExit('The original physical bank belongs to another action case.')
        if process['binding_sha256'] != digest(producer_binding_path) or manifest['binding_sha256'] != digest(producer_binding_path):
            raise SystemExit('Producer and consumer bindings differ.')
        for name, value in process['artifacts'].items():
            if digest(previous / name) != value:
                raise SystemExit('Original producer evidence changed: ' + name)
        directory = Path(manifest['directory']).resolve(strict=True)
        if not directory.is_relative_to(case) or directory.parts[-3:] != ('profiles', manifest['profile'], 'saves'):
            raise SystemExit('Physical bank is outside its owned profile.')
        if {path.name: digest(path) for path in directory.iterdir() if path.is_file()} != manifest['banks']:
            raise SystemExit('Original physical banks changed.')
    output = case / (('producer' if args.phase == 'produce' else args.phase) + ('-' + args.attempt if args.attempt else ''))
    output.mkdir(exist_ok=False)
    if ui_continuation:
        write(output / 'reviewed-ui-continuation.json', ui_continuation)
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
                binding['drivers'][driver_name]['path'], '--',
                '--native-resident', '--chapter-profile=' + args.chapter_profile, '--binding=' + str(binding_path), '--case-root=' + str(case), '--phase=' + args.phase, '--case=' + args.case, '--attempt=' + args.attempt, '--automatic-output=' + args.automatic_output]
    started = time.time()
    write(output / 'launch.json', {'started_unix': started, 'phase': args.phase, 'command': command,
                                   'binding_sha256': digest(binding_path), 'producer_binding_sha256': digest(producer_binding_path),
                                   'reviewed_ui_continuation': ui_continuation, 'xdg': xdg})
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
                                   'binding_sha256': digest(binding_path), 'producer_binding_sha256': digest(producer_binding_path),
                                   'reviewed_ui_continuation': ui_continuation, 'xdg': xdg,
                                   'artifacts': {path.name: digest(path) for path in output.iterdir() if path.is_file()}})
    print(json.dumps({'phase': args.phase, 'ok': passed, 'output': str(output)}))
    raise SystemExit(0 if passed else code or 1)


if __name__ == '__main__':
    main()
