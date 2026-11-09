"""救济分配只承认实际同地家庭和明确政治规则；不补造受领人或猜支出比例。"""


def _row(table, key):
    """坏引用类型只变成诊断，不能触发哈希异常或补一个假对象。"""
    value = table.get(key) if isinstance(table, dict) and isinstance(key, str) else None
    return value if isinstance(value, dict) else {}


def check_public_relief(pack, index):
    """仅显式public_relief/1检查；既有情景没有新默认分发或隐式民心收益。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('public_relief') is None:
            continue
        section = dynamic['public_relief']
        root = ('scenarios', number, 'dynamic_world', 'public_relief')

        def issue(code, path, message):
            """定位原始字段，便于原作者表单在错误处提示责任来源。"""
            issues.append((code, root + tuple(path), message))

        if not isinstance(section, dict) or type(section.get('version')) is not int or section.get('version') != 1:
            issue('RELIEF_VERSION', (), '救济资源须显式选择version=1')
            continue
        policies = section.get('policies')
        economy, governance = dynamic.get('local_economy'), dynamic.get('politics_governance')
        if not isinstance(policies, dict) or not 1 <= len(policies) <= 1000 or not isinstance(economy, dict) or economy.get('enabled') is not True or not isinstance(governance, dict) or governance.get('enabled') is not True:
            issue('RELIEF_DEPENDENCY', (), '明确分配表依赖真实地方经济和政治治理规则')
            continue
        initial = scenario.get('initial', {})
        wallets = {r.get('id'): r for r in initial.get('wallets', []) if isinstance(r, dict) and isinstance(r.get('id'), str)}
        inventories = {r.get('id'): r for r in initial.get('inventories', []) if isinstance(r, dict) and isinstance(r.get('id'), str)}
        totals = {}
        for key, policy in policies.items():
            path = ('policies', key)
            if not isinstance(policy, dict) or policy.get('id') != key or policy.get('site_id') not in scenario.get('enabled', {}).get('places', []) or not _row(index.get('organizations'), policy.get('organization_id')) or not _row(index.get('items'), policy.get('item_id')):
                issue('RELIEF_POLICY_SOURCE', path, '分配策略须引用启用地点、真实组织和物品')
                continue
            recipients = policy.get('recipients')
            if not isinstance(recipients, list) or not 1 <= len(recipients) <= 256:
                issue('RELIEF_RECIPIENTS', path, '每份完整计划须明确1至256个家庭受领项')
                continue
            seen_households, seen_cohorts = set(), set()
            cash, grain = 0, 0
            for n, recipient in enumerate(recipients):
                rp = path + ('recipients', n)
                if not isinstance(recipient, dict) or any(type(recipient.get(k)) is not int or not 0 <= recipient[k] <= 2147483647 for k in ('cash', 'grain')) or not (recipient['cash'] or recipient['grain']):
                    issue('RELIEF_SHARE', rp, '每项须有非零、非负整数份额，不能用bool作钱粮')
                    continue
                home = _row(economy.get('households'), recipient.get('household_group_id'))
                cohort = _row(economy.get('cohorts'), recipient.get('cohort_id'))
                hid, cid = recipient.get('household_group_id'), recipient.get('cohort_id')
                if not home or not cohort or cohort.get('household_group_id') != hid or cohort.get('site_id') != policy['site_id'] or cohort.get('private_wallet_id') != home.get('wallet_id') or cohort.get('private_inventory_id') != home.get('inventory_id'):
                    issue('RELIEF_HOUSEHOLD_SOURCE', rp, '家庭、cohort、同地私有容器必须三方一致')
                    continue
                owner = {'kind': 'household', 'id': hid}
                if wallets.get(home.get('wallet_id'), {}).get('owner') != owner or inventories.get(home.get('inventory_id'), {}).get('owner') != owner:
                    issue('RELIEF_PRIVATE_OWNER', rp, '不能把Actor或他人钱包当家庭受领容器')
                if hid in seen_households or cid in seen_cohorts:
                    issue('RELIEF_DUPLICATE_RECIPIENT', rp, '同一家庭或cohort不能重复列份额')
                seen_households.add(hid); seen_cohorts.add(cid)
                cash += recipient['cash']; grain += recipient['grain']
            totals[key] = (cash, grain)
        commissions = governance.get('commission_policies', {})
        if not isinstance(commissions, dict):
            issue('RELIEF_COMMISSION_SOURCE', (), '治理委托须有具名策略表')
            continue
        for key, commission in commissions.items():
            if not isinstance(commission, dict):
                continue
            plan = _row(policies, commission.get('relief_policy_id'))
            rule = _row(governance.get('appeasement_rules'), commission.get('rule_id'))
            if not plan or not rule or plan.get('item_id') != rule.get('grain_item_id') or totals.get(commission.get('relief_policy_id')) != (rule.get('cash_amount'), rule.get('grain_amount')):
                issue('RELIEF_FIXED_COST', ('commission', key), '安民分配必须精确等于原政治规则固定钱粮成本')
    return issues
