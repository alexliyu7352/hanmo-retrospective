"""公共接待移交只绑定原首职、纯接待Role和专项Grant，旧职责与文书不从今日岗位回填。"""
from copy import deepcopy
NAME='political_office_receptions'
CAPABILITY=NAME+'_v1'
COMMAND='political_assume_office_reception'
FIELDS={'id','succession_policy_id','role_id','reception_grant_id'}

def with_political_office_reception_schema(schema):
    """仅新增独立有限作者组，旧模块字段和版本不变。"""
    out=deepcopy(schema)
    row={'type':'object','properties':{k:{'$ref':'#/$defs/entity_id'} for k in sorted(FIELDS)},'required':sorted(FIELDS),'additionalProperties':False}
    # 只有跨包Role是全局实体；原policy/Office Grant仍为原动态账局部ID。
    row['properties']['role_id']={'$ref':'#/$defs/reference_roles'}
    group={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'object','minProperties':1,'maxProperties':4096,'additionalProperties':row}},'required':['version','policies'],'additionalProperties':False}
    props=out['$defs']['dynamic_world']['properties']
    if NAME in props and props[NAME]!=group:raise ValueError('Conflicting office reception schema')
    props[NAME]=group
    caps=out.get('properties',{}).get('required_capabilities',{}).get('items',{}).get('enum')
    if caps is not None and CAPABILITY not in caps:caps.append(CAPABILITY)
    return out

def check_political_office_receptions(pack,index):
    """原首府、旧Role初始人和专项Grant严格同源；不默许改教学、承包或OrgDuty责任。"""
    issues=[]
    for n,scenario in enumerate(pack.get('scenarios',[])):
        dynamic=scenario.get('dynamic_world',{})
        if not isinstance(dynamic,dict) or NAME not in dynamic:continue
        root=('scenarios',n,'dynamic_world',NAME)
        def fail(code,key,text):issues.append((code,root+tuple(key),text))
        group=dynamic[NAME];first=dynamic.get('political_successions',{})
        if (not isinstance(group,dict) or set(group)!={'version','policies'} or type(group.get('version')) is not int or group['version']!=1 or not isinstance(group.get('policies'),dict) or not 1<=len(group['policies'])<=4096):
            fail('PoliticalOfficeReceptionProfileMismatch',(),'须显式版本1与有限纯接待条款。');continue
        if (not {CAPABILITY,'political_successions_v1'}<=set(pack.get('required_capabilities',[])) or not isinstance(first,dict) or type(first.get('version')) is not int or first['version']!=1 or not isinstance(first.get('policies'),dict)):
            fail('PoliticalOfficeReceptionOriginalProfileRequired',(),'原首职目录及新能力必须明确。');continue
        org=dynamic.get('organization',{});used=set();enabled=scenario.get('enabled',{})
        for key,p in group['policies'].items():
            if not isinstance(p,dict) or set(p)!=FIELDS or p.get('id')!=key or any(not isinstance(p.get(k),str) or not 0<len(p[k])<=4096 for k in FIELDS):
                fail('PoliticalOfficeReceptionPolicyMismatch',(key,),'四个具名引用必须精确。');continue
            original=first['policies'].get(p['succession_policy_id'],{});office=org.get('offices',{}).get(original.get('leader_office_id'),{});role=index.get('roles',{}).get(p['role_id'],{});grant=org.get('grants',{}).get(p['reception_grant_id'],{});dialogue=index.get('dialogues',{}).get(role.get('dialogue_id'),{})
            if (not original or not office or p['role_id'] in used or not role or role.get('id')!=p['role_id'] or role.get('initial_actor')!=office.get('holder_actor_id') or not office.get('holder_actor_id') or role.get('organization_id')!=original.get('organization_id') or role.get('place_id')!=original.get('seat_place_id') or role.get('services')!=['information'] or not dialogue or dialogue.get('context_role')!=p['role_id'] or p['role_id'] not in enabled.get('roles',[]) or role.get('place_id') not in enabled.get('places',[])):
                fail('PoliticalOfficeReceptionOriginalRoleRequired',(key,),'只接原首任的同首府公共information岗位及原对白。')
            used.add(p['role_id'])
            if any(d.get('role_id')==p['role_id'] for d in org.get('duties',{}).values()):fail('PoliticalOfficeReceptionDutyDispositionRequired',(key,),'原OrgDuty固定岗位不得在本段挪用。')
            if (grant.get('id')!=p['reception_grant_id'] or grant.get('office_id')!=original.get('leader_office_id') or p['reception_grant_id'] not in office.get('grant_ids',[]) or grant.get('allowed_command_kinds')!=[COMMAND] or grant.get('scopes')!=[{'kind':'site','id':original.get('seat_place_id')}] or type(grant.get('amount_limit')) is not int or grant['amount_limit']!=0 or grant.get('wallet_ids')!=[] or grant.get('inventory_ids')!=[] or grant.get('may_delegate') is not False or type(grant.get('valid_from')) is not int or grant['valid_from']<0 or (grant.get('valid_until') is not None and (type(grant['valid_until']) is not int or grant['valid_until']<grant['valid_from']))):
                fail('PoliticalOfficeReceptionSpecificGrantRequired',(key,),'唯一零金额、不可委托的柜台专项Grant须覆盖真实签字时刻。')
    return issues
