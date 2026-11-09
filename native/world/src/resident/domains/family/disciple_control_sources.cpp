#include "resident/kernel/catalog_section_observation.hpp"
// 原私人控制意愿仍由既有say证明，本口仅提供独立选中的非血缘关系与当时成年人资格。
#include "disciple_control_sources.hpp"
#include "apprenticeship_receipts.hpp"
#include "succession_personal_assets.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <charconv>
#include <type_traits>
namespace hanmo::resident {namespace {
Outcome<DiscipleControlRelationSource> fail(const StableId& code){return {false,{},common_failure(code).error};}
// 只有真实原legacy接受/撤回头可作本冷口的界；激活后的死亡依据另沿原Life读取。
bool boundary(const CommonStore& c,const StableId& id){const StableId prefix="family_legacy:";if(id.rfind(prefix,0)!=0)return false;auto digits=id.substr(prefix.size());Integer number=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),number);const auto* head=c.state().facts.find(id);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&number>0&&number<c.state().next_instance_id&&std::to_string(number)==digits&&head&&head->id==id&&head->domain=="family_legacy"&&(head->kind=="family_legacy_control_consent"||head->kind=="family_legacy_control_revocation");}
Outcome<DiscipleControlRelationSource> exact(const DiscipleControlRelationContext& access,const StableId& request,const StableId& relation,Integer tick,const StableId& before,bool revocation=false){
    auto valid=validate_disciple_control_catalog(access.catalog,access.original_requests,access.common_catalog);if(!valid.ok)return {false,{},valid.error};if(!access.catalog.enabled)return fail("DiscipleControlProfileRequired");
    const auto* selected=access.catalog.requests.find(request);const auto* definition=access.original_requests.definitions.find(request);if(!selected||!definition)return fail("DiscipleControlOriginalRequestRequired");
    valid=validate_apprenticeship_result_bindings(access.common,access.apprenticeships);if(!valid.ok)return {false,{},valid.error};
    if(access.common_catalog.family_birth_identity_enabled&&!access.people.births)return fail("DiscipleControlBirthSourceRequired");
    if(tick<0||tick>access.common.tick()||(before.empty()&&tick!=access.common.tick()))return fail("DiscipleControlTimeMismatch");
    const auto* accepted=access.apprenticeships.replies.find(relation);if(!accepted||accepted->decision!="accepted"||!accepted->confirmation)return fail("DiscipleControlRelationshipRequired");const auto& source=*accepted->confirmation;
    if(source.teacher_actor_id!=definition->predecessor_actor_id||source.student_actor_id!=definition->successor_actor_id||source.skill_id!=selected->skill_id)return fail("DiscipleControlRelationshipMismatch");
    if(before.empty()){
        if(accepted->tick>tick)return fail("DiscipleControlTimeMismatch");
        for(const auto& row:access.apprenticeships.exits.rows())if(row.second.bond_id==relation&&!revocation)return fail("DiscipleControlRelationshipEnded");
    }else{
        if(!boundary(access.common,before))return fail("DiscipleControlConsentBoundaryRequired");auto old=read_apprenticeship_bond_before_fact(access.common,access.apprenticeships,relation,tick,before);if(!old.ok)return {false,{},old.error};if(!old.value||(old.value->exit_fact_id&&!revocation))return fail("DiscipleControlRelationshipBeforeConsentRequired");
    }
    for(const auto* actor:{&definition->predecessor_actor_id,&definition->successor_actor_id}){valid=validate_apprenticeship_person(access.common,access.common_catalog,access.people,*actor,tick,before);if(!valid.ok)return {false,{},valid.error};}
    return {true,{access.common.state().identity,request,relation,definition->predecessor_actor_id,definition->successor_actor_id,selected->skill_id,before,tick},{}};
}
}
Outcome<DiscipleControlCatalog> decode_disciple_control_catalog(const BoundaryValue& value){try{
    auto valid=validate_succession_json_unique(value);if(!valid.ok)return {false,{},valid.error};using namespace boundary;fields(value,{"version","profile","requests"},{},"family_disciple_legacy");if(get_int(value,"version","")!=1||get_string(value,"profile","")!="accepted_disciple_posthumous_control_v1")fail("UnsupportedDiscipleControlProfile","");DiscipleControlCatalog out;out.enabled=true;const auto& rows=object(field(value,"requests",""),"requests");if(rows.empty()||rows.size()>4096)fail("InvalidDiscipleControlRequestCount","");for(const auto& row:rows){fields(row.second,{"request_id","skill_id"},{},row.first);DiscipleControlRequestBasis basis{get_string(row.second,"request_id",""),get_string(row.second,"skill_id","")};if(row.first!=basis.request_id||row.first.empty()||row.first.size()>256||!out.requests.insert(row.first,basis))fail("InvalidDiscipleControlRequest",row.first);}return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_disciple_control_catalog(const DiscipleControlCatalog& catalog,const LegacyControlConsentCatalog& requests,const CommonCatalog& common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 32, "DiscipleControlCatalog", "validate_disciple_control_catalog", &catalog);

    if(!catalog.enabled)return catalog.requests.empty()?common_success():common_failure("DiscipleControlProfileRequired");if(!requests.enabled||catalog.requests.empty()||catalog.requests.size()>4096)return common_failure("DiscipleControlOriginalRequestsRequired");
    for(const auto& row:catalog.requests.rows()){const auto* original=requests.definitions.find(row.first);if(row.first!=row.second.request_id||!original||original->id!=row.first||original->predecessor_actor_id==original->successor_actor_id||!growth_skill_defined(row.second.skill_id,common.extended_skills))return common_failure("DiscipleControlOriginalRequestMismatch",row.first);}return common_success();
}
Status validate_disciple_control_catalog(const DiscipleControlCatalog& catalog,const LegacyControlConsentCatalog& requests,const CommonCatalog& common,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 33, "DiscipleControlCatalog", "validate_disciple_control_catalog", &catalog);

    auto valid=validate_disciple_control_catalog(catalog,requests,common);if(!valid.ok||!catalog.enabled)return valid;valid=validate_legacy_control_consent_catalog(requests,common,interaction);if(!valid.ok)return valid;
    for(const auto& row:catalog.requests.rows()){const auto* definition=requests.definitions.find(row.first);const DialogueDefinition* talk=nullptr;for(const auto& d:interaction.dialogues)if(d.id==definition->dialogue_id)talk=&d;const auto* role=talk?common.roles.find(talk->context_role):nullptr;if(!role||role->organization_id||role->initial_actor!=definition->successor_actor_id)return common_failure("DiscipleControlPersonalSpeakerRequired",row.first);for(const auto* id:{&definition->accept_node_id,&definition->revoke_node_id}){const DialogueNode* node=nullptr;for(const auto& n:talk->nodes)if(n.id==*id)node=&n;if(!node||node->kind!="say"||node->speaker!="$provider")return common_failure("DiscipleControlPersonalSayRequired",row.first);}}
    return common_success();
}
Outcome<DiscipleControlRelationSource> select_current_disciple_control_relation(const DiscipleControlRelationContext& access,const StableId& request){
    auto valid=validate_disciple_control_catalog(access.catalog,access.original_requests,access.common_catalog);if(!valid.ok)return {false,{},valid.error};const auto* definition=access.original_requests.definitions.find(request);const auto* basis=access.catalog.requests.find(request);if(!access.catalog.enabled||!definition||!basis)return fail("DiscipleControlOriginalRequestRequired");StableId selected;
    for(const auto& row:access.apprenticeships.replies.rows())if(row.second.decision=="accepted"&&row.second.confirmation){const auto& s=*row.second.confirmation;if(s.teacher_actor_id!=definition->predecessor_actor_id||s.student_actor_id!=definition->successor_actor_id||s.skill_id!=basis->skill_id)continue;bool ended=false;for(const auto& exit:access.apprenticeships.exits.rows())if(exit.second.bond_id==row.first)ended=true;if(ended)continue;if(!selected.empty())return fail("DiscipleControlRelationshipAmbiguous");selected=row.first;}
    return selected.empty()?fail("DiscipleControlRelationshipRequired"):exact(access,request,selected,access.common.tick(),{});
}
Outcome<DiscipleControlRelationSource> read_current_disciple_control_revocation_relation(const DiscipleControlRelationContext& access,const StableId& request,const StableId& relation){return exact(access,request,relation,access.common.tick(),{},true);}
Outcome<DiscipleControlRelationSource> read_disciple_control_relation_before_consent(const DiscipleControlRelationContext& access,const StableId& request,const StableId& relation,Integer tick,const StableId& before){if(before.empty())return fail("DiscipleControlConsentBoundaryRequired");const auto* head=access.common.state().facts.find(before);return exact(access,request,relation,tick,before,head&&head->kind=="family_legacy_control_revocation");}
// 事实前读口不接策略布尔尾参；要求声明与定义唯一，避免静默重载留下未定义符号。
static_assert(std::is_pointer_v<decltype(&read_disciple_control_relation_before_consent)>,"disciple historical reader signature must be unique");

}
