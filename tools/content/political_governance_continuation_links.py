"""明确已受领安民续办，只声明原真实接管规则对应的同ID未来公库。"""

from copy import deepcopy
NAME = 'political_governance_continuations'
CAPABILITY = NAME + '_v1'


def with_governance_continuation_schema(schema):
    """只声明显式version，不接受作者预置受领、占领、续办或结果绑定。"""
    out = deepcopy(schema)
    properties = out['$defs']['dynamic_world']['properties']
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]}},
             'required': ['version'], 'additionalProperties': False}
    if NAME in properties and properties[NAME] != group:
        raise ValueError('Conflicting governance continuation schema')
    properties[NAME] = group
    values = out.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if values is not None and CAPABILITY not in values:
        values.append(CAPABILITY)
    return out


def governance_continuation_grant_declares_assets(scenario, index, grant):
    """只解决目录初态产权；热门仍必须有原逐责任assume与新任完整专项Grant。"""
    world = scenario.get('dynamic_world', {})
    group = world.get('political_governance_continuations')
    if (not isinstance(group, dict) or type(group.get('version')) is not int
            or group['version'] != 1 or set(group) != {'version'}
            or grant.get('allowed_command_kinds') != ['political_continue_governance']):
        return False
    takeover, organization = world.get('political_takeover', {}), world.get('organization', {})
    if takeover.get('enabled') is not True or takeover.get('version') != 1:
        return False
    office = organization.get('offices', {}).get(grant.get('office_id'))
    wallets, stocks = grant.get('wallet_ids', []), grant.get('inventory_ids', [])
    if (not office or grant.get('id') not in office.get('grant_ids', [])
            or not isinstance(wallets, list) or len(wallets) != 1
            or not isinstance(stocks, list) or len(stocks) != 1
            or type(grant.get('amount_limit')) is not int or grant['amount_limit'] < 1
            or grant.get('continuity_policy_id') not in ('stop_when_vacant', 'continue_under_organization')):
        return False
    initial = scenario.get('initial', {})
    resources = {row['id']: row for row in initial.get('wallets', [])}
    resources.update({row['id']: row for row in initial.get('inventories', [])})
    for rule_id, rule in takeover.get('rules', {}).items():
        jurisdiction = world.get('politics', {}).get('jurisdictions', {}).get(rule.get('jurisdiction_id'))
        stronghold = index.get('strongholds', {}).get(rule.get('stronghold_id'))
        if (rule.get('id') != rule_id or not jurisdiction or not stronghold
                or rule.get('successor_office_id') != office.get('id')
                or stronghold.get('garrison_id') != rule.get('garrison_id')
                or grant.get('scopes') != [{'kind': 'site', 'id': rule.get('site_id')}]
                or wallets != [jurisdiction.get('public_wallet_id')]
                or stocks[0] not in jurisdiction.get('public_inventory_ids', [])
                or wallets[0] not in stronghold.get('public_wallet_ids', [])
                or stocks[0] not in stronghold.get('public_inventory_ids', [])):
            continue
        selected = [resources.get(value) for value in wallets + stocks]
        if all(row and row.get('owner', {}).get('kind') == 'organization'
               and row['owner'].get('id') in organization.get('organizations', {})
               for row in selected):
            return True
    return False


def check_political_governance_continuations(pack, index):
    """缺省不启用，显式新段必须同时具备原治理和接管完整作者组。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict) or 'political_governance_continuations' not in world:
            continue
        root = ('scenarios', number, 'dynamic_world', 'political_governance_continuations')
        def fail(code, message):
            issues.append((code, root, message))
        group = world['political_governance_continuations']
        if (not isinstance(group, dict) or set(group) != {'version'}
                or type(group.get('version')) is not int or group['version'] != 1):
            fail('GovernanceContinuationVersionMismatch', '安民续办须明确 version 1。')
            continue
        if 'political_governance_continuations_v1' not in pack.get('required_capabilities', []):
            fail('GovernanceContinuationCapabilityRequired', '安民续办须声明对应原生能力。')
        governance, relief, takeover = (world.get(key, {}) for key in ('politics_governance', 'public_relief', 'political_takeover'))
        if (world.get('profile') != 'local_society_v1' or not world.get('politics')
                or governance.get('enabled') is not True
                or type(relief.get('version')) is not int or relief['version'] != 1
                or takeover.get('enabled') is not True
                or type(takeover.get('version')) is not int or takeover['version'] != 1):
            fail('GovernanceContinuationDependenciesRequired', '安民续办依赖原政治治理、赈济与接管组。')
        grants = world.get('organization', {}).get('grants', {})
        selected = [grant for grant in grants.values()
                    if 'political_continue_governance' in grant.get('allowed_command_kinds', [])]
        if not selected:
            fail('GovernanceContinuationGrantRequired', '至少声明一份可实际办理的接任专项Grant。')
        for grant in selected:
            if not governance_continuation_grant_declares_assets(scenario, index, grant):
                fail('GovernanceContinuationGrantMismatch', '续办专项Grant须绑定同一接任署和辖区原钱粮库。')
    return issues
