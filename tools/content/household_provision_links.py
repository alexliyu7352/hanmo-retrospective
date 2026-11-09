"""核验家庭私粮的原产权、义务和唯一供养区间；不造钱粮或初始化事实。"""


def _record(table, key):
    """坏类型交诊断处理，不能由不可hash的作者字段引起编译器崩溃。"""
    value = table.get(key) if isinstance(table, dict) and isinstance(key, str) else None
    return value if isinstance(value, dict) else {}


def _check_scenario(scenario, index, root):
    """返回本情景诊断与完整通过的私人义务集合，供中央机构检查明确分流。"""
    dynamic = scenario.get('dynamic_world')
    economy = dynamic.get('local_economy', {}) if isinstance(dynamic, dict) else {}
    section = economy.get('household_provisioning') if isinstance(economy, dict) else None
    if section is None:
        return [], set()
    issues = []

    def issue(code, path, message):
        """错误只定位原作者字段，不替用户猜家庭授权。"""
        issues.append((code, root + tuple(path), message))

    if not isinstance(section, dict) or type(section.get('version')) is not int or section.get('version') != 1:
        issue('HOUSEHOLD_PROVISION_VERSION', (), '家庭自养须明确使用version=1')
        return issues, set()
    table = section.get('authorizations')
    if economy.get('enabled') is not True or not isinstance(table, dict) or not 1 <= len(table) <= 65536:
        issue('HOUSEHOLD_PROVISION_SHAPE', (), '启用的地方经济须声明1至65536条具名家庭自养来源')
        return issues, set()
    inventories = {row.get('id'): row for row in scenario.get('initial', {}).get('inventories', []) if isinstance(row, dict) and isinstance(row.get('id'), str)}
    used_obligations, used_bindings = set(), set()
    required = {'id', 'household_group_id', 'cohort_id', 'obligation_id', 'provision_binding_id', 'inventory_id', 'item_id', 'rate_profile_id'}
    for key, authorization in table.items():
        path = ('authorizations', key)
        if not isinstance(authorization, dict) or set(authorization) != required or any(not isinstance(authorization.get(field), str) or not authorization[field] for field in required) or authorization.get('id') != key:
            issue('HOUSEHOLD_PROVISION_RECORD', path, '授权须有完整且唯一的具名引用，不接受附加权限字段')
            continue
        a = authorization
        household = _record(economy.get('households'), a['household_group_id'])
        cohort = _record(economy.get('cohorts'), a['cohort_id'])
        inventory = inventories.get(a['inventory_id'], {})
        obligation = _record(economy.get('obligations'), a['obligation_id'])
        binding = _record(economy.get('provision_bindings'), a['provision_binding_id'])
        rate = _record(economy.get('rates'), a['rate_profile_id'])
        if not household or not cohort or household.get('inventory_id') != a['inventory_id'] or cohort.get('household_group_id') != a['household_group_id'] or cohort.get('private_inventory_id') != a['inventory_id'] or inventory.get('owner') != {'kind': 'household', 'id': a['household_group_id']}:
            issue('HOUSEHOLD_PROVISION_OWNER', path, '同一家庭、群体和实际初态私粮容器须三方产权一致')
        if a['obligation_id'] in used_obligations or a['provision_binding_id'] in used_bindings:
            issue('HOUSEHOLD_PROVISION_DUPLICATE', path, '同一义务或供养绑定不能重复登记私粮授权')
        used_obligations.add(a['obligation_id']); used_bindings.add(a['provision_binding_id'])
        subject = {'kind': 'cohort', 'id': a['cohort_id']}
        if not obligation or obligation.get('kind') != 'ration' or obligation.get('principal') != subject or obligation.get('beneficiary') != subject or obligation.get('source_wallet_or_inventory') != a['inventory_id'] or obligation.get('authority_ref') != a['id'] or obligation.get('item_id') != a['item_id'] or obligation.get('rate_profile') != a['rate_profile_id'] or obligation.get('destination') != '' or obligation.get('active') is not True or type(obligation.get('period_ticks')) is not int or obligation['period_ticks'] < 1:
            issue('HOUSEHOLD_PROVISION_OBLIGATION', path, '原ration义务必须由同一群体承担并自用私粮，不能借机构grant或另建收粮库存')
        if not rate or rate.get('per_capita') is not True or type(rate.get('numerator')) is not int or rate['numerator'] < 0 or type(rate.get('denominator')) is not int or rate['denominator'] < 1 or a['item_id'] not in index.get('items', {}):
            issue('HOUSEHOLD_PROVISION_RATE', path, '引用真实物品和原按人数整数口粮率')
        start, end, due = binding.get('from_tick'), binding.get('through_tick'), obligation.get('next_due_tick')
        if not binding or binding.get('subject') != subject or binding.get('obligation_id') != a['obligation_id'] or binding.get('payer_inventory') != a['inventory_id'] or binding.get('policy_owner') != 'macro' or binding.get('military_source_id', '') != '' or any(type(v) is not int for v in (start, end, due)) or not 0 <= start < due <= end < 9223372036854775807:
            issue('HOUSEHOLD_PROVISION_INTERVAL', path, '原唯一供养绑定须给出有限覆盖区间，首个到期日必须在区间内')
    return issues, set() if issues else used_obligations


def check_household_provision(pack, index):
    """中央作者门调用此叶；旧情景未启用该模块时无新增诊断或默认授权。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        root = ('scenarios', number, 'dynamic_world', 'local_economy', 'household_provisioning')
        found, _ = _check_scenario(scenario, index, root)
        issues.extend(found)
    return issues


def household_ration_authorizations(scenario, index):
    """只有整个私粮来源块完整通过，才允许中央跳过这些义务的机构issuer检查。"""
    _, authorizations = _check_scenario(scenario, index, ())
    return authorizations
