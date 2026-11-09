"""原院接册只接续原司法记录；新处分仍需另一个实际命令的专项权源。"""
CAPABILITY = 'political_shop_registry_successions_v1'
COMMAND = 'political_accept_shop_registry'
FIELDS = {'id', 'court_id', 'organization_id', 'office_id', 'role_id', 'dialogue_id', 'request_node_id'}


def check_political_shop_registry_successions(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        config = dynamic.get('political_shop_registry_successions')
        if config is None:
            continue
        root = ('scenarios', number, 'dynamic_world', 'political_shop_registry_successions')
        def issue(code, key, text):
            issues.append((code, root + tuple(key), text))
        if (not isinstance(config, dict) or set(config) != {'enabled', 'version', 'policies'}
                or config.get('enabled') is not True or type(config.get('version')) is not int
                or config['version'] != 1 or not isinstance(config.get('policies'), dict)
                or not 1 <= len(config['policies']) <= 4096):
            issue('POLITICAL_SHOP_REGISTRY_PROFILE', (), '须显式完整接册1与有限后继政策')
            continue
        law, shops, takeover = (dynamic.get(key, {}) for key in ('political_shop_licensing', 'private_shops', 'political_takeover'))
        if (CAPABILITY not in pack.get('required_capabilities', []) or type(law.get('version')) is not int
                or law.get('version') != 1 or shops.get('enabled') is not True or type(shops.get('version')) is not int
                or shops.get('version') != 1 or takeover.get('enabled') is not True
                or type(takeover.get('version')) is not int or takeover['version'] != 1):
            issue('POLITICAL_SHOP_REGISTRY_DEPENDENCIES', (), '依赖原法律1、首店1、接管1，不自动开启或补完旧源')
        organization = dynamic.get('organization', {})
        offices, grants = organization.get('offices', {}), organization.get('grants', {})
        jurisdictions = dynamic.get('politics', {}).get('jurisdictions', {})
        enabled = scenario.get('enabled', {})
        pairs = set()
        for key, policy in config['policies'].items():
            if (not isinstance(policy, dict) or set(policy) != FIELDS
                    or any(not isinstance(policy.get(field), str) or not 0 < len(policy[field]) <= 4096 for field in FIELDS)
                    or policy['id'] != key):
                issue('POLITICAL_SHOP_REGISTRY_POLICY', (key,), '后继政策须精确原院、公职与柜台，不可含运行批准或免罪字段')
                continue
            pair = policy['court_id'], policy['organization_id']
            if pair in pairs:
                issue('POLITICAL_SHOP_REGISTRY_POLICY', (key,), '同院同政权只声明一个固定后继柜台')
            pairs.add(pair)
            court = law.get('courts', {}).get(policy['court_id'], {})
            jurisdiction = jurisdictions.get(court.get('jurisdiction_id'), {})
            seat = jurisdiction.get('seat_place_id')
            office = offices.get(policy['office_id'], {})
            role = index.get('roles', {}).get(policy['role_id'], {})
            dialogue = index.get('dialogues', {}).get(policy['dialogue_id'], {})
            nodes = [node for node in dialogue.get('nodes', []) if node.get('id') == policy['request_node_id']]
            if (not court or not jurisdiction or office.get('organization_id') != policy['organization_id']
                    or role.get('organization_id') != policy['organization_id'] or role.get('place_id') != seat
                    or role.get('dialogue_id') != policy['dialogue_id'] or dialogue.get('context_role') != policy['role_id']
                    or len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') != '$provider'
                    or policy['role_id'] not in enabled.get('roles', []) or policy['dialogue_id'] not in enabled.get('dialogues', [])
                    or seat not in enabled.get('places', [])):
                issue('POLITICAL_SHOP_REGISTRY_ENDPOINT', (key,), '后继仅在原辖区柜台，由本政权既有公职办理')
            declared = any(g.get('id') == grant_id and g.get('office_id') == policy['office_id']
                and COMMAND in g.get('allowed_command_kinds', []) and not g.get('wallet_ids') and not g.get('inventory_ids')
                and type(g.get('amount_limit', 0)) is int and g.get('amount_limit', 0) >= 0
                and {'kind': 'site', 'id': seat} in g.get('scopes', [])
                for grant_id in office.get('grant_ids', []) for g in [grants.get(grant_id, {})])
            if not declared:
                issue('POLITICAL_SHOP_REGISTRY_GRANT', (key,), '接收原册须独立接册命令的原Office/site专项Grant，不能借签证或军官权')
    return issues
