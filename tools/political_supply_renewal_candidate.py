#!/usr/bin/env python3
"""生成明确NPC续签准入与真实Director接头；中央唯一写共享文件。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
BASE='ade5e3d9a6e8276166c0477231f18a5bf9f1120d'
p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--base',default=BASE);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
if a.base!=BASE or a.output.exists():raise SystemExit('Requires fixed ade5 base and fresh output')
F={};OLD={};K='native/world/src/resident/kernel/';D='native/world/src/resident/domains/'
def get(p):
 if p not in F:F[p]=OLD[p]=subprocess.check_output(['git','show',BASE+':'+p],cwd=a.repo,text=True)
 return F[p]
def change(p,old,new,count=1):
 s=get(p)
 if s.count(old)!=count:raise SystemExit(f'Anchor mismatch {p}: {old[:120]} ({s.count(old)} vs {count})')
 F[p]=s.replace(old,new)
def append_after_line(p,needle,extra):
 matches=[line for line in get(p).splitlines(True) if needle in line]
 if len(matches)!=1:raise SystemExit('Line mismatch '+p+': '+needle)
 change(p,matches[0],matches[0]+extra)
p=D+'director/director_model.hpp'
change(p,'field_appointment=11','field_appointment=11, political_supply_renewal=12')
change(p,'    std::optional<StableId> diplomatic_delegation_policy_id;','    std::optional<StableId> diplomatic_delegation_policy_id;\n    std::optional<StableId> supply_renewal_intent_id;')
change(p,'struct TaskAuthoredContinuationCatalog;','struct TaskAuthoredContinuationCatalog;\nstruct PoliticalSupplyRenewalCatalog;\nstruct DirectorSupplyRenewalProfile {bool economic_supply_pair=false;};')
change(p,'    bool military_appointments=false;','    std::shared_ptr<const PoliticalSupplyRenewalCatalog> supply_renewals;\n    bool military_appointments=false;')
append_after_line(p,'bool treaty_passage,DirectorMilitaryAppointmentProfile);','Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue&,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile,DirectorSupplyRenewalProfile);\n')
p=D+'director/director_codec.cpp'
change(p,'#include "director_model.hpp"','#include "director_model.hpp"\n#include "resident/domains/politics/supply/politics_supply_renewal_catalog.hpp"\n#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"')
change(p,'if(name=="field_appointment")','if(name=="political_supply_renewal")return DirectorTemplate::political_supply_renewal;if(name=="field_appointment")')
old='Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military){try{using namespace boundary;'
new='''Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military){return decode_director_catalog(value,regional_life,named_membership,politics,diplomacy,diplomacy_version,treaty_passage,military,DirectorSupplyRenewalProfile{});}
// 经营续签为独立显式目录；旧调用保持关闭，递归不能丢失具名pair资格。
Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military,DirectorSupplyRenewalProfile supply){try{using namespace boundary;if(value.find("political_supply_renewals")){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};}'''
change(p,old,new)
change(p,'{"diplomatic_response_version","authored_task_continuation"}','{"diplomatic_response_version","authored_task_continuation","political_supply_renewals"}',2)
append_after_line(p,'if(auto* continuation=value.find("authored_task_continuation"))','    if(auto* renewal=value.find("political_supply_renewals"))legacy.emplace_back("political_supply_renewals",*renewal);\n')
change(p,'diplomacy_version,treaty_passage,military);if(!parsed.ok)','diplomacy_version,treaty_passage,military,supply);if(!parsed.ok)')
change(p,'DirectorCatalog c;c.military_appointments','DirectorCatalog c;if(auto* renewal=value.find("political_supply_renewals")){if(!supply.economic_supply_pair||!politics)fail("PoliticalSupplyRenewalPairRequired","director");auto decoded=decode_political_supply_renewals(*renewal);if(!decoded.ok)return {false,{},decoded.error};c.supply_renewals=std::make_shared<const PoliticalSupplyRenewalCatalog>(std::move(decoded.value));}c.military_appointments')
change(p,'"diplomatic_delegation_policy_id"},row.first)','"diplomatic_delegation_policy_id","supply_renewal_intent_id"},row.first)',2)
change(p,'{"report_source_keys"},row.first);DirectorPolicy p;','{"report_source_keys","supply_renewal_intent_id"},row.first);DirectorPolicy p;')
change(p,'auto t=read_template(name);','auto t=read_template(name);if(t==DirectorTemplate::political_supply_renewal&&!c.supply_renewals)fail("PoliticalSupplyRenewalDirectorProfileRequired",row.first);')
change(p,'if(auto* delegation=v.find("diplomatic_delegation_policy_id"))','if(auto* renewal=v.find("supply_renewal_intent_id")){p.supply_renewal_intent_id=string(*renewal,row.first);if(!c.supply_renewals||!c.supply_renewals->intents.find(*p.supply_renewal_intent_id))fail("PoliticalSupplyRenewalIntentMismatch",row.first);}if(auto* delegation=v.find("diplomatic_delegation_policy_id"))')
change(p,'const bool delegation_template=','const bool renewal_template=std::find(p.templates.begin(),p.templates.end(),DirectorTemplate::political_supply_renewal)!=p.templates.end();if(renewal_template!=p.supply_renewal_intent_id.has_value())fail("InvalidPoliticalSupplyRenewalDirectorPolicy",row.first);const bool delegation_template=')
p=D+'director/director_validation.cpp'
change(p,'(t==DirectorTemplate::field_appointment?', '(t==DirectorTemplate::political_supply_renewal?!catalog.supply_renewals:t==DirectorTemplate::field_appointment?')
change(p,'&&!(catalog.field_appointments&&p.template_id=="field_appointment")','&&!(catalog.field_appointments&&p.template_id=="field_appointment")&&!(catalog.supply_renewals&&p.template_id=="political_supply_renewal")')
p=D+'politics/politics_director.hpp'
append_after_line(p,')> field_appointment;','    std::function<Outcome<std::optional<DecisionProposal>>(const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&)> supply_renewal;\n')
p=D+'director/director_rules.cpp'
append_after_line(p,'if(enabled(*policy,DirectorTemplate::field_appointment)', '    if(enabled(*policy,DirectorTemplate::political_supply_renewal)&&!(s.registrations.find(view.actor_id)&&s.registrations.find(view.actor_id)->obligations_only)){if(!catalog.supply_renewals||!politics||!politics->supply_renewal)return {false,{},common_failure("PoliticalSupplyRenewalDirectorAccessMissing",view.actor_id,ErrorKind::internal).error};auto proposed=politics->supply_renewal(view,*policy,p);if(!proposed.ok)return {false,{},proposed.error};if(proposed.value)return {true,{std::move(*proposed.value)},{}};}\n')
p=K+'economic_supply_policy_integration.hpp'
change(p,' const EconomicSupplyPolicyContext& context()const{return context_;}',' const EconomicSupplyPolicyContext& context()const{return context_;}\n const PoliticalTakeoverContext& political_context()const{return takeover_->context();}')
p=K+'dynamic_integration.cpp'
change(p,'#include "dynamic_integration.hpp"','#include "dynamic_integration.hpp"\n#include "political_supply_renewal_integration.hpp"')
append_after_line(p,'politics->field_appointment=', '        if(w.catalog->director->supply_renewals&&politics)politics->supply_renewal=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_supply_renewal_world(w,known,policy,base);};\n')
p=K+'world_bundle.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "political_supply_renewal_integration.hpp"')
append_after_line(p,'valid=validate_treaty_passage_director(', '        valid=validate_political_supply_renewal_world(*this);if(!valid.ok)return valid;\n')
change(p,'if(!dynamic_enabled()){valid=validate_economic_supply_world(*this);if(!valid.ok)return valid;}','if(!dynamic_enabled()){valid=validate_economic_supply_world(*this);if(!valid.ok)return valid;valid=validate_political_supply_renewal_world(*this);if(!valid.ok)return valid;}')
p=K+'native_initial_state.cpp'
change(p,'dynamic.find("political_field_appointments")!=nullptr});if(!response_director.ok)','dynamic.find("political_field_appointments")!=nullptr},DirectorSupplyRenewalProfile{dynamic.find("economic_supply_policies")!=nullptr});if(!response_director.ok)')
append_after_line(p,'auto supply_layout=validate_economic_supply_layout(', '    if(response_director.value.supply_renewals)admitted.module_versions.emplace_back("political_supply_renewals","1");\n')
p=K+'world_codec.cpp'
change(p,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "resident/domains/politics/supply/politics_supply_renewal_validation.hpp"')
change(p,'{"political_supply_adoptions",{"1"}}','{"political_supply_adoptions",{"1"}}, {"political_supply_renewals",{"1"}}')
# 只有身份marker，无新段和额外feature；既有Director检查点字节不追加。
append_after_line(p,'auto supply_layout=validate_economic_supply_layout(has("economic_supply_policies"),','        auto renewal_layout=validate_political_supply_renewal_modules(has("political_supply_renewals"),identity.catalog.module_versions);if(!renewal_layout.ok)return {false,{},renewal_layout.error};\n')
append_after_line(p,'const auto continuation_markers=', '            auto renewal_layout=validate_political_supply_renewal_modules(has("political_supply_renewals","1"),versions);if(!renewal_layout.ok)return {false,{},renewal_layout.error};\n')
change(p,'political_field_appointments_enabled(*result)});if(!director.ok)','political_field_appointments_enabled(*result)},DirectorSupplyRenewalProfile{result->economic_supply_policies_enabled});if(!director.ok)')
append_after_line(p,'if(result->director->authored_task_continuation){auto caps=', '        if(result->director->supply_renewals){auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"political_supply_renewals_v1")==caps.end())unsupported("PoliticalSupplyRenewalCapabilityRequired","director");if(!result->political_takeover||!result->organization||!result->economic_supply_policies)fail("PoliticalSupplyRenewalPairRequired","director");auto valid=validate_political_supply_renewals(*result->director->supply_renewals,*result->political_takeover,*result->organization,*result->economic_supply_policies);if(!valid.ok)return {false,{},valid.error};}\n')
append_after_line(p,'auto supply_layout=validate_economic_supply_layout(result->economic_supply_policies_enabled,','    if(result->director&&result->director->supply_renewals)id.module_versions.emplace_back("political_supply_renewals","1");\n')
p='game/ui/native_resident_backend.gd'
append_after_line(p,'for module in ["economic_supply_policies","political_supply_adoptions"]:', '\tif definition.get("dynamic_world",{}).get("director",{}).has("political_supply_renewals"): expected["module_versions"].append({"name":"political_supply_renewals","version":"1"})\n')
p='schemas/content.schema.json';schema=json.loads(get(p));schema['properties']['required_capabilities']['items']['enum'].append('political_supply_renewals_v1')
ref={'type':'string','minLength':1,'maxLength':4096}
intent={'type':'object','properties':{k:dict(ref) for k in ['id','takeover_rule_id','author_policy_id']},'required':['id','takeover_rule_id','author_policy_id'],'additionalProperties':False}
renewal={'type':'object','properties':{'version':{'type':'integer','enum':[1]},'intents':{'type':'object','minProperties':1,'maxProperties':64,'additionalProperties':intent}},'required':['version','intents'],'additionalProperties':False}
for key in ['dynamic_director','dynamic_director_legacy','dynamic_director_registry']:schema['$defs'][key]['properties']['political_supply_renewals']=renewal
schema['$defs']['dynamic_director_subject']['properties']['supply_renewal_intent_id']=dict(ref)
def enums(v):
 if isinstance(v,dict):
  if isinstance(v.get('enum'),list) and 'field_appointment' in v['enum']:v['enum'].append('political_supply_renewal')
  for value in v.values():enums(value)
 elif isinstance(v,list):
  for value in v:enums(value)
enums(schema);F[p]=json.dumps(schema,ensure_ascii=False,indent=2)+'\n'
p='tools/content/links.py'
append_after_line(p,'from .supply_policy_links import ', '    from .political_supply_renewal_links import check_political_supply_renewals\n')
change(p,' + check_economic_supply_policies(pack,index)',' + check_economic_supply_policies(pack,index) + check_political_supply_renewals(pack,index)')
p='game/data/rule_capabilities.json';data=json.loads(get(p));data['capabilities'].append({'capability_id':'political_supply_renewals_v1','rule_version':'political_supply_renewals_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.political_supply_renewals.version','equals':1}],'supported_semantics':[],'test_ids':['test_supply_policy_panel_guard','test_political_supply_choice_panel'],'evidence':[]});F[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
a.output.mkdir(parents=True);files=[];patch=[]
def oid(s):b=s.encode();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
for path,text in sorted(F.items()):
 if text==OLD[path]:continue
 f=a.output/'candidate'/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text)
 files.append({'path':path,'before_oid':oid(OLD[path]),'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()});patch.extend(difflib.unified_diff(OLD[path].splitlines(True),text.splitlines(True),'a/'+path,'b/'+path))
(a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY_SYNTAX_PENDING','shared':files,'central_registration_only':'native/world/resident.cmake','new_cpp':['politics_supply_renewal.cpp','politics_supply_renewal_catalog.cpp','politics_supply_renewal_director.cpp','politics_supply_renewal_validation.cpp','political_supply_renewal_integration.cpp'],'marker':'political_supply_renewals/1; no new section or feature; original commands and write sets','limits':['existing complete approval only','NPC must hold current exact Office at the known original site','no natural NPC World runtime yet']},indent=2)+'\n');print(len(files))
