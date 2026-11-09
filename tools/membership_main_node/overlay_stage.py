"""在已授权的唯一既存stage覆盖精确源码与新安装代；逐字节保全被替换文件，旧银行和库不动。"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys


def digest(path):
    """摘要核验原始文件，防止旧stage来源在复制间改变。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(path):
    """所有来源均须普通文件，不能借链接写入另一工作区。"""
    if path.is_symlink() or not path.is_file():
        raise ValueError('Expected regular file: ' + str(path))
    return {'path': str(path), 'sha256': digest(path)}


def main():
    """先保全原绑定与全部替换前字节，再一次覆盖当前源；最终真实native绑定由中央另做。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--helper-commit')
    parser.add_argument('--preparation', type=Path, required=True)
    parser.add_argument('--prior-bindings', type=Path, required=True)
    parser.add_argument('--prior-resource-game', type=Path, required=True)
    parser.add_argument('--preserve-installed', action='store_true')
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    preparation = args.preparation.resolve(strict=True)
    source_binding = preparation / 'runtime-source-binding.json'
    binding = json.loads(source_binding.read_text())
    stage = Path(binding['stage_game']).resolve(strict=True)
    prior_resource = args.prior_resource_game.resolve(strict=True)
    source = args.source_commit
    if subprocess.check_output(['git', '-C', str(repo), 'rev-parse', source + '^{commit}'], text=True).strip() != source or len(source) != 40:
        raise SystemExit('Exact source commit required.')
    final_path = preparation / 'runtime-stage-binding.json'
    if final_path.exists():
        raise SystemExit('Stage was already prepared; preserve the existing result before a reviewed amendment.')
    backup = preparation / 'stage-before'
    backup.mkdir(exist_ok=False)
    old_bindings = preparation / 'prior-bindings'
    old_bindings.mkdir(exist_ok=False)
    preserved_bindings = []
    for name in ['runtime-binding.json', 'runtime-source-binding.json', 'runtime-stage-binding.json', 'prepared-stage-final.json', 'stage-result.json']:
        path = args.prior_bindings / name
        if path.is_file():
            target = old_bindings / name
            target.write_bytes(path.read_bytes())
            if digest(target) != digest(path):
                raise SystemExit('Prior binding changed during preservation.')
            preserved_bindings.append(record(target))
    if not preserved_bindings:
        raise SystemExit('No actual prior stage bindings found.')
    old_files = {str(path.relative_to(stage)): record(path) for path in sorted(stage.rglob('*'))
                 if path.is_file() and '.godot' not in path.parts}
    changes = []

    def put(relative, raw, origin):
        """每个被替换文件先保留原字节及SHA，新增文件也记录，不整树复制。"""
        target = stage / relative
        if target.is_symlink():
            raise ValueError('Refuse linked stage output: ' + str(target))
        after = hashlib.sha256(raw).hexdigest()
        before = digest(target) if target.is_file() else None
        if before == after:
            return
        if before is not None:
            saved = backup / relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(target.read_bytes())
            if digest(saved) != before:
                raise ValueError('Failed to preserve actual old stage bytes: ' + relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        if digest(target) != after:
            raise ValueError('Incomplete stage write: ' + relative)
        changes.append({'path': str(target), 'relative': relative, 'before': before, 'after': after, 'origin': origin})

    paths = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', source,
                                     'game/core', 'game/ui', 'game/storage'], text=True).splitlines()
    paths = [name for name in paths if name.endswith('.gd')]
    paths += ['game/main.tscn', 'game/tests/test_dynamic_world_player_flow.gd',
              'game/tests/test_regional_life_player_flow.gd', 'game/tests/test_named_membership_player_flow.gd',
              'game/data/module_contracts.json', 'game/data/rule_capabilities.json',
              'game/data/pack_runtime.json', 'game/data/presentation.schema.json']
    source_files = {}
    if 'test_study_segments_player_flow.gd' in binding.get('drivers', {}):
        paths.append('game/tests/test_study_segments_player_flow.gd')
    for name in paths:
        origin_commit = args.helper_commit if args.helper_commit and name == 'game/tests/test_named_membership_player_flow.gd' else source
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', origin_commit + ':' + name])
        source_files[name] = hashlib.sha256(raw).hexdigest()
        put(name.removeprefix('game/'), raw, 'git:' + origin_commit + ':' + name)
    generation = Path(binding['install_generation'])
    installed_catalog = next(path for path in generation.rglob('catalog.hmcatalog.json') if binding['pack_set_digest'] in str(path))
    header = json.loads(installed_catalog.read_text())
    for name, field in [('schemas/content.schema.json', 'schema_sha256'), ('game/data/rule_capabilities.json', 'rule_capabilities_sha256')]:
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', source + ':' + name])
        if hashlib.sha256(raw).hexdigest() != header[field]:
            raise SystemExit('Formal composition differs from frozen source descriptor: ' + name)
    # 同一能力描述下的既存精确安装代继续可寻址；不能重编旧锁或以新目录代替旧银行来源。
    retained_index = None
    if args.preserve_installed:
        sys.path.insert(0, str(repo))
        from tools import content_stage as staging
        previous = stage / 'data/generated'
        staging.installed_index_records(previous)
        old_index = json.loads((previous / staging.INSTALLED_INDEX).read_text())
        new_index = json.loads((generation / 'data/generated' / staging.INSTALLED_INDEX).read_text())
        entries = {row['pack_set_digest']: row for row in old_index['sets']}
        for entry in old_index['sets']:
            lock = json.loads((previous / entry['directory'] / 'pack-set.lock.json').read_text())
            if lock['rule_capabilities_sha256'] != header['rule_capabilities_sha256']:
                raise SystemExit('Existing installed policy identity differs; preserve the old stage and refuse overlay.')
        for entry in new_index['sets']:
            key = entry['pack_set_digest']
            if key in entries and entries[key] != entry:
                raise SystemExit('Same installed lock has different source bytes.')
            entries[key] = entry
        retained_index = dict(new_index, sets=[entries[key] for key in sorted(entries)])
    for path in sorted(generation.rglob('*')):
        if path.is_file():
            if retained_index is not None and path.relative_to(generation).as_posix() == 'data/generated/installed-content.json':
                continue
            put(str(path.relative_to(generation)), path.read_bytes(), 'formal_install_generation')
    if retained_index is not None:
        from tools.content.io import canonical
        put('data/generated/installed-content.json', canonical(retained_index), 'exact_existing_and_new_installed_sets')
        staging.installed_index_records(stage / 'data/generated')
    sys.path.insert(0, str(repo))
    from tools.content import pack_preview
    project = pack_preview._project('named-membership-node', binding['catalog_path'], binding.get('ids', {}).get('scenario_id', 'society::scenario.society_membership_funded'))
    put('project.godot', project, 'official_preview_project_with_actual_main')
    # 原资源import只有相同逻辑路径、原PNG与固定引擎来源一致才重用，绝不编造缓存文件。
    imports = []
    for asset in sorted((generation / 'assets').rglob('*.png')):
        relative = asset.relative_to(generation)
        prior = prior_resource / relative
        side = Path(str(prior) + '.import')
        if not prior.is_file() or digest(prior) != digest(asset) or not side.is_file():
            raise SystemExit('Original exact import source unavailable: ' + str(relative))
        put(str(relative) + '.import', side.read_bytes(), 'same_path_original_import')
        cached = re.findall(r'res://(\.godot/imported/[^"\n]+)', side.read_text())
        if not cached:
            raise SystemExit('Import sidecar has no real cache destination: ' + str(side))
        for cache in sorted(set(cached)):
            original = prior_resource / cache
            put(cache, original.read_bytes(), 'same_path_original_import_cache')
            imports.append(record(stage / cache))
        imports.append(record(stage / (str(relative) + '.import')))
    changed_names = {row['relative'] for row in changes}
    for name, original in old_files.items():
        if name not in changed_names and digest(Path(original['path'])) != original['sha256']:
            raise SystemExit('Unrelated old stage bytes changed: ' + name)
    binding.update({'source_commit': source, 'reviewed_source_commit': None,
                    'helper_commit': args.helper_commit or source,
                    'source_files': source_files, 'source_overlay': changes,
                    'overlay_tool': record(Path(__file__).resolve()),
                    'previous_bindings': preserved_bindings, 'previous_files': old_files,
                    'native_artifact_status': 'MISSING_REVIEWED_ARTIFACT', 'gameplay_executed': False,
                    'inputs': [record(path) for path in sorted(stage.rglob('*')) if path.is_file() and '.godot' not in path.parts],
                    'imported_resources': imports + [record(stage / '.godot/global_script_class_cache.cfg')],
                    'preparation': record(source_binding)})
    final_path.write_text(json.dumps(binding, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'STAGE_PREPARED_UNBOUND', 'binding': str(final_path),
                      'source_commit': source, 'pack_set_digest': binding['pack_set_digest'],
                      'changed_files': len(changes), 'old_bindings_preserved': len(preserved_bindings), 'gameplay_executed': False}))


if __name__ == '__main__':
    main()
