"""仅在中央明确冻结后更新唯一既存Main stage；绑定当前源码、双内容锁、原导入和真实新库。"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys


def sha(raw):
    """对已读取原字节取摘要，不把文件名或旧回执视为同一产物。"""
    return hashlib.sha256(raw).hexdigest()


def record(path):
    """源和结果须为真实普通文件；拒绝经软链接读写另一个stage。"""
    if path.is_symlink() or not path.is_file() or any(parent.is_symlink() for parent in path.parents):
        raise ValueError('Expected regular file without linked parents: ' + str(path))
    return {'path': str(path.resolve()), 'sha256': sha(path.read_bytes())}


def frozen_bytes(repo, commit, relative):
    """所有产品和helper只从完整受审commit取得，不能混入工作区尚未提交修改。"""
    return subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':' + relative])


def main():
    """先验证完整写入计划和来源，再保全原件、覆盖一次并重核；不运行Godot或任何玩法。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['repo', 'preparation', 'output', 'native-extension', 'native-build-receipt']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--native-source-commit', required=True)
    parser.add_argument('--helper-commit')
    parser.add_argument('--prior-binding', type=Path, action='append', default=[])
    parser.add_argument('--fresh-recovery-stage', action='store_true')
    parser.add_argument('--prior-resource-game', type=Path, action='append', default=[])
    parser.add_argument('--execute-reviewed-freeze', action='store_true')
    args = parser.parse_args()
    if not args.execute_reviewed_freeze:
        raise SystemExit('This tool writes the shared stage. Wait for the parent freeze, then pass --execute-reviewed-freeze explicitly.')
    repo = args.repo.resolve(strict=True)
    source = args.source_commit
    native_source = args.native_source_commit
    helper_source = args.helper_commit or source
    if len(helper_source) != 40 or subprocess.check_output(['git', '-C', str(repo), 'rev-parse', helper_source + '^{commit}'], text=True).strip() != helper_source:
        raise ValueError('Exact helper commit required.')
    if len(native_source) != 40 or subprocess.check_output(['git', '-C', str(repo), 'rev-parse', native_source + '^{commit}'], text=True).strip() != native_source:
        raise ValueError('Exact native build source commit required.')
    if len(source) != 40 or subprocess.check_output(['git', '-C', str(repo), 'rev-parse', source + '^{commit}'], text=True).strip() != source:
        raise ValueError('Exact reviewed commit required.')
    preparation = args.preparation.resolve(strict=True)
    prepared = json.loads(preparation.read_text())
    profiles = {'HM-ACTIVITY-CONTINUATION-PREPARED/1': ('activity_continuation_main_node', ['activity_continuation_main.gd', 'run_node.py', 'run_batch.py', 'prepare_inputs.py', 'overlay_stage.py']),
                'HM-PLAYER-LIFE-PLAN-PREPARED/1': ('player_life_plan_main_node', ['player_life_plan_main.gd', 'run_node.py', 'prepare_inputs.py']),
                'HM-QINGCHUAN-REGION-PREPARED/1': ('player_life_plan_main_node', ['qingchuan_region_main.gd', 'run_node.py', 'prepare_inputs.py', 'bootstrap_recovery_stage.py'])}
    if prepared.get('format') not in profiles:
        raise ValueError('Wrong preparation contract.')
    helper_directory, helper_names = profiles[prepared['format']]
    regional = prepared['format'] == 'HM-QINGCHUAN-REGION-PREPARED/1'
    if regional and prepared.get('chapter_profile') != 'qingchuan_region':
        raise ValueError('Regional preparation requires its explicit chapter profile.')
    stage = Path(prepared['stage_game']).resolve(strict=True)
    if args.fresh_recovery_stage:
        if prepared.get('format') != 'HM-QINGCHUAN-REGION-PREPARED/1' or prepared.get('chapter_profile') != 'qingchuan_region':
            raise ValueError('Fresh recovery initialization is limited to the explicit regional profile.')
        if stage != Path('<本地路径>') or any(stage.iterdir()):
            raise ValueError('The assigned fresh recovery stage must be empty.')
        if args.prior_binding or args.prior_resource_game:
            raise ValueError('Fresh recovery has no original stage, imports, or prior binding to claim.')
    else:
        if not args.prior_binding:
            raise ValueError('Existing-stage overlays require the original prior bindings.')
        assigned_stage = Path('<本地路径>') if regional else Path('<本地路径>')
        if stage != assigned_stage:
            raise ValueError('Only the already assigned unique Main stage is allowed.')
        if not (stage / 'main.tscn').is_file() or not (stage / '.godot/global_script_class_cache.cfg').is_file():
            raise ValueError('Existing actual Main and verified class cache are required.')
    if sha(Path(prepared['engine']).read_bytes()) != prepared['engine_sha256']:
        raise ValueError('Pinned engine changed.')
    generation = Path(prepared['install_generation']).resolve(strict=True)
    for item in prepared['inputs']:
        if record(Path(item['path'])) != item:
            raise ValueError('Prepared input changed: ' + item['path'])
    library = args.native_extension.resolve(strict=True)
    receipt_path = args.native_build_receipt.resolve(strict=True)
    receipt = json.loads(receipt_path.read_text())
    if regional:
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'player_life_plan_main_node'))
        from bootstrap_recovery_stage import validate_recovery_receipt
        validate_recovery_receipt(receipt, native_source, record(library)['sha256'], prepared['engine_sha256'])
    elif receipt.get('state') != 'terminal' or receipt.get('configure_exit') != 0 or receipt.get('build_exit') != 0 or receipt.get('source_commit') != native_source or receipt.get('source_commit_after') != native_source or not receipt.get('source_clean') or receipt.get('finished_unix', 0) <= 0:
        raise ValueError('Successful terminal build receipt for the exact clean source is required.')
    native_production = {}
    for commit in [native_source, source]:
        names = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', commit, 'native'], text=True).splitlines()
        names = [name for name in names if '/tests/' not in name and '/benchmarks/' not in name and (Path(name).suffix in ['.cpp', '.hpp', '.h', '.cmake', '.in', '.json'] or Path(name).name == 'CMakeLists.txt')]
        native_production[commit] = {name: sha(frozen_bytes(repo, commit, name)) for name in names}
    if native_production[native_source] != native_production[source]:
        raise ValueError('Frontend commit contains native production changes after the library freeze.')
    native_record = record(library)
    if not regional and receipt.get('artifacts', {}).get(str(library), {}).get('sha256') != native_record['sha256']:
        raise ValueError('Actual library differs from its terminal build receipt.')
    if args.fresh_recovery_stage:
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'player_life_plan_main_node'))
        from bootstrap_recovery_stage import initialize
        initialize(args, prepared, source, native_source, helper_source, native_production[native_source],
                   native_record, receipt_path, sha, record, frozen_bytes)
        return
    registration = stage / '.godot/extension_list.cfg'
    if registration.read_text().strip() != 'res://native_resident_bridge/hanmo_world_bridge.gdextension':
        raise ValueError('Unexpected existing extension registration.')
    descriptor = stage / 'native_resident_bridge/hanmo_world_bridge.gdextension'
    libraries = set(re.findall(r'"res://([^"\n]+\.so)"', descriptor.read_text()))
    if libraries != {'native_resident_bridge/libhanmo_world_bridge.so'}:
        raise ValueError('Unexpected existing native library target.')

    sys.path.insert(0, str(repo))
    from tools import content_stage as staging
    from tools.content.io import canonical
    staging.installed_index_records(generation / 'data/generated')
    staging.installed_index_records(stage / 'data/generated')
    current_index = json.loads((stage / 'data/generated/installed-content.json').read_text())
    next_index = json.loads((generation / 'data/generated/installed-content.json').read_text())
    entries = {row['pack_set_digest']: row for row in current_index['sets']}
    for entry in next_index['sets']:
        if entry['pack_set_digest'] in entries and entries[entry['pack_set_digest']] != entry:
            raise ValueError('Identical installed lock has different source bytes.')
        header = json.loads((generation / 'data/generated' / entry['directory'] / 'catalog.hmcatalog.json').read_text())
        for name, field in [('schemas/content.schema.json', 'schema_sha256'), ('game/data/rule_capabilities.json', 'rule_capabilities_sha256')]:
            if sha(frozen_bytes(repo, source, name)) != header[field]:
                raise ValueError('Formal current catalog differs from final frozen descriptor: ' + name)
        entries[entry['pack_set_digest']] = entry
    merged = dict(next_index, sets=[entries[key] for key in sorted(entries)])
    for case, selected in prepared['cases'].items():
        if selected['pack_set_digest'] not in entries:
            raise ValueError('Missing exact selected content lock: ' + case)

    plan = {}
    origins = {}

    def queue(relative, raw, origin):
        """先合并所有写入并检查路径/冲突，完整计划合法以前不碰stage。"""
        relative = Path(relative).as_posix()
        if relative.startswith('/') or '..' in Path(relative).parts:
            raise ValueError('Unsafe stage path.')
        target = stage / relative
        if target.is_symlink() or any(parent.is_symlink() for parent in target.parents):
            raise ValueError('Linked stage path: ' + relative)
        if relative in plan and plan[relative] != raw:
            raise ValueError('Conflicting proposed bytes: ' + relative)
        plan[relative] = raw
        origins[relative] = origin

    paths = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', source,
                                     'game/core', 'game/ui', 'game/storage'], text=True).splitlines()
    product_paths = [name for name in paths if name.endswith('.gd')]
    extra_paths = ['game/main.tscn', 'game/tests/test_dynamic_world_player_flow.gd',
                   'game/tests/test_regional_life_player_flow.gd', 'game/tests/test_study_segments_player_flow.gd',
                   'game/data/module_contracts.json', 'game/data/rule_capabilities.json',
                   'game/data/pack_runtime.json', 'game/data/presentation.schema.json']
    regional_game_helpers = {}
    if regional:
        # 新恢复stage与后续覆盖保持同一产品身份集合；测试助手独立取精确helper commit。
        extra_paths = [name for name in extra_paths if not name.startswith('game/tests/')]
        extra_paths += ['game/project.godot', 'game/data/accepted_adult_appearances.json',
                        'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav', 'game/assets/audio/confirm.wav']
        addons = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', source, 'game/addons'], text=True).splitlines()
        extra_paths += [name for name in addons if Path(name).suffix in ['.gd', '.cfg']]
        for name in ['test_dynamic_world_player_flow.gd', 'test_regional_life_player_flow.gd',
                     'test_study_segments_player_flow.gd', 'test_qingchuan_region_player_flow.gd']:
            path = 'game/tests/' + name
            regional_game_helpers[name] = path.removeprefix('game/')
            queue(regional_game_helpers[name], frozen_bytes(repo, helper_source, path), 'helper_git:' + helper_source + ':' + path)
    source_files = {}
    for name in product_paths + extra_paths:
        raw = frozen_bytes(repo, source, name)
        source_files[name] = sha(raw)
        queue(name.removeprefix('game/'), raw, 'git:' + source + ':' + name)
    current_product = {str(path.relative_to(stage)) for directory in ['core', 'ui', 'storage'] for path in (stage / directory).rglob('*.gd')}
    extras = current_product - {name.removeprefix('game/') for name in product_paths}
    if extras:
        raise ValueError('Unexpected stale product scripts require review, not silent deletion: ' + str(sorted(extras)))
    for path in sorted(generation.rglob('*')):
        if path.is_file():
            relative = path.relative_to(generation).as_posix()
            if relative != 'data/generated/installed-content.json':
                queue(relative, path.read_bytes(), 'formal_current_install_generation')
    queue('data/generated/installed-content.json', canonical(merged), 'preserved_existing_and_exact_current_locks')
    queue('native_resident_bridge/libhanmo_world_bridge.so', library.read_bytes(), 'terminal_native_build:' + native_record['sha256'])

    resource_roots = [stage] + [path.resolve(strict=True) for path in args.prior_resource_game]
    import_origins = []
    imported_paths = {'.godot/global_script_class_cache.cfg', '.godot/extension_list.cfg'}
    for asset in sorted((generation / 'assets').rglob('*.png')):
        relative = asset.relative_to(generation).as_posix()
        candidates = [root / relative for root in resource_roots]
        prior = next((path for path in candidates if path.is_file() and sha(path.read_bytes()) == sha(asset.read_bytes()) and Path(str(path) + '.import').is_file()), None)
        if prior is None:
            raise ValueError('No byte-identical original import source: ' + relative)
        origin_root = next(root for root in resource_roots if root / relative == prior)
        sidecar = Path(str(prior) + '.import')
        targets = set(re.findall(r'res://(\.godot/imported/[^"\n]+)', sidecar.read_text()))
        if not targets:
            raise ValueError('Original sidecar has no imported target: ' + relative)
        queue(relative + '.import', sidecar.read_bytes(), 'verified_original_import:' + str(sidecar))
        imported_paths.add(relative + '.import')
        import_origins.extend([record(prior), record(sidecar)])
        for target in targets:
            original = origin_root / target
            queue(target, original.read_bytes(), 'verified_original_cache:' + str(original))
            imported_paths.add(target)
            import_origins.append(record(original))
    old_files = {str(path.relative_to(stage)): record(path) for path in sorted(stage.rglob('*')) if path.is_file()}
    prior_bindings = [record(path.resolve(strict=True)) for path in args.prior_binding]
    helpers = {}
    for name in helper_names:
        helpers[name] = frozen_bytes(repo, helper_source, 'tools/' + helper_directory + '/' + name)
    if helper_directory == 'player_life_plan_main_node':
        helpers['../activity_continuation_main_node/activity_continuation_main.gd'] = frozen_bytes(repo, helper_source, 'tools/activity_continuation_main_node/activity_continuation_main.gd')

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    backup = output / 'stage-before'
    (output / 'overlay-plan.json').write_text(json.dumps({'source_commit': source, 'native_source_commit': native_source, 'stage_game': str(stage), 'source_files': source_files, 'previous_files': old_files, 'planned_files': {name: {'sha256': sha(raw), 'origin': origins[name]} for name, raw in sorted(plan.items())}}, ensure_ascii=False, indent=2) + '\n')
    changes = []
    for relative, raw in sorted(plan.items()):
        target = stage / relative
        before = sha(target.read_bytes()) if target.is_file() else None
        after = sha(raw)
        if before == after:
            continue
        if before is not None:
            saved = backup / relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(target.read_bytes())
            if sha(saved.read_bytes()) != before:
                raise ValueError('Unable to preserve previous stage file: ' + relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        if sha(target.read_bytes()) != after:
            raise ValueError('Stage write did not verify: ' + relative)
        change = {'relative': relative, 'before': before, 'after': after, 'origin': origins[relative]}
        changes.append(change)
        with (output / 'overlay-progress.jsonl').open('a') as progress:
            progress.write(json.dumps(change, ensure_ascii=False) + '\n')
    for name, expected in source_files.items():
        if sha((stage / name.removeprefix('game/')).read_bytes()) != expected:
            raise ValueError('Final product source mismatch: ' + name)
    changed = {item['relative'] for item in changes}
    for relative, original in old_files.items():
        if relative not in changed and record(stage / relative) != original:
            raise ValueError('Unrelated original stage bytes changed: ' + relative)
    staging.installed_index_records(stage / 'data/generated')
    for name, raw in helpers.items():
        path = output / 'helpers' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    (output / 'native-build-receipt.json').write_bytes(receipt_path.read_bytes())
    for index, item in enumerate(prior_bindings):
        path = output / ('prior-binding-%d.json' % index)
        path.write_bytes(Path(item['path']).read_bytes())
        if sha(path.read_bytes()) != item['sha256']:
            raise ValueError('Previous binding changed during preservation.')
    final = dict(prepared, source_commit=source, reviewed_source_commit=source, source_status='FROZEN_COMMIT_BYTES',
                 native_source_commit=native_source, helper_commit=helper_source, native_production_files=native_production[native_source],
                 source_files=source_files, source_overlay=changes, previous_files=old_files,
                 previous_bindings=prior_bindings, preparation=record(preparation),
                 drivers={name: record(output / 'helpers' / name) for name in helpers},
                 native_extension=record(stage / 'native_resident_bridge/libhanmo_world_bridge.so'),
                 native_build_receipt=record(output / 'native-build-receipt.json'),
                 registration=record(registration), descriptor=record(descriptor),
                 imported_resources=[record(stage / name) for name in sorted(imported_paths)],
                 import_origins=import_origins,
                 inputs=[record(path) for path in sorted(stage.rglob('*')) if path.is_file() and '.godot' not in path.parts],
                 native_artifact_status='BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED', gameplay_executed=False)
    if regional:
        final['drivers'].update({name: record(stage / path) for name, path in regional_game_helpers.items()})
    final_path = output / 'runtime-stage-binding.json'
    final_path.write_text(json.dumps(final, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': final['native_artifact_status'], 'binding': str(final_path),
                      'source_commit': source, 'native_source_commit': native_source, 'product_scripts_verified': len(product_paths),
                      'changed_files': len(changes), 'preserved_installed_sets': len(current_index['sets']),
                      'gameplay_executed': False}))


if __name__ == '__main__':
    main()
