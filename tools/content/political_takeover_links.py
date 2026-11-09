"""接管作者只配置原据点、接任公职与明确整合规则，不预置已占领或已承担事实。"""


def political_takeover_grant_declares_assets(scenario, index, grant):
    """只证明显式条件性目录声明；当前执行仍须取得实际占领与逐责任批准。"""
    world = scenario.get('dynamic_world', {})
    config = world.get('political_takeover', {})
    organization = world.get('organization', {})
    if (config.get('enabled') is not True or type(config.get('version')) is not int
            or config['version'] != 1
            or grant.get('allowed_command_kinds') != ['political_assume_public_responsibilities']):
        return False
    offices, groups = organization.get('offices', {}), organization.get('organizations', {})
    office = offices.get(grant.get('office_id'))
    if not office or grant.get('id') not in office.get('grant_ids', []):
        return False
    initial = scenario.get('initial', {})
    wallets = {row['id']: row for row in initial.get('wallets', [])}
    inventories = {row['id']: row for row in initial.get('inventories', [])}

    def exact(left, right):
        return (isinstance(left, list) and isinstance(right, list)
                and len(left) == len(set(left)) and len(right) == len(set(right))
                and set(left) == set(right))

    for key, rule in config.get('rules', {}).items():
        hold = index.get('strongholds', {}).get(rule.get('stronghold_id'), {})
        if (rule.get('id') != key or rule.get('successor_office_id') != office['id']
                or grant.get('scopes') != [{'kind': 'site', 'id': rule.get('site_id')}]
                or not hold or hold.get('garrison_id') != rule.get('garrison_id')
                or not exact(grant.get('wallet_ids'), hold.get('public_wallet_ids', []))
                or not exact(grant.get('inventory_ids'), hold.get('public_inventory_ids', []))):
            continue
        resources = [wallets.get(value) for value in grant['wallet_ids']]
        resources += [inventories.get(value) for value in grant['inventory_ids']]
        if all(row and row.get('owner', {}).get('kind') == 'organization'
               and row['owner'].get('id') in groups for row in resources):
            return True
    return False


def check_political_takeover(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict) or 'political_takeover' not in world:
            continue
        root = ('scenarios', number, 'dynamic_world', 'political_takeover')

        def issue(code, path, message):
            issues.append((code, root + tuple(path), message))

        config = world['political_takeover']
        organization, politics = world.get('organization', {}), world.get('politics', {})
        if (not isinstance(config, dict) or config.get('enabled') is not True
                or type(config.get('version')) is not int or config['version'] != 1
                or world.get('profile') != 'local_society_v1'
                or not isinstance(organization, dict) or organization.get('enabled') is not True
                or not isinstance(politics, dict) or politics.get('enabled') is not True
                or not {'military', 'native_dynamic_world'} <= set(pack.get('required_capabilities', []))):
            issue('POLITICAL_TAKEOVER_PROFILE', (), '接管须显式启用原军事、组织与政治能力，不从资产当前Owner推导授权')
            continue
        rules = config.get('rules')
        if not isinstance(rules, dict) or not rules or len(rules) > 4096:
            issue('POLITICAL_TAKEOVER_RULES', ('rules',), '须有具名且有界的接管规则')
            continue
        enabled = scenario.get('enabled', {})
        offices, grants = organization.get('offices', {}), organization.get('grants', {})
        districts, groups = politics.get('jurisdictions', {}), organization.get('organizations', {})
        covered, district_holds = set(), {}
        for key, rule in rules.items():
            path = ('rules', key)
            if not isinstance(rule, dict) or rule.get('id') != key:
                issue('POLITICAL_TAKEOVER_RULE_ID', path, '规则键须等于稳定ID')
                continue
            district = districts.get(rule.get('jurisdiction_id'), {})
            office = offices.get(rule.get('successor_office_id'), {})
            site = index.get('places', {}).get(rule.get('site_id'), {})
            stronghold = index.get('strongholds', {}).get(rule.get('stronghold_id'), {})
            garrison = index.get('garrisons', {}).get(rule.get('garrison_id'), {})
            if (not district or not office or not site or not stronghold or not garrison
                    or district.get('seat_place_id') != rule.get('site_id')
                    or office.get('organization_id') not in groups
                    or stronghold.get('garrison_id') != rule.get('garrison_id')
                    or stronghold.get('settlement_id') != site.get('settlement_id')
                    or garrison.get('organization_id') != district.get('controlling_organization_id')
                    or district.get('public_wallet_id') not in stronghold.get('public_wallet_ids', [])
                    or not set(district.get('public_inventory_ids', [])) <= set(stronghold.get('public_inventory_ids', []))
                    or rule.get('stronghold_id') not in enabled.get('strongholds', [])
                    or rule.get('garrison_id') not in enabled.get('garrisons', [])
                    or rule.get('site_id') not in enabled.get('places', [])):
                issue('POLITICAL_TAKEOVER_SCOPE', path, '辖区治所、实际启用据点及接任公职必须来自同一作者世界')
                continue
            binding = (stronghold['id'], office['organization_id'])
            if binding in covered:
                issue('POLITICAL_TAKEOVER_AMBIGUOUS', path, '同一据点与接任组织只能命中一条规则')
            covered.add(binding)
            previous_hold = district_holds.setdefault(district['id'], stronghold['id'])
            if previous_hold != stronghold['id']:
                issue('POLITICAL_TAKEOVER_AMBIGUOUS_JURISDICTION', path, '一个辖区只能沿同一原据点的真实接管链转移控制')
            source = rule.get('occupation_rule', {})
            if (not isinstance(source, dict) or source.get('rule_id') != key
                    or type(source.get('rule_version')) is not int or source['rule_version'] != 1
                    or type(source.get('integration_ticks')) is not int or not 0 < source['integration_ticks'] <= 2147483647
                    or type(source.get('income_basis_points')) is not int or not 0 <= source['income_basis_points'] <= 10000):
                issue('POLITICAL_TAKEOVER_INTEGRATION_RULE', path + ('occupation_rule',), '整合时长与收入比例必须显式配置；20半日与50%没有全局默认')
            authorized = False
            for grant_id in office.get('grant_ids', []):
                grant = grants.get(grant_id, {})
                if (grant.get('office_id') == office['id']
                        and 'political_assume_public_responsibilities' in grant.get('allowed_command_kinds', [])
                        and {'kind': 'site', 'id': rule['site_id']} in grant.get('scopes', [])
                        and type(grant.get('amount_limit')) is int and grant['amount_limit'] >= 0
                        and set(stronghold.get('public_wallet_ids', [])) <= set(grant.get('wallet_ids', []))
                        and set(stronghold.get('public_inventory_ids', [])) <= set(grant.get('inventory_ids', []))):
                    authorized = True
                    break
            if not authorized:
                issue('POLITICAL_TAKEOVER_GRANT', path, '须由同一专项Grant完整覆盖治所和原军事公共资产清单；私户收款人不能因此改派')
    return issues
