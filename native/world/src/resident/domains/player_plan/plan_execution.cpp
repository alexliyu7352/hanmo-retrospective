#include "resident/kernel/complete_command_capture_dispatch.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
// 有限执行器复用原领域报价、行动和回执；不替玩家回答问题或重放已经发生的收益。
#include "plan_execution.hpp"
#include "plan_sources.hpp"
#include "plan_lifecycle.hpp"
#include "plan_time2.hpp"
#include "resident/kernel/exact_time_world.hpp"
#include "resident/kernel/exact_run_read.hpp"
#include "plan_binding.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/continuation_snapshot.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/domains/military/training_rules.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 规则失败不生成空的成功执行，也不改动当前候选。
template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
// 当前可知的人类决定立即停住，未送达远方消息不会影响许可。
Outcome<StableId> human_stop(const WorldBundle& w,const StableId& actor,const std::optional<StableId>& action){
    if(pending_encounter(w.interaction,actor))return {true,"EncounterPending",{}};
    if(w.services.in_open_battle(actor))return {true,"BattlePending",{}};
    if(!siege_for_actor(w.military,actor).empty())return {true,"SiegePending",{}};
    // 课程全程manual UI窗不是未答题状态；本人答完再明确启动许可后可以继续原课程。
    const auto* person=w.common.actor(actor);const auto* current=person?w.common.action(person->action_id):nullptr;
    if(current&&current->kind=="study_skill"){const auto* value=w.growth.facts.find(current->id);const auto* fact=value?std::get_if<StudyFact>(value):nullptr;if(fact&&fact->challenge&&growth::challenge_pending(*fact->challenge))return {true,"StudyChallengePending",{}};}
    if(military_manual_window(w.common,w.military,actor))return {true,"PlayerPlanManualInputRequired",{}};
    std::optional<TaskShopOrderActionSource> shop_order;
    if(action&&w.task_shop_orders_enabled()){
        if(!w.tasks.shop_orders)return {true,"PlayerPlanActionSourceRequired",{}};
        auto source=query_task_shop_order_action(w.common,w.tasks,*w.tasks.shop_orders,actor,*action);if(!source.ok)return {true,"PlayerPlanActionSourceRequired",{}};shop_order=std::move(source.value);
    }
    std::optional<TaskPersonalMessageActionSource> personal_message;
    if(action&&w.catalog->task_personal_messages){if(!w.tasks.personal_messages)return {true,"PlayerPlanActionSourceRequired",{}};auto source=query_task_personal_message_action(w.common,w.tasks,*w.tasks.personal_messages,actor,*action);if(!source.ok)return {true,"PlayerPlanActionSourceRequired",{}};personal_message=std::move(source.value);}
    std::optional<TaskPersonalReportActionSource> personal_report;
    std::optional<TaskPersonalAcceptanceActionSource> personal_acceptance;
    if(action&&w.catalog->task_personal_reports){if(!w.tasks.personal_reports)return {true,"PlayerPlanActionSourceRequired",{}};auto source=query_task_personal_report_action(w.common,w.tasks,*w.tasks.personal_reports,actor,*action);if(!source.ok)return {true,"PlayerPlanActionSourceRequired",{}};personal_report=std::move(source.value);}
    if(action&&w.catalog->task_personal_acceptances){if(!w.tasks.personal_acceptances)return {true,"PlayerPlanActionSourceRequired",{}};auto source=query_task_personal_acceptance_action(w.common,w.tasks,*w.tasks.personal_acceptances,actor,*action);if(!source.ok)return {true,"PlayerPlanActionSourceRequired",{}};personal_acceptance=std::move(source.value);}
    std::optional<TaskMessageReturnActionSource> message_return;
    if(action&&w.catalog->task_message_returns){if(!w.tasks.message_returns)return {true,"PlayerPlanActionSourceRequired",{}};auto source=query_task_message_return_action(w.common,*w.tasks.message_returns,actor,*action);if(!source.ok)return {true,"PlayerPlanActionSourceRequired",{}};message_return=std::move(source.value);}
    std::optional<GrowthTacticLessonActionSource> lesson;
    if(action&&player_plan_tactic_lesson_action(w,*action)){auto original=read_growth_tactic_lesson_action_source(w.common,w.growth_tactic_lessons,actor,*action);if(!original.ok)return {true,"PlayerPlanActionSourceRequired",{}};lesson=std::move(original.value);}
    std::optional<ContactInquiryActionSource> inquiry;
    if(action&&w.catalog->contact_inquiries){if(!w.interaction.contact_inquiries)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",*action,ErrorKind::internal).error};auto source=query_contact_inquiry_action_world(w,actor,*action);if(!source.ok)return {false,{},std::move(source.error)};inquiry=std::move(source.value);}
    std::optional<VisitAppointmentMeetingActionSource> visit_meeting;
    if(action&&w.catalog->visit_appointment_invitations){if(!w.interaction.visit_appointment_meetings)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",*action,ErrorKind::internal).error};auto source=query_visit_appointment_meeting_action_world(w,actor,*action);if(!source.ok)return {false,{},std::move(source.error)};visit_meeting=std::move(source.value);}
    std::optional<PersonalAppointmentActionSource> appointment;
    if(action&&w.catalog->personal_appointments){if(!w.interaction.personal_appointments)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",*action,ErrorKind::internal).error};auto source=query_personal_appointment_action_world(w,actor,*action);if(!source.ok)return {false,{},std::move(source.error)};appointment=std::move(source.value);}
    if(personal_message&&!player_plan_personal_message_meetings_match(*personal_message,w.interaction.interactions))return {true,"PlayerPlanInteractionPending",{}};
    if(personal_report&&!player_plan_personal_report_meetings_match(*personal_report,w.interaction.interactions))return {true,"PlayerPlanInteractionPending",{}};
    if(personal_acceptance&&!player_plan_personal_acceptance_meetings_match(*personal_acceptance,w.interaction.interactions))return {true,"PlayerPlanInteractionPending",{}};
    // 已开工交货的原say会面不是新的用户决定；只放同session/context/node，别的对话继续停。
    for(const auto& session:w.interaction.interactions)if(session.actor_id==actor){
        if(action&&!personal_message&&!personal_report&&!personal_acceptance&&session.phase=="waiting"&&session.pending_action_id==*action)continue;
        if(shop_order&&task_shop_order_continuation_meeting_matches(*shop_order,session))continue;
        if(personal_message&&task_personal_message_continuation_meeting_matches(*personal_message,session))continue;
        if(personal_report&&task_personal_report_continuation_meeting_matches(*personal_report,session))continue;
        if(personal_acceptance&&task_personal_acceptance_continuation_meeting_matches(*personal_acceptance,session))continue;
        if(message_return&&task_message_return_continuation_meeting_matches(*message_return,session))continue;
        if(lesson&&player_plan_tactic_lesson_meeting_matches(*lesson,session))continue;
        if(inquiry&&player_plan_contact_inquiry_meeting_matches(*inquiry,session))continue;
        if(visit_meeting&&player_plan_visit_appointment_meeting_matches(*visit_meeting,session))continue;
        if(appointment&&appointment->status=="active"){const auto& c=appointment->contact;if(session.id==c.session_id&&session.actor_id==c.actor_id&&session.dialogue_id==c.dialogue_id&&session.node_id==c.node_id&&session.phase=="say"&&session.pending_action_id.empty()&&!session.pending_command&&session.context.provider_actor_id==c.provider_actor_id&&session.context.role_id==c.role_id&&session.context.place_id==c.place_id&&!session.context.kind&&!session.context.encounter_id&&!session.context.input_error)continue;}
        return {true,"PlayerPlanInteractionPending",{}};
    }
    if(w.named_membership_enabled())for(const auto& row:w.organization.membership_offers.rows()){const auto& offer=row.second;if(offer.target_actor_id==actor&&offer.status==MembershipOfferStatus::pending&&offer.created_tick<=w.common.tick()&&offer.expires_tick>w.common.tick()&&membership_fact_known(w.common,w.social,actor,offer.source_fact_id))return {true,"PlayerPlanMembershipDecisionRequired",{}};}
    return {true,{}, {}};
}

// 已到达只能由当前真实位置证明，队列文字不能完成旅行或委托。
bool at_target(const WorldBundle& w,const StableId& actor,const PlayerPlanStepData& data){auto location=w.common.resolved_location(actor);const auto* at=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;if(!at)return false;if(const auto* move=std::get_if<PlayerPlanMove>(&data))return at->place_id==move->place_id;if(const auto* travel=std::get_if<PlayerPlanTravel>(&data)){const auto* target=std::get_if<StableId>(&travel->request.destination);return target&&*target==at->settlement_id;}return false;}
// 停止也是有限成功准备，供中央同候选保存明确原因；不采纳失败的领域apply。
Outcome<PlayerPlanExecution> stopped(PlayerPlanExecution execution,const StableId& reason,bool confirmation=false){execution.kind=PlayerPlanExecutionKind::stop;execution.reason=reason;execution.needs_confirmation=confirmation;execution.nested.reset();return {true,std::move(execution),{}};}
// 不以相同kind替代原命令；数据只能由当前步骤再次产生。
bool command_matches(const PlayerPlanStep& step,const PlayerPlanExecution& execution){if(!execution.nested)return execution.kind!=PlayerPlanExecutionKind::start&&execution.kind!=PlayerPlanExecutionKind::advance;if(execution.nested->nested)return false;if(execution.kind==PlayerPlanExecutionKind::advance)return execution.nested->command.kind=="advance"&&std::holds_alternative<AdvanceCommand>(execution.nested->command.data);auto command=player_plan_domain_command(step.data);if(!command.ok||execution.nested->command.kind!=command.value.kind)return false;
    if(const auto* requested=std::get_if<TravelCommandPayload>(&command.value.data)){const auto* actual=std::get_if<TravelCommandPayload>(&execution.nested->command.data);if(!actual||requested->index()!=actual->index())return false;if(const auto* move=std::get_if<MoveLocalCommand>(requested))return move->place_id==std::get<MoveLocalCommand>(*actual).place_id;if(const auto* travel=std::get_if<TravelCommand>(requested)){const auto& candidate=std::get<TravelCommand>(*actual);const auto* a=std::get_if<StableId>(&travel->destination);const auto* b=std::get_if<StableId>(&candidate.destination);return a&&b&&*a==*b&&travel->via==candidate.via&&travel->has_destination==candidate.has_destination&&travel->has_via==candidate.has_via;}return false;}
    const auto* a=std::get_if<GrowthCommand>(&command.value.data);const auto* b=std::get_if<GrowthCommand>(&execution.nested->command.data);if(!a||!b||a->kind!=b->kind||a->activity!=b->activity||a->skill_id!=b->skill_id||a->ticks!=b->ticks||a->challenge_mode!=b->challenge_mode||a->estate_id!=b->estate_id||a->duration!=b->duration||bool(a->context)!=bool(b->context))return false;
    if(a->context){const auto& x=*a->context;const auto& y=*b->context;if(x.place_id!=y.place_id||x.provider_actor_id!=y.provider_actor_id||x.role_id!=y.role_id||x.kind!=y.kind||x.encounter_id!=y.encounter_id||x.input_error!=y.input_error)return false;}
    return b->item_id.empty()&&b->shop_id.empty()&&b->side.empty()&&b->payer_wallet_id.empty()&&b->action_id.empty()&&b->step_id.empty()&&b->option_id.empty()&&!b->quoted_total&&!b->market_revision&&!b->bargain_quote_id;
}
// 回执中的原业务success/code优先于通用ok；失败课程或中断休整不会触发下一步。
std::pair<bool,StableId> completion_status(const ActionCompletion& result){bool success=result.ok&&result.success.value_or(true);StableId code=result.code.value_or(StableId{});if(result.completed&&!*result.completed)success=false;if(result.growth){success=success&&result.growth->success;if(!result.growth->code.empty())code=result.growth->code;}if(result.task){success=success&&result.task->success;if(result.task->code)code=*result.task->code;}if(result.family){success=success&&result.family->success.value_or(true);if(result.family->code)code=*result.family->code;}return {success,code};}
}
Outcome<PlayerPlanExecution> prepare_player_plan_execution(const WorldBundle& w,const StableId& actor,const PlayerPlanState& state){
    if(!state.current||state.current->actor_id!=actor||actor!=w.common.state().metadata.player_actor_id)return fail<PlayerPlanExecution>("PlayerPlanPlayerRequired");
    const auto* step=next_player_plan_step(state);if(!step)return fail<PlayerPlanExecution>("PlayerPlanCompleted");
    PlayerPlanExecution execution;execution.source=w.common.state().identity;execution.actor_id=actor;execution.plan_id=state.current->id;execution.step_id=step->id;execution.action_id=step->action_id;execution.approved_scope_revision=state.current->approved_scope_revision;
    if(state.version==2){if(!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time)||!w.world_timeline)return fail<PlayerPlanExecution>("PlayerPlanTime2ProfileRequired");auto at=read_world_moment(w.common.tick(),*w.world_timeline);if(!at.ok)return {false,{},at.error};execution.exact_source_moment=std::move(at.value);}
    else if(w.catalog->exact_time)return fail<PlayerPlanExecution>("PlayerPlanTime2Required");
    // 已终止的失败意向不能因新预算复活，原改道后行动仍完全由手动命令管理。
    if(step->completed_tick)return stopped(std::move(execution),step->stop_reason.empty()?StableId{"PlayerPlanActionFailed"}:step->stop_reason,true);
    if(!state.current->stop_reason.empty())return stopped(std::move(execution),state.current->stop_reason);
    if(!state.current->budget)return stopped(std::move(execution),"PlayerPlanBudgetRequired",true);
    if(const auto* continued=std::get_if<PlayerPlanContinue>(&step->data))if(!execution.action_id)execution.action_id=continued->action_id;
    auto stop=human_stop(w,actor,execution.action_id);if(!stop.ok)return {false,{},std::move(stop.error)};if(!stop.value.empty())return stopped(std::move(execution),stop.value,true);
    const auto* person=w.common.actor(actor);if(!person||!person->alive||!person->available)return stopped(std::move(execution),"ActorUnavailable");
    if(execution.action_id){
        const auto* action=w.common.action(*execution.action_id);if(!action||action->actor_id!=actor)return stopped(std::move(execution),"PlayerPlanActionReferenceMissing");
        if(state.version==2){ContinuationSnapshot snapshot;snapshot.continuation_version=2;snapshot.actor_id=actor;snapshot.action_id=action->id;snapshot.action_kind=action->kind;snapshot.action_start_tick=action->start_tick;snapshot.wallet_id=person->wallet_id;auto stopped_at=read_original_continuation_stop(w,snapshot);if(!stopped_at.ok)return {false,{},std::move(stopped_at.error)};if(stopped_at.value!=RunStop::awaiting_permit)return stopped(std::move(execution),stopped_at.value==RunStop::paused?"PlayerPlanActionPaused":"PlayerPlanActionBlocked",stopped_at.value==RunStop::awaiting_input);}
        else if(bounded_continuation_kind(action->kind)){ContinuationSnapshot snapshot;snapshot.actor_id=actor;snapshot.action_id=action->id;snapshot.action_kind=action->kind;snapshot.action_start_tick=action->start_tick;snapshot.wallet_id=person->wallet_id;auto domain=continuation_domain_stop(w,snapshot);if(domain!=RunStop::awaiting_permit)return stopped(std::move(execution),domain==RunStop::paused?"PlayerPlanActionPaused":"PlayerPlanActionBlocked",domain==RunStop::awaiting_input);}
        else if(action->kind=="service"&&w.personal_recovery_enabled()&&w.personal_recovery.services.find(action->id)){if(!player_plan_recovery_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="tactic_lesson"){if(!player_plan_tactic_lesson_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="deliver_message"){if(!player_plan_personal_message_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="report_offer"){if(!player_plan_personal_report_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}else if(action->kind=="accept_offer"){if(!player_plan_personal_acceptance_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind==task_message_return_collect_command){if(!player_plan_message_return_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind==visit_appointment_meeting_action_kind){auto original=read_player_plan_visit_appointment_meeting_continuation(w,*action);if(!original.ok)return {false,{},std::move(original.error)};if(!original.value)return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind==contact_inquiry_action_kind){auto original=read_player_plan_contact_inquiry_continuation(w,*action);if(!original.ok)return {false,{},std::move(original.error)};if(!original.value)return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind==personal_appointment_action_kind){auto original=read_player_plan_appointment_continuation(w,*action);if(!original.ok)return {false,{},std::move(original.error)};if(!original.value)return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="family_child_care"){if(!player_plan_child_care_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="family_child_care_relief"){if(!player_plan_child_care_relief_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="task_shop_order_deliver"){if(!player_plan_shop_order_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else if(action->kind=="naval_repair"){if(!player_plan_naval_repair_continuation(w,*action))return stopped(std::move(execution),"PlayerPlanActionSourceRequired");}
        else {const auto* payload=w.growth.actions.find(action->id);if(action->kind!="service"||!payload||payload->request.activity!="work")return stopped(std::move(execution),"PlayerPlanUnsupportedAction");}
        if(person->action_id!=action->id)return stopped(std::move(execution),"PlayerPlanActionReferenceMissing");
        AdvanceCommand advance;
        if(state.version==2){auto remaining=read_player_plan_remaining_duration(state);if(!remaining.ok)return {false,{},remaining.error};auto positive=compare_fractions(remaining.value.value,CanonicalFraction{});if(!positive.ok)return {false,{},positive.error};if(positive.value==0)return stopped(std::move(execution),"PlayerPlanTimeBudgetExceeded");
            // 先夹原剩余额和下一整数界，再构造target，最大合法预算不会造成中间tick溢出。
            if(execution.exact_source_moment->half_day_tick==std::numeric_limits<Integer>::max())return fail<PlayerPlanExecution>("WorldTickOverflow");WorldMoment boundary{execution.exact_source_moment->half_day_tick+1,{}};auto until=elapsed_world_duration(*execution.exact_source_moment,boundary);if(!until.ok)return {false,{},until.error};auto less=compare_fractions(remaining.value.value,until.value.value);if(!less.ok)return {false,{},less.error};if(less.value<0){auto end=add_world_duration(*execution.exact_source_moment,remaining.value);if(!end.ok)return {false,{},end.error};advance.requested_to=std::move(end.value);}else advance.requested_to=std::move(boundary);
        }else if(state.current->budget->used_ticks>=state.current->budget->max_additional_ticks)return stopped(std::move(execution),"PlayerPlanTimeBudgetExceeded");
        auto prepared=prepare_command(w,actor,{"advance",std::move(advance)});if(!prepared.ok){if(prepared.error.kind!=ErrorKind::rule)return {false,{},std::move(prepared.error)};return stopped(std::move(execution),prepared.error.original.code);}execution.kind=PlayerPlanExecutionKind::advance;execution.nested=prepared.value->operation;return {true,std::move(execution),{}};
    }
    if(!person->action_id.empty())return stopped(std::move(execution),"ActorBusy");
    auto source=validate_player_plan_source(w,actor,*step);if(!source.ok)return stopped(std::move(execution),source.error.original.code);
    if(std::holds_alternative<PlayerPlanTaskReminder>(step->data)){execution.kind=PlayerPlanExecutionKind::reminder;execution.reason="PlayerPlanTaskNeedsConfirmation";execution.needs_confirmation=true;return {true,std::move(execution),{}};}
    if(at_target(w,actor,step->data)){if(std::holds_alternative<PlayerPlanTravel>(step->data))return stopped(std::move(execution),"PlayerPlanAlreadyAtDestination",true);execution.kind=PlayerPlanExecutionKind::complete;return {true,std::move(execution),{}};}
    if((std::holds_alternative<PlayerPlanWork>(step->data)||std::holds_alternative<PlayerPlanCourse>(step->data))&&!step->authorized)return stopped(std::move(execution),"PlayerPlanStepAuthorizationRequired",true);
    auto command=player_plan_domain_command(step->data);if(!command.ok)return stopped(std::move(execution),command.error.original.code);
    auto prepared=prepare_command(w,actor,command.value);if(!prepared.ok){if(prepared.error.kind!=ErrorKind::rule)return {false,{},std::move(prepared.error)};return stopped(std::move(execution),prepared.error.original.code);}
    if(step->terms){const auto* quote=std::get_if<GrowthQuote>(&prepared.value->quote->data);if(!quote)return stopped(std::move(execution),"PlayerPlanMissingSourceQuote");auto matched=match_player_plan_terms(*step->terms,*quote,person->wallet_id);if(!matched.ok)return stopped(std::move(execution),matched.error.original.code,true);}
    execution.kind=PlayerPlanExecutionKind::start;execution.nested=prepared.value->operation;return {true,std::move(execution),{}};
}
// 原计划绑定复核后借外层捕获执行一次；保留内层具名失败与整候选回滚。
Outcome<ReceiptDetails> apply_player_plan_execution(WorldBundle& w,const PlayerPlanState& state,const StableId& actor,const PlayerPlanExecution& execution,CompleteExactCommandCapture* capture){
    const auto& now=w.common.state().identity;if(now.world_id!=execution.source.world_id||now.execution_epoch!=execution.source.execution_epoch||now.revision!=execution.source.revision||now.tick!=execution.source.tick)return fail<ReceiptDetails>("PlayerPlanStaleExecution");
    if(state.version==2){if(!execution.exact_source_moment||!w.world_timeline)return fail<ReceiptDetails>("PlayerPlanExactSourceRequired");auto current=read_world_moment(w.common.tick(),*w.world_timeline);if(!current.ok)return {false,{},current.error};auto same=compare_world_moments(current.value,*execution.exact_source_moment);if(!same.ok)return {false,{},same.error};if(same.value!=0||!state.current||state.current->approved_scope_revision!=execution.approved_scope_revision)return fail<ReceiptDetails>("PlayerPlanStaleExecution");}else if(execution.exact_source_moment)return fail<ReceiptDetails>("UnexpectedPlayerPlanExactSource");
    const auto* step=next_player_plan_step(state);if(!state.current||state.current->id!=execution.plan_id||state.current->actor_id!=actor||execution.actor_id!=actor||!step||step->id!=execution.step_id||!command_matches(*step,execution))return fail<ReceiptDetails>("PlayerPlanExecutionMismatch");
    if(execution.kind==PlayerPlanExecutionKind::start&&step->started_tick)return fail<ReceiptDetails>("PlayerPlanStartAlreadyRecorded");
    if(execution.nested)return capture?apply_command_with_complete_capture(w,actor,*execution.nested,*capture):apply_command(w,actor,*execution.nested);
    return {true,ReceiptDetails{Done{},{}},{}};
}
Status reconcile_player_plan_commit(WorldBundle& w,PlayerPlanState& state,const StableId& actor,const PlayerPlanExecution* execution,const ReceiptDetails& receipt,const StableId& result_id){
    if(!state.current)return execution?common_failure("PlayerPlanExecutionMismatch"):common_success();
    const auto plan_id=state.current->id;auto* step=next_player_plan_step(state);
    const CommitResult* actual_result=nullptr;for(const auto& row:w.common.state().results.rows())if(row.second&&row.second->result_id==result_id){if(actual_result)return common_failure("PlayerPlanDuplicateResult",result_id);actual_result=row.second.get();}
    if(state.current->actor_id!=actor){
        // 他人的推进仅可消费原私信本次真实完成，停止许可而不续开下一件事。
        if(step&&step->action_id&&!step->completed_tick&&player_plan_personal_message_action(w,*step->action_id)){
            if(!actual_result)return common_failure("PlayerPlanPersonalMessageResultRequired",result_id);
            auto source=player_plan_personal_message_completion(w,state.current->actor_id,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
            if(source.value){const bool success=source.value->status=="delivered";auto status=player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);if(!status.ok)return status;player_plan_pause(state,"PlayerPlanManualCommand");}
        }if(step&&step->action_id&&!step->completed_tick&&player_plan_personal_report_action(w,*step->action_id)){
            if(!actual_result)return common_failure("PlayerPlanPersonalReportResultRequired",result_id);
            auto source=player_plan_personal_report_completion(w,state.current->actor_id,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
            if(source.value){const bool success=source.value->status=="reported";auto status=player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);if(!status.ok)return status;player_plan_pause(state,"PlayerPlanManualCommand");}
        }if(step&&step->action_id&&!step->completed_tick&&player_plan_personal_acceptance_action(w,*step->action_id)){
            if(!actual_result)return common_failure("PlayerPlanPersonalAcceptanceResultRequired",result_id);
            auto source=player_plan_personal_acceptance_completion(w,state.current->actor_id,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
            if(source.value){const bool success=source.value->status=="accepted";auto status=player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);if(!status.ok)return status;player_plan_pause(state,"PlayerPlanManualCommand");}
        }
        // 他人的真实推进也可完成已经接受的课程；只登记本结果的精确教学来源，停止自动后续。
        if(step&&step->action_id&&!step->completed_tick&&player_plan_tactic_lesson_action(w,*step->action_id)){
            if(!actual_result)return common_failure("PlayerPlanTacticLessonResultRequired",result_id);
            auto source=player_plan_tactic_lesson_completion(w,state.current->actor_id,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
            if(source.value){auto status=player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);if(!status.ok)return status;player_plan_pause(state,"PlayerPlanManualCommand");}
        }
        return common_success();
    }
    if(execution){
        // 同一真实回执再次核对是幂等读取；其他回执不能覆盖已完成边界。
        for(const auto& recorded:state.current->steps)if(recorded.id==execution->step_id&&recorded.completion_result_id==std::optional<StableId>(result_id)&&execution->actor_id==actor&&execution->plan_id==plan_id)return common_success();
        if(execution->actor_id!=actor||execution->plan_id!=plan_id||!step||execution->step_id!=step->id)return common_failure("PlayerPlanExecutionMismatch");
        if(execution->kind==PlayerPlanExecutionKind::stop||execution->kind==PlayerPlanExecutionKind::reminder)return player_plan_stop(state,step->id,execution->reason,execution->needs_confirmation);
        if(execution->kind==PlayerPlanExecutionKind::complete)return player_plan_record_completed(state,plan_id,step->id,std::nullopt,result_id,w.common.tick(),true);
        if(execution->kind==PlayerPlanExecutionKind::start){
            if(const auto* action=std::get_if<ActionStarted>(&receipt.data)){const auto* real=w.common.action(action->action_id);if(!real||real->actor_id!=actor||!execution->nested||real->kind!=execution->nested->command.kind)return common_failure("PlayerPlanStartedActionMismatch");return player_plan_record_started(state,plan_id,step->id,action->action_id,result_id,w.common.tick());}
            if(!std::holds_alternative<PlayerPlanMove>(step->data)||!std::holds_alternative<Done>(receipt.data)||!at_target(w,actor,step->data))return common_failure("PlayerPlanStartReceiptMissing");
            return player_plan_record_completed(state,plan_id,step->id,std::nullopt,result_id,w.common.tick(),true);
        }
        if(execution->kind==PlayerPlanExecutionKind::advance&&!step->action_id){if(!execution->action_id||!std::holds_alternative<PlayerPlanContinue>(step->data))return common_failure("PlayerPlanActionReferenceMissing");auto recorded=player_plan_record_started(state,plan_id,step->id,execution->action_id,result_id,w.common.tick());if(!recorded.ok)return recorded;}
    }else {
        player_plan_pause(state,"PlayerPlanManualCommand");
        // 提醒只观察本人本次真实报告结算；买货、到场、部分交付和旧终态均不算办理。
        if(step&&!step->started_tick)if(const auto* reminder=std::get_if<PlayerPlanTaskReminder>(&step->data)){
            const auto* task=w.tasks.tasks.find(reminder->task_id);const auto* advanced=std::get_if<AdvanceDetails>(&receipt.data);
            const bool reported=task&&task->assignee==actor&&task->reported_tick==std::optional<Integer>(w.common.tick())&&!task->settlement_id.empty()&&(task->status=="success"||task->status=="failed"||task->status=="cancelled");
            if(reported&&advanced&&step->source.kind==PlayerPlanSourceKind::accepted_task&&step->source.source_id==reminder->task_id)for(const auto& row:advanced->completed_actions.rows()){
                const auto& completed=row.second;if(!completed.task||completed.task->task_id!=std::optional<StableId>(reminder->task_id)||!completion_status(completed).first)continue;
                auto started=player_plan_record_started(state,plan_id,step->id,row.first,result_id,w.common.tick());if(!started.ok)return started;
                const auto status=completion_status(completed);auto recorded=player_plan_record_completed(state,plan_id,step->id,row.first,result_id,w.common.tick(),status.first,status.second);if(!recorded.ok)return recorded;
                // 完成的是办理提醒，任务成败仍读原TaskRecord；人工操作后许可保持停驻。
                player_plan_pause(state,"PlayerPlanManualCommand");return common_success();
            }
        }
        if(step&&step->action_id&&!step->completed_tick){
            // 原本人手动隐藏推进也可能完成交信；只消费本次原结果，之后保持人工暂停。
            if(player_plan_personal_message_action(w,*step->action_id)){
                if(!actual_result)return common_failure("PlayerPlanPersonalMessageResultRequired",result_id);
                auto source=player_plan_personal_message_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(source.value){const bool success=source.value->status=="delivered";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);}
            }if(player_plan_personal_report_action(w,*step->action_id)){
                if(!actual_result)return common_failure("PlayerPlanPersonalReportResultRequired",result_id);
                auto source=player_plan_personal_report_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(source.value){const bool success=source.value->status=="reported";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);}
            }if(player_plan_personal_acceptance_action(w,*step->action_id)){
                if(!actual_result)return common_failure("PlayerPlanPersonalAcceptanceResultRequired",result_id);
                auto source=player_plan_personal_acceptance_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(source.value){const bool success=source.value->status=="accepted";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);}
            }
            if(player_plan_tactic_lesson_action(w,*step->action_id)){
                if(!actual_result)return common_failure("PlayerPlanTacticLessonResultRequired",result_id);
                auto source=player_plan_tactic_lesson_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(source.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);
            }
            // 提交尾已绑定所有本次新增closure；零时真实死亡和取消也按这个缓存实例结束原疗程。
            if(w.personal_recovery_enabled()&&w.personal_recovery.services.find(*step->action_id)){
                if(!actual_result)return common_failure("PlayerPlanRecoveryResultRequired",result_id);
                auto source=player_plan_recovery_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(source.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);
            }
            if(const auto* effect=std::get_if<GrowthEffect>(&receipt.data))if(effect->action_id==*step->action_id){
                const auto* fact=w.growth.facts.find(*step->action_id);
                if(effect->kind=="study_cancel"){const auto* study=fact?std::get_if<StudyFact>(fact):nullptr;if(!study||study->phase!="completed"||study->success||study->code!="StudyCancelled"||study->completed_tick!=w.common.tick())return common_failure("PlayerPlanCancellationSourceMissing");return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),false,"StudyCancelled");}
                if(effect->kind=="rest_cancel"){const auto* rest=fact?std::get_if<RestFact>(fact):nullptr;if(!rest||rest->phase!="completed"||rest->stop_reason!="cancelled"||rest->completed_tick!=w.common.tick())return common_failure("PlayerPlanCancellationSourceMissing");return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),false,"RestCancelled");}
            }
            if(const auto* cancelled=std::get_if<NavalRepairReceipt>(&receipt.data))if(cancelled->action_id==*step->action_id&&cancelled->completed&&!cancelled->replayed){
                auto source=player_plan_naval_repair_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!source.ok)return {false,{},source.error};if(!source.value)return common_failure("PlayerPlanNavalRepairMismatch",*step->action_id);return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->reason);
            }
            if(std::holds_alternative<FamilyEffect>(receipt.data)&&w.family_birth_enabled()&&w.family_birth.care_starts.find(*step->action_id)){
                auto source=player_plan_child_care_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!source.ok)return {false,{},source.error};if(source.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);
            }
            if(std::holds_alternative<FamilyEffect>(receipt.data)&&w.family_care_relief_enabled()&&w.child_care_relief.starts.find(*step->action_id)){
                auto source=player_plan_child_care_relief_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!source.ok)return {false,{},source.error};if(source.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);
            }
            const bool return_action=w.catalog->task_message_returns&&w.tasks.message_returns&&std::any_of(w.tasks.message_returns->starts.rows().begin(),w.tasks.message_returns->starts.rows().end(),[&](const auto& row){return row.second.action_id==*step->action_id;});
            const bool visit_meeting_action=w.catalog->visit_appointment_invitations&&w.interaction.visit_appointment_meetings&&std::any_of(w.interaction.visit_appointment_meetings->started.rows().begin(),w.interaction.visit_appointment_meetings->started.rows().end(),[&](const auto& row){return row.second.action_id==*step->action_id;});
            const bool inquiry_action=w.catalog->contact_inquiries&&w.interaction.contact_inquiries&&std::any_of(w.interaction.contact_inquiries->started.rows().begin(),w.interaction.contact_inquiries->started.rows().end(),[&](const auto& row){return row.second.action_id==*step->action_id;});
            const bool appointment_action=w.catalog->personal_appointments&&w.interaction.personal_appointments&&std::any_of(w.interaction.personal_appointments->started.rows().begin(),w.interaction.personal_appointments->started.rows().end(),[&](const auto& row){return row.second.action_id==*step->action_id;});
            const bool care_action=(w.family_birth_enabled()&&w.family_birth.care_starts.find(*step->action_id))||(w.family_care_relief_enabled()&&w.child_care_relief.starts.find(*step->action_id));
            bool repair_action=false;if(w.naval_repairs_enabled()){auto source=query_naval_repair_action(w.common,w.naval,*w.catalog->naval,*step->action_id);if(!source.ok)return {false,{},source.error};repair_action=source.value.has_value();}
            bool shop_action=false;if(w.task_shop_orders_enabled()){if(!w.tasks.shop_orders)return common_failure("TaskShopOrderProfileRequired");auto source=query_task_shop_order_action(w.common,w.tasks,*w.tasks.shop_orders,actor,*step->action_id);if(!source.ok)return {false,{},source.error};shop_action=source.value.has_value();}
            // 原手动改道/救助终止这一意向；原行动可以继续，绝不把新目标冒充原到达。
            const CommitResult* current=nullptr;for(const auto& row:w.common.state().results.rows())if(row.second&&row.second->result_id==result_id)current=row.second.get();
            const auto* action=w.common.action(*step->action_id);const bool travel=std::holds_alternative<PlayerPlanTravel>(step->data)||(std::holds_alternative<PlayerPlanContinue>(step->data)&&!player_plan_tactic_lesson_action(w,*step->action_id)&&!repair_action&&!care_action&&!shop_action&&!appointment_action&&!inquiry_action&&!visit_meeting_action&&!return_action&&!player_plan_personal_message_action(w,*step->action_id)&&!player_plan_personal_report_action(w,*step->action_id)&&!player_plan_personal_acceptance_action(w,*step->action_id)&&((action&&action->kind=="travel")||w.travel.rescues.find(*step->action_id)||(!action&&!w.growth.facts.find(*step->action_id)&&!w.growth.life_activities.find(*step->action_id))));
            if(travel&&current&&(current->kind=="rescue"||current->kind=="return"||current->kind=="reroute"))return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),false,"PlayerPlanTravelChanged");
            // 舟战续行/接管为零时业务回执，只在原永久事实确实终止本人原旅程时记失败。
            if(travel&&std::holds_alternative<NavalBattleCommandReceipt>(receipt.data)&&!action){
                auto source=player_plan_naval_termination_reason(w,actor,*step->action_id,receipt,w.common.tick());
                if(!source.ok)return {false,{},source.error};
                if(source.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),false,*source.value);
            }
        }
    }
    // 只消费本次真实完成集合中的已绑定ID；删除行动头或同类新行动都不能推断成功。
    const auto* advance=std::get_if<AdvanceDetails>(&receipt.data);if(!advance||!step||!step->action_id||step->completed_tick)return common_success();
    const auto* completed=advance->completed_actions.find(*step->action_id);if(!completed)return common_success();
    if(player_plan_tactic_lesson_action(w,*step->action_id)){
        if(!actual_result)return common_failure("PlayerPlanTacticLessonResultRequired",result_id);
        auto source=player_plan_tactic_lesson_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};
        if(!source.value)return common_failure("PlayerPlanTacticLessonMismatch",*step->action_id);
        return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),source.value->success,source.value->code);
    }
    if(player_plan_personal_message_action(w,*step->action_id)){
        if(!actual_result)return common_failure("PlayerPlanPersonalMessageResultRequired",result_id);
        auto source=player_plan_personal_message_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};if(!source.value)return common_failure("PlayerPlanPersonalMessageMismatch",*step->action_id);
        const bool success=source.value->status=="delivered";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);
    }if(player_plan_personal_report_action(w,*step->action_id)){
        if(!actual_result)return common_failure("PlayerPlanPersonalReportResultRequired",result_id);
        auto source=player_plan_personal_report_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};if(!source.value)return common_failure("PlayerPlanPersonalReportMismatch",*step->action_id);
        const bool success=source.value->status=="reported";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);
    }if(player_plan_personal_acceptance_action(w,*step->action_id)){
        if(!actual_result)return common_failure("PlayerPlanPersonalAcceptanceResultRequired",result_id);
        auto source=player_plan_personal_acceptance_completion(w,actor,*step->action_id,*actual_result);if(!source.ok)return {false,{},source.error};if(!source.value)return common_failure("PlayerPlanPersonalAcceptanceMismatch",*step->action_id);
        const bool success=source.value->status=="accepted";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:source.value->reason);
    }
    auto message_return=player_plan_message_return_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id);if(!message_return.ok)return {false,{},message_return.error};if(message_return.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),message_return.value->status=="collected",message_return.value->status=="collected"?StableId{}:message_return.value->reason);
    auto visit_meeting=player_plan_visit_appointment_meeting_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id);if(!visit_meeting.ok)return {false,{},visit_meeting.error};if(visit_meeting.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),visit_meeting.value->success,visit_meeting.value->success?StableId{"VisitAppointmentMeetingFulfilled"}:visit_meeting.value->reason);
    auto inquiry=player_plan_contact_inquiry_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id);if(!inquiry.ok)return {false,{},inquiry.error};if(inquiry.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),inquiry.value->success,inquiry.value->success?StableId{"ContactInquiryAnswered"}:inquiry.value->reason);
    auto appointment=player_plan_appointment_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id);if(!appointment.ok)return {false,{},appointment.error};if(appointment.value)return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),appointment.value->success,appointment.value->success?StableId{"PersonalAppointmentFulfilled"}:appointment.value->reason);
    if(w.personal_recovery_enabled()&&w.personal_recovery.services.find(*step->action_id)){
        if(!actual_result)return common_failure("PlayerPlanRecoveryResultRequired",result_id);
        auto recovery=player_plan_recovery_completion(w,actor,*step->action_id,*actual_result);if(!recovery.ok)return {false,{},recovery.error};if(!recovery.value)return common_failure("PlayerPlanRecoveryMismatch",*step->action_id);return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),recovery.value->success,recovery.value->code);
    }
    auto shop=player_plan_shop_order_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!shop.ok)return {false,{},shop.error};
    if(shop.value){const bool success=shop.value->status=="delivered";return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),success,success?StableId{}:shop.value->reason);}
    auto repair=player_plan_naval_repair_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!repair.ok)return {false,{},repair.error};
    auto care=player_plan_child_care_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!care.ok)return {false,{},care.error};
    auto relief_care=player_plan_child_care_relief_completion(w,actor,*step->action_id,receipt,w.common.tick());if(!relief_care.ok)return {false,{},relief_care.error};
    auto navigation=player_plan_naval_navigation_termination_reason(w,actor,*step->action_id,receipt,w.common.tick());if(!navigation.ok)return {false,{},navigation.error};
    const auto status=completion_status(*completed);return player_plan_record_completed(state,plan_id,step->id,step->action_id,result_id,w.common.tick(),status.first,status.second);
}
} // 中央继续负责毛支出观察、permit撤销、完整世界验证与原子采纳。
