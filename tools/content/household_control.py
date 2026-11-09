"""初态家庭事务委托：明确本人接受和有限权限，不能由同址、公职或亲缘推导。"""
PROFILE = 'initial_household_stewardship_v1'
OPERATION = 'configure_household_provisioning'
IDS = ('id', 'actor_id', 'household_group_id', 'source_role_id')
REQUIRED = set(IDS) | {'operation', 'effective_from_tick', 'actor_accepted'}


def _id(value):
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_household_control(pack, index):
    issues = []
    roles = dict(index.get('roles', {}))
    roles.update({row['id']: row for row in pack.get('roles', []) if isinstance(row, dict) and _id(row.get('id'))})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if not isinstance(scenario, dict) or 'household_control' not in scenario:
            continue
        path = ('scenarios', number, 'household_control')

        def issue(code, suffix, message):
            issues.append((code, path + suffix, message))

        config = scenario['household_control']
        if not isinstance(config, dict) or set(config) != {'profile', 'initial_mandates'} or config.get('profile') != PROFILE:
            issue('HOUSEHOLD_CONTROL_PROFILE', (), '须显式选择初态家庭事务委托政策')
            continue
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or not isinstance(dynamic.get('local_economy'), dict):
            issue('HOUSEHOLD_CONTROL_ECONOMY', (), '须有同一情景的真实聚合户及地方经济')
            continue
        mandates = config['initial_mandates']
        if not isinstance(mandates, list) or not 1 <= len(mandates) <= 4096:
            issue('HOUSEHOLD_CONTROL_MANDATES', ('initial_mandates',), '初态须明确1至4096份不同的有限事务委托')
            continue
        initial = scenario.get('initial', {})
        actors = {row.get('actor_id'): row for row in initial.get('actors', []) if isinstance(row, dict)}
        bindings = {row.get('role_id'): row for row in initial.get('roles', []) if isinstance(row, dict)}
        wallets = {row.get('id'): row for row in initial.get('wallets', []) if isinstance(row, dict)}
        inventories = {row.get('id'): row for row in initial.get('inventories', []) if isinstance(row, dict)}
        households = dynamic['local_economy'].get('households', {})
        enabled = scenario.get('enabled', {})
        seen, assigned = set(), set()
        for position, mandate in enumerate(mandates):
            here = ('initial_mandates', position)
            if not isinstance(mandate, dict) or not REQUIRED <= set(mandate) or set(mandate) - REQUIRED - {'through_tick'} or not all(_id(mandate.get(key)) for key in IDS):
                issue('HOUSEHOLD_CONTROL_FIELDS', here, '须有唯一ID、具名本人、聚合户、来源岗位、权限、起止和本人接受')
                continue
            if mandate['id'] in seen or mandate['household_group_id'] in assigned:
                issue('HOUSEHOLD_CONTROL_DUPLICATE', here, '本版每户仅一份明确初态委托，不自动拼成共管')
            seen.add(mandate['id']); assigned.add(mandate['household_group_id'])
            if mandate['operation'] != OPERATION or mandate['actor_accepted'] is not True or type(mandate['effective_from_tick']) is not int or mandate['effective_from_tick'] != 0:
                issue('HOUSEHOLD_CONTROL_TERMS', here, '仅承认tick0已由本人接受的家庭供养配置权')
            if 'through_tick' in mandate and (type(mandate['through_tick']) is not int or not 0 <= mandate['through_tick'] <= 2147483647):
                issue('HOUSEHOLD_CONTROL_RANGE', here, '截止半日须为非负整数；未知期限不能写作null')
            actor = actors.get(mandate['actor_id'], {})
            role = roles.get(mandate['source_role_id'], {})
            binding = bindings.get(mandate['source_role_id'], {})
            if (mandate['actor_id'] not in enabled.get('actors', []) or actor.get('alive') is not True
                    or type(actor.get('age_years')) is not int or not 18 <= actor['age_years'] <= 120):
                issue('HOUSEHOLD_CONTROL_ACTOR', here, '受托人须为本情景已有初态的在世成年人')
            if (mandate['source_role_id'] not in enabled.get('roles', []) or not role
                    or 'organization_id' not in role or role['organization_id'] is not None
                    or role.get('initial_actor') != mandate['actor_id']
                    or binding.get('actor_id') != mandate['actor_id'] or binding.get('enabled') is not True):
                issue('HOUSEHOLD_CONTROL_ROLE', here, '须绑定本人明确在任的私务岗位，公职不授家庭私产配置权')
            household = households.get(mandate['household_group_id'], {})
            owner = {'kind': 'household', 'id': mandate['household_group_id']}
            if (not household or wallets.get(household.get('wallet_id'), {}).get('owner') != owner
                    or inventories.get(household.get('inventory_id'), {}).get('owner') != owner):
                issue('HOUSEHOLD_CONTROL_ASSETS', here, '真实聚合户的钱粮容器须同源归该户，不把受托人改为所有者')
    return issues
