"""治理代理作者只授有限原安民签字，不把may_delegate或新职位推断成公共财权。"""
from copy import deepcopy
NAME='political_governance_agencies'
CAPABILITY=NAME+'_v1'
COMMANDS=['political_offer_governance_agency','political_revoke_governance_agency']
IDS={'id','jurisdiction_id','office_id','appointment_grant_id','commission_grant_id','commission_policy_id','public_inventory_id'}
COUNTS={'response_ticks','mandate_ticks','maximum_issues'}

def with_political_governance_agency_schema(schema):
    """独立1版组使用原动态局部ID，只有选中模块才增加能力及字段。"""
    out=deepcopy(schema)
    fields={k:{'$ref':'#/$defs/entity_id'} for k in sorted(IDS)}
    fields['public_inventory_id']={'$ref':'#/$defs/reference_inventories'}
    fields.update({k:{'type':'integer','minimum':1,'maximum':4096} for k in sorted(COUNTS)})
    policy={'type':'object','properties':fields,'required':sorted(IDS|COUNTS),'additionalProperties':False}
    group={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'object','minProperties':1,'maxProperties':4096,'additionalProperties':policy}},'required':['version','policies'],'additionalProperties':False}
    props=out['$defs']['dynamic_world']['properties']
    if NAME in props and props[NAME]!=group:raise ValueError('Conflicting governance agency schema')
    props[NAME]=group
    caps=out.get('properties',{}).get('required_capabilities',{}).get('items',{}).get('enum')
    if caps is not None and CAPABILITY not in caps:caps.append(CAPABILITY)
    return out

def check_political_governance_agencies(pack,index):
    """专项委任与原财权分别精确核准；不接受初态空职、副手或拼接多个Grant凑足预算。"""
    issues=[]
    for n,s in enumerate(pack.get('scenarios',[])):
        d=s.get('dynamic_world',{})
        if not isinstance(d,dict) or NAME not in d:continue
        root=('scenarios',n,'dynamic_world',NAME)
        def fail(code,key,text):issues.append((code,root+tuple(key),text))
        group=d[NAME];g=d.get('politics_governance',{});relief=d.get('public_relief',{});politics=d.get('politics',{});org=d.get('organization',{})
        if not isinstance(group,dict) or set(group)!={'version','policies'} or type(group.get('version')) is not int or group['version'] not in (1,2) or not isinstance(group.get('policies'),dict) or not 1<=len(group['policies'])<=4096:
            fail('PoliticalGovernanceAgencyProfileMismatch',(),'须显式1或2版与有限策略表。');continue
        if NAME+'_v'+str(group['version']) not in pack.get('required_capabilities',[]) or not s.get('family_life') or g.get('enabled') is not True or relief.get('version')!=1 or politics.get('enabled') is not True or org.get('enabled') is not True:
            fail('PoliticalGovernanceAgencyDependenciesRequired',(),'原治理、赈济、生命与公职账必须同启。');continue
        if group['version']==2 and ('political_governance_agency_intents_v1' not in pack.get('required_capabilities',[]) or not isinstance(d.get('director'),dict) or 'governance_agency_intents' not in d['director']):
            fail('PoliticalGovernanceAgencyIntentPairRequired',(),'2版须显式本人意图和独立能力。');continue
        for key,p in group['policies'].items():
            if not isinstance(p,dict) or set(p)!=IDS|COUNTS or p.get('id')!=key or any(not isinstance(p.get(k),str) or not 0<len(p[k])<=4096 for k in IDS) or any(type(p.get(k)) is not int or not 1<=p[k]<=4096 for k in COUNTS) or p['response_ticks']>p['mandate_ticks']:
                fail('InvalidPoliticalGovernanceAgencyPolicy',(key,),'键、原来源与有限期限须精确。');continue
            j=politics.get('jurisdictions',{}).get(p['jurisdiction_id'],{});o=org.get('offices',{}).get(p['office_id'],{});a=org.get('grants',{}).get(p['appointment_grant_id'],{});c=org.get('grants',{}).get(p['commission_grant_id'],{});plan=g.get('commission_policies',{}).get(p['commission_policy_id'],{});rule=g.get('appeasement_rules',{}).get(plan.get('rule_id'),{})
            if not j or j.get('revision')!=0 or j.get('office_id')!=p['office_id'] or j.get('political_organization_id')!=j.get('controlling_organization_id') or o.get('organization_id')!=j.get('political_organization_id') or o.get('status')!='active' or not o.get('holder_actor_id') or o.get('deputy_actor_id') is not None:
                fail('PoliticalGovernanceAgencyOriginalOfficeRequired',(key,),'原辖区与真实现职必须同一组织，不从空职或副手获得权力。')
            def dates(grant):
                return type(grant.get('valid_from')) is int and grant['valid_from']>=0 and (grant.get('valid_until') is None or type(grant['valid_until']) is int and grant['valid_until']>=grant['valid_from'])
            if a.get('id')!=p['appointment_grant_id'] or a.get('office_id')!=p['office_id'] or p['appointment_grant_id'] not in o.get('grant_ids',[]) or a.get('allowed_command_kinds')!=COMMANDS or a.get('scopes')!=[{'kind':'site','id':j.get('seat_place_id')}] or type(a.get('amount_limit')) is not int or a['amount_limit']!=0 or a.get('wallet_ids')!=[] or a.get('inventory_ids')!=[] or a.get('may_delegate') is not False or not dates(a):
                fail('PoliticalGovernanceAgencySpecificGrantRequired',(key,),'委任专项Grant仅零金额签受管理，不混公共钱粮。')
            if p['appointment_grant_id']==p['commission_grant_id'] or not plan or not rule or plan.get('relief_policy_id') not in relief.get('policies',{}) or c.get('id')!=p['commission_grant_id'] or c.get('office_id')!=p['office_id'] or p['commission_grant_id'] not in o.get('grant_ids',[]) or c.get('may_delegate') is not True or 'political_commission_governance' not in c.get('allowed_command_kinds',[]) or {'kind':'site','id':j.get('seat_place_id')} not in c.get('scopes',[]) or type(c.get('amount_limit')) is not int or type(rule.get('cash_amount')) is not int or c.get('amount_limit',-1)<rule.get('cash_amount',0) or j.get('public_wallet_id') not in c.get('wallet_ids',[]) or p['public_inventory_id'] not in c.get('inventory_ids',[]) or p['public_inventory_id'] not in j.get('public_inventory_ids',[]) or not dates(c):
                fail('PoliticalGovernanceAgencyCommissionGrantIncomplete',(key,),'一个原安民Grant须独自涵盖同库、模板及全部承诺。')
    return issues
