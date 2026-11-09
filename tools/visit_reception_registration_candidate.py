"""固定9af追加拜帖有限NPC意向，保政治续签12与旧Director持久字节。"""
import argparse,hashlib,json,subprocess
from pathlib import Path
from visit_message_registration_candidate import replace,after
BASE='9af075b424faba4486fba134d0ed19d5c32412c6'
ROOT='native/world/src/resident/domains/interaction/visit_messages/'
INCLUDE='#include "resident/domains/interaction/visit_messages/visit_reception_world.hpp"\n'
SHARED=['native/world/src/resident/domains/director/'+n for n in ['director_model.hpp','director_codec.cpp','director_validation.cpp','director_registry.cpp','director_rules.cpp']]+['native/world/src/resident/kernel/'+n for n in ['dynamic_integration.cpp','world_codec.cpp','native_initial_state.cpp','world_bundle.cpp']]+['game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/content/dynamic_world_links.py','tools/verify.py']
OWNED=[ROOT+n for n in ['visit_reception_catalog.hpp','visit_reception_catalog.cpp','visit_reception_director.hpp','visit_reception_director.cpp','visit_reception_world.hpp','visit_reception_world.cpp']]+['native/world/tests/resident/visit_reception_test.cpp','tools/visit_reception_registration_candidate.py','tools/content/visit_reception_links.py','tools/test_visit_reception_author.py','tools/build_visit_reception_author.py','game/tests/test_visit_reception_catalog.gd','content/packs/authored/task_contacts_1_11/pack.json','content/packs/authored/task_contacts_1_11/content.json','content/packs/authored/task_contacts_1_11/README.md','content/packs/selections/visit-reception-1.11.0.json']


def transform(path,s):
    """精确锚点只生成隔离候选；命中不唯一即保红退出。"""
    if path.endswith('/director_model.hpp'):
        s=replace(s,'struct AffairsState;','struct AffairsState;\nstruct VisitReceptionCatalog;struct VisitReceptionRead;')
        s=replace(s,'political_supply_renewal=12 };','political_supply_renewal=12, visit_reception=13 };')
        s=replace(s,'struct DirectorCatalog {','struct DirectorCatalog {\n    std::shared_ptr<const VisitReceptionCatalog> visit_reception;')
        s=replace(s,'const TaskResponsibilityDirectorContext* task_responsibility=nullptr);','const TaskResponsibilityDirectorContext* task_responsibility=nullptr,const VisitReceptionRead* visit_reception=nullptr);')
        return replace(s,'DirectorMilitaryAppointmentProfile,DirectorSupplyRenewalProfile);','DirectorMilitaryAppointmentProfile,DirectorSupplyRenewalProfile,std::shared_ptr<const VisitReceptionCatalog> visit_reception=nullptr);')
    if path.endswith('/director_codec.cpp'):
        s='#include "resident/domains/interaction/visit_messages/visit_reception_catalog.hpp"\n'+s
        s=replace(s,'DirectorTemplate read_template(const StableId& name){','DirectorTemplate read_template(const StableId& name){if(name=="visit_reception")return DirectorTemplate::visit_reception;')
        s=replace(s,'DirectorSupplyRenewalProfile supply){try','DirectorSupplyRenewalProfile supply,std::shared_ptr<const VisitReceptionCatalog> reception){try')
        s=replace(s,'diplomacy_version,treaty_passage,military,supply);if(!parsed.ok)','diplomacy_version,treaty_passage,military,supply,reception);if(!parsed.ok)')
        s=replace(s,'DirectorCatalog c;if(auto* renewal','DirectorCatalog c;c.visit_reception=std::move(reception);if(auto* renewal')
        s=replace(s,'auto t=read_template(name);if(t==DirectorTemplate::political_supply_renewal','auto t=read_template(name);if(t==DirectorTemplate::visit_reception&&!c.visit_reception)fail("VisitReceptionProfileRequired",row.first);if(t==DirectorTemplate::political_supply_renewal')
        return replace(s,'t!=DirectorTemplate::private_livelihood&&t!=DirectorTemplate::maintain_commitment&&t!=DirectorTemplate::political_career;','t!=DirectorTemplate::private_livelihood&&t!=DirectorTemplate::maintain_commitment&&t!=DirectorTemplate::political_career&&t!=DirectorTemplate::visit_reception;')
    if path.endswith('/director_validation.cpp'):
        s=replace(s,'(t==DirectorTemplate::political_supply_renewal?', '(t==DirectorTemplate::visit_reception?!catalog.visit_reception:t==DirectorTemplate::political_supply_renewal?')
        s=replace(s,'t!=DirectorTemplate::private_livelihood&&t!=DirectorTemplate::maintain_commitment&&t!=DirectorTemplate::political_career;','t!=DirectorTemplate::private_livelihood&&t!=DirectorTemplate::maintain_commitment&&t!=DirectorTemplate::political_career&&t!=DirectorTemplate::visit_reception;')
        return replace(s,'&&!(catalog.supply_renewals&&p.template_id=="political_supply_renewal"))','&&!(catalog.supply_renewals&&p.template_id=="political_supply_renewal")&&!(catalog.visit_reception&&p.template_id=="visit_reception"))')
    if path.endswith('/director_registry.cpp'):
        s=replace(s,'catalog.diplomatic_delegation||catalog.treaty_passage)&&response','catalog.diplomatic_delegation||catalog.treaty_passage||catalog.visit_reception)&&response')
        s=replace(s,'DirectorTemplate::treaty_ratification)!=policy->templates.end())){auto allowed=response','DirectorTemplate::treaty_ratification)!=policy->templates.end()||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::visit_reception)!=policy->templates.end())){auto allowed=response')
        return replace(s,'next.reason="DiplomaticResponseReady";','next.reason=catalog.visit_reception?"AuthoredConversationReady":"DiplomaticResponseReady";')
    if path.endswith('/director_rules.cpp'):
        s='#include "resident/domains/interaction/visit_messages/visit_reception_director.hpp"\n'+s
        s=replace(s,'const TaskResponsibilityDirectorContext* task_responsibility){','const TaskResponsibilityDirectorContext* task_responsibility,const VisitReceptionRead* visit_reception){')
        call='if(catalog.visit_reception&&enabled(*policy,DirectorTemplate::visit_reception)&&!(s.registrations.find(view.actor_id)&&s.registrations.find(view.actor_id)->obligations_only)){if(!visit_reception)return {false,{},common_failure("VisitReceptionDirectorAccessMissing",view.actor_id,ErrorKind::internal).error};if(const auto* intent=visit_reception_actor_intent(*catalog.visit_reception,view.actor_id)){auto visit=propose_visit_reception(*visit_reception,s,view,intent->id,p);if(!visit.ok)return {false,{},visit.error};if(visit.value)return {true,{std::move(*visit.value)},{}};}}'
        s=replace(s,'    if(view.interaction_busy){','    if(view.interaction_busy){\n        if(!obligation)'+call)
        return after(s,'if(enabled(*policy,DirectorTemplate::political_supply_renewal)', '    '+call)
    if path.endswith('/dynamic_integration.cpp'):
        s=INCLUDE+s
        s=replace(s,'        if(!w.diplomacy_enabled())return {true,false,{}};','        if(w.catalog->director->visit_reception&&std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::visit_reception)!=policy.templates.end()){auto visit=visit_reception_conversation_world(w,actor);if(!visit.ok||visit.value)return visit;}\n        if(!w.diplomacy_enabled())return {true,false,{}};')
        s=replace(s,'if(status.ok){outcome.kind=DecisionOutcome::Kind::adopted;status=record_director_result(w.director,proposal,outcome,tick);','if(status.ok){outcome.kind=DecisionOutcome::Kind::adopted;auto bound=bind_visit_reception_world(w,proposal);if(!bound.ok)status={false,{},bound.error};else status=record_director_result(w.director,bound.value,outcome,tick);')
        s=after(s,'auto issued=PoliticalTakeoverExecutionWorldAccess::create(w);if(!issued.ok)', '        std::unique_ptr<VisitReceptionWorldAccess> reception;if(w.catalog->director->visit_reception){auto access=VisitReceptionWorldAccess::create(w);if(!access.ok)return {false,{},access.error};reception=std::move(access.value);}')
        return replace(s,'task_responsibility?&task_responsibility->context():nullptr);if(!proposed.ok)','task_responsibility?&task_responsibility->context():nullptr,reception?&reception->context():nullptr);if(!proposed.ok)')
    if path.endswith('/world_bundle.cpp'):
        return after(INCLUDE+s,'valid=validate_visit_message_world(*this);','    valid=validate_visit_reception_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/native_initial_state.cpp'):
        s=INCLUDE+s
        preflight='    // Director首次解码必须取得真实完整来访目录；这些只读目录不分配ID或改变旧身份顺序。\n    const auto& reception_data=field(catalog,"data","catalog");\n    auto visit_reception=decode_visit_reception_profile(scenario,strings(field(reception_data,"required_capabilities","data"),"required_capabilities"));if(!visit_reception.ok)return {false,{},visit_reception.error};\n    std::shared_ptr<const VisitReceptionCatalog> reception_catalog;\n    if(visit_reception.value){\n        auto messages_profile=decode_visit_message_profile(scenario,strings(field(reception_data,"required_capabilities","data"),"required_capabilities"));if(!messages_profile.ok)return {false,{},messages_profile.error};if(!messages_profile.value)return {false,{},common_failure("VisitReceptionProfileMismatch").error};\n        auto reception_common=decode_common_catalog(reception_data);if(!reception_common.ok)return {false,{},reception_common.error};\n        auto reception_interaction=decode_interaction_catalog(reception_data,std::make_shared<const CommonCatalog>(std::move(reception_common.value)));if(!reception_interaction.ok)return {false,{},reception_interaction.error};\n        auto reception_messages=decode_visit_message_catalog(field(scenario,"visit_messages","scenario"),reception_interaction.value);if(!reception_messages.ok)return {false,{},reception_messages.error};\n        auto parsed_reception=decode_visit_reception_catalog(field(scenario,"visit_reception_intents","scenario"),reception_messages.value,reception_interaction.value);if(!parsed_reception.ok)return {false,{},parsed_reception.error};\n        reception_catalog=std::make_shared<const VisitReceptionCatalog>(std::move(parsed_reception.value));\n    }\n'
        s=replace(s,'    auto response_director=decode_director_catalog(',preflight+'    auto response_director=decode_director_catalog(')
        s=replace(s,'DirectorSupplyRenewalProfile{dynamic.find("economic_supply_policies")!=nullptr});','DirectorSupplyRenewalProfile{dynamic.find("economic_supply_policies")!=nullptr},reception_catalog);')
        return after(s,'if(innate.value)admitted.module_versions.emplace_back("family_innate_learning","1");','    if(visit_reception.value)admitted.module_versions.emplace_back("visit_reception_intents","1");')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'{"visit_messages",{"1"}},','{"visit_messages",{"1"}}, {"visit_reception_intents",{"1"}},')
        s=after(s,'if(has("visit_messages")){','        if(has("visit_reception_intents")&&(!dynamic||!has("visit_messages")))fail("VisitReceptionProfileMismatch","checkpoint/module_versions");')
        s=after(s,'if(has("visit_messages","1")!=(version("visit_messages")==1)','            if(has("visit_reception_intents","1")&&(!dynamic||!has("visit_messages","1")))return {false,{},common_failure("VisitReceptionProfileMismatch").error};')
        s=after(s,'auto visit_messages=decode_visit_message_profile(scenario,','    auto visit_reception=decode_visit_reception_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!visit_reception.ok)return {false,{},visit_reception.error};')
        anchor='auto director=decode_director_catalog('
        assert s.count(anchor)==1
        s=replace(s,anchor,'std::shared_ptr<const VisitReceptionCatalog> reception_catalog;if(visit_reception.value){auto parsed=decode_visit_reception_catalog(field(scenario,"visit_reception_intents","scenario"),*result->interaction->visit_messages,*result->interaction);if(!parsed.ok)return {false,{},parsed.error};reception_catalog=std::make_shared<const VisitReceptionCatalog>(std::move(parsed.value));}'+anchor)
        s=replace(s,'DirectorSupplyRenewalProfile{result->economic_supply_policies_enabled});','DirectorSupplyRenewalProfile{result->economic_supply_policies_enabled},reception_catalog);')
        return after(s,'if(result->growth->innate_learning)id.module_versions.emplace_back','    if(result->director&&result->director->visit_reception)id.module_versions.emplace_back("visit_reception_intents","1");')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'expected["module_versions"].append({"name":"family_innate_learning"','\tif definition.get("visit_reception_profile","")=="authored_local_reception_v1": expected["module_versions"].append({"name":"visit_reception_intents","version":"1"})')
    if path.endswith('/content.schema.json'):
        d=json.loads(s);scenario=d['$defs']['scenario']['properties'];scenario['visit_reception_profile']={'type':'string','enum':['authored_local_reception_v1']}
        fields={'id':{'type':'string','minLength':1},'actor_id':{'$ref':'#/$defs/reference_actors'},'message_policy_id':{'type':'string','minLength':1},'purpose':{'type':'string','enum':['deposit','receive']},'cooldown_ticks':{'type':'integer','minimum':1,'maximum':120},'route':{'type':'array','maxItems':16,'items':{'type':'object','additionalProperties':False,'required':['node_id'],'properties':{'node_id':{'type':'string','minLength':1},'option_id':{'type':'string','minLength':1}}}}}
        scenario['visit_reception_intents']={'type':'object','additionalProperties':False,'required':['version','intents'],'properties':{'version':{'type':'integer','enum':[1]},'intents':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}}
        def extend(v):
            if isinstance(v,dict):
                if isinstance(v.get('enum'),list) and 'political_supply_renewal' in v['enum']:v['enum'].append('visit_reception')
                for x in v.values():extend(x)
            elif isinstance(v,list):
                for x in v:extend(x)
        extend(d);d['properties']['required_capabilities']['items']['enum'].append('visit_reception_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);d['capabilities'].append({'capability_id':'visit_reception_v1','rule_version':'authored_local_reception_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_reception_profile','equals':'authored_local_reception_v1'}],'supported_semantics':[],'test_ids':['test_visit_reception_catalog'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .visit_message_links import','    from .visit_reception_links import check_visit_reception')
        return replace(s,' + check_visit_messages(pack,index)',' + check_visit_messages(pack,index) + check_visit_reception(pack,index)')
    if path=='tools/content/dynamic_world_links.py':
        return replace(s,"private_only = policy['templates'] == ['private_livelihood']", "private_only = (scenario.get('visit_reception_profile') == 'authored_local_reception_v1' and 'visit_reception' in policy['templates'] and set(policy['templates']) <= {'private_livelihood','maintain_commitment','await_adjustment','visit_reception'}) or policy['templates'] == ['private_livelihood']")
    if path=='tools/verify.py':
        return replace(s,'EXCLUDED_TESTS = {',"EXCLUDED_TESTS = {\n    'test_visit_reception_catalog': '需正规1.11完整作者索引与匹配Schema两参数；仅核Snapshot/native_request身份和目录保留，实际NPC原生门另行同代运行，不裸跑或冒World。',")
    raise ValueError(path)


def main():
    """每叶记录固定Git原件OID；不写中央、不复用未冻结同名结果。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);p.add_argument('--wip',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip();shared=[];owned=[]
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);data=transform(path,before.decode()).encode();target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);shared.append({'path':path,'base_oid':oid(before),'result_oid':oid(data)})
    for path in OWNED:
        data=(a.owned_repo/path).read_bytes()
        if not a.wip:assert data==subprocess.check_output(['git','-C',str(a.owned_repo),'show',head+':'+path]),path
        target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);owned.append({'path':path,'oid':oid(data)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'source':head,'wip':a.wip,'shared':shared,'owned':owned,'state':'SOURCE_ONLY','build_cpp':[ROOT+n for n in ['visit_reception_catalog.cpp','visit_reception_director.cpp','visit_reception_world.cpp']],'build_headers':[ROOT+n for n in ['visit_reception_catalog.hpp','visit_reception_director.hpp','visit_reception_world.hpp']],'tests':['native/world/tests/resident/visit_reception_test.cpp']},ensure_ascii=False,indent=2)+'\n');print(a.output/'manifest.json')


if __name__=='__main__':main()
