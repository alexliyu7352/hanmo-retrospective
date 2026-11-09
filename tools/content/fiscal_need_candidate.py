"""以实际家庭工资和商户产粮生成财政欠款事项的独立作者情景。"""
import argparse
import copy
import json
from pathlib import Path

from market_supply_candidate import build as build_market, git_oid


def build(source, expected_oid):
    """沿已冻结的初态雇佣关系；仅新情景声明税期和钱粮事项映射。"""
    output, source_receipt = build_market(source, expected_oid)
    scenario = output['scenarios'][0]
    original_initial = copy.deepcopy(scenario['initial'])
    dynamic = scenario['dynamic_world']
    economy = dynamic['local_economy']
    district = dynamic['fiscal_settlement']['districts']['fiscal.qingchuan']
    site = district['site_id']
    # 两名商户雇工及有限土地在本情景开局即位于县城治所，非运行中的移动或征调。
    economy['cohorts']['cohort.grain']['site_id'] = site
    economy['production_units']['production.grain']['site_id'] = site
    economy['production_management']['assets']['asset.production.grain.land']['site_id'] = site
    district.update(period_ticks=4, first_due_tick=4)
    household_source = district['sources']['tax.city_households']
    household_source['base_cash_due'] = 12
    merchant_source = {
        'id': 'tax.grain_production',
        'taxpayer': {'kind': 'organization', 'id': 'society::org.grain'},
        'wallet_id': 'society::wallet.grain',
        'inventory_id': 'society::inventory.grain',
        'grain_item_id': 'society::item.millet',
        'production_unit_ids': ['production.grain'],
        'base_cash_due': 0, 'grain_share_bp': 5000, 'civilian_grain_reserve': 2,
    }
    district['sources'][merchant_source['id']] = merchant_source
    policies = {}
    for unit, source_id in [('cash', household_source['id']), ('grain', merchant_source['id'])]:
        key = 'fiscal_need.qingchuan.' + unit
        policies[key] = {'id': key, 'district_id': district['id'], 'source_id': source_id,
                         'unit': unit, 'semantic_key': 'fiscal.qingchuan.' + unit}
    dynamic['affairs']['fiscal_need_policies'] = {'version': 1, 'policies': policies}
    scenario.update(id='scenario.qingchuan_fiscal_needs', name_key='fiscal_needs.scenario.name',
                    seed_text='青川税粮事项', player_actor='society::actor.city_steward')
    scenario['enabled']['scenarios'] = [scenario['id']]
    descriptions = [row for row in scenario['player_start_descriptions'] if row['actor_id'] == scenario['player_actor']]
    if len(descriptions) != 1 or scenario['initial'] != original_initial:
        raise ValueError('Original incumbent or initial assets changed')
    descriptions[0]['text_key'] = 'fiscal_needs.start.steward'
    output.update(id='fiscal_needs', version='1.0.0')
    output['texts']['fiscal_needs.scenario.name'] = '青川税粮事项'
    output['texts']['fiscal_needs.start.steward'] = (
        '你以县仓书佐的现任身份继续。先依法签发常赋，再在地方经济的财政页查看实际钱粮责任。'
        '本情景采用加速的四半日税期与家庭十二钱应付示例：初次工资不足，后续工资会补清旧欠。'
        '两名商户雇工开局已在县城经营，粮税只计真实产粮并先保留两份民用粮。'
        '每笔事项由原评税和付款收据更新，不需另点交钱；未启用税政时没有税款事项。')
    # 独立显式子情景只去掉工资事项投影开关，工资义务/付款本身完整保留。
    fiscal_only = copy.deepcopy(scenario)
    fiscal_only.update(id='scenario.qingchuan_fiscal_needs_only', name_key='fiscal_needs.only.name')
    fiscal_only['enabled']['scenarios'] = [fiscal_only['id']]
    del fiscal_only['dynamic_world']['affairs']['obligation_needs_version']
    del fiscal_only['dynamic_world']['affairs']['obligation_policies']
    output['scenarios'].append(fiscal_only)
    output['texts']['fiscal_needs.only.name'] = '青川税粮事项（仅财政事项）'
    receipt = {
        'source_git_oid': expected_oid,
        'source_pack': 'hanmo.political_economy/1.1.0',
        'source_scenario': source_receipt['source_scenario'],
        'employment_generator_git_oid': git_oid(Path(__file__).with_name('market_supply_candidate.py').read_bytes()),
        'target_scenario': 'fiscalneeds::scenario.qingchuan_fiscal_needs',
        'module': 'fiscal_need_policies/1', 'affairs_codec': 3,
        'wage_need_policy_preserved': True, 'initial_people_and_assets_unchanged': True,
        'independent_fiscal_only_scenario': 'fiscalneeds::scenario.qingchuan_fiscal_needs_only',
        'anonymous_people': source_receipt['anonymous_people'], 'anonymous_labor': source_receipt['anonymous_labor'],
        'initial_employment_only': True, 'runtime_hiring_enabled': False,
        'inventory_physical_location_not_claimed': True,
        'provisional_author_parameters': {'period_ticks': 4, 'household_cash_due': 12,
            'merchant_grain_share_bp': 5000, 'merchant_civilian_grain_reserve': 2},
        'policy_activation': 'requires_actual_political_set_tax_command',
        'payment_source': 'existing_automatic_settlement_from_actual_household_wages_and_merchant_production',
        'runtime_verified': False, 'compose_verified': False,
    }
    return output, receipt


def main():
    """只新增无资源作者包，旧1.1父包及其锁和人物资产均保持原字节。"""
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
    manifest.update(id='hanmo.fiscal_needs', version='1.0.0', namespace='fiscalneeds', dependencies=dependencies)
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in [('content.json', content), ('pack.json', manifest), ('source-receipt.json', receipt)]:
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
