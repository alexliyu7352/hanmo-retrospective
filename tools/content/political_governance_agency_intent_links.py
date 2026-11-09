"""三位NPC各持明确一次意图；作者声明不产生委任、Task或公共资金授权。"""
from copy import deepcopy

NAME = 'governance_agency_intents'
CAPABILITY = 'political_governance_agency_intents_v1'
AGENCY = 'political_governance_agencies'
TEMPLATE = 'governance_agency'
IDS = ('id', 'agency_policy_id', 'principal_actor_id', 'delegate_actor_id', 'executor_actor_id',
       'principal_director_policy_id', 'delegate_director_policy_id', 'executor_director_policy_id')
CHOICES = ('delegate_accepts', 'executor_accepts')


def with_political_governance_agency_intent_schema(schema):
    """只新2版目录可搭配17号意图；旧1版结构本身没有新增状态字段。"""
    out = deepcopy(schema)
    agency = out['$defs']['dynamic_world']['properties'][AGENCY]
    if agency['properties']['version']['enum'] not in ([1], [1, 2]):
        raise ValueError('Conflicting governance agency versions')
    agency['properties']['version']['enum'] = [1, 2]
    fields = {key: {'type': 'string', 'minLength': 1, 'maxLength': 4096} for key in IDS}
    for key in ('principal_actor_id', 'delegate_actor_id', 'executor_actor_id'):
        fields[key] = {'$ref': '#/$defs/reference_actors'}
    fields.update({key: {'type': 'boolean'} for key in CHOICES})
    record = {'type': 'object', 'properties': fields, 'required': list(IDS + CHOICES), 'additionalProperties': False}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
        'intents': {'type': 'object', 'minProperties': 1, 'maxProperties': 64, 'additionalProperties': record}},
        'required': ['version', 'intents'], 'additionalProperties': False}
    for name in ('dynamic_director', 'dynamic_director_legacy', 'dynamic_director_registry'):
        props = out['$defs'][name]['properties']
        if NAME in props and props[NAME] != group:
            raise ValueError('Conflicting governance agency intent schema')
        props[NAME] = deepcopy(group)
    out['$defs']['dynamic_director_subject']['properties']['governance_agency_intent_id'] = {'type': 'string', 'minLength': 1, 'maxLength': 4096}
    def templates(value):
        """仅扩明确模板名；是否有权启用仍由新版本与配对链接门核验。"""
        if isinstance(value, dict):
            if isinstance(value.get('enum'), list) and 'maintain_commitment' in value['enum'] and TEMPLATE not in value['enum']:
                value['enum'].append(TEMPLATE)
            for child in value.values(): templates(child)
        elif isinstance(value, list):
            for child in value: templates(child)
    templates(out)
    caps = out.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if caps is not None:
        for cap in (AGENCY + '_v2', CAPABILITY):
            if cap not in caps: caps.append(cap)
    return out


def check_political_governance_agency_intents(pack, index):
    """精确原首职、三本人和各策略引用；范围/Grant仍复用唯一原代理目录校验。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict): continue
        director = world.get('director', {})
        if not isinstance(director, dict): continue
        root = ('scenarios', number, 'dynamic_world', 'director', NAME)
        def fail(code, message):
            """沿原作者诊断路径报告，拒绝不会修改任何输入目录。"""
            issues.append((code, root, message))
        subjects, policies = director.get('subjects', {}), director.get('policies', {})
        selected = [(key, p, key in subjects) for key, p in list(subjects.items()) + list(policies.items())
                    if isinstance(p, dict) and (TEMPLATE in p.get('templates', []) or 'governance_agency_intent_id' in p)]
        group = director.get(NAME)
        agencies = world.get(AGENCY, {})
        if group is None:
            if selected or agencies.get('version') == 2: fail('GovernanceAgencyIntentProfileRequired', '2版与模板须显式选择本人意图组。')
            continue
        if agencies.get('version') != 2 or type(agencies.get('version')) is not int or any(cap not in pack.get('required_capabilities', []) for cap in (AGENCY + '_v2', CAPABILITY)):
            fail('GovernanceAgencyIntentVersionPairRequired', '须成对选择代理2版和意图1版能力。')
        if not isinstance(group, dict) or set(group) != {'version', 'intents'} or type(group.get('version')) is not int or group['version'] != 1 or not isinstance(group.get('intents'), dict) or not 1 <= len(group['intents']) <= 64:
            fail('InvalidGovernanceAgencyIntentCatalog', '只接受显式1版、1至64条意图和无运行载荷的目录。'); continue
        intents, used_agencies = group['intents'], set()
        valid = {}
        for key, intent in intents.items():
            if not isinstance(intent, dict) or set(intent) != set(IDS + CHOICES) or intent.get('id') != key or any(not isinstance(intent.get(k), str) or not 0 < len(intent[k]) <= 4096 for k in IDS) or any(type(intent.get(k)) is not bool for k in CHOICES):
                fail('InvalidGovernanceAgencyIntent', '三位本人、各策略与两项选择须全部明示。'); continue
            actors = [intent[k] for k in ('principal_actor_id', 'delegate_actor_id', 'executor_actor_id')]
            if len(set(actors)) != 3 or (not intent['delegate_accepts'] and intent['executor_accepts']) or intent['agency_policy_id'] in used_agencies:
                fail('InvalidGovernanceAgencyIntent', '三本人须不同；同原策略只一份意图，拒绝代理时不许暗设受领。')
            used_agencies.add(intent['agency_policy_id'])
            if any(a not in index.get('actors', {}) or a not in scenario.get('enabled', {}).get('actors', []) for a in actors):
                fail('GovernanceAgencyIntentActorRequired', '三本人须在本局真实作者目录与启用表。')
            policy = agencies.get('policies', {}).get(intent['agency_policy_id'], {})
            office = world.get('organization', {}).get('offices', {}).get(policy.get('office_id'), {})
            jurisdiction = world.get('politics', {}).get('jurisdictions', {}).get(policy.get('jurisdiction_id'), {})
            if not policy or office.get('holder_actor_id') != intent['principal_actor_id'] or office.get('deputy_actor_id') is not None or jurisdiction.get('office_id') != office.get('id') or jurisdiction.get('political_organization_id') != office.get('organization_id'):
                fail('GovernanceAgencyIntentOriginalOfficeRequired', '只原实际首长可提出代理，不能从副手或空职产生邀请。')
            valid[key] = intent
            for role, actor in zip(('principal', 'delegate', 'executor'), actors):
                policy_id = intent[role + '_director_policy_id']
                source = policies.get(policy_id) if director.get('registry_version') == 1 else subjects.get(policy_id)
                if not isinstance(source, dict) or TEMPLATE not in source.get('templates', []) or source.get('governance_agency_intent_id') != key or source.get('organization_id') != office.get('organization_id') or (director.get('registry_version') != 1 and policy_id != actor):
                    fail('GovernanceAgencyIntentDirectorPolicyMismatch', '三名当事人的原策略须逐一声明本意图与同一组织。')
        for key, policy, static in selected:
            intent = valid.get(policy.get('governance_agency_intent_id'))
            if not intent or TEMPLATE not in policy.get('templates', []) or key not in [intent[role + '_director_policy_id'] for role in ('principal', 'delegate', 'executor')]:
                fail('GovernanceAgencyIntentDirectorPolicyMismatch', '旁人策略不得消费当事人的显式意图。')
    return issues
