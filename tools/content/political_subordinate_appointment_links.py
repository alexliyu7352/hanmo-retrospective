"""真实下属空缺、原首职专项权和原政治事业意图；不注入提名、接受或到任结果。"""
from copy import deepcopy
NAME='political_subordinate_appointments'
INTENTS='political_subordinate_appointment_intents'
CAPABILITY=NAME+'_v1'
COMMANDS=['political_offer_subordinate_appointment','political_withdraw_subordinate_appointment']
FIELDS={'id','jurisdiction_id','head_succession_policy_id','appointment_grant_id','appointment_policy_id'}
INTENT_FIELDS={'id','appointment_policy_id','issuer_actor_id','candidate_actor_id','issuer_director_policy_id','candidate_director_policy_id'}

def with_political_subordinate_appointment_schema(schema):
    """只加两个具名独立组；原schema不原址修改。"""
    out=deepcopy(schema)
    for name,fields,table,maximum in [(NAME,FIELDS,'policies',4096),(INTENTS,INTENT_FIELDS,'intents',64)]:
        row={'type':'object','properties':{k:{'$ref':'#/$defs/entity_id'} for k in sorted(fields)},'required':sorted(fields),'additionalProperties':False}
        if name==INTENTS:
            for actor in ['issuer_actor_id','candidate_actor_id']:row['properties'][actor]={'$ref':'#/$defs/reference_actors'}
        group={'type':'object','properties':{'version':{'type':'integer','enum':[1]},table:{'type':'object','minProperties':1,'maxProperties':maximum,'additionalProperties':row}},'required':['version',table],'additionalProperties':False}
        props=out['$defs']['dynamic_world']['properties']
        if name in props and props[name]!=group:raise ValueError('Conflicting subordinate appointment schema')
        props[name]=group
        caps=out.get('properties',{}).get('required_capabilities',{}).get('items',{}).get('enum')
        if caps is not None and name+'_v1' not in caps:caps.append(name+'_v1')
    return out

def check_political_subordinate_appointments(pack,index):
    """同原目录逐条交叉引用，作者意图不能替代真实本人意愿或授予下级日常资源。"""
    issues=[];caps=set(pack.get('required_capabilities',[]))
    for n,s in enumerate(pack.get('scenarios',[])):
        d=s.get('dynamic_world',{})
        if not isinstance(d,dict) or (NAME not in d and INTENTS not in d):continue
        root=('scenarios',n,'dynamic_world',NAME)
        def fail(code,key,text):issues.append((code,root+tuple(key),text))
        g=d.get(NAME);heads=d.get('political_successions',{});org=d.get('organization',{});politics=d.get('politics',{})
        if not isinstance(g,dict) or set(g)!={'version','policies'} or type(g.get('version')) is not int or g['version']!=1 or not isinstance(g.get('policies'),dict) or not 1<=len(g['policies'])<=4096:
            fail('PoliticalSubordinateAppointmentProfileRequired',(),'必须显式有限1版补任目录。');continue
        if not {CAPABILITY,'political_successions_v1'}<=caps or not isinstance(heads,dict) or heads.get('version')!=1 or not isinstance(heads.get('policies'),dict) or not isinstance(org,dict) or not isinstance(politics,dict):
            fail('PoliticalSubordinateOriginalProfileRequired',(),'须同候选原首职、辖区与组织目录。');continue
        seen=set();grants=set()
        for key,p in g['policies'].items():
            if not isinstance(p,dict) or set(p)!=FIELDS or p.get('id')!=key or any(not isinstance(p.get(k),str) or not 0<len(p[k])<=4096 for k in FIELDS):
                fail('PoliticalSubordinatePolicyMismatch',(key,),'具名字段与表键必须精确。');continue
            j=politics.get('jurisdictions',{}).get(p['jurisdiction_id'],{});head=heads['policies'].get(p['head_succession_policy_id'],{});target=org.get('offices',{}).get(j.get('office_id'),{});leader=org.get('offices',{}).get(head.get('leader_office_id'),{});grant=org.get('grants',{}).get(p['appointment_grant_id'],{});terms=politics.get('appointment_policies',{}).get(p['appointment_policy_id'],{})
            if not j or type(j.get('revision')) is not int or j['revision']!=0 or j.get('political_organization_id')!=j.get('controlling_organization_id') or not head or head.get('organization_id')!=j.get('political_organization_id') or head.get('jurisdiction_id')==j.get('id') or not target or target.get('organization_id')!=head.get('organization_id') or target.get('id')==head.get('leader_office_id') or target.get('deputy_actor_id') is not None or not leader or leader.get('deputy_actor_id') is not None or target.get('id') in seen or j.get('seat_place_id') not in s.get('enabled',{}).get('places',[]):
                fail('PoliticalSubordinateOriginalOfficeRequired',(key,),'只填同组织原下属职位；首职、副手和易手另循原制度。')
            seen.add(target.get('id'))
            if grant.get('id')!=p['appointment_grant_id'] or grant.get('office_id')!=head.get('leader_office_id') or p['appointment_grant_id'] not in leader.get('grant_ids',[]) or grant.get('id') in grants or grant.get('allowed_command_kinds')!=COMMANDS or grant.get('scopes')!=[{'kind':'site','id':head.get('seat_place_id')}] or type(grant.get('amount_limit')) is not int or grant['amount_limit']!=0 or grant.get('may_delegate') is not False or grant.get('wallet_ids')!=[] or grant.get('inventory_ids')!=[] or type(grant.get('valid_from')) is not int or grant['valid_from']<0 or (grant.get('valid_until') is not None and (type(grant['valid_until']) is not int or grant['valid_until']<grant['valid_from'])):
                fail('PoliticalSubordinateAppointmentGrantRequired',(key,),'原首职专项零资源提名/撤回权，不传授下级Grant。')
            grants.add(grant.get('id'))
            if not terms or any(type(terms.get(k)) is not int or not 1<=terms[k]<=2147483647 for k in ['response_ticks','arrival_ticks','minimum_administration']) or type(terms.get('minimum_merit')) is not int or terms['minimum_merit']<0:
                fail('PoliticalSubordinateAppointmentTermsRequired',(key,),'须原资格门槛和有限答受/赴任窗。')
        if INTENTS not in d:continue
        intents=d[INTENTS];director=d.get('director',{})
        if not isinstance(intents,dict) or set(intents)!={'version','intents'} or type(intents.get('version')) is not int or intents['version']!=1 or not isinstance(intents.get('intents'),dict) or not 1<=len(intents['intents'])<=64 or INTENTS+'_v1' not in caps or not isinstance(director,dict):
            fail('PoliticalSubordinateIntentProfileRequired',(),'须明确原事业策略的有限NPC意图。');continue
        policies=director.get('policies',{}) if director.get('registry_version')==1 else director.get('subjects',{});pairs=set()
        for key,i in intents['intents'].items():
            if not isinstance(i,dict) or set(i)!=INTENT_FIELDS or i.get('id')!=key or any(not isinstance(i.get(k),str) or not 0<len(i[k])<=4096 for k in INTENT_FIELDS):fail('PoliticalSubordinateIntentMismatch',(key,),'意图字段与表键须精确。');continue
            p=g['policies'].get(i['appointment_policy_id'],{});head=heads['policies'].get(p.get('head_succession_policy_id'),{});office=org.get('offices',{}).get(head.get('leader_office_id'),{});issuer=policies.get(i['issuer_director_policy_id'],{});candidate=policies.get(i['candidate_director_policy_id'],{});choice=candidate.get('political_career',{});staffing=issuer.get('political_career',{}).get('staffing_posts',[]);triple=(i['appointment_policy_id'],i['issuer_actor_id'],i['candidate_actor_id'])
            if triple in pairs or i['issuer_actor_id']==i['candidate_actor_id'] or any(a not in s.get('enabled',{}).get('actors',[]) for a in [i['issuer_actor_id'],i['candidate_actor_id']]) or office.get('holder_actor_id')!=i['issuer_actor_id'] or any('political_career' not in x.get('templates',[]) for x in [issuer,candidate]) or not any(x.get('issuing_office_id')==head.get('leader_office_id') and x.get('jurisdiction_id')==p.get('jurisdiction_id') and x.get('appointment_policy_id')==p.get('appointment_policy_id') and i['candidate_actor_id'] in x.get('candidate_actor_ids',[]) for x in staffing):fail('PoliticalSubordinateCareerPolicyRequired',(key,),'须原在任首长与双方原事业策略及明确候选名单。')
            pairs.add(triple)
            if choice.get('response') not in ['accept_qualified','decline','defer'] or (choice.get('response')=='accept_qualified' and (not choice.get('acceptable_organization_ids') or not choice.get('acceptable_jurisdiction_ids'))):fail('PoliticalSubordinateCareerResponseRequired',(key,),'本人范围和答受意图不能从调度预算推断。')
    return issues
