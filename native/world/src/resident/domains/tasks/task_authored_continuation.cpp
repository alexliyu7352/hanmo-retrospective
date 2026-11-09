// 信件目的地址来自原委托与原岗位，不追踪远方真人；实际递交、护送和清算仍走旧业务口。
#include "task_authored_continuation.hpp"
#include "task_model.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/model/procurement_timing.hpp"
namespace hanmo::resident { namespace {
using Result=Outcome<std::optional<DecisionProposal>>;
// 只给本次独有提案设置等待原因，不能修改借来的旧计划或强制半日等待行动。
Result waiting(DecisionProposal value,const StableId& code){
    auto& transition=const_cast<DirectorTransition&>(*value.transition);
    transition.phase=PlanPhase::waiting;transition.wait_reason=code;
    value.command.reset();return {true,std::move(value),{}};
}
// 岗位在当前已知地点才读实际任职和占用；私约与信件始终要求原具名本人。
std::optional<InteractionContext> local_context(const CommonStore& common,
    const CommonCatalog& catalog,const KnownSubjectView& view,const StableId& role_id,
    const StableId& expected_actor){
    const auto* definition=catalog.roles.find(role_id);
    if(!definition||definition->place_id!=view.place_id)return {};
    const auto* assignment=common.state().roles.find(role_id);
    if(!assignment||!assignment->enabled||(!expected_actor.empty()&&assignment->actor_id!=expected_actor)
        ||assignment->actor_id==view.actor_id)return {};
    const auto* person=common.actor(assignment->actor_id);
    if(!person||!person->alive||!person->available||!person->action_id.empty()
        ||std::find(view.locally_occupied_actor_ids.begin(),view.locally_occupied_actor_ids.end(),person->id)!=view.locally_occupied_actor_ids.end())return {};
    auto location=common.resolved_location(person->id);
    const auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!place||place->place_id!=view.place_id)return {};
    return InteractionContext{view.place_id,person->id,role_id,{},{},{},{}};
}
// 原寻路器只返回邻接移动或城口行旅；不能把一个远方目的场所直接作为移动命令。
Result move_to(const TravelCatalog& travel,const KnownSubjectView& view,
    const StableId& destination,DecisionProposal proposal){
    proposal.command=director_move_toward(travel,view,destination);
    if(!proposal.command)return waiting(std::move(proposal),"TaskContinuationRouteUnavailable");
    auto& next=const_cast<DirectorTransition&>(*proposal.transition);
    next.phase=PlanPhase::delivering;next.next_step_index=3;
    return {true,std::move(proposal),{}};
}
// 所有实体变化由统一命令采用执行；本层不调用Task writer或Common recipe。
Result task_command(DecisionProposal proposal,const TaskRecord& task,
    const InteractionContext& context,const StableId& kind){
    TaskCommand command;command.kind=kind;command.offer_id=task.offer_id;
    command.offer_ref=task.offer_ref;command.context=context;
    proposal.command=std::make_shared<const CommandPayload>(CommandPayload{kind,std::move(command)});
    auto& next=const_cast<DirectorTransition&>(*proposal.transition);
    next.phase=kind=="deliver_message"?PlanPhase::delivering:PlanPhase::reporting;
    next.next_step_index=kind=="deliver_message"?3:4;
    return {true,std::move(proposal),{}};
}
}
Outcome<std::optional<DecisionProposal>> propose_authored_task_continuation(
    const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,
    const CommonCatalog& shared,const TravelCatalog& travel,const KnownSubjectView& view,
    const StableId& task_id,const DecisionProposal& original,const TaskAuthoredContinuationCatalog* policy){
    if(!policy)return {true,std::nullopt,{}};
    const auto failed=[&](const StableId& code)->Result{return {false,{},common_failure(code,task_id).error};};
    if(policy->version!=1&&policy->version!=2)return failed("UnsupportedAuthoredTaskContinuationVersion");
    const auto* task=tasks.tasks.find(task_id);
    if(!original.transition||original.subject_id!=view.actor_id||view.subject.kind!=SubjectKind::actor
        ||view.subject.id!=view.actor_id||view.revision!=common.revision()||view.tick!=common.tick()
        ||original.transition->expected_revision!=common.revision()||!task||task->id!=task_id
        ||task->assignee!=view.actor_id)return failed("InvalidTaskContinuationSource");
    const auto* definition=resolve_task_definition(tasks,catalog,*task);
    if(!definition)return failed("DirectorTaskDefinitionMissing");
    if(task->contract_id||(definition->template_id!="message"&&definition->template_id!="escort"))return {true,std::nullopt,{}};
    if(task->status!="active"&&task->status!="ready")return failed("TaskNotActive");
    DecisionProposal proposal=original;
    proposal.transition=std::make_shared<DirectorTransition>(*original.transition);
    auto& next=const_cast<DirectorTransition&>(*proposal.transition);
    next.template_id="maintain_commitment";next.goal_ref=task_id;next.commitment_ids={task_id};
    next.wait_reason.clear();next.current_action_id.reset();proposal.command.reset();
    const auto* actor=common.actor(view.actor_id);
    if(!actor||!actor->alive||!actor->available||!view.alive||!view.available)return waiting(std::move(proposal),"ActorUnavailable");
    if(!actor->action_id.empty()||!view.current_action_id.empty()||view.interaction_busy)return waiting(std::move(proposal),"TaskContinuationOccupied");
    if(std::holds_alternative<FollowingLocation>(actor->location))return waiting(std::move(proposal),"TaskContinuationNeedsIndependentLeader");
    auto location=common.resolved_location(actor->id);
    const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||here->place_id!=view.place_id||here->settlement_id!=view.settlement_id)return waiting(std::move(proposal),"TaskContinuationNotAtPlace");
    const bool escort=definition->template_id=="escort";
    if(escort&&!task->escort)return failed("InvalidEscortState");
    const bool failed_escort=escort&&!task->escort->failure.empty();
    const bool expired=!task_finish_within(common.tick(),task_handover_ticks,task->deadline_tick,authored_task_late_grace_ticks);
    // 已逾原宽限只回原办理人清算；护卫的原失败报告仍按旧规则可报告损失。
    if(task->status=="ready"||expired||failed_escort){
        const auto* role=shared.roles.find(definition->report_role);
        if(!role)return failed("DirectorReportRoleMissing");
        if(view.place_id!=role->place_id)return move_to(travel,view,role->place_id,std::move(proposal));
        const auto issuer=definition->issuer.kind==Owner::Kind::actor?definition->issuer.id:StableId{};
        auto context=local_context(common,shared,view,definition->report_role,issuer);
        if(!context)return waiting(std::move(proposal),"ReportProviderUnavailable");
        return task_command(std::move(proposal),*task,*context,expired&&!failed_escort?"cancel_task":"report_offer");
    }
    if(escort){
        const auto* members=common.members(view.actor_id);
        if(!members||std::find(members->begin(),members->end(),definition->goal.actor_id)==members->end())return waiting(std::move(proposal),"EscortPersonNotFollowing");
        if(view.place_id==definition->goal.destination_place_id)return waiting(std::move(proposal),"EscortArrivalPending");
        return move_to(travel,view,definition->goal.destination_place_id,std::move(proposal));
    }
    const auto* bag=common.inventory(actor->inventory_id);
    if(!bag)return failed("UnknownInventory");
    auto quantity=common.count(bag->id,definition->goal.message_id,{{Owner::Kind::task,task_id,{}}});
    if(!quantity.ok)return {false,{},quantity.error};
    if(quantity.value!=1)return waiting(std::move(proposal),"NoUndeliveredMessage");
    proposal.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,bag->id}));
    // 只使用原收件人已作者化地址，不扫描远方现任岗位；到场才核真实同人。
    const RoleDefinition* address=nullptr;
    for(const auto& row:policy->message_addresses)if(row.offer_id==task->offer_id){
        if(address)return failed("InvalidTaskContinuationAddress");
        address=shared.roles.find(row.recipient_role_id);
        if(!address||address->initial_actor!=definition->goal.receiver_actor
            ||std::find(address->services.begin(),address->services.end(),"deliver_message")==address->services.end())return failed("InvalidTaskContinuationAddress");
    }
    if(!address)return waiting(std::move(proposal),"MessageRecipientAddressUnknown");
    if(view.place_id!=address->place_id)return move_to(travel,view,address->place_id,std::move(proposal));
    auto context=local_context(common,shared,view,address->id,definition->goal.receiver_actor);
    if(!context)return waiting(std::move(proposal),"MessageRecipientUnavailable");
    return task_command(std::move(proposal),*task,*context,"deliver_message");
}
}
