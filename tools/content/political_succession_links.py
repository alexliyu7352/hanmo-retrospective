"""原组织首职的显式继位政策；作者不预置提名、同意、死亡或就任事实。"""
from copy import deepcopy
NAME = 'political_successions'
CAPABILITY = NAME + '_v1'
TEXT_FIELDS = ('id', 'organization_id', 'leader_office_id', 'seat_place_id', 'nomination_grant_id', 'jurisdiction_id')
FIELDS = set(TEXT_FIELDS) | {'response_ticks', 'accession_ticks'}
COMMANDS = {'political_nominate_successor', 'political_withdraw_successor'}


def with_political_succession_schema(schema):
    """纯schema补片，不改变旧组织或Office定义的默认行为。"""
    result = deepcopy(schema)
    policy = {'type': 'object', 'properties': {
        **{name: {'$ref': '#/$defs/entity_id'} for name in TEXT_FIELDS},
        'seat_place_id': {'$ref': '#/$defs/reference_places'},
        'organization_id': {'$ref': '#/$defs/reference_organizations'},
        **{name: {'type': 'integer', 'minimum': 1, 'maximum': 2147483647}
           for name in ('response_ticks', 'accession_ticks')}},
        'required': sorted(FIELDS), 'additionalProperties': False}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
        'policies': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096,
                     'additionalProperties': policy}}, 'required': ['version', 'policies'], 'additionalProperties': False}
    properties = result['$defs']['dynamic_world']['properties']
    if NAME in properties and properties[NAME] != group:
        raise ValueError('Conflicting political succession schema')
    properties[NAME] = group
    caps = result.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if caps is not None and CAPABILITY not in caps:
        caps.append(CAPABILITY)
    return result


def check_political_successions(pack, index):
    """明确原组织、首职和辖区一一对应；生前意愿与原Task实交通知分别真实办理。"""
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
            fail('PoliticalSuccessionProfileMismatch', (), '须明确版本1和有限政策；不得预置运行事实。')
            continue
        if CAPABILITY not in pack.get('required_capabilities', []):
            fail('PoliticalSuccessionCapabilityRequired', (), '须声明显式政治继位能力。')
        org = dynamic.get('organization', {})
        politics = dynamic.get('politics', {})
        notices = scenario.get('task_message_notices', {})
        if (dynamic.get('profile') != 'local_society_v1' or org.get('enabled') is not True
                or not politics.get('jurisdictions')
                or scenario.get('family_life', {}).get('profile') != 'verified_military_deaths_v1'
                or type(notices.get('version')) is not int or notices['version'] != 1
                or notices.get('profile') != 'witnessed_death_letters_v1'
                or not notices.get('bindings')
                or scenario.get('task_message_handoff_profile') not in
                   ('observed_letter_delivery_v1', 'observed_letter_delivery_and_report_v2')):
            fail('PoliticalSuccessionDependenciesRequired', (), '须有原组织、政治辖区、真实死亡和实交通知来源。')
        organizations, offices = org.get('organizations', {}), org.get('offices', {})
        grants = org.get('grants', {})
        jurisdictions = politics.get('jurisdictions', {})
        used_orgs, used_offices = set(), set()
        for key, policy in group['policies'].items():
            if (not isinstance(policy, dict) or set(policy) != FIELDS or policy.get('id') != key
                    or any(not isinstance(policy.get(name), str) or not 0 < len(policy[name]) <= 4096 for name in TEXT_FIELDS)
                    or any(type(policy.get(name)) is not int or not 1 <= policy[name] <= 2147483647 for name in ('response_ticks', 'accession_ticks'))):
                fail('PoliticalSuccessionPolicyMismatch', (key,), '原首职、辖区、专项Grant与两个有限时窗必须精确声明。')
                continue
            principal = organizations.get(policy['organization_id'], {})
            office = offices.get(policy['leader_office_id'], {})
            court = jurisdictions.get(policy['jurisdiction_id'], {})
            if (policy['organization_id'] in used_orgs or policy['leader_office_id'] in used_offices
                    or principal.get('status') != 'active'
                    or principal.get('succession_policy_id') != key
                    or principal.get('headquarters_site') != policy['seat_place_id']
                    or policy['leader_office_id'] not in principal.get('office_ids', [])
                    or office.get('organization_id') != policy['organization_id']
                    or office.get('status') != 'active' or not office.get('holder_actor_id')
                    or court.get('office_id') != policy['leader_office_id']
                    or court.get('seat_place_id') != policy['seat_place_id']
                    or court.get('political_organization_id') != policy['organization_id']
                    or court.get('controlling_organization_id') != policy['organization_id']
                    or court.get('revision') != 0):
                fail('PoliticalSuccessionOriginalOfficeRequired', (key,), '只选同一原组织、原首府辖区和原在任首职。')
            used_orgs.add(policy['organization_id']); used_offices.add(policy['leader_office_id'])
            grant = grants.get(policy['nomination_grant_id'], {})
            if (grant.get('id') != policy['nomination_grant_id']
                    or grant.get('office_id') != policy['leader_office_id']
                    or policy['nomination_grant_id'] not in office.get('grant_ids', [])
                    or not COMMANDS.issubset(set(grant.get('allowed_command_kinds', [])))
                    or {'kind': 'site', 'id': policy['seat_place_id']} not in grant.get('scopes', [])):
                fail('PoliticalSuccessionNominationGrantRequired', (key,), '原同一专项Grant完整授权提名和撤回，不借副手身份。')
            if not any(binding.get('decedent_actor_id') == office.get('holder_actor_id') for binding in notices.get('bindings', []) if isinstance(binding, dict)):
                fail('PoliticalSuccessionDeathNoticePolicyRequired', (key,), '须有原首任死亡的正常实交通知政策；仍不预置死亡。')
    return issues
