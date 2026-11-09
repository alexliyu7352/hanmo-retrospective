"""核验有限围城政策和人口续接的真实作者来源；配置不产生同意、编组或交接事实。"""


CAPABILITY = 'siege_integration_v1'


def check_siege_integration(pack, index):
    """只在显式选中情景验证政策，旧情景不得靠孤立续接条款取得新能力。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        integration = dynamic.get('siege_integration')
        population = dynamic.get('population_military', {})
        policies = population.get('integration_policies', {})
        root = ('scenarios', number, 'dynamic_world')

        def issue(code, path, message):
            """诊断保持真实作者路径，不以修改源字典补齐缺失的许可。"""
            issues.append((code, root + tuple(path), message))

        if integration is None:
            if policies:
                issue('SIEGE_INTEGRATION_PROFILE', ('population_military', 'integration_policies'),
                      '人口接收政策须由本情景明确启用的真实围城整编能力消费')
            continue
        at = ('siege_integration',)
        if not {'military', 'native_dynamic_world', CAPABILITY} <= set(pack['required_capabilities']):
            issue('SIEGE_INTEGRATION_CAPABILITY', at, '整编必须声明军事、原生动态世界和独立版本能力')
        enabled = scenario.get('enabled', {})
        units = [unit for camp in scenario.get('initial', {}).get('garrisons', []) for unit in camp['units']]
        officers = {unit['commander_actor_id'] for unit in units}
        seen = set()
        for offset, policy in enumerate(integration['officer_policies']):
            path = at + ('officer_policies', offset)
            actor = policy['actor_id']
            if actor in seen:
                issue('SIEGE_OFFICER_POLICY_DUPLICATE', path, '每位真实人物在一个情景只可有一份有限政策')
            seen.add(actor)
            if actor not in enabled.get('actors', []) or actor not in officers:
                issue('SIEGE_OFFICER_POLICY_SOURCE', path + ('actor_id',), '政策须属于本情景实际启用且有部队职责来源的军官')
            if any(org not in enabled.get('organizations', []) for org in policy['allowed_recipient_organization_ids']):
                issue('SIEGE_OFFICER_POLICY_RECIPIENT', path, '允许接收方必须是本情景实际存在的组织；空名单表示明确拒绝')
        selected = integration['default_terms']['population_policy_ids']
        if set(selected) != set(policies):
            issue('SIEGE_POPULATION_POLICY_TERMS', at + ('default_terms', 'population_policy_ids'),
                  '默认条款须逐项引用本情景明确登记的接收责任政策，不猜接收人或遗漏工资责任')
        if policies and not population.get('enabled'):
            issue('SIEGE_POPULATION_POLICY_SOURCE', at, '人口续接不能缺少实际军籍adapter')
    return issues
