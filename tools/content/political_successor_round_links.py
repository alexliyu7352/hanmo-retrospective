"""已正式任职者的新轮提名只选独立有限政策和零资金专项Grant；不改死亡继位1默认行为。"""
from copy import deepcopy

NAME = 'political_successor_rounds'
CAPABILITY = NAME + '_v1'
TEXT_FIELDS = ('id', 'succession_policy_id', 'nomination_grant_id')
FIELDS = set(TEXT_FIELDS) | {'response_ticks', 'accession_ticks'}
COMMANDS = {'political_nominate_next_successor', 'political_withdraw_next_successor'}


def with_political_successor_round_schema(schema):
    """只加入显式独立作者选择；旧动态世界不自动生成任何意愿或任职。"""
    result = deepcopy(schema)
    policy = {'type': 'object', 'properties': {
        **{key: {'$ref': '#/$defs/entity_id'} for key in TEXT_FIELDS},
        **{key: {'type': 'integer', 'minimum': 1, 'maximum': 2147483647} for key in ('response_ticks', 'accession_ticks')}},
        'required': sorted(FIELDS), 'additionalProperties': False}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
        'policies': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096,
                     'additionalProperties': policy}}, 'required': ['version', 'policies'], 'additionalProperties': False}
    properties = result['$defs']['dynamic_world']['properties']
    if NAME in properties and properties[NAME] != group:
        raise ValueError('Conflicting political successor round schema')
    properties[NAME] = group
    caps = result.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if caps is not None and CAPABILITY not in caps:
        caps.append(CAPABILITY)
    return result


def check_political_successor_rounds(pack, index):
    """原政策、真实任期和新Grant同源，候选接受点仍须真实当前权源重新办理。"""
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
            fail('PoliticalSuccessorRoundProfileMismatch', (), '须有显式版本1及有限政策，不能預置已提名或已就任事实。')
            continue
        old = dynamic.get('political_successions', {})
        if (CAPABILITY not in pack.get('required_capabilities', [])
                or 'political_successions_v1' not in pack.get('required_capabilities', [])
                or type(old.get('version')) is not int or old['version'] != 1
                or not isinstance(old.get('policies'), dict)):
            fail('PoliticalSuccessorRoundOriginalProfileRequired', (), '须保原显式首职继位政策，并独立声明新轮提名能力。')
        org = dynamic.get('organization', {})
        used = set()
        for key, p in group['policies'].items():
            if (not isinstance(p, dict) or set(p) != FIELDS or p.get('id') != key
                    or any(not isinstance(p.get(name), str) or not 0 < len(p[name]) <= 4096 for name in TEXT_FIELDS)
                    or any(type(p.get(name)) is not int or not 1 <= p[name] <= 2147483647 for name in ('response_ticks', 'accession_ticks'))):
                fail('PoliticalSuccessorRoundPolicyMismatch', (key,), '原政策引用、专项Grant和有限答复期必须明确。')
                continue
            original = old.get('policies', {}).get(p['succession_policy_id'], {})
            office = org.get('offices', {}).get(original.get('leader_office_id'), {})
            grant = org.get('grants', {}).get(p['nomination_grant_id'], {})
            if not original or p['succession_policy_id'] in used:
                fail('PoliticalSuccessorRoundOriginalPolicyRequired', (key,), '每项原首职政策最多对应一个新轮提名规则。')
            used.add(p['succession_policy_id'])
            if (grant.get('id') != p['nomination_grant_id']
                    or p['nomination_grant_id'] == original.get('nomination_grant_id')
                    or grant.get('office_id') != original.get('leader_office_id')
                    or p['nomination_grant_id'] not in office.get('grant_ids', [])
                    or type(grant.get('amount_limit')) is not int or grant['amount_limit'] != 0
                    or grant.get('wallet_ids') != [] or grant.get('inventory_ids') != []
                    or not COMMANDS.issubset(set(grant.get('allowed_command_kinds', [])))
                    or {'kind': 'site', 'id': original.get('seat_place_id')} not in grant.get('scopes', [])):
                fail('PoliticalSuccessorRoundSpecificGrantRequired', (key,), '同一原首职专项Grant须完整授权签发/撤回，不绑定钱货预算。')
    return issues
