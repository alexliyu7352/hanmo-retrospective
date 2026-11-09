#!/usr/bin/env python3
"""生成独立两人rest作者fixture；不修改/冒充原scenario或全国兼容回执。"""
import copy, hashlib, json, pathlib, sys
source=json.loads(pathlib.Path(sys.argv[1]).read_text()); original=source['data']
d={k:({} if isinstance(v,dict) else []) for k,v in original.items()}
# 只复用原真实rest条款、两个人物定义及地理静态值；fixture范围在新scenario显式选择。
actor_ids=['actor.player','actor.innkeeper']; scenario='engineering.exact_rest_only'; place='west.inn'; settlement='city.west'; role='role.inn'
d['actors']={k:copy.deepcopy(original['actors'][k]) for k in actor_ids}
for a in d['actors'].values():
 a['initial_place']=place;a['dialogue_id']=None;a.pop('travel_plan',None);a['combat']={'learned':[]};a['initial_goods']=[]
d['roles']={role:copy.deepcopy(original['roles'][role])};d['roles'][role]['services']=['rest'];d['roles'][role]['activities']={'rest':copy.deepcopy(original['roles'][role]['activities']['rest'])}
# 原ServiceTerms合法作者H=2用于跨期停驻；支付/成长/身体公式沿原handler。
d['roles'][role]['activities']['rest']['duration']=2
d['places']={place:copy.deepcopy(original['places'][place])};d['places'][place]['parent_place']=None;d['places'][place]['passages']=[]
d['settlements']={settlement:copy.deepcopy(original['settlements'][settlement])};d['settlements'][settlement]['entry_place']=place
map_id='map.qingchuan';d['world_maps']={map_id:copy.deepcopy(original['world_maps'][map_id])}
d['wallets']={'wallet.player':{'id':'wallet.player','owner':{'kind':'actor','id':'actor.player'},'initial_balance':1000},'wallet.inn':copy.deepcopy(original['wallets']['wallet.inn'])}
d['required_capabilities']=['multi_actor','places','economy','services','world_terrain','regional_life_v1']
d['texts']={a['name_key']:k for k,a in d['actors'].items()};d['flags']={}
features={k:'base' for k in ['face_shape','eyes','brows','nose','mouth','hair','beard','skin_palette','build']}
appearance={'kind':'fixed','generator_version':'appearance-v1','attire_style':'plain','base_features':features}
initial={'actors':[],'offer_ids':[],'roles':[{'role_id':role,'actor_id':'actor.innkeeper','enabled':True}],'wallets':list(copy.deepcopy(d['wallets']).values()),'inventories':[],'relations':[],'estates':[],'garrisons':[],'offices':[],'strongholds':[]}
for aid in actor_ids:
 a=d['actors'][aid];initial['actors'].append({'actor_id':aid,'place_id':place,'alive':True,'available':True,'age_years':a['initial_age_years'],'attributes':a['attributes'],'skills':a['skills'],'combat':{'learned':[]},'appearance_definition':appearance,'initial_goods':[]})
enabled={k:list(v) for k,v in d.items() if isinstance(v,dict)};enabled['actors']=actor_ids;enabled['roles']=[role]
economy={k:{} for k in ['households','cohorts','labor_allocations','recipes','production_units','rates','obligations','provision_bindings','production_bindings','supply_sources']};economy.update(enabled=True,named_actors_excluded=actor_ids)
organization={k:{} for k in ['organizations','offices','grants','institutional_issuers','member_policies','duties','members','activity_terms','initial_activities']};organization['enabled']=True
sc={'id':scenario,'name_key':'engineering.exact_rest_only','player_actor':'actor.player','playable_actor_ids':['actor.player'],'world_map_id':map_id,'seed_text':'exact-rest-only','enabled':enabled,'initial':initial,'dynamic_world':{'profile':'local_society_v1','founder_policy':'fixed_actor','local_economy':economy,'organization':organization,'affairs':{'need_policies':{}},'director':{'review_interval_ticks':1,'retry_delay_ticks':1,'decision_budget':1,'candidates_per_subject':1,'subjects':{}},'regional_life':{'actor_topics':[],'need_topics':[]}},'world_time':{'profile':'rational_rest_first_chain_v1','scale_ref':{'definition_id':'engineering.rest.time','version':1,'units_per_half_day':60},'macro_providers':[],'fixed_actions':[{'kind':'service','activity':'rest'}]}}
# 锁内来源摘要是此独立测试作者定义的身份，artifact由生产render精确重算。
digest=hashlib.sha256(b'hanmo-engineering-exact-rest-only-v1').hexdigest()
def ref(i,v=1):return {'id':i,'version':v,'catalog_digest':digest}
actors=[];seeds=[]
for a in initial['actors']:
 aid=a['actor_id'];o={'catalog_digest':digest,'scenario_id':scenario,'row_key':aid,'initial_tick':0};key=aid+':initial-condition';body=(57,42,1) if aid=='actor.player' else (100,0,0)
 seeds.append({'seed_key':key,'actor_id':aid,'source':o,'hp':body[0],'fatigue':body[1],'injury':body[2],'episodes':[],'activity_limit_profile':ref('original_service_rest')})
 actors.append({'actor_id':aid,'identity_definition_ref':ref(aid),'alive':True,'hp':body[0],'fatigue':body[1],'injury':body[2],'age_basis':{'kind':'DeclaredAgeAtStart','years':a['age_years'],'as_of_tick':0,'calendar_year_ordinal':0,'source':o},'condition_seed_refs':[{'seed_key':key}],'placement':{'kind':'AtPlace','place_ref':place},'service_seed_refs':[],'office_seed_refs':[],'property_business_seed_refs':[],'initial_knowledge_refs':[],'appearance_source_ref':ref(aid)})
sc['actor_foundation']={'construction_profile_ref':ref('selected_existing_actor_context_v2',2),'provenance_policy_ref':ref('character_origin_read_v2',2),'rest_rule_ref':ref('original_service_rest'),'rest_purpose_ref':ref('rest'),'provider_purpose_ref':ref('provide_rest'),'initial_condition_seeds':seeds,'actor_states':actors}
d['scenarios']={scenario:sc};d['active_static_actor_ids']=actor_ids
result={'ok':True,'data':d,'content_id':'engineering.exact_rest_only','content_version':'1','content_digest':digest,'schema_sha256':source['schema_sha256']}
pathlib.Path(sys.argv[2]).write_text(json.dumps(result,ensure_ascii=False,separators=(',',':'))+'\n')
