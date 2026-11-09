// 原地址不是远方人物定位；只在实际抵达后从眼前岗位/会话确认该Office本人。
#include "task_responsibility_director.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
namespace {
using Result=Outcome<std::optional<DecisionProposal>>;
// 修改的是本次复制的transition，不回写调用方的旧计划或公平轮询状态。
DirectorTransition& transition(DecisionProposal& p){return const_cast<DirectorTransition&>(*p.transition);}
Result wait(DecisionProposal p,const StableId& code){p.command.reset();transition(p).phase=PlanPhase::waiting;transition(p).wait_reason=code;return {true,std::move(p),{}};}
// 当场真人必须确属冻结目标Office的现任有权者；同组织另一有权者不够。
Outcome<bool> local_representative(const CommonStore& c,const TaskResponsibilityDirectorContext& context,
    const KnownSubjectView& view,const EnvoyTaskAcceptance& accepted,bool report,const StableId& actor){
    if(actor.empty()||actor==view.actor_id)return {true,false,{}};
    const auto* person=c.actor(actor);if(!person||!person->alive||!person->available||!person->action_id.empty())return {true,false,{}};
    auto location=c.resolved_location(actor);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||here->place_id!=view.place_id||here->settlement_id!=view.settlement_id)return {true,false,{}};
    const auto& office=report?accepted.report_office_id:accepted.target_office_id;
    const auto epoch=context.organization.authority_epoch(office);if(!epoch)return {true,false,{}};
    const ActingAuthority authority{report?accepted.issuer_organization_id:accepted.target_organization_id,office,*epoch,actor};
    auto allowed=context.organization.authorize(authority,report?"diplomatic_receive_envoy_report":"diplomatic_receive_passage",{ScopeRef::Kind::site,view.place_id},0);
    if(!allowed.ok){if(allowed.error.kind==ErrorKind::rule)return {true,false,{}};return {false,false,allowed.error};}
    return {true,true,{}};
}
// 确认使用真实原命令报价；动作类型与task/provider不允许回调替换。
Result propose_envoy(DecisionProposal p,const TaskResponsibilityDirectorContext& context,
    const CommonStore& c,const StableId& actor,const StableId& task,const StableId& provider,bool report){
    if(!context.preview_envoy)return {false,{},common_failure("TaskEnvoyContinuationPreviewRequired",task,ErrorKind::internal).error};
    auto allowed=context.preview_envoy(actor,report?EnvoyTaskActionKind::report:EnvoyTaskActionKind::deliver,task,provider,c.revision());
    if(!allowed.ok){if(allowed.error.kind==ErrorKind::rule)return wait(std::move(p),allowed.error.original.code);return {false,{},allowed.error};}
    DiplomacyCommand command=report?DiplomacyCommand{DiplomaticEnvoyReportCommand{task,provider,c.revision()}}:DiplomacyCommand{DiplomaticEnvoyDeliverCommand{task,provider,c.revision()}};
    p.command=std::make_shared<const CommandPayload>(CommandPayload{report?"envoy_report":"envoy_deliver",std::move(command)});
    transition(p).phase=report?PlanPhase::reporting:PlanPhase::delivering;transition(p).next_step_index=report?4:3;transition(p).wait_reason.clear();return {true,std::move(p),{}};
}
}
Result propose_task_responsibility_continuation(const CommonStore& common,const TaskState& tasks,
    const CommonCatalog& shared,const TravelCatalog& travel,const KnownSubjectView& view,
    const StableId& task_id,const DecisionProposal& base,const TaskAuthoredContinuationCatalog* policy,
    const TaskResponsibilityDirectorContext* context){
    if(!policy||policy->version==1)return {true,std::nullopt,{}};
    const auto fail=[&](const StableId& code)->Result{return {false,{},common_failure(code,task_id,ErrorKind::internal).error};};
    if(policy->version!=2)return fail("UnsupportedAuthoredTaskContinuationVersion");
    if(!context)return fail("TaskResponsibilityContinuationReadRequired");
    if(!base.transition||base.subject_id!=view.actor_id||view.subject.kind!=SubjectKind::actor||view.subject.id!=view.actor_id
        ||view.tick!=common.tick()||view.revision!=common.revision()||base.transition->expected_revision!=common.revision())return fail("InvalidTaskContinuationSource");
    auto source=read_task_responsibility_continuation(common,tasks,context->sources,view.actor_id,task_id);if(!source.ok)return {false,{},source.error};if(!source.value)return {true,std::nullopt,{}};
    DecisionProposal proposal=base;proposal.transition=std::make_shared<DirectorTransition>(*base.transition);proposal.command.reset();auto& next=transition(proposal);next.template_id="maintain_commitment";next.goal_ref=task_id;next.commitment_ids={task_id};next.current_action_id.reset();next.wait_reason.clear();
    const auto* actor=common.actor(view.actor_id);if(!actor||!actor->alive||!actor->available||!view.alive||!view.available)return wait(std::move(proposal),"ActorUnavailable");
    if(!source.value->action_id.empty()){next.current_action_id=source.value->action_id;return wait(std::move(proposal),source.value->pending_code.empty()?"TaskResponsibilityActionPending":source.value->pending_code);}
    if(!actor->action_id.empty()||!view.current_action_id.empty())return wait(std::move(proposal),"TaskContinuationOccupied");
    if(const auto* governance=std::get_if<GovernanceTaskSource>(&source.value->responsibility)){
        if(governance->completion)return fail("TaskResponsibilityNotPending");
        return wait(std::move(proposal),source.value->pending_code.empty()?"GovernanceSettlementPending":source.value->pending_code);
    }
    const auto& envoy=std::get<EnvoyTaskSource>(source.value->responsibility);if(envoy.completion)return fail("TaskResponsibilityNotPending");if(!source.value->mission_continuity)return fail("TaskResponsibilityContinuityRequired");
    const auto& continuity=*source.value->mission_continuity;
    if(continuity.kind!=DiplomaticMissionContinuityKind::continue_work){
        StableId code=continuity.code;if(code.empty())code=continuity.kind==DiplomaticMissionContinuityKind::pause_envoy?"EnvoyUnavailable":continuity.kind==DiplomaticMissionContinuityKind::needs_reassignment?"EnvoyNeedsReassignment":"EnvoyTerminationPending";
        return wait(std::move(proposal),code);
    }
    const bool report=envoy.delivery.has_value();const auto& accepted=envoy.acceptance;
    if(report&&(!accepted.return_report_required||source.value->module_version!=2))return wait(std::move(proposal),"EnvoyReportUnavailable");
    if(!report&&common.tick()>=accepted.delivery_expires_tick)return wait(std::move(proposal),"EnvoyDeliveryExpired");
    if(std::holds_alternative<FollowingLocation>(actor->location))return wait(std::move(proposal),"TaskContinuationNeedsIndependentLeader");
    auto location=common.resolved_location(actor->id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||here->place_id!=view.place_id||here->settlement_id!=view.settlement_id)return wait(std::move(proposal),"TaskContinuationNotAtPlace");
    const auto& destination=report?accepted.report_site_id:accepted.delivery_site_id;
    if(!shared.places.find(destination))return fail("TaskResponsibilityDestinationMissing");
    // 现会面有最高占用效力，不能为了沿既知地址行旅而自动关闭他人的交谈。
    const InteractionSession* meeting=nullptr;for(const auto& session:context->interaction.interactions)if(session.actor_id==view.actor_id||session.context.provider_actor_id==view.actor_id){if(meeting)return fail("AmbiguousInteractionContact");meeting=&session;}
    if(view.place_id!=destination){if(meeting||view.interaction_busy)return wait(std::move(proposal),"TaskContinuationOccupied");proposal.command=director_move_toward(travel,view,destination);if(!proposal.command)return wait(std::move(proposal),"TaskContinuationRouteUnavailable");next.phase=report?PlanPhase::reporting:PlanPhase::delivering;next.next_step_index=report?4:3;return {true,std::move(proposal),{}};}
    if(meeting){
        const auto provider=meeting->actor_id==view.actor_id?meeting->context.provider_actor_id:meeting->actor_id;
        auto allowed=local_representative(common,*context,view,accepted,report,provider);if(!allowed.ok)return {false,{},allowed.error};
        if(!allowed.value)return wait(std::move(proposal),"EnvoyUnrelatedConversation");
        auto contact=preview_face_to_face_contact(common,context->interaction,context->interaction_catalog,context->interaction_access,view.actor_id,provider);
        if(!contact.ok){if(contact.error.kind==ErrorKind::rule)return wait(std::move(proposal),contact.error.original.code);return {false,{},contact.error};}
        return propose_envoy(std::move(proposal),*context,common,view.actor_id,task_id,provider,report);
    }
    if(view.interaction_busy)return wait(std::move(proposal),"TaskContinuationOccupied");
    for(const auto& row:shared.roles.rows()){
        if(row.second.place_id!=destination)continue;const auto* assignment=common.state().roles.find(row.first);
        if(!assignment||!assignment->enabled||std::find(view.locally_occupied_actor_ids.begin(),view.locally_occupied_actor_ids.end(),assignment->actor_id)!=view.locally_occupied_actor_ids.end())continue;
        auto allowed=local_representative(common,*context,view,accepted,report,assignment->actor_id);if(!allowed.ok)return {false,{},allowed.error};if(!allowed.value)continue;
        InteractionCommand talk=Talk{row.first,{}};auto quote=preview_interaction(common,context->interaction,context->interaction_catalog,context->interaction_access,view.actor_id,talk);
        if(!quote.ok){if(quote.error.kind==ErrorKind::rule)continue;return {false,{},quote.error};}
        proposal.command=std::make_shared<const CommandPayload>(CommandPayload{"talk",std::move(talk)});next.phase=report?PlanPhase::reporting:PlanPhase::delivering;next.next_step_index=report?4:3;return {true,std::move(proposal),{}};
    }
    return wait(std::move(proposal),"EnvoyLocalRepresentativeUnavailable");
}
}
