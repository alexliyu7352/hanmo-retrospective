"""Build one explicit delegated-household scenario from the preserved household /1 author pack."""
import argparse
import copy
import hashlib
import json
from pathlib import Path


def oid(raw):
    return hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()


def build(source, expected_oid):
    raw = (source / 'content.json').read_bytes()
    if oid(raw) != expected_oid:
        raise ValueError('Household source identity mismatch')
    content = json.loads(raw)
    manifest = json.loads((source / 'pack.json').read_text())
    if manifest['id'] != 'hanmo.household_economy' or manifest['version'] != '1.0.0' or manifest['resources'] or content['version'] != '1.0.0':
        raise ValueError('Requires original resource-free household /1.0 author pack')
    originals = [s for s in content['scenarios'] if s['id'] == 'scenario.qingchuan_self_provision']
    if len(originals) != 1 or content['roles']:
        raise ValueError('Unexpected original household author source')
    original = originals[0]
    scenario = copy.deepcopy(original)
    actor = 'society::actor.city_steward'
    role_id = 'role.household_provision_steward'
    authorization_id = 'provision_authority.city_households'
    economy = scenario['dynamic_world']['local_economy']
    authorization = economy['household_provisioning']['authorizations'][authorization_id]
    cohort = economy['cohorts'][authorization['cohort_id']]
    if scenario['player_actor'] != actor or authorization['household_group_id'] != 'household.city':
        raise ValueError('Expected original existing adult and actual household')
    scenario.update(id='scenario.qingchuan_household_management', name_key='household.management.name', seed_text='青川家户受托供养')
    scenario['enabled']['scenarios'] = [scenario['id']]
    scenario['enabled']['roles'].append(role_id)
    scenario['initial']['roles'].append({'role_id': role_id, 'actor_id': actor, 'enabled': True})
    scenario['household_control'] = {'profile': 'initial_household_stewardship_v1', 'initial_mandates': [{
        'id': 'household_mandate.city_provision', 'actor_id': actor,
        'household_group_id': authorization['household_group_id'], 'source_role_id': role_id,
        'operation': 'configure_household_provisioning', 'effective_from_tick': 0,
        'actor_accepted': True, 'through_tick': 20}]}
    scenario['dynamic_world']['household_provision_management'] = {'version': 1, 'authorization_ids': [authorization_id]}
    for description in scenario['player_start_descriptions']:
        if description['actor_id'] == actor:
            description['text_key'] = 'household.management.start'
    # The one new accepted private duty is explicit; no people, money, food or old duties change.
    for key in original['initial']:
        if key != 'roles' and scenario['initial'][key] != original['initial'][key]:
            raise ValueError('Household management cannot mint or reassign initial assets')
    result = copy.deepcopy(content)
    result['version'] = '1.1.0'
    result['scenarios'].append(scenario)
    result['roles'].append({'id': role_id, 'name_key': 'household.management.role', 'organization_id': None,
                            'initial_actor': actor, 'place_id': cohort['site_id'], 'services': ['information']})
    result['texts'].update({
        'household.management.name': '青川家户受托供养',
        'household.management.role': '家户供养受托人',
        'household.management.start': '你仍是原来的县仓书佐，另有明确接受的家户私务委托。本示例委托至半日20，可在本人所在地方的经济页安排下一完整周期用粮、恢复原需求或撤回未生效安排。县署职权与家户私产分别核验。家户私仓仍从原安民分发获得实际粮食；暂停用粮会留下真实口粮短缺，已经发生的周期不会补算。'})
    manifest['version'] = '1.1.0'
    receipt = {'source_content_git_oid': expected_oid, 'source_pack': 'hanmo.household_economy/1.0.0',
               'scenario': 'household::scenario.qingchuan_household_management',
               'explicit_author_example_control_through_tick': 20,
               'initial_people_money_food_unchanged': True, 'old_scenario_unchanged': result['scenarios'][0] == original,
               'new_private_role': role_id, 'runtime_registered': False}
    return result, manifest, receipt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--source-content-oid', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    content, manifest, receipt = build(args.source_root, args.source_content_oid)
    args.output.mkdir(parents=True, exist_ok=False)
    for name, data in [('content.json', content), ('pack.json', manifest), ('source-receipt.json', receipt)]:
        (args.output / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
