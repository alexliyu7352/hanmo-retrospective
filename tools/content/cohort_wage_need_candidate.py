"""独立压力情景用原工资与真实税款回库演示匿名工资欠账，不改变父包资产。"""
import argparse
import copy
import json
from pathlib import Path

from fiscal_need_candidate import build as build_fiscal
from market_supply_candidate import git_oid


def build(source, expected_oid):
    """四个显式投影组合共享同一真实义务，映射开关不改变原支付规则。"""
    output, source_receipt = build_fiscal(source, expected_oid)
    prototype = output['scenarios'][0]
    initial = copy.deepcopy(prototype['initial'])
    economy = prototype['dynamic_world']['local_economy']
    cohort = economy['cohorts']['cohort.city']
    payer = next(row for row in initial['wallets'] if row['id'] == 'society::wallet.city')
    if cohort['headcount'] != 6 or payer['initial_balance'] != 200:
        raise ValueError('Pressure example requires the exact original six-person / 200-money source')
    # 原商户义务也引用 city rate；压力示例必须只改县城这份责任。
    obligation = economy['obligations']['obligation.city_household_wage']
    pressure_rate = copy.deepcopy(economy['rates'][obligation['rate_profile']])
    pressure_rate.update(id='rate.city_household_wage.pressure', numerator=34)
    economy['rates'][pressure_rate['id']] = pressure_rate
    obligation['rate_profile'] = pressure_rate['id']
    policy = {'id': 'cohort_wage_need.city', 'obligation_id': 'obligation.city_household_wage',
              'organization_id': 'society::org.city', 'semantic_key': 'wage.city.households'}
    prototype['dynamic_world']['affairs']['cohort_wage_need_policies'] = {
        'version': 1, 'policies': {policy['id']: policy}}
    variants = [('all', True, True), ('only', False, False),
                ('named', True, False), ('fiscal', False, True)]
    scenarios = []
    for name, named, fiscal in variants:
        scenario = copy.deepcopy(prototype)
        scenario_id = 'scenario.qingchuan_cohort_wages_' + name
        name_key = 'cohort_wages.' + name + '.name'
        scenario.update(id=scenario_id, name_key=name_key, seed_text='青川匿名工资事项')
        scenario['enabled']['scenarios'] = [scenario_id]
        affairs = scenario['dynamic_world']['affairs']
        if not named:
            del affairs['obligation_needs_version']
            del affairs['obligation_policies']
        if not fiscal:
            del affairs['fiscal_need_policies']
        for row in scenario['player_start_descriptions']:
            if row['actor_id'] == scenario['player_actor']:
                row['text_key'] = 'cohort_wages.start.steward'
        if scenario['initial'] != initial:
            raise ValueError('Original people or initial assets changed')
        labels = {'all': '三类事项共存', 'only': '仅匿名工资事项', 'named': '匿名与具名工资', 'fiscal': '匿名工资与税赋'}
        output['texts'][name_key] = '青川工资责任（' + labels[name] + '）'
        scenarios.append(scenario)
    output.update(id='cohort_wage_needs', version='1.0.0', scenarios=scenarios)
    output['texts']['cohort_wages.start.steward'] = (
        '你以原县仓书佐身份查阅本机构工资责任。本情景是资金压力示例：'
        '原六人家庭每四半日每人应付三十四钱，原公库二百钱不足首期二百零四钱。'
        '先依法签发常赋；实际税款回库后，原工资规则会自动先补最旧欠期。'
        '地方经济页沿真实欠据显示待清偿、已付部分或已结清，不用另点发钱。'
        '上述工资率仅属本作者情景，初始人物、家庭钱粮与公库均沿原值。')
    receipt = {'source_git_oid': expected_oid, 'source_pack': 'hanmo.political_economy/1.1.0',
               'fiscal_generator_git_oid': git_oid(Path(__file__).with_name('fiscal_need_candidate.py').read_bytes()),
               'source_scenario': source_receipt['source_scenario'],
               'target_scenarios': ['cohortwages::' + row['id'] for row in scenarios],
               'module': 'cohort_wage_need_policies/1', 'affairs_codec': 4,
               'initial_people_and_assets_unchanged': True,
               'provisional_author_parameters': {'cash_wage_per_person_per_four_ticks': 34},
               'actual_original_basis': {'cohort_headcount': 6, 'payer_cash': 200, 'existing_issuer_limit': 1000},
               'expected_path_unverified': 'due204/pay200 at4; actual tax12 arrives; automatic old4 clears at5; next due remains separate',
               'independent_named_fiscal_switches': True,
               'pressure_rate_id': pressure_rate['id'],
               'other_obligation_rates_unchanged': True,
               'runtime_verified': False, 'compose_verified': False}
    return output, receipt


def main():
    """仅新增无资源作者包，父包和其初态/锁保留原字节。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--source-content-oid', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    content, receipt = build(args.source_root / 'content.json', args.source_content_oid)
    manifest = json.loads((args.source_root / 'pack.json').read_text())
    if manifest.get('id') != 'hanmo.political_economy' or manifest.get('version') != '1.1.0' or manifest.get('resources'):
        raise ValueError('Requires original resource-free governance pack')
    dependencies = copy.deepcopy(manifest['dependencies'])
    dependencies.append({'id': 'hanmo.political_economy', 'min_version': '1.1.0', 'max_version_exclusive': '1.2.0'})
    manifest.update(id='hanmo.cohort_wage_needs', version='1.0.0', namespace='cohortwages', dependencies=dependencies)
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in [('content.json', content), ('pack.json', manifest), ('source-receipt.json', receipt)]:
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
