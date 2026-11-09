"""正式准备原Alpha和青川1.6的有限继续入口；不安装世界、不运行Main、不改共享stage。"""
from pathlib import Path
import argparse
import json
import subprocess
import sys


def main():
    """当前原件通过原prepare/compose/staging，生成独立安装代与待最终库绑定的输入清单。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['repo', 'output', 'stage-game', 'engine']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(repo))
    from tools import content_stage as staging
    from tools.content.io import canonical, prepare_pack, read_pack
    from tools.content.packs import LEGACY_SOURCES
    from tools.membership_main_node.prepare_inputs import materialize_pinned_resources, record, digest

    pack = repo / 'content/packs/authored/qingchuan_study_1_6'
    recovered = materialize_pinned_resources(pack)
    composed = target / 'study-compose'
    command = [sys.executable, str(repo / 'tools/content_pipeline.py'), 'compose', '--packs', str(pack),
               '--selection', str(repo / 'content/packs/selections/qingchuan-study-1.6.0.json'), '--output', str(composed)]
    reply = subprocess.run(command, text=True, capture_output=True)
    (target / 'compose.stdout.json').write_text(reply.stdout)
    (target / 'compose.stderr.log').write_text(reply.stderr)
    if reply.returncode:
        raise SystemExit(reply.returncode)
    generation = target / 'install-generation'
    generated = generation / 'data/generated'
    generated.mkdir(parents=True)
    descriptors = [record(repo / 'schemas/content.schema.json'), record(repo / staging.CAPABILITY)]
    prepared = {}
    for name, source in LEGACY_SOURCES.items():
        prepared[name] = prepare_pack(source, generated / (name + '.hmcatalog.json'))
    (target / 'foundation-prepare.json').write_text(json.dumps(prepared, ensure_ascii=False, indent=2) + '\n')
    staging.write_schema_index(repo, generated)
    (generated / 'content.schema.json').write_bytes((repo / 'schemas/content.schema.json').read_bytes())
    (generated / 'runtime_capabilities.json').write_bytes(canonical(staging.runtime_declaration(staging.load_contract(repo / staging.CAPABILITY), path=repo / staging.CAPABILITY)))
    sources, _memberships = staging.stage_installed_sets(repo, generated, target / 'no-old-generation', [composed])
    staging.installed_index_records(generated)
    index = read_pack(generated / staging.INSTALLED_INDEX)
    alpha = None
    study = None
    for entry in index['sets']:
        installed = generated / entry['directory']
        header = read_pack(installed / 'catalog.hmcatalog.json')
        if 'scenario.alpha_life' in header['catalog']['scenarios']:
            alpha = (entry, header)
        if 'qingstudy::scenario.first_term' in header['catalog']['scenarios']:
            study = (entry, header)
        for name, path in staging.files_in(installed).items():
            if not name.startswith('assets/'):
                continue
            destination = generation / name
            if destination.exists() and destination.read_bytes() != path.read_bytes():
                raise ValueError('Installed resources conflict: ' + name)
            if not destination.exists():
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(path.read_bytes())
    if alpha is None or study is None:
        raise ValueError('Original Alpha and study selections must both be installed.')
    (generation / 'data/presentation.json').write_bytes((generated / alpha[0]['directory'] / 'presentation.json').read_bytes())
    if any(digest(Path(row['path'])) != row['sha256'] for row in descriptors):
        raise ValueError('Descriptors changed during preparation.')
    if any(path.read_bytes() != raw for path, raw in sources.items()):
        raise ValueError('Original sources changed during preparation.')

    alpha_ids = {'scenario_id': 'scenario.alpha_life', 'player_actor_id': 'actor.player',
                 'pack_set_digest': alpha[0]['pack_set_digest'], 'catalog_path': 'res://data/generated/' + alpha[0]['directory'] + '/catalog.hmcatalog.json'}
    study_ids = {'scenario_id': 'qingstudy::scenario.first_term', 'player_actor_id': 'qingstudy::actor.apprentice',
                 'pack_set_digest': study[0]['pack_set_digest'], 'catalog_path': 'res://data/generated/' + study[0]['directory'] + '/catalog.hmcatalog.json'}
    for selected, header in [(alpha_ids, alpha[1]), (study_ids, study[1])]:
        if header['catalog']['scenarios'][selected['scenario_id']]['player_actor'] != selected['player_actor_id']:
            raise ValueError('Original player binding changed.')
    source = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    runtime = Path(__file__).resolve().parent
    binding = {'format': 'HM-ACTIVITY-CONTINUATION-PREPARED/1', 'source_commit': source,
               'source_status': subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True),
               'reviewed_source_commit': None, 'stage_game': str(args.stage_game.resolve(strict=True)),
               'engine': str(args.engine.resolve(strict=True)), 'engine_sha256': digest(args.engine),
               'install_generation': str(generation), 'cases': {'travel': alpha_ids, 'rest': alpha_ids, 'study': study_ids},
               'recovered_pinned_resources': recovered,
               'inputs': descriptors + [record(path) for path in sorted(generation.rglob('*')) if path.is_file()],
               'drivers': {name: record(runtime / name) for name in ['activity_continuation_main.gd', 'run_node.py', 'prepare_inputs.py']},
               'native_artifact_status': 'MISSING_REVIEWED_ARTIFACT', 'gameplay_executed': False}
    (target / 'runtime-source-binding.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'PREPARED_UNBOUND', 'binding': str(target / 'runtime-source-binding.json'),
                      'cases': binding['cases'], 'gameplay_executed': False}))


if __name__ == '__main__':
    main()
