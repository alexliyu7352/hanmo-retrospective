"""治理邀请仅未受领本人答复；不预置运行事实或扩大原公约。"""
from copy import deepcopy
NAME = 'political_governance_invitation_replies'
CAPABILITY = NAME + '_v1'


def with_governance_invitation_schema(schema):
    """作者仅能明选版本，不能注入关闭事实、接受或提交绑定。"""
    value = deepcopy(schema)
    shape = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]}},
             'required': ['version'], 'additionalProperties': False}
    properties = value['$defs']['dynamic_world']['properties']
    if NAME in properties and properties[NAME] != shape:
        raise ValueError('Conflicting governance invitation reply schema')
    properties[NAME] = shape
    capabilities = value.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if capabilities is not None and CAPABILITY not in capabilities:
        capabilities.append(CAPABILITY)
    return value


def check_governance_invitation_replies(pack, index):
    """原政治/赈济/生命完整；撤回词必须同一可委任Grant明确声明。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or NAME not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', NAME)
        def fail(code, message):
            issues.append((code, path, message))
        group = dynamic[NAME]
        if not isinstance(group, dict) or set(group) != {'version'} or type(group['version']) is not int or group['version'] != 1:
            fail('GovernanceInvitationVersionMismatch', '邀请答复须明确 version 1。')
            continue
        if CAPABILITY not in pack.get('required_capabilities', []):
            fail('GovernanceInvitationCapabilityRequired', '须声明原生邀请答复能力。')
        relief = dynamic.get('public_relief', {})
        if (dynamic.get('profile') != 'local_society_v1' or not dynamic.get('politics')
                or dynamic.get('politics_governance', {}).get('enabled') is not True
                or type(relief.get('version')) is not int or relief['version'] != 1
                or scenario.get('family_life') != {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}):
            fail('GovernanceInvitationDependenciesRequired', '须有原政治治理、赈济与具名生命来源。')
        for grant in dynamic.get('organization', {}).get('grants', {}).values():
            kinds = grant.get('allowed_command_kinds', [])
            if 'political_withdraw_governance_invitation' in kinds:
                if 'political_commission_governance' not in kinds or grant.get('may_delegate') is not True:
                    fail('GovernanceInvitationWithdrawalGrantMismatch', '撤回只能在原可委任Grant明确加词。')
    return issues
