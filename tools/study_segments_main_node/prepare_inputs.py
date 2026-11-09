"""准备原青川1.6课程的唯一内容安装代；只读已有stage，不创建世界或运行玩法。"""
from pathlib import Path
import argparse
import json
import subprocess
import sys


def main():
    """当前Schema与原能力身份正式compose，记录真实资源与helper；缺受审原生库时明确禁止执行。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['repo', 'output', 'stage-game', 'engine']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(repo))
    from tools import content_stage as staging
    from tools.content.io import canonical, read_pack
    from tools.membership_main_node.prepare_inputs import materialize_pinned_resources, record, digest
    pack = repo / 'content/packs/authored/qingchuan_study_1_6'
    selection = repo / 'content/packs/selections/qingchuan-study-1.6.0.json'
    recovered = materialize_pinned_resources(pack)
    composed = target / 'compose'
    descriptors = [record(repo / 'schemas/content.schema.json'), record(repo / staging.CAPABILITY)]
    command = [sys.executable, str(repo / 'tools/content_pipeline.py'), 'compose', '--packs', str(pack),
               '--selection', str(selection), '--output', str(composed)]
    reply = subprocess.run(command, text=True, capture_output=True)
    (target / 'compose.stdout.json').write_text(reply.stdout)
    (target / 'compose.stderr.log').write_text(reply.stderr)
    if reply.returncode:
        raise SystemExit(reply.returncode)
    header = read_pack(composed / 'catalog.hmcatalog.json')
    generation = target / 'install-generation'
    generated = generation / 'data/generated'
    generated.mkdir(parents=True)
    staging.write_schema_index(repo, generated)
    (generated / 'content.schema.json').write_bytes((repo / 'schemas/content.schema.json').read_bytes())
    (generated / 'runtime_capabilities.json').write_bytes(canonical(staging.runtime_declaration(staging.load_contract(repo / staging.CAPABILITY), path=repo / staging.CAPABILITY)))
    staging.stage_installed_sets(repo, generated, target / 'no-old-generation', [composed], foundation_sets=False)
    staging.installed_index_records(generated)
    installed_record = staging.composed_set_record(composed, repo)
    installed = generated / installed_record['directory']
    (generation / 'data/presentation.json').write_bytes((installed / 'presentation.json').read_bytes())
    for name, path in staging.files_in(installed).items():
        if name.startswith('assets/'):
            destination = generation / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(path.read_bytes())
    if any(digest(Path(item['path'])) != item['sha256'] for item in descriptors):
        raise SystemExit('Descriptors changed during composition; preserve this input and prepare a new generation.')
    scenario = 'qingstudy::scenario.first_term'
    actor = 'qingstudy::actor.apprentice'
    if header['catalog']['scenarios'][scenario]['player_actor'] != actor:
        raise SystemExit('Original author player binding changed.')
    source = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    runtime = Path(__file__).resolve().parent
    binding = {'format': 'HM-STUDY-SEGMENTS-PREPARED/1', 'source_commit': source,
               'source_status': subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True),
               'reviewed_source_commit': None, 'stage_game': str(args.stage_game.resolve(strict=True)),
               'engine': str(args.engine.resolve(strict=True)), 'engine_sha256': digest(args.engine),
               'pack_set_digest': header['pack_set_digest'],
               'catalog_path': 'res://data/generated/' + installed_record['directory'] + '/catalog.hmcatalog.json',
               'ids': {'scenario_id': scenario, 'player_actor_id': actor, 'pack_set_digest': header['pack_set_digest']},
               'install_generation': str(generation), 'recovered_pinned_resources': recovered,
               'inputs': descriptors + [record(path) for path in sorted(generation.rglob('*')) if path.is_file()],
               'drivers': {name: record(runtime / name) for name in ['study_segments_main.gd', 'run_node.py', 'prepare_inputs.py']},
               'native_artifact_status': 'MISSING_REVIEWED_ARTIFACT', 'gameplay_executed': False}
    binding['drivers']['test_study_segments_player_flow.gd'] = record(repo / 'game/tests/test_study_segments_player_flow.gd')
    (target / 'runtime-source-binding.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'PREPARED_UNBOUND', 'binding': str(target / 'runtime-source-binding.json'),
                      'pack_set_digest': header['pack_set_digest'], 'gameplay_executed': False}))


if __name__ == '__main__':
    main()
