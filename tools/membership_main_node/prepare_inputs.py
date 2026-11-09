"""只准备一份正式任职目录和安装代；不复制game工区、不覆盖旧stage、不运行原生或Main。"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys


def digest(path):
    """摘要始终来自实际普通文件，不以路径或Git声明替代字节。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(path):
    """来源绑定拒绝链接，并保留精确路径及摘要供中央最终库冻结使用。"""
    if path.is_symlink() or not path.is_file():
        raise ValueError('Expected regular source file: ' + str(path))
    return {'path': str(path.resolve()), 'sha256': digest(path)}


def materialize_pinned_resources(pack):
    """稀疏checkout只从本仓库已有原blob恢复缺席资源；摘要不匹配即停，不覆盖现有文件。"""
    owner = Path(subprocess.check_output(['git', '-C', str(pack), 'rev-parse', '--show-toplevel'], text=True).strip())
    recovered = []
    for item in json.loads((pack / 'pack.json').read_text())['resources']:
        path = pack / item['path']
        if not path.is_file():
            relative = path.relative_to(owner).as_posix()
            raw = subprocess.check_output(['git', '-C', str(owner), 'show', 'HEAD:' + relative])
            if hashlib.sha256(raw).hexdigest() != item['sha256']:
                raise ValueError('Original Git resource disagrees with manifest: ' + relative)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            recovered.append(relative)
        if path.is_symlink() or digest(path) != item['sha256']:
            raise ValueError('Existing authored resource disagrees with manifest: ' + str(path))
    return recovered


def options(case, pack_digest):
    """四个正式情景共用一套真人/商品/道路ID，玩家接受与拒绝是同情景两次真实选择。"""
    scenario = 'recipient' if case.startswith('recipient') else case
    return {'scenario_id': 'society::scenario.society_membership_' + scenario,
            'player_actor_id': 'society::actor.apprentice', 'pack_set_digest': pack_digest,
            'invitee_actor_id': 'society::actor.study_swordswoman' if case == 'deferred' else 'society::actor.study_caravanner',
            'terms_id': 'terms.city_contract', 'membership_place_id': 'society::place.courtyard',
            'market_place_id': 'society::place.grain', 'merchant_role': 'society::role.merchant',
            'merchant': 'society::actor.merchant', 'shop': 'society::shop.grain', 'item_id': 'society::item.millet',
            'contract_payee_wallet_id': 'society::wallet.city_wages', 'deadline_tick': 120,
            'restock_owned_units': 4 if case == 'short' else 2,
            'work_place_id': 'society::place.grain', 'work_topic_id': 'topic.caravanner.work',
            'follow_exit_place_id': 'society::place.courtyard', 'follow_settlement_id': 'society::settlement.city_outpost',
            'follow_place_id': 'society::place.city_outpost'}


def main():
    """当前中央Schema/能力精确compose到全新输出，显式标记未绑定原生库且未执行玩法。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--pack', type=Path, required=True)
    parser.add_argument('--selection', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--stage-game', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(repo))
    from tools import content_stage as staging
    from tools.content.io import canonical, read_pack
    recovered_resources = materialize_pinned_resources(args.pack.resolve(strict=True))
    composed = target / 'compose'
    command = [sys.executable, str(repo / 'tools/content_pipeline.py'), 'compose', '--packs',
               str(args.pack.resolve(strict=True)), '--selection', str(args.selection.resolve(strict=True)), '--output', str(composed)]
    descriptor = [record(repo / 'schemas/content.schema.json'), record(repo / staging.CAPABILITY)]
    result = subprocess.run(command, text=True, capture_output=True)
    (target / 'compose.stdout.json').write_text(result.stdout)
    (target / 'compose.stderr.log').write_text(result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
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
            copy = generation / name
            copy.parent.mkdir(parents=True, exist_ok=True)
            copy.write_bytes(path.read_bytes())
    if any(digest(Path(row['path'])) != row['sha256'] for row in descriptor):
        raise SystemExit('Central descriptor changed during composition; preserve this preparation and use a new generation.')
    pack_digest = header['pack_set_digest']
    cases = {case: options(case, pack_digest) for case in ['funded', 'short', 'deferred', 'recipient_accept', 'recipient_decline']}
    for case in cases.values():
        scenario = header['catalog']['scenarios'][case['scenario_id']]
        if scenario['player_actor'] != case['player_actor_id']:
            raise SystemExit('Formal player binding mismatch.')
    source = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    source_status = subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True)
    runtime = Path(__file__).resolve().parent
    binding = {'format': 'HM-NAMED-MEMBERSHIP-PREPARED/1', 'source_commit': source,
               'source_status': source_status, 'reviewed_source_commit': None,
               'stage_game': str(args.stage_game.resolve(strict=True)),
               'engine': str(args.engine.resolve(strict=True)), 'engine_sha256': digest(args.engine),
               'pack_set_digest': pack_digest,
               'catalog_path': 'res://data/generated/' + installed_record['directory'] + '/catalog.hmcatalog.json',
               'install_generation': str(generation), 'cases': cases,
               'recovered_pinned_resources': recovered_resources,
               'inputs': descriptor + [record(path) for path in sorted(generation.rglob('*')) if path.is_file()],
               'drivers': {name: record(runtime / name) for name in ['named_membership_main.gd', 'run_node.py', 'prepare_inputs.py']},
               'native_artifact_status': 'MISSING_REVIEWED_ARTIFACT', 'gameplay_executed': False}
    (target / 'runtime-source-binding.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'PREPARED_UNBOUND', 'binding': str(target / 'runtime-source-binding.json'),
                      'pack_set_digest': pack_digest, 'install_generation': str(generation), 'gameplay_executed': False}))


if __name__ == '__main__':
    main()
