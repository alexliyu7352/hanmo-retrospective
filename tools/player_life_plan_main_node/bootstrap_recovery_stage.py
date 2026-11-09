"""仅初始化空的青川恢复stage；原stage和原银行失落时不伪造历史绑定或导入缓存。"""
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone



def validate_recovery_receipt(receipt, native_source, library_sha256, engine_sha256):
    """新恢复构建使用其真实回执结构，逐项核终态和字节，不补写旧回执字段。"""
    if (receipt.get('format') != 'HANMO-RECOVERY-NATIVE-BUILD/1'
            or receipt.get('status') != 'BUILD_COMPLETE_RUNTIME_NOT_RUN'
            or receipt.get('source_commit') != native_source
            or receipt.get('source_commit_after') != native_source
            or receipt.get('source_status_after') != ''
            or receipt.get('new_recovery_build') is not True
            or receipt.get('historical_artifacts_recreated') is not False
            or receipt.get('dependencies_unchanged_during_build') is not True
            or receipt.get('engine', {}).get('sha256') != engine_sha256
            or receipt.get('configuration', {}).get('test_faults') is not False):
        raise ValueError('Exact successful recovery build-source and engine identity required.')
    stages = receipt.get('stages', [])
    names = {stage.get('name') for stage in stages}
    if (not {'configure', 'build-hanmo_resident_journey', 'build-hanmo_world_bridge'} <= names
            or any(stage.get('exit_code') != 0 or not stage.get('completed_at') for stage in stages)):
        raise ValueError('Recovery configure and native build stages must have completed successfully.')
    completed = datetime.fromisoformat(receipt['completed_at'])
    if completed.tzinfo is None or completed >= datetime.now(timezone.utc):
        raise ValueError('Recovery build must already have terminated.')
    libraries = [item for item in receipt.get('artifacts', [])
                 if Path(item.get('path', '')).name == 'libhanmo_world_bridge.so']
    if len(libraries) != 1 or libraries[0].get('sha256') != library_sha256:
        raise ValueError('Native extension bytes differ from the actual recovery build artifact.')

def initialize(args, prepared, source, native_source, helper_source, native_files,
               native_record, receipt_path, sha, record, frozen_bytes):
    """验证受审源码、原生和正式代后一次落盘；失败保留新目录，不覆盖或自动重试。"""
    repo = args.repo.resolve(strict=True)
    stage = Path(prepared['stage_game']).resolve(strict=True)
    generation = Path(prepared['install_generation']).resolve(strict=True)
    output = args.output.resolve()
    if any(stage.iterdir()) or output.exists():
        raise ValueError('Fresh stage and new evidence output are required.')
    sys.path.insert(0, str(repo))
    from tools import content_stage as staging
    staging.installed_index_records(generation / 'data/generated')
    index = json.loads((generation / 'data/generated/installed-content.json').read_text())
    locks = {entry['pack_set_digest'] for entry in index['sets']}
    for case in prepared['cases'].values():
        if case['pack_set_digest'] not in locks:
            raise ValueError('Prepared case does not have its exact installed set.')
    for entry in index['sets']:
        header = json.loads((generation / 'data/generated' / entry['directory'] / 'catalog.hmcatalog.json').read_text())
        for path, key in [('schemas/content.schema.json', 'schema_sha256'),
                          ('game/data/rule_capabilities.json', 'rule_capabilities_sha256')]:
            if sha(frozen_bytes(repo, source, path)) != header[key]:
                raise ValueError('Frozen source and composed descriptor differ: ' + path)
    plan = {}
    origins = {}

    def queue(relative, raw, origin):
        """所有候选在首次写盘前核路径与冲突，禁止链接和stage外目标。"""
        relative = Path(relative).as_posix()
        if relative.startswith('/') or '..' in Path(relative).parts:
            raise ValueError('Unsafe stage path: ' + relative)
        target = stage / relative
        if target.is_symlink() or any(parent.is_symlink() for parent in target.parents):
            raise ValueError('Linked stage path: ' + relative)
        if relative in plan and plan[relative] != raw:
            raise ValueError('Conflicting prepared stage bytes: ' + relative)
        plan[relative] = raw
        origins[relative] = origin

    source_paths = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', source,
                                            'game/core', 'game/ui', 'game/storage', 'game/addons'], text=True).splitlines()
    source_paths = [path for path in source_paths if Path(path).suffix in ['.gd', '.cfg']]
    source_paths += ['game/main.tscn', 'game/project.godot', 'game/data/module_contracts.json',
                     'game/data/rule_capabilities.json', 'game/data/pack_runtime.json',
                     'game/data/presentation.schema.json', 'game/data/accepted_adult_appearances.json',
                     'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav',
                     'game/assets/audio/confirm.wav']
    source_files = {}
    for path in source_paths:
        raw = frozen_bytes(repo, source, path)
        source_files[path] = sha(raw)
        queue(path.removeprefix('game/'), raw, 'git:' + source + ':' + path)
    helper_names = ['qingchuan_region_main.gd', 'run_node.py', 'prepare_inputs.py', 'bootstrap_recovery_stage.py']
    helpers = {}
    for name in helper_names:
        helpers[name] = frozen_bytes(repo, helper_source, 'tools/player_life_plan_main_node/' + name)
    helpers['../activity_continuation_main_node/activity_continuation_main.gd'] = frozen_bytes(
        repo, helper_source, 'tools/activity_continuation_main_node/activity_continuation_main.gd')
    game_helpers = {}
    for name in ['test_dynamic_world_player_flow.gd', 'test_regional_life_player_flow.gd',
                 'test_study_segments_player_flow.gd', 'test_qingchuan_region_player_flow.gd']:
        path = 'game/tests/' + name
        raw = frozen_bytes(repo, helper_source, path)
        game_helpers[name] = path.removeprefix('game/')
        queue(game_helpers[name], raw, 'helper_git:' + helper_source + ':' + path)
    for path in sorted(generation.rglob('*')):
        if path.is_file():
            record(path)
            queue(path.relative_to(generation).as_posix(), path.read_bytes(), 'formal_recovery_install_generation')
    template = frozen_bytes(repo, source, 'native/godot/hanmo_world.gdextension.in')
    descriptor = template.replace(b'@HANMO_BRIDGE_LIBRARY@', b'res://native_resident_bridge/libhanmo_world_bridge.so')
    if b'@' in descriptor:
        raise ValueError('Unresolved native descriptor template.')
    queue('native_resident_bridge/hanmo_world_bridge.gdextension', descriptor, 'frozen_native_descriptor_template')
    queue('native_resident_bridge/libhanmo_world_bridge.so', Path(native_record['path']).read_bytes(), 'terminal_native_build:' + native_record['sha256'])
    queue('.godot/extension_list.cfg', b'res://native_resident_bridge/hanmo_world_bridge.gdextension\n', 'new_recovery_registration')
    output.mkdir(parents=True, exist_ok=False)
    (output / 'initialization-plan.json').write_text(json.dumps({
        'evidence_origin': 'NEW_RECOVERY_RUN_NOT_ORIGINAL_RESTORED_PROOF',
        'source_commit': source, 'native_source_commit': native_source, 'helper_commit': helper_source,
        'stage_game': str(stage), 'previous_files': {}, 'previous_bindings': [],
        'planned_files': {name: {'sha256': sha(raw), 'origin': origins[name]} for name, raw in sorted(plan.items())}
    }, ensure_ascii=False, indent=2) + '\n')
    for relative, raw in sorted(plan.items()):
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        if record(target)['sha256'] != sha(raw):
            raise ValueError('Fresh stage write verification failed: ' + relative)
    for name, raw in helpers.items():
        path = output / 'helpers' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    # 固定Godot只执行本地资源导入和脚本扫描，不运行Main、创建世界或提出GUI验收。
    command = [prepared['engine'], '--headless', '--audio-driver', 'Dummy', '--editor', '--import', '--path', str(stage), '--quit']
    started = time.time()
    reply = subprocess.run(command, capture_output=True, text=True, env=dict(os.environ))
    (output / 'fresh-import.stdout.log').write_text(reply.stdout)
    (output / 'fresh-import.stderr.log').write_text(reply.stderr)
    import_ok = reply.returncode == 0 and 'SCRIPT ERROR' not in reply.stdout + reply.stderr and 'ERROR:' not in reply.stdout + reply.stderr
    (output / 'fresh-import-process.json').write_text(json.dumps({'command': command, 'exit_code': reply.returncode,
        'ok': import_ok, 'started_unix': started, 'finished_unix': time.time(),
        'cache_origin': 'NEW_PINNED_ENGINE_IMPORT_NOT_RECOVERED_ORIGINAL_CACHE'}, indent=2) + '\n')
    if not import_ok:
        raise ValueError('New pinned-engine resource import failed; preserve raw logs and stage.')
    imported_paths = {'.godot/global_script_class_cache.cfg', '.godot/extension_list.cfg'}
    for asset in [name for name in plan if Path(name).suffix in ['.png', '.wav']]:
        sidecar = stage / (asset + '.import')
        record(sidecar)
        targets = set(re.findall(r'res://(\.godot/imported/[^"\n]+)', sidecar.read_text()))
        if not targets:
            raise ValueError('New resource import has no concrete cache target: ' + asset)
        imported_paths.add(asset + '.import')
        imported_paths.update(targets)
    for relative, raw in plan.items():
        if record(stage / relative)['sha256'] != sha(raw):
            raise ValueError('Import changed frozen source or resource: ' + relative)
    staging.installed_index_records(stage / 'data/generated')
    (output / 'native-build-receipt.json').write_bytes(receipt_path.read_bytes())
    final = dict(prepared, source_commit=source, reviewed_source_commit=source,
                 source_status='FROZEN_COMMIT_BYTES', native_source_commit=native_source, helper_commit=helper_source,
                 source_files=source_files, native_production_files=native_files,
                 previous_files={}, previous_bindings=[], source_overlay=[],
                 preparation=record(args.preparation.resolve(strict=True)),
                 drivers={name: record(output / 'helpers' / name) for name in helpers},
                 native_extension=record(stage / 'native_resident_bridge/libhanmo_world_bridge.so'),
                 native_build_receipt=record(output / 'native-build-receipt.json'),
                 registration=record(stage / '.godot/extension_list.cfg'),
                 descriptor=record(stage / 'native_resident_bridge/hanmo_world_bridge.gdextension'),
                 imported_resources=[record(stage / path) for path in sorted(imported_paths)],
                 import_origins=[], import_evidence=record(output / 'fresh-import-process.json'),
                 inputs=[record(path) for path in sorted(stage.rglob('*')) if path.is_file() and '.godot' not in path.parts],
                 native_artifact_status='BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED', gameplay_executed=False,
                 evidence_origin='NEW_RECOVERY_RUN_NOT_ORIGINAL_RESTORED_PROOF')
    final['drivers'].update({name: record(stage / path) for name, path in game_helpers.items()})
    final_path = output / 'runtime-stage-binding.json'
    final_path.write_text(json.dumps(final, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': final['native_artifact_status'], 'binding': str(final_path),
                     'source_commit': source, 'native_source_commit': native_source,
                     'helper_commit': helper_source, 'gameplay_executed': False,
                     'evidence_origin': final['evidence_origin']}))
