"""普通公职辞任是本人申请和原首职专项批准；作者不注入申请/批准或另一份任职真值。"""
from copy import deepcopy
NAME='political_office_resignations'
CAPABILITY=NAME+'_v1'
COMMAND='political_answer_office_resignation'
FIELDS={'id','jurisdiction_id','approval_succession_policy_id','approval_grant_id','response_ticks'}

def with_political_office_resignation_schema(schema):
    out=deepcopy(schema)
    row={'type':'object','properties':{k:{'$ref':'#/$defs/entity_id'} for k in sorted(FIELDS-{'response_ticks'})},'required':sorted(FIELDS),'additionalProperties':False}
    row['properties']['response_ticks']={'type':'integer','minimum':1,'maximum':2147483647}
    group={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'object','minProperties':1,'maxProperties':4096,'additionalProperties':row}},'required':['version','policies'],'additionalProperties':False}
    props=out['$defs']['dynamic_world']['properties']
    if NAME in props and props[NAME]!=group:raise ValueError('Conflicting resignation schema')
    props[NAME]=group
    caps=out.get('properties',{}).get('required_capabilities',{}).get('items',{}).get('enum')
    if caps is not None and CAPABILITY not in caps:caps.append(CAPABILITY)
    return out

def check_political_office_resignations(pack,index):
    issues=[]
    for n,scenario in enumerate(pack.get('scenarios',[])):
        dynamic=scenario.get('dynamic_world',{})
        if not isinstance(dynamic,dict) or NAME not in dynamic:continue
        root=('scenarios',n,'dynamic_world',NAME)
        def fail(code,key,text):issues.append((code,root+tuple(key),text))
        group=dynamic[NAME];heads=dynamic.get('political_successions',{})
        if not isinstance(group,dict) or set(group)!={'version','policies'} or type(group.get('version')) is not int or group['version'] not in (1,2) or not isinstance(group.get('policies'),dict) or not 1<=len(group['policies'])<=4096:
            fail('PoliticalOfficeResignationProfileMismatch',(),'须显式版本1或2和有限辞任规程。');continue
        if not {NAME+'_v'+str(group['version']),'political_successions_v1'}<=set(pack.get('required_capabilities',[])) or not isinstance(heads,dict) or type(heads.get('version')) is not int or heads['version']!=1 or not isinstance(heads.get('policies'),dict):
            fail('PoliticalOfficeResignationOriginalProfileRequired',(),'须原首职目录和两项显式能力。');continue
        org=dynamic.get('organization',{});politics=dynamic.get('politics',{});used=set()
        if not isinstance(org,dict) or not isinstance(politics,dict):
            fail('PoliticalOfficeResignationOriginalProfileRequired',(),'须真实组织与辖区目录。');continue
        for key,p in group['policies'].items():
            if not isinstance(p,dict) or set(p)!=FIELDS or p.get('id')!=key or any(not isinstance(p.get(k),str) or not 0<len(p[k])<=4096 for k in FIELDS-{'response_ticks'}) or type(p.get('response_ticks')) is not int or not 1<=p['response_ticks']<=2147483647:
                fail('PoliticalOfficeResignationPolicyMismatch',(key,),'具名引用和正整数答复窗必须完整。');continue
            j=politics.get('jurisdictions',{}).get(p['jurisdiction_id'],{});head=heads['policies'].get(p['approval_succession_policy_id'],{});target=org.get('offices',{}).get(j.get('office_id'),{});superior=org.get('offices',{}).get(head.get('leader_office_id'),{});g=org.get('grants',{}).get(p['approval_grant_id'],{})
            if not j or j.get('revision')!=0 or type(j.get('revision')) is not int or j.get('controlling_organization_id')!=j.get('political_organization_id') or not head or head.get('organization_id')!=j.get('political_organization_id') or not target or target.get('organization_id')!=head.get('organization_id') or target.get('id')==head.get('leader_office_id') or target.get('deputy_actor_id') is not None or not superior or superior.get('deputy_actor_id') is not None or superior.get('organization_id')!=head.get('organization_id') or target.get('id') in used or j.get('seat_place_id') not in scenario.get('enabled',{}).get('places',[]):
                fail('PoliticalOfficeResignationSubordinateOfficeRequired',(key,),'只办理原同组织非首职且无副手的辖区职务。')
            used.add(target.get('id'))
            if g.get('id')!=p['approval_grant_id'] or g.get('office_id')!=head.get('leader_office_id') or p['approval_grant_id'] not in superior.get('grant_ids',[]) or g.get('allowed_command_kinds')!=[COMMAND] or g.get('scopes')!=[{'kind':'site','id':head.get('seat_place_id')}] or type(g.get('amount_limit')) is not int or g['amount_limit']!=0 or g.get('may_delegate') is not False or g.get('wallet_ids')!=[] or g.get('inventory_ids')!=[] or type(g.get('valid_from')) is not int or g['valid_from']<0 or (g.get('valid_until') is not None and (type(g['valid_until']) is not int or g['valid_until']<g['valid_from'])):
                fail('PoliticalOfficeResignationApprovalGrantRequired',(key,),'仅原首职零金额、不可转委、同座所的专项批准权。')
    return issues
