"""NPC 首长只能答复真实当面申请，有限任期理由不替代原职和专项 Grant。"""
from copy import deepcopy
NAME='office_resignation_responses'
MODULE='political_office_resignation_responses'
CAPABILITY=MODULE+'_v1'
RESIGNATIONS='political_office_resignations'
TEMPLATE='office_resignation_response'
FIELD='office_resignation_response_intent_id'
IDS=('id','resignation_policy_id','approver_actor_id','director_policy_id')

def with_political_office_resignation_response_schema(schema):
    out=deepcopy(schema)
    group=out['$defs']['dynamic_world']['properties'][RESIGNATIONS]
    if group['properties']['version']['enum'] not in ([1],[1,2]):raise ValueError('Conflicting resignation versions')
    group['properties']['version']['enum']=[1,2]
    fields={k:{'type':'string','minLength':1,'maxLength':4096} for k in IDS}
    fields['approver_actor_id']={'$ref':'#/$defs/reference_actors'}
    fields['minimum_service_ticks']={'type':'integer','minimum':0,'maximum':4096}
    record={'type':'object','properties':fields,'required':list(IDS)+['minimum_service_ticks'],'additionalProperties':False}
    responses={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'intents':{'type':'object','minProperties':1,'maxProperties':64,'additionalProperties':record}},'required':['version','intents'],'additionalProperties':False}
    for name in ('dynamic_director','dynamic_director_legacy','dynamic_director_registry'):
        props=out['$defs'][name]['properties']
        if NAME in props and props[NAME]!=responses:raise ValueError('Conflicting resignation response schema')
        props[NAME]=deepcopy(responses)
    out['$defs']['dynamic_director_subject']['properties'][FIELD]={'type':'string','minLength':1,'maxLength':4096}
    def templates(v):
        if isinstance(v,dict):
            if isinstance(v.get('enum'),list) and 'maintain_commitment' in v['enum'] and TEMPLATE not in v['enum']:v['enum'].append(TEMPLATE)
            for child in v.values():templates(child)
        elif isinstance(v,list):
            for child in v:templates(child)
    templates(out)
    caps=out.get('properties',{}).get('required_capabilities',{}).get('items',{}).get('enum')
    if caps is not None:
        for cap in (RESIGNATIONS+'_v2',CAPABILITY):
            if cap not in caps:caps.append(cap)
    return out

def check_political_office_resignation_responses(pack,index):
    issues=[]
    for number,scenario in enumerate(pack.get('scenarios',[])):
        world=scenario.get('dynamic_world',{})
        if not isinstance(world,dict):continue
        director=world.get('director',{})
        if not isinstance(director,dict):continue
        root=('scenarios',number,'dynamic_world','director',NAME)
        def fail(code,text):issues.append((code,root,text))
        group=director.get(NAME);resignations=world.get(RESIGNATIONS,{})
        subjects=director.get('subjects',{});policies=director.get('policies',{})
        selected=[(key,p,key in subjects) for key,p in list(subjects.items())+list(policies.items()) if isinstance(p,dict) and (FIELD in p or TEMPLATE in p.get('templates',[]))]
        if group is None:
            if resignations.get('version')==2 or selected:fail('OfficeResignationResponseProfileRequired','辞任2和18模板须显式同启答复意图。')
            continue
        if type(resignations.get('version')) is not int or resignations['version']!=2 or not {RESIGNATIONS+'_v2',CAPABILITY}<=set(pack.get('required_capabilities',[])):fail('OfficeResignationResponseVersionPairRequired','须辞任2和有限答复1的明确能力。')
        if not isinstance(group,dict) or set(group)!={'version','intents'} or type(group.get('version')) is not int or group['version']!=1 or not isinstance(group.get('intents'),dict) or not 1<=len(group['intents'])<=64:
            fail('InvalidOfficeResignationResponseCatalog','只接受1版有限目录，不放运行回执。');continue
        valid={};used=set()
        for key,i in group['intents'].items():
            if not isinstance(i,dict) or set(i)!=set(IDS)|{'minimum_service_ticks'} or i.get('id')!=key or any(not isinstance(i.get(k),str) or not 0<len(i[k])<=4096 for k in IDS) or type(i.get('minimum_service_ticks')) is not int or not 0<=i['minimum_service_ticks']<=4096:
                fail('InvalidOfficeResignationResponseIntent','本人、策略、原辞任规程与有限任期门限须完整。');continue
            if i['resignation_policy_id'] in used:fail('OfficeResignationResponseDuplicateTarget','每个辞任规程只一个明确答复意图。')
            used.add(i['resignation_policy_id']);p=resignations.get('policies',{}).get(i['resignation_policy_id'],{});head=world.get('political_successions',{}).get('policies',{}).get(p.get('approval_succession_policy_id'),{});office=world.get('organization',{}).get('offices',{}).get(head.get('leader_office_id'),{})
            if not p or not head or not office or office.get('holder_actor_id')!=i['approver_actor_id'] or office.get('deputy_actor_id') is not None or i['approver_actor_id'] not in index.get('actors',{}) or i['approver_actor_id'] not in scenario.get('enabled',{}).get('actors',[]):fail('OfficeResignationResponseOriginalApproverRequired','只显式原首长本人可办理，副手不能冒审批人。')
            valid[key]=(i,head)
        for key,p,legacy in selected:
            pair=valid.get(p.get(FIELD));i,head=pair if pair else ({},{})
            if not pair or key!=i.get('director_policy_id') or p.get('organization_id')!=head.get('organization_id') or TEMPLATE not in p.get('templates',[]) or (legacy and key!=i.get('approver_actor_id')):fail('OfficeResignationResponseDirectorPolicyMismatch','原策略与本人意图双向一致。')
        for i,head in valid.values():
            p=policies.get(i['director_policy_id'],subjects.get(i['director_policy_id'],{}))
            if p.get(FIELD)!=i['id'] or TEMPLATE not in p.get('templates',[]) or p.get('organization_id')!=head.get('organization_id'):fail('OfficeResignationResponseDirectorPolicyMismatch','每条意图必须有真实策略producer。')
    return issues
