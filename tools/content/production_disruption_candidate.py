"""复用原市场初态生成有限雨损作者情景；不建立另一套人、钱粮或劳力。"""
import argparse
import copy
import json
from pathlib import Path

from market_supply_candidate import build as build_market, git_oid
from production_disruption_links import CAPABILITY, POLICY, PROFILE


def build(source, expected_oid):
    """从固定治理源复用已授权初态雇佣，再只声明本地闭区间雨损。"""
    content, market_receipt = build_market(source, expected_oid)
    market_raw = (json.dumps(content, ensure_ascii=False, indent=2) + '\n').encode()
    market_oid = git_oid(market_raw)
    if market_oid != '8036a78ebb51c92c7bcdd5008cb3ac3feeefd2b0':
        raise ValueError('Original market scenario differs; review source before deriving')
    scenario = content['scenarios'][0]
    before_initial = copy.deepcopy(scenario['initial'])
    before_economy = copy.deepcopy(scenario['dynamic_world']['local_economy'])
    economy = scenario['dynamic_world']['local_economy']
    unit = economy['production_units']['production.city']
    if unit['site_id'] != 'society::place.courtyard' or unit['capacity'] != 4:
        raise ValueError('Original public production site or capacity changed')
    cause = {'id': 'disruption.city_yard_rain', 'source_key': 'authored.city_yard_rain',
             'site_id': unit['site_id'], 'reason_key': 'disruption.reason.rain',
             'from_tick': 4, 'through_tick': 8, 'remaining_yield_bp': 5000,
             'unit_ids': [unit['id']]}
    scenario['dynamic_world']['production_disruptions'] = {
        'version': 1, 'profile': PROFILE, 'combination_policy': POLICY,
        'causes': {cause['id']: cause}}
    scenario.update(id='scenario.qingchuan_production_disruption',
                    name_key='disruption.scenario.name', seed_text='青川有限雨损',
                    player_actor='society::actor.city_steward')
    scenario['enabled']['scenarios'] = [scenario['id']]
    descriptions = [row for row in scenario['player_start_descriptions']
                    if row['actor_id'] == scenario['player_actor']]
    if len(descriptions) != 1:
        raise ValueError('Actual incumbent player entry missing')
    descriptions[0]['text_key'] = 'disruption.start.steward'
    content.update(id='production_disruptions', version='1.0.0')
    if CAPABILITY in content['required_capabilities']:
        raise ValueError('Original source unexpectedly declares disruption capability')
    content['required_capabilities'].append(CAPABILITY)
    content['texts'].update({
        'disruption.scenario.name': '青川有限雨损',
        'disruption.reason.rain': '场院雨损（明确作者示例）',
        'disruption.start.steward': (
            '以原县仓书佐身份继续，查看地方经济中的县署生产。'
            '本情景明确安排半日4至8的场院雨损，保留原潜在产量的一半；'
            '原投入、工资和口粮照常结算，届满不补回损失。'
            '两名粮肆雇工及其原经营资源仍在，商户生产不受这项县署雨损影响。')})
    if scenario['initial'] != before_initial or economy != before_economy:
        raise ValueError('Disruption authoring altered original resources or economy')
    return content, {
        'recovery': 'reimplemented_from_approved_contract_not_original_lost_json',
        'source_content_git_oid': expected_oid, 'market_content_git_oid': market_oid,
        'market_generator_git_oid': git_oid(Path(__file__).with_name('market_supply_candidate.py').read_bytes()),
        'source_scenario': market_receipt['target_scenario'],
        'target_scenario': 'disruption::scenario.qingchuan_production_disruption',
        'initial_people_assets_and_economy_unchanged': True,
        'module': 'production_disruptions/1', 'required_capability': CAPABILITY,
        'combination_policy': POLICY,
        'provisional_author_parameters': {'from_tick': 4, 'through_tick': 8, 'remaining_yield_bp': 5000},
        'runtime_registered': False, 'compose_verified': False}


def main():
    """写一个无资源新包；原父包及已安装舞台均不变。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--source-content-oid', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    content, receipt = build(args.source_root / 'content.json', args.source_content_oid)
    manifest = json.loads((args.source_root / 'pack.json').read_text())
    if manifest.get('id') != 'hanmo.political_economy' or manifest.get('version') != '1.1.0' or manifest.get('resources'):
        raise ValueError('Requires resource-free original political economy 1.1.0')
    dependencies = copy.deepcopy(manifest['dependencies'])
    dependencies.append({'id': 'hanmo.political_economy', 'min_version': '1.1.0', 'max_version_exclusive': '1.2.0'})
    manifest.update(id='hanmo.production_disruptions', version='1.0.0', namespace='disruption', dependencies=dependencies)
    manifest['required_capabilities'].append(CAPABILITY)
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in [('content.json', content), ('pack.json', manifest), ('source-receipt.json', receipt)]:
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
