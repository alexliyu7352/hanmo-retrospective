// 同一机会的一次明确暂拒只保存首次来源；重新承接仍走原Need认领规则。
#include "task_runtime_offer_response.hpp"
#include "task_model.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/director/director_model.hpp"
#include <set>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
bool same_response(const RuntimeTaskOfferResponse& a,const RuntimeTaskOfferResponse& b){return a.actor_id==b.actor_id&&a.offer_id==b.offer_id&&a.need_id==b.need_id&&a.affair_id==b.affair_id&&a.terms_version==b.terms_version&&a.session_id==b.session_id&&a.need_revision==b.need_revision&&a.response_tick==b.response_tick&&same_context(a.context,b.context)&&a.source.report_id==b.source.report_id&&a.source.fact_id==b.source.fact_id&&a.source.source_key==b.source.source_key&&a.source.observation_tick==b.source.observation_tick&&a.source.learned_tick==b.source.learned_tick&&a.source.local_public==b.source.local_public;}
const KnownReport* received(const SocialState& social,const StableId& actor,const StableId& report){if(!social.dynamic)return nullptr;auto* table=social.dynamic->known_reports.find(std::to_string(static_cast<int>(SubjectKind::actor))+":"+actor);return table?table->find(report):nullptr;}
bool report_at_context(const InteractionCatalog& catalog,const Report& report,const InteractionContext& context){
    if(!catalog.common||!report.observation.supply)return false;const auto* place=catalog.common->places.find(context.place_id);if(!place)return false;
    const auto& site=report.observation.supply->site_id;return site==context.place_id||site==place->settlement_id;
}
// 原社会域已验证报告payload与唯一事实；此处再核本次回应引用和当时可见来源。
Status validate_row(const CommonStore& common,const TaskState& tasks,const AffairsState& affairs,const SocialState& social,const InteractionCatalog& catalog,const RuntimeTaskOfferResponse& row){
    const auto* offer=tasks.offer_instances.find(row.offer_id);const auto* need=affairs.needs.find(row.need_id);
    const auto* role=catalog.common?catalog.common->roles.find(row.context.role_id):nullptr;
    if(!offer||!need||!role||!common.actor(row.actor_id)||!common.actor(row.context.provider_actor_id)||row.actor_id==row.context.provider_actor_id||row.session_id.empty())return common_failure("InvalidRuntimeOfferResponse");
    if(row.need_id!=offer->source_need_id||row.affair_id!=need->affair_id||row.terms_version!=offer->terms_version||row.need_revision<1||row.need_revision>need->revision||row.response_tick<need->created_tick||row.response_tick<offer->available_from_tick||row.response_tick>common.tick()||(offer->expires_tick&&row.response_tick>*offer->expires_tick))return common_failure("InvalidRuntimeOfferResponseTerms");
    if(!face_to_face_contact_context_valid(row.context)||row.context.kind||row.context.role_id!=offer->definition.accept_role||role->place_id!=row.context.place_id||offer->accept_scope.kind!=ScopeRef::Kind::site||row.context.place_id!=offer->accept_scope.id||(offer->issuer.kind==Owner::Kind::actor&&offer->issuer.id!=row.context.provider_actor_id))return common_failure("InvalidRuntimeOfferResponseContext");
    const auto* report=social.dynamic?social.dynamic->reports.find(row.source.report_id):nullptr;
    const auto* fact=report?common.state().facts.find(report->fact_id):nullptr;
    if(!report||!fact||fact->id!=row.source.fact_id||report->fact_id!=row.source.fact_id||report->payload_kind!=fact->kind||report->observation.id!=row.source.fact_id||!report->observation.supply||report->observation_tick!=row.source.observation_tick||report->observation.supply->source_key!=row.source.source_key||row.source.source_key!=need->source_key||row.source.observation_tick<0||row.source.observation_tick>row.source.learned_tick||row.source.learned_tick>row.response_tick)return common_failure("InvalidRuntimeOfferResponseSource");
    if(row.source.local_public){if(report->visibility_scope!=ReportVisibility::local_public||row.source.learned_tick!=row.response_tick||!report_at_context(catalog,*report,row.context))return common_failure("InvalidRuntimeOfferResponseVisibility");}
    else{const auto* learned=received(social,row.actor_id,report->id);if(!learned||learned->receiver.kind!=SubjectKind::actor||learned->receiver.id!=row.actor_id||learned->learned_tick!=row.source.learned_tick)return common_failure("InvalidRuntimeOfferResponseVisibility");}
    return common_success();
}
}
const RuntimeTaskOfferResponse* runtime_task_offer_response(const RuntimeTaskOfferResponses& state,const StableId& actor,const StableId& offer){for(const auto& row:state.responses)if(row.actor_id==actor&&row.offer_id==offer)return &row;return nullptr;}
Outcome<RuntimeTaskOfferResponseQuote> preview_runtime_task_offer_decline(const CommonStore& common,const TaskState& tasks,const AffairsState& affairs,const SocialState& social,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const KnownSubjectView& known,const StableId& actor,const TaskCommand& command){
    using Result=Outcome<RuntimeTaskOfferResponseQuote>;const auto fail=[](const StableId& code)->Result{return {false,{},common_failure(code).error};};
    if(command.kind!="decline_runtime_offer")return fail("UnsupportedAction");const auto ref=task_command_offer(command);
    if(ref.kind!=OfferRef::Kind::runtime||command.quantity||!command.need_revision||(!command.offer_id.empty()&&command.offer_id!=ref.id))return fail("InvalidRuntimeOfferResponseCommand");
    if(known.actor_id!=actor||known.subject.kind!=SubjectKind::actor||known.subject.id!=actor||known.tick!=common.tick()||known.revision!=common.revision())return fail("StaleRuntimeOfferKnowledge");
    const auto* offer=tasks.offer_instances.find(ref.id);const auto* need=offer?affairs.needs.find(offer->source_need_id):nullptr;
    // 不存在与本人未获知都走同一拒绝，不能探测全作者/全世界动态机会。
    const KnownSupply* observed=nullptr;if(offer&&need)for(const auto& supply:known.supplies)if(supply.source_key==need->source_key){observed=&supply;break;}
    if(!observed||!social.dynamic||std::find(known.known_report_ids.begin(),known.known_report_ids.end(),observed->report_id)==known.known_report_ids.end())return fail("SourceUnknown");
    auto face=preview_face_to_face_contact(common,interaction,catalog,access,actor,command.context.provider_actor_id);if(!face.ok)return {false,{},face.error};
    if(face.value.session_actor_id!=actor||command.context.place_id!=face.value.context.place_id||known.place_id!=command.context.place_id)return fail("StaleTaskConversation");
    auto contact=validate_interaction(common,interaction,catalog,access,actor,command.context,"accept_offer");if(!contact.ok)return {false,{},contact.error};
    if(contact.value.role_id!=offer->definition.accept_role||contact.value.place_id!=offer->accept_scope.id)return fail("RoleNotAuthorized");
    if(offer->status!=OfferStatus::published||offer->available_from_tick>common.tick()||(offer->expires_tick&&*offer->expires_tick<common.tick()))return fail("OfferUnavailable");
    if(*command.need_revision!=need->revision)return fail("StaleNeedRevision");
    if(need->status!=NeedStatus::open&&need->status!=NeedStatus::partial)return fail("NeedNotOpen");
    auto available=need_available_amount(affairs,*need);if(!available.ok)return {false,{},available.error};if(available.value<=0)return fail("NeedAlreadyClaimed");
    if(const auto* old=task_for(tasks,actor,offer->id))if(old->contract_id){const auto* contract=tasks.contracts.find(*old->contract_id);if(!contract)return fail("InvalidTaskContract");if(contract->status!=ContractStatus::settled)return fail("OfferAlreadyAccepted");}
    RuntimeTaskOfferResponse row;row.actor_id=actor;row.offer_id=offer->id;row.need_id=need->id;row.affair_id=need->affair_id;row.terms_version=offer->terms_version;row.session_id=face.value.session_id;row.need_revision=need->revision;row.response_tick=common.tick();row.context=contact.value;
    row.source={observed->report_id,observed->fact_id,observed->source_key,observed->observation_tick,observed->learned_tick,false};
    if(const auto* learned=received(social,actor,observed->report_id))row.source.learned_tick=learned->learned_tick;else{row.source.local_public=true;row.source.learned_tick=common.tick();}
    auto valid=validate_row(common,tasks,affairs,social,catalog,row);if(!valid.ok)return {false,{},valid.error};
    return {true,{common.state().identity,std::move(face.value),std::move(row)},{}};
}
Status record_runtime_task_offer_decline(const CommonStore& common,const TaskState& tasks,const AffairsState& affairs,const SocialState& social,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const KnownSubjectView& known,const StableId& actor,const TaskCommand& command,const RuntimeTaskOfferResponseQuote& quote,RuntimeTaskOfferResponses& state){
    if(!same_identity(quote.source_identity,common.state().identity))return common_failure("StaleRuntimeOfferResponseQuote");
    auto contact=revalidate_face_to_face_contact(common,interaction,catalog,access,quote.contact);if(!contact.ok)return contact;
    auto fresh=preview_runtime_task_offer_decline(common,tasks,affairs,social,interaction,catalog,access,known,actor,command);if(!fresh.ok)return {false,{},fresh.error};
    if(!same_response(quote.response,fresh.value.response)||quote.contact.actor_id!=actor||quote.contact.target_actor_id!=fresh.value.contact.target_actor_id||quote.contact.session_actor_id!=actor||quote.contact.session_id!=fresh.value.contact.session_id)return common_failure("StaleRuntimeOfferResponseQuote");
    if(runtime_task_offer_response(state,actor,quote.response.offer_id))return common_success();state.responses.push_back(fresh.value.response);return common_success();
}
Status validate_runtime_task_offer_responses(const CommonStore& common,const TaskState& tasks,const AffairsState& affairs,const SocialState& social,const InteractionCatalog& catalog,const RuntimeTaskOfferResponses& state){
    std::set<std::pair<StableId,StableId>> seen;for(const auto& row:state.responses){if(!seen.emplace(row.actor_id,row.offer_id).second)return common_failure("DuplicateRuntimeOfferResponse");auto valid=validate_row(common,tasks,affairs,social,catalog,row);if(!valid.ok)return valid;}return common_success();
}
}
