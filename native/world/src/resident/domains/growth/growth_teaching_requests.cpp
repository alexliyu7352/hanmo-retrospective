#include "resident/kernel/catalog_section_observation.hpp"
#include "growth_teaching_requests.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool id(const StableId& value){return !value.empty()&&value.size()<=256;}
const DialogueDefinition* dialogue(const InteractionCatalog& catalog,const StableId& name){for(const auto& row:catalog.dialogues)if(row.id==name)return &row;return nullptr;}
const DialogueNode* node(const DialogueDefinition& definition,const StableId& name){for(const auto& row:definition.nodes)if(row.id==name)return &row;return nullptr;}
bool context_equal(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
}
bool growth_teaching_agreement_equal(const GrowthTeachingAgreement& a,const GrowthTeachingAgreement& b){return std::tie(a.request_id,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.accepted_tick,a.accepted_revision)==std::tie(b.request_id,b.teacher_actor_id,b.student_actor_id,b.skill_id,b.dialogue_id,b.node_id,b.node_kind,b.session_id,b.accepted_tick,b.accepted_revision)&&context_equal(a.context,b.context);}
// 目录是明确的本人求教语义；普通台词和相同地点都不能替代这份具名声明。
Status validate_growth_teaching_request_catalog(const GrowthTeachingRequestCatalog& requests,const CommonCatalog& shared,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 47, "GrowthTeachingRequestCatalog", "validate_growth_teaching_request_catalog", &requests);

    if(!requests.enabled)return requests.definitions.empty()?common_success():common_failure("TeachingRequestsCapabilityRequired");
    if(requests.definitions.empty()||requests.definitions.size()>4096)return common_failure("InvalidTeachingRequestCatalogSize");
    std::set<std::tuple<StableId,StableId,StableId,StableId,StableId>> bindings;
    for(const auto& entry:requests.definitions.rows()){
        const auto& request=entry.second;
        for(const auto* field:{&request.id,&request.teacher_actor_id,&request.student_actor_id,&request.skill_id,&request.dialogue_id,&request.node_id})if(!id(*field))return common_failure("InvalidTeachingRequestDefinition",entry.first);
        if(request.id!=entry.first||request.teacher_actor_id==request.student_actor_id)return common_failure("InvalidTeachingRequestParties",entry.first);
        for(const auto* actor:{&request.teacher_actor_id,&request.student_actor_id})if(std::find(shared.actor_ids.begin(),shared.actor_ids.end(),*actor)==shared.actor_ids.end())return common_failure("UnknownTeachingRequestActor",*actor);
        if(!shared.personal_skill_kinds.find(request.skill_id))return common_failure("UnknownTeachingRequestSkill",request.skill_id);
        const auto* definition=dialogue(interaction,request.dialogue_id);
        if(!definition||definition->context_role.empty()||!shared.roles.find(definition->context_role))return common_failure("TeachingRequestOrdinaryDialogueRequired",entry.first);
        const auto* at=node(*definition,request.node_id);
        if(!at||(at->kind!="say"&&at->kind!="choice"&&at->kind!="action"))return common_failure("InvalidTeachingRequestNode",entry.first);
        if(at->kind=="action"&&(!at->command||at->command->kind!="teach_skill"||at->command->skill_id!=request.skill_id))return common_failure("TeachingRequestActionMismatch",entry.first);
        if(!bindings.emplace(request.teacher_actor_id,request.student_actor_id,request.skill_id,request.dialogue_id,request.node_id).second)return common_failure("DuplicateTeachingRequestBinding",entry.first);
    }
    return common_success();
}
// 本函数不核掌握/成长数值，后续仍必须经过原teach_skill完整资格和费用规则。
Outcome<GrowthTeachingRequestQuote> preview_growth_teaching_request(const CommonStore& common,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const GrowthTeachingRequestCatalog& requests,const StableId& teacher,const StableId& student,const StableId& skill){
    if(!requests.enabled)return {false,{},common_failure("TeachingRequestsUnavailable").error};
    if(!interaction.common)return {false,{},common_failure("MissingTeachingCommonCatalog").error};
    const auto valid=validate_growth_teaching_request_catalog(requests,*interaction.common,interaction);if(!valid.ok)return {false,{},valid.error};
    const auto source=preview_authored_node_source(common,state,interaction,access,teacher,student);if(!source.ok)return {false,{},source.error};
    if(source.value.context.kind)return {false,{},common_failure("TeachingRequestOrdinaryContextRequired").error};
    const GrowthTeachingRequestDefinition* match=nullptr;
    for(const auto& entry:requests.definitions.rows()){
        const auto& request=entry.second;
        if(request.teacher_actor_id==teacher&&request.student_actor_id==student&&request.skill_id==skill&&request.dialogue_id==source.value.dialogue_id&&request.node_id==source.value.node_id){match=&request;break;}
    }
    if(!match)return {false,{},common_failure("TeachingRequestNotOffered").error};
    GrowthTeachingAgreement agreed{match->id,teacher,student,skill,source.value.dialogue_id,source.value.node_id,source.value.node_kind,source.value.session_id,source.value.context,source.value.source_identity.tick,source.value.source_identity.revision};
    const auto checked=validate_growth_teaching_agreement_source(common.state(),*interaction.common,interaction,requests,agreed);if(!checked.ok)return {false,{},checked.error};
    return {true,{source.value,std::move(agreed)},{}};
}
// 在共同内核分配新action/占用双方之前重验；旧预览不得跨节点、跨人或跨执行代接受。
Status revalidate_growth_teaching_request(const CommonStore& common,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const GrowthTeachingRequestCatalog& requests,const GrowthTeachingRequestQuote& quote){
    const auto& proof=quote.source;const auto& agreed=quote.agreement;
    // 两份分别有效的会话和协议也不能拼接；同意双方必须就是此临时证明的双方。
    if(std::tie(proof.actor_id,proof.provider_actor_id,proof.session_id,proof.dialogue_id,proof.node_id,proof.node_kind,proof.source_identity.tick,proof.source_identity.revision)!=std::tie(agreed.teacher_actor_id,agreed.student_actor_id,agreed.session_id,agreed.dialogue_id,agreed.node_id,agreed.node_kind,agreed.accepted_tick,agreed.accepted_revision)||!context_equal(proof.context,agreed.context))return common_failure("TeachingRequestSourceMismatch");
    auto source=revalidate_authored_node_source(common,state,interaction,access,quote.source);if(!source.ok)return source;
    const auto fresh=preview_growth_teaching_request(common,state,interaction,access,requests,quote.agreement.teacher_actor_id,quote.agreement.student_actor_id,quote.agreement.skill_id);
    if(!fresh.ok)return {false,{},fresh.error};
    if(!growth_teaching_agreement_equal(quote.agreement,fresh.value.agreement))return common_failure("StaleTeachingRequest");
    return common_success();
}
// 永久来源只核结构与具体请求绑定；离场、死亡或新执行代不会抹去已经发生的接受。
Status validate_growth_teaching_agreement_source(const CommonState& common,const CommonCatalog& shared,const InteractionCatalog& interaction,const GrowthTeachingRequestCatalog& requests,const GrowthTeachingAgreement& agreed){
    if(!requests.enabled)return common_failure("TeachingRequestsUnavailable");
    const auto valid=validate_growth_teaching_request_catalog(requests,shared,interaction);if(!valid.ok)return valid;
    const auto* request=requests.definitions.find(agreed.request_id);
    if(!request||std::tie(request->teacher_actor_id,request->student_actor_id,request->skill_id,request->dialogue_id,request->node_id)!=std::tie(agreed.teacher_actor_id,agreed.student_actor_id,agreed.skill_id,agreed.dialogue_id,agreed.node_id))return common_failure("TeachingAgreementBindingMismatch");
    const auto* definition=dialogue(interaction,agreed.dialogue_id);const auto* at=node(*definition,agreed.node_id);const auto* role=shared.roles.find(definition->context_role);
    if(!id(agreed.session_id)||agreed.node_kind!=at->kind||!face_to_face_contact_context_valid(agreed.context)||agreed.context.kind||agreed.context.provider_actor_id!=agreed.student_actor_id||agreed.context.role_id!=definition->context_role||role->place_id!=agreed.context.place_id||!shared.places.find(agreed.context.place_id))return common_failure("InvalidTeachingAgreementSource");
    if(!common.actors.find(agreed.teacher_actor_id)||!common.actors.find(agreed.student_actor_id))return common_failure("MissingTeachingAgreementActor");
    if(agreed.accepted_tick<0||agreed.accepted_tick>common.identity.tick||agreed.accepted_revision<0||agreed.accepted_revision>common.identity.revision)return common_failure("InvalidTeachingAgreementTime");
    return common_success();
}
}
