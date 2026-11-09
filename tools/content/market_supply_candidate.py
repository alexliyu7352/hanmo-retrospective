"""把原匿名劳力的一部分声明为商户初态雇佣，接原生产、工资、口粮和市场库存。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path


def git_oid(raw):
    """冻结输入原字节，拒绝把后来的同名作者包当作相同来源。"""
    return hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()


def build(source, expected_oid):
    """只生成独立情景；总人口、总劳力及初始钱货不变，所有经营资源仍由实际商户持有。"""
    raw = source.read_bytes()
    if git_oid(raw) != expected_oid:
        raise ValueError('Governance source GitOID mismatch')
    content = json.loads(raw)
    if content.get('format') != 'HM-CONTENT' or content.get('version') != '1.1.0':
        raise ValueError('Requires formal political economy 1.1.0 author content')
    if any(value for key, value in content.items() if isinstance(value, list) and key not in ('scenarios', 'required_capabilities')):
        raise ValueError('Source contains local world entities; explicit reference migration required')
    originals = [s for s in content['scenarios'] if s['id'] == 'scenario.qingchuan_governance']
    if len(originals) != 1:
        raise ValueError('Requires exactly one formal governance scenario')
    original = originals[0]
    scenario = copy.deepcopy(original)
    dynamic = scenario['dynamic_world']
    economy = dynamic['local_economy']
    cohorts = economy['cohorts']
    city = cohorts['cohort.city']
    if (city['headcount'], city['labor_capacity']) != (8, 6):
        raise ValueError('Expected original eight residents and six workers')
    if economy.get('household_provisioning'):
        raise ValueError('Market example keeps original institutional provision')
    merchant = dynamic['organization']['organizations']['society::org.grain']
    if merchant['kind'] != 'merchant' or merchant['headquarters_site'] != 'society::place.grain':
        raise ValueError('Actual local merchant organization missing')
    owner = {'kind': 'organization', 'id': merchant['id']}
    for table, key in [('wallets', 'society::wallet.grain'), ('inventories', 'society::inventory.grain')]:
        rows = [row for row in scenario['initial'][table] if row['id'] == key]
        if len(rows) != 1 or rows[0]['owner'] != owner:
            raise ValueError('Merchant asset source differs: ' + key)
    city_allocation = economy['labor_allocations']['allocation.city']
    city_unit = economy['production_units']['production.city']
    if city_allocation['workers'] != 6 or city_unit['capacity'] != 6 or city_allocation['cohort_id'] != city['id']:
        raise ValueError('Original public labor allocation differs')
    city.update(headcount=6, labor_capacity=4)
    city_allocation['workers'] = 4
    city_unit['capacity'] = 4
    grain = copy.deepcopy(city)
    grain.update(id='cohort.grain', headcount=2, labor_capacity=2,
                 site_id=merchant['headquarters_site'], affiliation_id=merchant['id'])
    cohorts[grain['id']] = grain
    allocation = copy.deepcopy(city_allocation)
    allocation.update(id='allocation.grain', cohort_id=grain['id'], workers=2,
                      activity_or_production_id='production.grain', commitment_id='commitment.allocation.grain.initial_employment')
    economy['labor_allocations'][allocation['id']] = allocation
    recipe = copy.deepcopy(economy['recipes']['recipe.city'])
    if recipe['period_ticks'] != 4 or recipe['inputs'] or len(recipe['outputs']) != 1:
        raise ValueError('Original finite grain recipe differs')
    recipe.update(id='recipe.grain')
    # 示例产率为每位实际有效劳力每四半日两粮；只是本情景作者参数，不改变公共默认值。
    recipe['outputs'][0].update(numerator=2, denominator=1)
    economy['recipes'][recipe['id']] = recipe
    unit = copy.deepcopy(city_unit)
    unit.update(id='production.grain', site_id=grain['site_id'], owner=owner,
                output_inventory_id='society::inventory.grain', labor_allocation_ids=[allocation['id']],
                capacity=2, recipe_id=recipe['id'])
    economy['production_units'][unit['id']] = unit
    economy['production_bindings'][unit['id']] = {'unit_id': unit['id'], 'policy_owner': 'macro'}
    management = economy['production_management']
    city_plan = management['plans'][city_unit['id']]
    if len(city_plan['asset_uses']) != 1 or city_plan['asset_uses'][0]['units'] != 6:
        raise ValueError('Original finite public land plan differs')
    city_asset = management['assets'][city_plan['asset_uses'][0]['asset_id']]
    if city_asset['capacity_units'] != 6:
        raise ValueError('Original public land capacity differs')
    city_asset['capacity_units'] = 4
    city_plan['asset_uses'][0]['units'] = 4
    asset = copy.deepcopy(city_asset)
    asset.update(id='asset.production.grain.land', site_id=grain['site_id'], owner=owner,
                 source_key='initial_merchant_land:production.grain', capacity_units=2)
    management['assets'][asset['id']] = asset
    management['plans'][unit['id']] = {'unit_id': unit['id'], 'season_profile_id': '',
                                     'asset_uses': [{'asset_id': asset['id'], 'units': 2, 'batches_per_unit': 1}]}
    subject = {'kind': 'cohort', 'id': grain['id']}
    wage = copy.deepcopy(economy['obligations']['obligation.city_household_wage'])
    wage.update(id='obligation.grain_household_wage', principal=owner, beneficiary=subject,
                authority_ref='issuer.grain', source_wallet_or_inventory='society::wallet.grain')
    ration = copy.deepcopy(economy['obligations']['obligation.city_ration'])
    ration.update(id='obligation.grain_ration', principal=owner, beneficiary=subject,
                  authority_ref='issuer.grain', source_wallet_or_inventory='society::inventory.grain')
    for obligation in (wage, ration):
        economy['obligations'][obligation['id']] = obligation
    provision = copy.deepcopy(economy['provision_bindings']['provision.city'])
    provision.update(id='provision.grain', subject=subject,
                     payer_inventory='society::inventory.grain', obligation_id=ration['id'])
    economy['provision_bindings'][provision['id']] = provision
    dynamic['organization']['institutional_issuers']['issuer.grain'] = {
        'id': 'issuer.grain', 'organization_id': merchant['id'], 'continuity_policy_id': 'continue_under_organization',
        'obligation_ids': [wage['id'], ration['id']],
        'source_container_ids': ['society::wallet.grain', 'society::inventory.grain'], 'amount_limit': 2}
    before = original['dynamic_world']['local_economy']
    for field in ('headcount', 'labor_capacity'):
        if sum(c[field] for c in cohorts.values()) != sum(c[field] for c in before['cohorts'].values()):
            raise ValueError('Anonymous population changed: ' + field)
    if sum(a['workers'] for a in economy['labor_allocations'].values()) != sum(a['workers'] for a in before['labor_allocations'].values()):
        raise ValueError('Initial labor duplicated')
    if scenario['initial'] != original['initial'] or economy['households'] != before['households']:
        raise ValueError('Initial people, assets or household containers changed')
    scenario.update(id='scenario.qingchuan_market_supply', name_key='market_supply.scenario.name', seed_text='青川粮肆生产')
    if scenario['enabled'].get('scenarios') != [original['id']]:
        raise ValueError('Original scenario self-reference differs')
    scenario['enabled']['scenarios'] = [scenario['id']]
    scenario['player_actor'] = 'society::actor.apprentice'
    descriptions = [r for r in scenario['player_start_descriptions'] if r['actor_id'] == scenario['player_actor']]
    if len(descriptions) != 1:
        raise ValueError('Original playable apprentice entry missing')
    descriptions[0]['text_key'] = 'market_supply.start.apprentice'
    output = copy.deepcopy(content)
    for key, value in output.items():
        if isinstance(value, list) and key != 'required_capabilities':
            output[key] = []
    output.update(id='market_supply', version='1.0.0', scenarios=[scenario])
    output['texts']['market_supply.scenario.name'] = '青川粮肆生产'
    output['texts']['market_supply.start.apprentice'] = (
        '你仍是程禾。城户八人中，有两人在开局时已受雇于粮肆，其工资和口粮由商户负担；'
        '其余六人留在县署。到粮肆向在场商人询价，等候四半日后再看实际库存与报价。'
        '商户按实际劳力生产，购买会花掉真钱并减少库存；县署公粮仍归县署。')
    return output, {'source_git_oid': expected_oid, 'source_scenario': 'civicecon::scenario.qingchuan_governance',
                    'target_scenario': 'marketsupply::scenario.qingchuan_market_supply',
                    'initial_people_and_assets_unchanged': True, 'anonymous_people': 11, 'anonymous_labor': 8,
                    'initial_employment_only': True, 'runtime_hiring_enabled': False,
                    'provisional': {'merchant_workers': 2, 'period_ticks': 4, 'gross_grain_per_worker': 2,
                                    'ration_per_person': 1, 'wage_per_person': 1},
                    'runtime_verified': False, 'compose_verified': False}


def main():
    """生成新作者包，不覆盖旧包或自动发起编译、安装和运行。"""
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
    manifest.update(id='hanmo.market_supply', version='1.0.0', namespace='marketsupply', dependencies=dependencies)
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in [('content.json', content), ('pack.json', manifest), ('source-receipt.json', receipt)]:
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
