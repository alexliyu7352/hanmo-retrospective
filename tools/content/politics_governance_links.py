"""安民作者语义核具名公职、规则与逐辖区初值；不把示例参数升级成全局默认。"""


def check_politics_governance(pack, index):
    """两段共同开启，政治只验本域与完整权源；真实家庭份额由S5检查器负责。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict):
            continue
        present = 'politics_governance' in world or 'public_relief' in world
        if not present:
            continue
        root = ('scenarios', number, 'dynamic_world', 'politics_governance')

        def issue(code, path, message):
            """沿原诊断三元组定位作者字段，不在编辑器悄悄补值。"""
            issues.append((code, root + tuple(path), message))

        gov, relief = world.get('politics_governance'), world.get('public_relief')
        politics = world.get('politics', {})
        if (not isinstance(gov, dict) or gov.get('enabled') is not True
                or not isinstance(relief, dict) or type(relief.get('version')) is not int
                or relief['version'] != 1 or world.get('profile') != 'local_society_v1'
                or politics.get('enabled') is not True
                or 'native_dynamic_world' not in pack.get('required_capabilities', [])):
            issue('GOVERNANCE_GROUP_PROFILE', (), '安民须由实际政治与救济两段共同开启，任务能力由该组合派生')
            continue
        jurisdictions = politics.get('jurisdictions', {})
        initial = gov.get('initial_support', {})
        if (not isinstance(initial, dict) or set(initial) != set(jurisdictions)
                or any(type(value) is not int or not 0 <= value <= 100 for value in initial.values())):
            issue('GOVERNANCE_INITIAL_SUPPORT', ('initial_support',), '每个真实辖区须有作者明确的0至100民心，缺省不猜数值')
        rules, commissions = gov.get('appeasement_rules', {}), gov.get('commission_policies', {})
        if not isinstance(rules, dict) or not isinstance(commissions, dict) or not rules or not commissions:
            issue('GOVERNANCE_RULES_REQUIRED', (), '安民须有具名规则和委任策略')
            continue
        texts = index.get('texts', pack.get('texts', {}))
        items = index.get('items', {})
        valid_rules = {}
        for key, rule in rules.items():
            path = ('appeasement_rules', key)
            if not isinstance(rule, dict) or rule.get('id') != key or not isinstance(rule.get('version'), str) or not rule['version']:
                issue('GOVERNANCE_RULE_ID', path, '规则须具有稳定ID和明确版本')
                continue
            if rule.get('name_key') not in texts or rule.get('grain_item_id') not in items:
                issue('GOVERNANCE_RULE_SOURCE', path, '规则名称和救济粮必须来自真实作者文本与物品')
            ranges = {'duration_ticks': (1, 2147483647), 'cash_amount': (1, 2147483647),
                      'grain_amount': (1, 2147483647), 'base_support_gain': (0, 100), 'support_cap': (1, 100)}
            if any(type(rule.get(k)) is not int or not lo <= rule[k] <= hi for k, (lo, hi) in ranges.items()):
                issue('GOVERNANCE_RULE_VALUES', path, '时长、固定支出与民心边界须为有效整数')
                continue
            valid_rules[key] = rule
        organization = world.get('organization', {})
        offices, grants = organization.get('offices', {}), organization.get('grants', {})
        for key, commission in commissions.items():
            path = ('commission_policies', key)
            if not isinstance(commission, dict) or commission.get('id') != key:
                issue('GOVERNANCE_COMMISSION_ID', path, '委任策略键必须等于其真实ID')
                continue
            rule = valid_rules.get(commission.get('rule_id'))
            plan = relief.get('policies', {}).get(commission.get('relief_policy_id'))
            if rule is None or not isinstance(plan, dict):
                issue('GOVERNANCE_COMMISSION_SOURCE', path, '委任须引用本版本实际安民规则与救济计划')
                continue
            if (type(commission.get('offer_valid_ticks')) is not int or commission['offer_valid_ticks'] < 1
                    or type(commission.get('completion_deadline_ticks')) is not int
                    or commission['completion_deadline_ticks'] < rule['duration_ticks']):
                issue('GOVERNANCE_COMMISSION_TIME', path, '接受与完工窗口必须明确，完工期限不能短于真实劳动')
            matching = [j for j in jurisdictions.values() if j.get('seat_place_id') == plan.get('site_id')
                        and j.get('political_organization_id') == plan.get('organization_id')]
            if not matching:
                issue('GOVERNANCE_PLAN_JURISDICTION', path, '分配计划须属于实际辖区和治所')
            for jurisdiction in matching:
                covered = False
                for office in offices.values():
                    if office.get('organization_id') != jurisdiction.get('political_organization_id'):
                        continue
                    for grant_id in office.get('grant_ids', []):
                        grant = grants.get(grant_id, {})
                        if ('political_commission_governance' in grant.get('allowed_command_kinds', [])
                                and {'kind': 'site', 'id': jurisdiction['seat_place_id']} in grant.get('scopes', [])
                                and jurisdiction['public_wallet_id'] in grant.get('wallet_ids', [])
                                and set(jurisdiction['public_inventory_ids']) & set(grant.get('inventory_ids', []))
                                and type(grant.get('amount_limit')) is int and grant['amount_limit'] >= rule['cash_amount']):
                            covered = True
                if not covered:
                    issue('GOVERNANCE_COMPLETE_GRANT', path, '须有同一真实公职Grant完整覆盖治所、公钱、公粮及固定费用')
    return issues
