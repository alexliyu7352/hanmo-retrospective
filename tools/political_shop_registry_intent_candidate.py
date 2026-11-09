#!/usr/bin/env python3
"""固定02b三方候选：具名NPC接册意图，不新增命令或冷段。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
BASE='02b054991342bfd4b6115a6e008b4691a315f7b0'
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--base',default=BASE);a=p.parse_args()
if a.base!=BASE or a.output.exists():raise SystemExit('Requires fixed02b and fresh output')
F={};OLD={};D='native/world/src/resident/domains/';K='native/world/src/resident/kernel/'
def get(path):
 if path not in F:F[path]=OLD[path]=subprocess.check_output(['git','show',BASE+':'+path],cwd=a.repo,text=True)
 return F[path]
def change(path,old,new,count=1):
 s=get(path)
 if s.count(old)!=count:raise SystemExit(f'Anchor mismatch {path}: {old[:100]} ({s.count(old)} vs {count})')
 F[path]=s.replace(old,new)
def after(path,needle,extra):
 lines=[line for line in get(path).splitlines(True) if needle in line]
 if len(lines)!=1:raise SystemExit(f'Line mismatch {path}: {needle}: {len(lines)}')
 change(path,lines[0],lines[0]+extra)
p=D+'director/director_model.hpp'
change(p,'visit_reception=13','visit_reception=13, shop_registry_acceptance=14')
change(p,'    std::optional<StableId> supply_renewal_intent_id;','    std::optional<StableId> supply_renewal_intent_id;\n    std::optional<StableId> shop_registry_intent_id;')
change(p,'struct PoliticalSupplyRenewalCatalog;','struct PoliticalSupplyRenewalCatalog;\nstruct PoliticalShopRegistryIntentCatalog;\nstruct DirectorShopRegistryProfile {bool enabled=false;};')
change(p,'    std::shared_ptr<const VisitReceptionCatalog> visit_reception;','    std::shared_ptr<const PoliticalShopRegistryIntentCatalog> shop_registry_intents;\n    std::shared_ptr<const VisitReceptionCatalog> visit_reception;')
after(p,'std::shared_ptr<const VisitReceptionCatalog> visit_reception=nullptr);','Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue&,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile,DirectorSupplyRenewalProfile,std::shared_ptr<const VisitReceptionCatalog>,DirectorShopRegistryProfile);\n')
p=D+'director/director_codec.cpp'
change(p,'#include "director_model.hpp"','#include "director_model.hpp"\n#include "resident/domains/politics/licensing/political_shop_registry_intents.hpp"')
change(p,'if(name=="visit_reception")','if(name=="shop_registry_acceptance")return DirectorTemplate::shop_registry_acceptance;if(name=="visit_reception")')
old='Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military,DirectorSupplyRenewalProfile supply,std::shared_ptr<const VisitReceptionCatalog> reception){try{using namespace boundary;'
new='''Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military,DirectorSupplyRenewalProfile supply,std::shared_ptr<const VisitReceptionCatalog> reception){return decode_director_catalog(value,regional_life,named_membership,politics,diplomacy,diplomacy_version,treaty_passage,military,supply,std::move(reception),DirectorShopRegistryProfile{});}
Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military,DirectorSupplyRenewalProfile supply,std::shared_ptr<const VisitReceptionCatalog> reception,DirectorShopRegistryProfile registries){try{using namespace boundary;if(value.find("political_shop_registry_intents")){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};}'''
change(p,old,new)
change(p,'"authored_task_continuation","political_supply_renewals"}','"authored_task_continuation","political_supply_renewals","political_shop_registry_intents"}',2)
after(p,'if(auto* renewal=value.find("political_supply_renewals"))legacy', '    if(auto* registry=value.find("political_shop_registry_intents"))legacy.emplace_back("political_shop_registry_intents",*registry);\n')
change(p,'diplomacy_version,treaty_passage,military,supply,reception);if(!parsed.ok)','diplomacy_version,treaty_passage,military,supply,reception,registries);if(!parsed.ok)')
change(p,'DirectorCatalog c;c.visit_reception=', 'DirectorCatalog c;if(auto* registry=value.find("political_shop_registry_intents")){if(!registries.enabled||!politics)fail("PoliticalShopRegistryIntentProfileRequired","director");auto parsed=decode_political_shop_registry_intents(*registry);if(!parsed.ok)return {false,{},parsed.error};c.shop_registry_intents=std::make_shared<const PoliticalShopRegistryIntentCatalog>(std::move(parsed.value));}c.visit_reception=')
change(p,'"supply_renewal_intent_id"},row.first)','"supply_renewal_intent_id","shop_registry_intent_id"},row.first)',3)
change(p,'auto t=read_template(name);','auto t=read_template(name);if(t==DirectorTemplate::shop_registry_acceptance&&!c.shop_registry_intents)fail("PoliticalShopRegistryIntentProfileRequired",row.first);')
change(p,'if(auto* renewal=v.find("supply_renewal_intent_id"))','if(auto* registry=v.find("shop_registry_intent_id")){p.shop_registry_intent_id=string(*registry,row.first);if(!c.shop_registry_intents||!c.shop_registry_intents->intents.find(*p.shop_registry_intent_id))fail("PoliticalShopRegistryIntentSourceMismatch",row.first);}if(auto* renewal=v.find("supply_renewal_intent_id"))')
change(p,'const bool renewal_template=','const bool registry_template=std::find(p.templates.begin(),p.templates.end(),DirectorTemplate::shop_registry_acceptance)!=p.templates.end();if(registry_template!=p.shop_registry_intent_id.has_value())fail("InvalidPoliticalShopRegistryDirectorPolicy",row.first);const bool renewal_template=')
p=D+'director/director_validation.cpp'
change(p,'(t==DirectorTemplate::visit_reception?', '(t==DirectorTemplate::shop_registry_acceptance?!catalog.shop_registry_intents:t==DirectorTemplate::visit_reception?')
change(p,'&&!(catalog.visit_reception&&p.template_id=="visit_reception")','&&!(catalog.visit_reception&&p.template_id=="visit_reception")&&!(catalog.shop_registry_intents&&p.template_id=="shop_registry_acceptance")')
p=D+'politics/politics_director.hpp'
after(p,')> supply_renewal;','    std::function<Outcome<std::optional<DecisionProposal>>(const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&)> shop_registry_acceptance;\n')
p=D+'director/director_rules.cpp'
after(p,'if(enabled(*policy,DirectorTemplate::political_supply_renewal)', '    if(enabled(*policy,DirectorTemplate::shop_registry_acceptance)&&!(s.registrations.find(view.actor_id)&&s.registrations.find(view.actor_id)->obligations_only)){if(!catalog.shop_registry_intents||!politics||!politics->shop_registry_acceptance)return {false,{},common_failure("PoliticalShopRegistryDirectorAccessMissing",view.actor_id,ErrorKind::internal).error};auto proposed=politics->shop_registry_acceptance(view,*policy,p);if(!proposed.ok)return {false,{},proposed.error};if(proposed.value)return {true,{std::move(*proposed.value)},{}};}\n')
p=K+'dynamic_integration.cpp'
change(p,'#include "dynamic_integration.hpp"','#include "dynamic_integration.hpp"\n#include "political_shop_registry_director_integration.hpp"')
after(p,'politics->supply_renewal=', '        if(w.catalog->director->shop_registry_intents&&politics)politics->shop_registry_acceptance=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_shop_registry_director_world(w,known,policy,base);};\n')
p=K+'world_bundle.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "political_shop_registry_director_integration.hpp"')
after(p,'valid=validate_political_shop_registry_world(*this);','    valid=validate_political_shop_registry_director_world(*this);if(!valid.ok)return valid;\n')
p=K+'native_initial_state.cpp'
change(p,',reception_catalog);if(!response_director.ok)',',reception_catalog,DirectorShopRegistryProfile{dynamic.find("political_shop_registry_successions")!=nullptr});if(!response_director.ok)')
after(p,'admitted.module_versions.emplace_back("political_shop_registry_successions","1");','    if(response_director.value.shop_registry_intents)admitted.module_versions.emplace_back("political_shop_registry_intents","1");\n')
p=K+'world_codec.cpp'
change(p,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "resident/domains/politics/licensing/political_shop_registry_intent_validation.hpp"')
change(p,'{"political_shop_registry_successions",{"1"}}','{"political_shop_registry_successions",{"1"}} ,{"political_shop_registry_intents",{"1"}}')
after(p,'auto renewal_layout=validate_political_supply_renewal_modules(has("political_supply_renewals"),','        auto registry_intents_layout=validate_political_shop_registry_intent_modules(has("political_shop_registry_intents"),identity.catalog.module_versions);if(!registry_intents_layout.ok)return {false,{},registry_intents_layout.error};\n')
after(p,'auto renewal_layout=validate_political_supply_renewal_modules(has("political_supply_renewals","1"),','            auto registry_intents_layout=validate_political_shop_registry_intent_modules(has("political_shop_registry_intents","1"),versions);if(!registry_intents_layout.ok)return {false,{},registry_intents_layout.error};\n')
change(p,',reception_catalog);if(!director.ok)',',reception_catalog,DirectorShopRegistryProfile{dynamic->find("political_shop_registry_successions")!=nullptr});if(!director.ok)')
after(p,'result->political_shop_registry_successions=std::make_shared<const PoliticalShopRegistrySuccessionCatalog>', '        if(result->director&&result->director->shop_registry_intents){if(!result->political_shop_registry_successions||!result->common||std::find(caps.begin(),caps.end(),"political_shop_registry_intents_v1")==caps.end())fail("PoliticalShopRegistryIntentProfileRequired","director");auto checked=validate_political_shop_registry_intents(*result->director->shop_registry_intents,*result->common,*result->political_shop_registry_successions);if(!checked.ok)return {false,{},checked.error};}\n')
after(p,'if(result->political_shop_registry_successions)id.module_versions.emplace_back', '    if(result->director&&result->director->shop_registry_intents)id.module_versions.emplace_back("political_shop_registry_intents","1");\n')
p='game/ui/native_resident_backend.gd'
after(p,'expected["module_versions"].append({"name":"political_shop_registry_successions"', '\tif definition.get("dynamic_world",{}).get("director",{}).has("political_shop_registry_intents"): expected["module_versions"].append({"name":"political_shop_registry_intents","version":"1"})\n')
p='schemas/content.schema.json';schema=json.loads(get(p));schema['properties']['required_capabilities']['items']['enum'].append('political_shop_registry_intents_v1');ref={'type':'string','minLength':1,'maxLength':4096};intent={'type':'object','properties':{k:dict(ref) for k in ['id','actor_id','registry_policy_id']},'required':['id','actor_id','registry_policy_id'],'additionalProperties':False};cfg={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'intents':{'type':'object','minProperties':1,'maxProperties':64,'additionalProperties':intent}},'required':['version','intents'],'additionalProperties':False}
intent['properties']['actor_id']={'$ref':'#/$defs/reference_actors'}
for key in ['dynamic_director','dynamic_director_legacy','dynamic_director_registry']:schema['$defs'][key]['properties']['political_shop_registry_intents']=cfg
schema['$defs']['dynamic_director_subject']['properties']['shop_registry_intent_id']=dict(ref)
def enums(v):
 if isinstance(v,dict):
  if isinstance(v.get('enum'),list) and 'visit_reception' in v['enum']:v['enum'].append('shop_registry_acceptance')
  for x in v.values():enums(x)
 elif isinstance(v,list):
  for x in v:enums(x)
enums(schema);F[p]=json.dumps(schema,ensure_ascii=False,indent=2)+'\n'
p='tools/content/links.py'
after(p,'from .political_supply_renewal_links import ', '    from .political_shop_registry_intent_links import check_political_shop_registry_intents\n')
change(p,' + check_political_supply_renewals(pack,index)',' + check_political_supply_renewals(pack,index) + check_political_shop_registry_intents(pack,index)')
p='game/data/rule_capabilities.json';data=json.loads(get(p));data['capabilities'].append({'capability_id':'political_shop_registry_intents_v1','rule_version':'political_shop_registry_intents_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.political_shop_registry_intents.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_shop_registry_panel'],'evidence':[]});F[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
a.output.mkdir(parents=True);files=[];patch=[]
def oid(s):b=s.encode();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
for path,text in sorted(F.items()):
 if text==OLD[path]:continue
 f=a.output/'candidate'/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text);files.append({'path':path,'before_oid':oid(OLD[path]),'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()});patch.extend(difflib.unified_diff(OLD[path].splitlines(True),text.splitlines(True),'a/'+path,'b/'+path))
(a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY_SYNTAX_PENDING','shared':files,'central_registration_only':'native/world/resident.cmake','new_cpp':['political_shop_registry_intents.cpp','political_shop_registry_director.cpp','political_shop_registry_intent_validation.cpp','political_shop_registry_director_integration.cpp'],'marker':'political_shop_registry_intents/1; template14; no new section/feature','limits':['actual authority and registered original court only','remote path never reads occupation approval','runtime pending']},indent=2)+'\n');print(len(files))
