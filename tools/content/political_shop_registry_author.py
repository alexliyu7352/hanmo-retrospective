"""为已有原院和真实后继公职生成显式接册声明；不造占领、批准、初册或案件事实。"""
from copy import deepcopy
from .political_shop_registry_links import CAPABILITY, COMMAND, check_political_shop_registry_successions


def build_political_shop_registry_declaration(pack, index, scenario_id, *, policy_id, court_id,
        organization_id, office_id, role_id, dialogue_id, request_node_id, grant_id, text_key):
    """先在独立副本核全原院/柜台/专项Grant，再返回精确追加片；调用方决定如何合入。"""
    candidate=deepcopy(pack)
    selected=[s for s in candidate.get('scenarios',[]) if s.get('id')==scenario_id]
    if len(selected)!=1: raise ValueError('PoliticalShopRegistryScenarioRequired')
    dynamic=selected[0].get('dynamic_world',{})
    court=dynamic.get('political_shop_licensing',{}).get('courts',{}).get(court_id)
    if not court: raise ValueError('PoliticalShopRegistryOriginalCourtRequired')
    jurisdiction=dynamic.get('politics',{}).get('jurisdictions',{}).get(court.get('jurisdiction_id'),{})
    office=dynamic.get('organization',{}).get('offices',{}).get(office_id)
    if not office or office.get('organization_id')!=organization_id: raise ValueError('PoliticalShopRegistryOriginalOfficeRequired')
    if not text_key or text_key not in candidate.get('texts',{}): raise ValueError('PoliticalShopRegistryCommandTextRequired')
    grants=dynamic['organization'].get('grants',{})
    if grant_id in grants: raise ValueError('PoliticalShopRegistryGrantAlreadyDeclared')
    grant={'id':grant_id,'office_id':office_id,'allowed_command_kinds':[COMMAND],
        'scopes':[{'kind':'site','id':jurisdiction.get('seat_place_id')}], 'wallet_ids':[], 'inventory_ids':[],
        'amount_limit':0,'valid_from':0,'valid_until':None,'may_delegate':False,'continuity_policy_id':'stop_when_vacant'}
    grants[grant_id]=grant;office.setdefault('grant_ids',[]).append(grant_id)
    profile=dynamic.setdefault('political_shop_registry_successions',{'enabled':True,'version':1,'policies':{}})
    if policy_id in profile.get('policies',{}): raise ValueError('PoliticalShopRegistryPolicyAlreadyDeclared')
    policy={'id':policy_id,'court_id':court_id,'organization_id':organization_id,'office_id':office_id,
        'role_id':role_id,'dialogue_id':dialogue_id,'request_node_id':request_node_id}
    profile['policies'][policy_id]=policy
    if CAPABILITY not in candidate.setdefault('required_capabilities',[]):candidate['required_capabilities'].append(CAPABILITY)
    issues=check_political_shop_registry_successions(candidate,index)
    if issues:raise ValueError(str(issues))
    return {'capability':CAPABILITY,'policy':policy,'grant':grant,'command_name_keys':{COMMAND:text_key}}
