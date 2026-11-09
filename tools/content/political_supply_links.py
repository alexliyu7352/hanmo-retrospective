"""供给签权的条件性作者声明；只有实际占领和专项公职才能在运行时发行政策。"""

COMMAND = 'political_issue_supply_policy'


def political_supply_grant_declares_assets(scenario, index, grant):
    """仅允许接任机构原公库或同据点待捕获公库，额外私产和命令一律拒绝。"""
    world = scenario.get('dynamic_world', {})
    config, takeover = world.get('economic_supply_policies', {}), world.get('political_takeover', {})
    if (not isinstance(config, dict) or type(config.get('version')) is not int or config['version'] != 1
            or takeover.get('enabled') is not True or type(takeover.get('version')) is not int
            or takeover['version'] != 1 or grant.get('allowed_command_kinds') != [COMMAND]
            or type(grant.get('amount_limit')) is not int or grant['amount_limit'] < 0):
        return False
    organization = world.get('organization', {})
    office = organization.get('offices', {}).get(grant.get('office_id'), {})
    groups = organization.get('organizations', {})
    if (not office or office.get('organization_id') not in groups
            or grant.get('id') not in office.get('grant_ids', [])):
        return False
    initial = scenario.get('initial', {})
    wallets = {row['id']: row for row in initial.get('wallets', [])}
    inventories = {row['id']: row for row in initial.get('inventories', [])}

    def valid(ids, records, selected):
        """整份Grant逐项校验，不能只核被询问的那个容器。"""
        if not isinstance(ids, list) or not all(isinstance(value, str) for value in ids) or len(ids) != len(set(ids)):
            return False
        for value in ids:
            owner = records.get(value, {}).get('owner', {})
            if (owner.get('kind') != 'organization' or owner.get('id') not in groups
                    or (owner['id'] != office['organization_id'] and value not in selected)):
                return False
        return True

    for key, rule in takeover.get('rules', {}).items():
        hold = index.get('strongholds', {}).get(rule.get('stronghold_id'), {})
        if (rule.get('id') != key or rule.get('successor_office_id') != office.get('id')
                or grant.get('scopes') != [{'kind': 'site', 'id': rule.get('site_id')}]
                or not hold or hold.get('garrison_id') != rule.get('garrison_id')):
            continue
        if (valid(grant.get('wallet_ids'), wallets, hold.get('public_wallet_ids', []))
                and valid(grant.get('inventory_ids'), inventories, hold.get('public_inventory_ids', []))):
            return True
    return False
