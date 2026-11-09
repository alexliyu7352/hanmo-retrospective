#include "world_checkpoint_write_scope.hpp"
// 玩家有限生活安排复用原submit和单worker；每份许可只采纳一个原命令或半日。
#include "world_impl.hpp"
#include "player_plan_integration.hpp"
#include "resident/domains/player_plan/plan_lifecycle.hpp"
#include "resident/domains/player_plan/plan_time2.hpp"
#include "exact_run_read.hpp"
#include "exact_time_world.hpp"
#include "continuation_snapshot.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 失败保持世界和原请求序号，控制器只记录此次许可的停车原因。
template<class T> Outcome<T> failed(const StableId& code){return {false,{},common_failure(code).error};}
bool same(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 原领域停车理由保留，不把需要本人决定的步骤判作预算不足。
RunStop plan_stop(const StableId& reason){if(reason=="PlayerPlanCompleted")return RunStop::completed;if(reason=="PlayerPlanTimeBudgetExceeded")return RunStop::budget_exhausted;if(reason=="PlayerPlanSpendBudgetExceeded")return RunStop::money_budget_exhausted;if(reason=="PlayerPlanActionPaused"||reason=="StudyPaused"||reason=="TravelPaused")return RunStop::paused;if(reason=="Stranded"||reason=="InsufficientFunds")return RunStop::stranded;if(reason=="EncounterPending"||reason=="BattlePending"||reason=="SiegePending"||reason=="StudyChallengePending"||reason=="PlayerPlanManualInputRequired"||reason=="PlayerPlanInteractionPending"||reason=="PlayerPlanMembershipDecisionRequired"||reason=="PlayerPlanTaskNeedsConfirmation"||reason=="PlayerPlanStepAuthorizationRequired")return RunStop::awaiting_input;return RunStop::blocked;}
// 累计数始终来自同候选已保存预算，控制器没有第二套消费账。
Status project_budget(RunProgress& progress,const WorldBundle& world){const auto& state=world.player_plan;if(!state.current||!state.current->budget)return common_failure("PlayerPlanBudgetRequired");const auto& budget=*state.current->budget;
 if(state.version==2){if(!world.catalog->exact_time||!world.world_timeline)return common_failure("PlayerPlanTime2ProfileRequired");auto remain=read_player_plan_remaining_duration(state);if(!remain.ok)return {false,{},remain.error};auto remaining_ticks=ceil_half_day_duration(remain.value);if(!remaining_ticks.ok)return {false,{},remaining_ticks.error};auto consumed=duration_to_scale_units(*budget.exact_used_duration,world.world_timeline->scale_ref.units_per_half_day);if(!consumed.ok)return {false,{},consumed.error};auto remaining=duration_to_scale_units(remain.value,world.world_timeline->scale_ref.units_per_half_day);if(!remaining.ok)return {false,{},remaining.error};auto moment=read_world_moment(world.common.tick(),*world.world_timeline);if(!moment.ok)return {false,{},moment.error};
  if(!progress.exact_spent_units)progress.exact_spent_units.emplace();if(!progress.exact_remaining_units)progress.exact_remaining_units.emplace();if(!progress.exact_source_moment)progress.exact_source_moment.emplace();progress.exact_spent_units->value.swap(consumed.value.value);progress.exact_remaining_units->value.swap(remaining.value.value);progress.exact_source_moment->sub_half_day.swap(moment.value.sub_half_day);progress.exact_source_moment->half_day_tick=moment.value.half_day_tick;progress.remaining_half_days=remaining_ticks.value;
 }else {progress.exact_spent_units.reset();progress.exact_remaining_units.reset();progress.exact_source_moment.reset();progress.remaining_half_days=budget.max_additional_ticks-budget.used_ticks;}
 progress.max_half_days=budget.max_additional_ticks;progress.max_gross_outflow=budget.max_gross_spend;progress.committed_half_days=budget.used_ticks;progress.gross_outflow=budget.gross_spend;progress.remaining_gross_outflow=budget.max_gross_spend-budget.gross_spend;progress.identity=world.common.state().identity;return common_success();
}
// 同一PlanTarget一直绑定真实批准版本；原执行进度不改变该版本。
bool same_scope(const BeginRun& request,const PlayerPlanState& state){if(state.version==1)return !request.target;if(!state.current||!request.target)return false;const auto* target=std::get_if<PlanRunDriveTarget>(&*request.target);return target&&target->plan_id==state.current->id&&target->approved_scope_revision==state.current->approved_scope_revision&&target->approved_scope_revision>0;}

}
// begin只接受本人已明确保存的同一计划额度，保持世界停在原稳定边界。
Outcome<RunProgress> ResidentWorld::Impl::begin_plan_run(const BeginRun& request){
    WorldCheckpointWriteScope checkpoint_write(active_checkpoint_capture.lock());
    if(!active_storage_restore.expired())return failed<RunProgress>("StorageRestoreBusy");
    if(!world||!world->player_plan_enabled())return failed<RunProgress>("PlayerPlanProfileRequired");
    if(request.resume_saved_scope||!request.action_id.empty())return failed<RunProgress>("InvalidPlanRunRequest");
    const auto& state=world->player_plan;if(!state.current||state.current->id!=request.plan_id||state.current->actor_id!=request.actor_id||!state.current->budget)return failed<RunProgress>("PlayerPlanBudgetRequired");
    if(!same_scope(request,state))return failed<RunProgress>("PlayerPlanScopeMismatch");
    const auto& budget=*state.current->budget;Integer remaining_ticks=budget.max_additional_ticks-budget.used_ticks;if(state.version==2){auto remaining=read_player_plan_remaining_duration(state);if(!remaining.ok)return {false,{},remaining.error};auto projected=ceil_half_day_duration(remaining.value);if(!projected.ok)return {false,{},projected.error};remaining_ticks=projected.value;}
    if(request.max_half_days!=remaining_ticks||request.max_gross_outflow!=budget.max_gross_spend-budget.gross_spend)return failed<RunProgress>("PlayerPlanBudgetMismatch");
    const auto* actor=world->common.actor(request.actor_id);const auto* wallet=actor?world->common.wallet(actor->wallet_id):nullptr;if(!wallet||wallet->owner.kind!=Owner::Kind::actor||wallet->owner.id!=request.actor_id)return failed<RunProgress>("RunPersonalWalletRequired");
    ActivePlanRun active;active.request=request;auto& progress=active.progress;progress.identity=world->common.state().identity;progress.run_id=request.run_id;progress.actor_id=request.actor_id;progress.plan_id=request.plan_id;progress.action_kind="player_plan";progress.wallet_id=actor->wallet_id;auto projected=project_budget(progress,*world);if(!projected.ok)return {false,{},projected.error};
    if(!next_player_plan_step(state))progress.stop=RunStop::completed;else if(!state.current->stop_reason.empty())progress.stop=plan_stop(state.current->stop_reason);
    used_run_ids.push_back(request.run_id);run.reset();restored_continuation.reset();plan_run=std::move(active);return {true,plan_run->progress,{}};
}
// 重复permit只读旧结果；每个新permit最多一次原submit，零时长启动也计真实毛支出。
Outcome<RunProgress> ResidentWorld::Impl::resume_plan_run(ResidentWorld& owner,const ResumeRun& request){
    WorldCheckpointWriteScope checkpoint_write(active_checkpoint_capture.lock());
    if(!active_storage_restore.expired())return failed<RunProgress>("StorageRestoreBusy");
    auto& active=*plan_run;auto& progress=active.progress;
    if(request.permit_seq==progress.permit_seq&&request.permit_seq>0)return {true,progress,{}};
    if(progress.permit_seq==std::numeric_limits<Integer>::max()||request.permit_seq!=progress.permit_seq+1||request.grant_half_days!=1)return failed<RunProgress>("InvalidRunPermit");
    if(!same(world->common.state().identity,request.expected))return failed<RunProgress>("StalePreview");
    if(progress.stop!=RunStop::awaiting_permit)return {true,progress,{}};
    progress.permit_seq=request.permit_seq;progress.error.reset();
    if(!same(world->common.state().identity,progress.identity)){progress.stop=RunStop::blocked;return {true,progress,{}};}
    const auto& state=world->player_plan;if(!state.current||state.current->id!=active.request.plan_id||!state.current->budget||!same_scope(active.request,state)){progress.stop=RunStop::blocked;return {true,progress,{}};}
    if(progress.remaining_half_days==0){progress.stop=RunStop::budget_exhausted;return {true,progress,{}};}
    auto execution=prepare_player_plan_execution(*world,active.request.actor_id,state);if(!execution.ok){progress.error=execution.error;progress.stop=execution.error.kind==ErrorKind::rule?plan_stop(execution.error.original.code):RunStop::failed;return {true,progress,{}};}
    if(world->player_plan.version==2&&execution.value.kind==PlayerPlanExecutionKind::advance&&world->world_timeline->boundary_state==PeriodBoundaryState::ready){
        // Opening只发生在已有真实正推进意图之后；本次开期实际毛支出在同原Plan预算中计一次，时间为零。
        run_spend_guard=RunSpendGuard{progress.wallet_id,progress.remaining_gross_outflow,0};Status opened;try{opened=open_exact_period();}catch(...){run_spend_guard.reset();throw;}run_spend_guard.reset();
        if(!opened.ok){progress.error=opened.error;progress.stop=RunStop::failed;return {true,progress,{}};}auto refreshed=project_budget(progress,*world);if(!refreshed.ok)return {false,{},refreshed.error};
        if(!same_scope(active.request,world->player_plan)){progress.stop=RunStop::blocked;return {true,progress,{}};}execution=prepare_player_plan_execution(*world,active.request.actor_id,world->player_plan);if(!execution.ok){progress.error=execution.error;progress.stop=execution.error.kind==ErrorKind::rule?plan_stop(execution.error.original.code):RunStop::failed;return {true,progress,{}};}
    }
    const auto* sequence=world->common.state().request_sequences.find(active.request.actor_id);if(!sequence||*sequence>=2147483647){progress.stop=RunStop::failed;progress.error=common_failure("InvalidRequestSequence").error;return {true,progress,{}};}
    CommandEnvelope command;command.world_id=progress.identity.world_id;command.execution_epoch=progress.identity.execution_epoch;command.actor_id=active.request.actor_id;command.request_seq=*sequence+1;command.expected_revision=progress.identity.revision;command.payload=std::make_shared<const CommandPayload>(CommandPayload{"player_plan_step",PlayerPlanCommand{"player_plan_step",PlayerPlanInternalStep{execution.value.plan_id,execution.value.step_id}}});
    const auto before_revision=world->common.revision();auto result=owner.submit(command);if(!result.ok){progress.error=result.error;progress.stop=result.error.kind==ErrorKind::rule?plan_stop(result.error.original.code):RunStop::failed;return {true,progress,{}};}
    if(!world->player_plan.current||world->player_plan.current->id!=active.request.plan_id||!world->player_plan.current->budget){progress.stop=RunStop::blocked;return {true,progress,{}};}
    if(world->common.revision()<=before_revision){progress.stop=RunStop::failed;progress.error=common_failure("RunNoProgress",{},ErrorKind::internal).error;return {true,progress,{}};}
    const auto& current=*world->player_plan.current;auto projected=project_budget(progress,*world);if(!projected.ok)return {false,{},projected.error};
    if(!next_player_plan_step(world->player_plan))progress.stop=RunStop::completed;
    else if(!current.stop_reason.empty())progress.stop=plan_stop(current.stop_reason);
    else if(progress.remaining_half_days==0)progress.stop=RunStop::budget_exhausted;
    return {true,progress,{}};
}
// 查询不生成命令，人工操作或换世界后不把旧许可借给改动后的安排。
Outcome<RunProgress> ResidentWorld::Impl::poll_plan_run(const RunRef& reference){
    if(!plan_run||plan_run->progress.run_id!=reference.run_id)return failed<RunProgress>("UnknownRun");auto& progress=plan_run->progress;
    if(progress.stop==RunStop::awaiting_permit&&(!same(world->common.state().identity,progress.identity)||!same_scope(plan_run->request,world->player_plan)))progress.stop=RunStop::blocked;
    return {true,progress,{}};
}
// /2纯gate复用同一个已绑定run与原领域prepare；缺adapter与技术故障保原错误，不伪业务停车。
Outcome<RunDriveGate> ResidentWorld::read_run_drive_gate(const RunRef& reference) const {
 try {
  if(!impl_->world)return failed<RunDriveGate>("NoWorld");const auto& world=*impl_->world;const auto identity=world.common.state().identity;
  if(reference.world_id!=identity.world_id||reference.execution_epoch!=identity.execution_epoch)return failed<RunDriveGate>("StaleWorld");
  RunDriveGate gate;gate.source_identity=identity;if(world.catalog->exact_time){if(!world.world_timeline)return failed<RunDriveGate>("ExactTimelinePairRequired");auto now=read_world_moment(world.common.tick(),*world.world_timeline);if(!now.ok)return {false,{},now.error};gate.exact_source_moment=std::move(now.value);}
  if(impl_->plan_run&&impl_->plan_run->progress.run_id==reference.run_id){const auto& active=*impl_->plan_run;const auto& state=world.player_plan;
   // 新PlanTarget只对明确/2批准版本发行，旧/1仍沿原旧入口与字节。
   if(state.version!=2||!active.request.target||!std::holds_alternative<PlanRunDriveTarget>(*active.request.target))return failed<RunDriveGate>("PlanRunDrive2Required");gate.target=*active.request.target;gate.budget_projection=active.progress;gate.stop=active.progress.stop;
   if(!same(identity,active.progress.identity)||!same_scope(active.request,state)){gate.stop=RunStop::blocked;gate.reason="PlayerPlanScopeChanged";return {true,std::move(gate),{}};}
   auto projected=project_budget(gate.budget_projection,world);if(!projected.ok)return {false,{},projected.error};
   if(gate.stop!=RunStop::awaiting_permit)return {true,std::move(gate),{}};
   auto execution=prepare_player_plan_execution(world,active.request.actor_id,state);if(!execution.ok){if(execution.error.kind!=ErrorKind::rule)return {false,{},execution.error};gate.reason=execution.error.original.code;gate.stop=plan_stop(gate.reason);return {true,std::move(gate),{}};}
   gate.step_ref=execution.value.step_id;gate.bound_action_ref=execution.value.action_id;
   if(execution.value.kind==PlayerPlanExecutionKind::advance)gate.next=RunDriveNext::world_half_day;
   else if(execution.value.kind==PlayerPlanExecutionKind::start||execution.value.kind==PlayerPlanExecutionKind::complete)gate.next=RunDriveNext::original_command;
   else {gate.reason=execution.value.reason;gate.stop=plan_stop(gate.reason);}
   return {true,std::move(gate),{}};
  }
  if(!impl_->run||impl_->run->progress.run_id!=reference.run_id)return failed<RunDriveGate>("UnknownRun");const auto& active=*impl_->run;gate.target=ActionRunDriveTarget{active.request.action_id,active.action_kind};gate.bound_action_ref=active.request.action_id;gate.budget_projection=active.progress;gate.stop=active.progress.stop;
  if(!same(identity,active.progress.identity)){gate.stop=RunStop::blocked;gate.reason="RunSourceChanged";return {true,std::move(gate),{}};}
  if(gate.stop!=RunStop::awaiting_permit)return {true,std::move(gate),{}};
  ContinuationSnapshot snapshot;snapshot.actor_id=active.request.actor_id;snapshot.action_id=active.request.action_id;snapshot.action_kind=active.action_kind;snapshot.action_start_tick=active.action_start_tick;snapshot.wallet_id=active.wallet_id;
  if(world.catalog->exact_time)snapshot.continuation_version=2;
  auto stopped=read_original_continuation_stop(world,snapshot);if(!stopped.ok)return {false,{},std::move(stopped.error)};gate.stop=stopped.value;
  if(gate.stop==RunStop::awaiting_permit){if(active.progress.remaining_half_days<=0)gate.stop=RunStop::budget_exhausted;else if(active.request.max_gross_outflow>0&&active.progress.remaining_gross_outflow==0)gate.stop=RunStop::money_budget_exhausted;else gate.next=RunDriveNext::world_half_day;}
  return {true,std::move(gate),{}};
 }catch(const std::bad_alloc&){return {false,{},common_failure("RunDriveReadResourceExhausted","run_drive",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("RunDriveReadResourceExhausted","run_drive",ErrorKind::internal).error};}
}
} // cold只保存PlayerPlanState；run ID、permit序号和活跃许可从不恢复。
