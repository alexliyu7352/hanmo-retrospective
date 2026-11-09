"""接任税政只声明原辖区公库与原税源日程；不预置已占领政策或实际税收。"""

COMMAND = 'political_set_occupation_tax'
CAPABILITY = 'occupation_fiscal_v1'


def occupation_fiscal_grant_declares_assets(scenario, index, grant):
    """条件性公库声明不能借任意命令或额外资源越过原Owner校验。"""
    world = scenario.get('dynamic_world', {})
    config = world.get('political_occupation_fiscal', {})
    takeover = world.get('political_takeover', {})
    if config != {'enabled': True, 'version': 1} or takeover.get('enabled') is not True:
        return False
    organization = world.get('organization', {})
    office = organization.get('offices', {}).get(grant.get('office_id'), {})
    if (not office or grant.get('id') not in office.get('grant_ids', [])
            or grant.get('allowed_command_kinds') != [COMMAND]
            or len(grant.get('wallet_ids', [])) != 1
            or not grant.get('inventory_ids')
            or len(set(grant['inventory_ids'])) != len(grant['inventory_ids'])):
        return False
    initial = scenario.get('initial', {})
    wallets = {row['id']: row for row in initial.get('wallets', [])}
    inventories = {row['id']: row for row in initial.get('inventories', [])}
    groups = organization.get('organizations', {})
    for rule in takeover.get('rules', {}).values():
        district = world.get('politics', {}).get('jurisdictions', {}).get(rule.get('jurisdiction_id'), {})
        if (not district or rule.get('successor_office_id') != office.get('id')
                or grant.get('scopes') != [{'kind': 'site', 'id': rule.get('site_id')}]
                or grant['wallet_ids'] != [district.get('public_wallet_id')]
                or not set(grant['inventory_ids']) <= set(district.get('public_inventory_ids', []))):
            continue
        selected = [wallets.get(value) for value in grant['wallet_ids']]
        selected += [inventories.get(value) for value in grant['inventory_ids']]
        if all(row and row.get('owner', {}).get('kind') == 'organization'
               and row['owner'].get('id') in groups for row in selected):
            return True
    return False


def check_political_occupation_fiscal(pack, index):
    """新税政与旧日程、真实占领和专用Grant必须同作者启用。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if 'political_occupation_fiscal' not in world:
            continue
        path = ('scenarios', number, 'dynamic_world', 'political_occupation_fiscal')
        config = world['political_occupation_fiscal']
        if (config != {'enabled': True, 'version': 1}
                or type(config.get('version')) is not int
                or world.get('profile') != 'local_society_v1'
                or world.get('political_takeover', {}).get('enabled') is not True
                or world.get('political_fiscal', {}).get('enabled') is not True
                or world.get('fiscal_settlement', {}).get('version') != 1
                or CAPABILITY not in pack.get('required_capabilities', [])):
            issues.append(('OCCUPATION_FISCAL_PROFILE', path, '新税政须配对原接管、旧财政日程和显式能力'))
            continue
        grants = world.get('organization', {}).get('grants', {})
        valid = [grant for grant in grants.values() if occupation_fiscal_grant_declares_assets(scenario, index, grant)]
        for key, rule in world['political_takeover'].get('rules', {}).items():
            if not any(grant.get('office_id') == rule.get('successor_office_id')
                       and grant.get('scopes') == [{'kind': 'site', 'id': rule.get('site_id')}]
                       for grant in valid):
                issues.append(('OCCUPATION_FISCAL_GRANT', path + ('rules', key), '接任公职须明确声明原辖区公库的新税政专项Grant'))
    return issues
