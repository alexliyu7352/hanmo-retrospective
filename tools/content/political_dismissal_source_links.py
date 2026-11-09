"""原撤职独立历史来源目录；不新增任免命令、资源授权或初始撤职结果。"""
from copy import deepcopy
NAME = 'political_dismissal_sources'
CAPABILITY = NAME + '_v1'
FIELDS = {'id', 'jurisdiction_id', 'issuer_office_id', 'grant_id'}

def with_political_dismissal_source_schema(schema):
    """新增有限独立目录，不改变旧politics/Grant字段。"""
    out = deepcopy(schema)
    row = {'type': 'object', 'properties': {k: {'$ref': '#/$defs/entity_id'} for k in sorted(FIELDS)},
           'required': sorted(FIELDS), 'additionalProperties': False}
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]},
             'policies': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096, 'additionalProperties': row}},
             'required': ['version', 'policies'], 'additionalProperties': False}
    props = out['$defs']['dynamic_world']['properties']
    if NAME in props and props[NAME] != group:
        raise ValueError('Conflicting dismissal source schema')
    props[NAME] = group
    caps = out.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if caps is not None and CAPABILITY not in caps:
        caps.append(CAPABILITY)
    return out

def check_political_dismissal_sources(pack, index):
    """原同治所上级与同组织目标、独立零资源首个Grant、稳定初辖区须一一对应。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        d = scenario.get('dynamic_world', {})
        if not isinstance(d, dict) or NAME not in d:
            continue
        root = ('scenarios', number, 'dynamic_world', NAME)
        def fail(code, key, text):
            issues.append((code, root + tuple(key), text))
        group = d[NAME]
        if not isinstance(group, dict) or set(group) != {'version', 'policies'} or type(group.get('version')) is not int or group['version'] != 1 or not isinstance(group.get('policies'), dict) or not 1 <= len(group['policies']) <= 4096:
            fail('PoliticalDismissalSourceProfileRequired', (), '须显式有限1版原撤职来源目录。')
            continue
        if not {CAPABILITY, 'family_life_v1'} <= set(pack.get('required_capabilities', [])) or scenario.get('family_life', {}).get('profile') != 'verified_military_deaths_v1' or not d.get('politics', {}).get('enabled'):
            fail('PoliticalDismissalOriginalProfileRequired', (), '须原政治/生命来源与明确新能力。')
            continue
        org, politics = d.get('organization', {}), d.get('politics', {})
        seen = set()
        for key, p in group['policies'].items():
            if not isinstance(p, dict) or set(p) != FIELDS or p.get('id') != key or any(not isinstance(p.get(k), str) or not 0 < len(p[k]) <= 4096 for k in FIELDS):
                fail('PoliticalDismissalPolicyMismatch', (key,), '字段和表键须精确。')
                continue
            j = politics.get('jurisdictions', {}).get(p['jurisdiction_id'], {})
            leader = org.get('offices', {}).get(p['issuer_office_id'], {})
            target = org.get('offices', {}).get(j.get('office_id'), {})
            organization = org.get('organizations', {}).get(j.get('political_organization_id'), {})
            g = org.get('grants', {}).get(p['grant_id'], {})
            if not j or type(j.get('revision')) is not int or j['revision'] != 0 or j.get('political_organization_id') != j.get('controlling_organization_id') or j.get('id') in seen or not leader or leader.get('id') != p['issuer_office_id'] or leader.get('organization_id') != j.get('political_organization_id') or not target or target.get('id') == leader.get('id') or target.get('organization_id') != j.get('political_organization_id') or leader.get('deputy_actor_id') is not None or target.get('deputy_actor_id') is not None or organization.get('status') != 'active' or organization.get('headquarters_site') != j.get('seat_place_id') or leader.get('id') not in organization.get('office_ids', []) or any(v.get('office_id') == leader.get('id') for v in politics.get('jurisdictions', {}).values()):
                fail('PoliticalDismissalOriginalOfficeRequired', (key,), '只处理原独立上级和同治所岗位，跨辖区及副手另有制度。')
            seen.add(j.get('id'))
            if g.get('id') != p['grant_id'] or g.get('office_id') != leader.get('id') or p['grant_id'] not in leader.get('grant_ids', []) or g.get('allowed_command_kinds') != ['political_dismiss'] or g.get('scopes') != [{'kind': 'site', 'id': j.get('seat_place_id')}] or type(g.get('amount_limit')) is not int or g['amount_limit'] != 0 or g.get('wallet_ids') != [] or g.get('inventory_ids') != [] or g.get('may_delegate') is not False or type(g.get('valid_from')) is not int or g['valid_from'] < 0 or (g.get('valid_until') is not None and (type(g['valid_until']) is not int or g['valid_until'] < g['valid_from'])):
                fail('PoliticalDismissalSpecificGrantRequired', (key,), '须原撤职专项零资源Grant。')
    return issues
