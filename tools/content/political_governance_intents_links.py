"""NPC原安民一次意图显式登记；只选原双方和原政策，不携运行事实或代行续办许可。"""
from copy import deepcopy

NAME = 'political_governance_intents'
CAPABILITY = NAME + '_v1'
TEMPLATE = 'governance_commission'
FIELDS = ('id', 'issuer_actor_id', 'executor_actor_id', 'office_id', 'jurisdiction_id',
          'commission_policy_id', 'public_inventory_id')


def with_political_governance_intents_schema(schema):
    """新意图目录与原Director政策字段分别显式开启，旧缺省目录不加值。"""
    out = deepcopy(schema)
    record = {'type': 'object', 'properties': {key: {'type': 'string', 'minLength': 1, 'maxLength': 4096} for key in FIELDS},
              'required': list(FIELDS), 'additionalProperties': False}
    record['properties']['issuer_actor_id'] = {'$ref': '#/$defs/reference_actors'}
    record['properties']['executor_actor_id'] = {'$ref': '#/$defs/reference_actors'}
    record['properties']['public_inventory_id'] = {'$ref': '#/$defs/reference_inventories'}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
            'intents': {'type': 'object', 'minProperties': 1, 'maxProperties': 64,
                        'additionalProperties': record}},
             'required': ['version', 'intents'], 'additionalProperties': False}
    for name in ('dynamic_director', 'dynamic_director_legacy', 'dynamic_director_registry'):
        props = out['$defs'][name]['properties']
        if NAME in props and props[NAME] != group:
            raise ValueError('Conflicting governance intents schema')
        props[NAME] = deepcopy(group)
    out['$defs']['dynamic_director_subject']['properties']['governance_intent_id'] = {
        'type': 'string', 'minLength': 1, 'maxLength': 4096}
    def templates(value):
        """仅扩原具名模板枚举，不影响其他整数或业务版本。"""
        if isinstance(value, dict):
            if isinstance(value.get('enum'), list) and 'maintain_commitment' in value['enum'] and TEMPLATE not in value['enum']:
                value['enum'].append(TEMPLATE)
            for child in value.values():
                templates(child)
        elif isinstance(value, list):
            for child in value:
                templates(child)
    templates(out)
    capabilities = out.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if capabilities is not None and CAPABILITY not in capabilities:
        capabilities.append(CAPABILITY)
    return out


def check_political_governance_intents(pack, index):
    """只验作者宣告的意愿和旧权源覆盖，实际当前任职/在场/足额钱粮仍由原preview核。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict):
            continue
        director = world.get('director', {})
        policies = list(director.get('subjects', {}).items()) + list(director.get('policies', {}).items())
        selected = [(key, policy) for key, policy in policies
                    if TEMPLATE in policy.get('templates', []) or 'governance_intent_id' in policy]
        if NAME not in director:
            if selected:
                issues.append(('GovernanceIntentProfileRequired', ('scenarios', number, 'dynamic_world', 'director'),
                               '安民签发/受领模板须显式选择对应意图组。'))
            continue
        root = ('scenarios', number, 'dynamic_world', 'director', NAME)
        def fail(code, message):
            """保持作者诊断的原路径格式。"""
            issues.append((code, root, message))
        group = director[NAME]
        if not isinstance(group, dict) or set(group) != {'version', 'intents'} or type(group['version']) is not int or group['version'] != 1:
            fail('GovernanceIntentVersionMismatch', '须明确安民意图 version 1 和唯一 intents 表。'); continue
        intents = group['intents']
        if not isinstance(intents, dict) or not 1 <= len(intents) <= 64:
            fail('GovernanceIntentLimit', '意图表须为 1 至 64 条具名记录。'); continue
        if CAPABILITY not in pack.get('required_capabilities', []):
            fail('GovernanceIntentCapabilityRequired', '须声明对应原生能力。')
        organization, politics, governance, relief = (
            world.get(key, {}) for key in ('organization', 'politics', 'politics_governance', 'public_relief'))
        if world.get('profile') != 'local_society_v1' or any(value.get('enabled') is not True for value in (organization, politics, governance)) or type(relief.get('version')) is not int or relief['version'] != 1:
            fail('GovernanceIntentDependenciesRequired', '须先启原政治、治理任务和公共赈济组。')
        seen = set()
        for key, record in intents.items():
            if not isinstance(record, dict) or set(record) != set(FIELDS) or record.get('id') != key or any(not isinstance(record.get(field), str) or not 0 < len(record[field]) <= 4096 for field in FIELDS) or record['issuer_actor_id'] == record['executor_actor_id']:
                fail('InvalidGovernanceIntent', '意图须明确不同签发者和本人受领者，不能预置运行状态。'); continue
            identity = tuple(record[field] for field in FIELDS[1:])
            if identity in seen:
                fail('DuplicateGovernanceIntent', '同一对双方、原署和原政策不能重复登记。')
            seen.add(identity)
            jurisdiction = politics.get('jurisdictions', {}).get(record['jurisdiction_id'], {})
            office = organization.get('offices', {}).get(record['office_id'], {})
            commission = governance.get('commission_policies', {}).get(record['commission_policy_id'], {})
            rule = governance.get('appeasement_rules', {}).get(commission.get('rule_id'), {})
            if any(record[field] not in index.get('actors', {}) or record[field] not in scenario.get('enabled', {}).get('actors', []) for field in ('issuer_actor_id', 'executor_actor_id')) or not rule or office.get('organization_id') != jurisdiction.get('political_organization_id') or record['public_inventory_id'] not in jurisdiction.get('public_inventory_ids', []):
                fail('GovernanceIntentSourceMismatch', '双方、原辖区、原署和原委任政策必须实际存在。'); continue
            covered = False
            for grant_id in office.get('grant_ids', []):
                grant = organization.get('grants', {}).get(grant_id, {})
                if grant.get('office_id') == record['office_id'] and grant.get('may_delegate') is True and 'political_commission_governance' in grant.get('allowed_command_kinds', []) and {'kind': 'site', 'id': jurisdiction.get('seat_place_id')} in grant.get('scopes', []) and jurisdiction.get('public_wallet_id') in grant.get('wallet_ids', []) and record['public_inventory_id'] in grant.get('inventory_ids', []) and type(grant.get('amount_limit')) is int and type(rule.get('cash_amount')) is int and grant['amount_limit'] >= rule['cash_amount']:
                    covered = True
            if not covered:
                fail('GovernanceIntentOriginalGrantRequired', '原署必须有明确可委任他人的完整同库Grant。')
        if not selected:
            fail('GovernanceIntentDirectorPolicyRequired', '至少一个原Director政策须明确消费这份意图。')
        used = set()
        for key, policy in selected:
            intent = intents.get(policy.get('governance_intent_id'))
            if TEMPLATE not in policy.get('templates', []) or not isinstance(intent, dict):
                fail('GovernanceIntentDirectorPolicyMismatch', '模板与具名意图引用必须共同存在。'); continue
            used.add(policy['governance_intent_id'])
            jurisdiction = politics.get('jurisdictions', {}).get(intent.get('jurisdiction_id'), {})
            if policy.get('organization_id') != jurisdiction.get('political_organization_id') or (key in director.get('subjects', {}) and key not in (intent.get('issuer_actor_id'), intent.get('executor_actor_id'))):
                fail('GovernanceIntentDirectorSubjectMismatch', '只有原双方所属政策可消费此意图，不向旁人派发。')
        if used != set(intents):
            fail('GovernanceIntentUnused', '每份意图须被至少一份原主体政策明确选择。')
    return issues
