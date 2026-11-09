"""后任生前让职只来自已正式就任的本任根与独立专项Grant，旧继位意愿不代替双方交接。"""
from copy import deepcopy

NAME = 'political_term_handovers'
CAPABILITY = NAME + '_v1'
TEXT_FIELDS = ('id', 'successor_round_policy_id', 'handover_grant_id')
FIELDS = set(TEXT_FIELDS) | {'response_ticks'}
COMMANDS = {'political_offer_term_handover', 'political_withdraw_term_handover'}


def with_political_term_handover_schema(schema):
    """仅登记有界条款，新局不得预置已签受或原Office结果。"""
    result = deepcopy(schema)
    policy = {'type': 'object', 'properties': {
        **{key: {'$ref': '#/$defs/entity_id'} for key in TEXT_FIELDS},
        'response_ticks': {'type': 'integer', 'minimum': 1, 'maximum': 2147483647}},
        'required': sorted(FIELDS), 'additionalProperties': False}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
        'policies': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096, 'additionalProperties': policy}},
        'required': ['version', 'policies'], 'additionalProperties': False}
    properties = result['$defs']['dynamic_world']['properties']
    if NAME in properties and properties[NAME] != group:
        raise ValueError('Conflicting political term handover schema')
    properties[NAME] = group
    caps = result.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if caps is not None and CAPABILITY not in caps:
        caps.append(CAPABILITY)
    return result


def check_political_term_handovers(pack, index):
    """新旧政策和原首职均须精确关联；专用Grant不得夹入钱货或其他公权命令。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or NAME not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', NAME)
        def fail(code, key, text):
            issues.append((code, path + tuple(key), text))
        group = dynamic[NAME]
        if (not isinstance(group, dict) or set(group) != {'version', 'policies'}
                or type(group.get('version')) is not int or group['version'] != 1
                or not isinstance(group.get('policies'), dict) or not 1 <= len(group['policies']) <= 4096):
            fail('PoliticalTermHandoverProfileMismatch', (), '须显式选择版本1和有限后任交接条款。')
            continue
        rounds = dynamic.get('political_successor_rounds', {})
        first = dynamic.get('political_successions', {})
        if (CAPABILITY not in pack.get('required_capabilities', [])
                or 'political_successor_rounds_v1' not in pack.get('required_capabilities', [])
                or 'political_successions_v1' not in pack.get('required_capabilities', [])
                or not isinstance(rounds, dict) or type(rounds.get('version')) is not int or rounds['version'] != 1
                or not isinstance(rounds.get('policies'), dict)
                or not isinstance(first, dict) or type(first.get('version')) is not int or first['version'] != 1
                or not isinstance(first.get('policies'), dict)):
            fail('PoliticalTermHandoverOriginalProfileRequired', (), '须保原首职与显式后任政策，新让职另行选择。')
            continue
        org = dynamic.get('organization', {})
        used = set()
        for key, p in group['policies'].items():
            if (not isinstance(p, dict) or set(p) != FIELDS or p.get('id') != key
                    or any(not isinstance(p.get(name), str) or not 0 < len(p[name]) <= 4096 for name in TEXT_FIELDS)
                    or type(p.get('response_ticks')) is not int or not 1 <= p['response_ticks'] <= 2147483647):
                fail('PoliticalTermHandoverPolicyMismatch', (key,), '原后任制度、独立Grant和有限期限必须明确。')
                continue
            prior = rounds['policies'].get(p['successor_round_policy_id'], {})
            original = first['policies'].get(prior.get('succession_policy_id'), {})
            office = org.get('offices', {}).get(original.get('leader_office_id'), {})
            grant = org.get('grants', {}).get(p['handover_grant_id'], {})
            if not prior or not original or p['successor_round_policy_id'] in used:
                fail('PoliticalTermHandoverOriginalPolicyRequired', (key,), '同一原首职后任制度只对应一项新生前交接。')
            used.add(p['successor_round_policy_id'])
            kinds = grant.get('allowed_command_kinds', [])
            if (grant.get('id') != p['handover_grant_id']
                    or p['handover_grant_id'] in (prior.get('nomination_grant_id'), original.get('nomination_grant_id'))
                    or grant.get('office_id') != original.get('leader_office_id')
                    or p['handover_grant_id'] not in office.get('grant_ids', [])
                    or type(grant.get('amount_limit')) is not int or grant['amount_limit'] != 0
                    or grant.get('wallet_ids') != [] or grant.get('inventory_ids') != []
                    or not isinstance(kinds, list) or len(kinds) != 2 or set(kinds) != COMMANDS
                    or {'kind': 'site', 'id': original.get('seat_place_id')} not in grant.get('scopes', [])):
                fail('PoliticalTermHandoverSpecificGrantRequired', (key,), '专项Grant只含本次签发与撤回，接受点仍重核当时权源。')
    return issues
