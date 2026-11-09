#include "world_checkpoint_write_scope.hpp"
#include "world_stage_observation.hpp"
#include "exact_time_world.hpp"
#include "exact_run_read.hpp"
#include "resident/domains/player_plan/plan_time2.hpp"
// 有限许可只驱动同一个preview/submit入口；停止不暂停行动，不撤回已采纳半日。
#include "world_impl.hpp"
#include "continuation_snapshot.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 控制器错误沿原规则错误合同返回，不制造部分成功回执。
template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
bool source_matches(const WorldIdentity& now,const WorldIdentity& requested){return now.world_id==requested.world_id&&now.execution_epoch==requested.execution_epoch;}
bool full_matches(const WorldIdentity& now,const WorldIdentity& requested){return source_matches(now,requested)&&now.revision==requested.revision&&now.tick==requested.tick;}
// 许可预算沿原Integer条款，但不是业务固定H；精确换算不得套用固定action的checked H*N门。
Outcome<WorkUnits> run_budget_units(Integer half_days,Integer scale){auto value=normalize_fraction(std::to_string(half_days),"1");if(!value.ok)return {false,{},value.error};return duration_to_scale_units(HalfDayDuration{value.value},scale);}
// 任何停点都撤回后续许可；再次明确继续必须使用新的run ID。
bool terminal(RunStop stop){return stop!=RunStop::awaiting_permit;}
// 临时支出观察仅覆盖当前submit，所有返回和异常离开路径都会清除。
template<class T> struct ResetOnExit { std::optional<T>& value;~ResetOnExit(){value.reset();} };
// 快照投影冻结的是原许可条款，世界身份取当前稳定边界。
template<class T> ContinuationSnapshot snapshot_for(const WorldBundle& world,const T& run){ContinuationSnapshot s;s.source_identity=world.common.state().identity;s.actor_id=StableId(run.request.actor_id);s.action_id=StableId(run.request.action_id);s.action_kind=StableId(run.action_kind);s.wallet_id=StableId(run.wallet_id);s.action_start_tick=run.action_start_tick;s.started_tick=run.started_tick;s.max_half_days=run.request.max_half_days;s.max_gross_outflow=run.request.max_gross_outflow;s.committed_half_days=run.progress.committed_half_days;s.gross_outflow=run.progress.gross_outflow;s.stop=run.progress.stop;s.exact_started_at=run.exact_started_at;s.exact_committed_duration=run.exact_committed_duration;if(world.catalog->exact_time){s.continuation_version=2;s.exact_source_moment=WorldMoment{world.common.tick(),world.world_timeline->sub_half_day};if(exact_complete_selected(*world.catalog->exact_time))s.action_started_source=run.exact_action_started_source;else for(const auto& row:world.growth.fixed_service_boundaries.rows())if(row.second.action_ref==s.action_id&&row.second.kind==LifeFixedBoundaryKind::started){s.action_started_source=ContinuationStartedSource{"growth","growth_fixed_service_boundary",row.first,1};break;}}return s;}
// 完整包停点只从原owner读；旧profile继续原有限分派，技术错误不能伪装成许可结束。
Outcome<RunStop> read_run_stop(const WorldBundle& world,const ContinuationSnapshot& snapshot){
    // 原共用输入/可用性门与专业owner同一个Outcome入口，错误不经旧wrapper降成blocked。
    return read_original_continuation_stop(world,snapshot);
}
// 剩余额度由冻结预算与成功回执得出，收入与预留都不会扩大它。
template<class T> void remaining(T& run){run.progress.remaining_half_days=run.request.max_half_days-run.progress.committed_half_days;run.progress.remaining_gross_outflow=run.request.max_gross_outflow-run.progress.gross_outflow;run.progress.actor_id=run.request.actor_id;run.progress.action_id=run.request.action_id;run.progress.action_kind=run.action_kind;run.progress.wallet_id=run.wallet_id;run.progress.max_half_days=run.request.max_half_days;run.progress.max_gross_outflow=run.request.max_gross_outflow;}
// 领域要求本人输入、真实资源不足及许可不足保留各自停止含义。
RunStop failed_stop(const RuleError& error){const auto& code=error.original.code;if(code=="RunMoneyBudgetExceeded")return RunStop::money_budget_exhausted;if(code=="InsufficientFunds"||code=="Stranded")return RunStop::stranded;if(code=="EncounterPending"||code=="BattlePending"||code=="SiegePending"||code=="StudyChallengePending")return RunStop::awaiting_input;if(code=="StudyPaused"||code=="TravelPaused")return RunStop::paused;return error.kind==ErrorKind::rule?RunStop::blocked:RunStop::failed;}
}
// 内部Opening只在本唯一WorldBundle提交；失败原Ready世界保持，成功不制造玩家seq/结果。
Status ResidentWorld::Impl::open_exact_period(){
    world_stage_observation::Scope opening_observation(world_stage_observation::Stage::opening_call);
    WorldCheckpointWriteScope checkpoint_write(active_checkpoint_capture.lock());
 if(!active_storage_restore.expired())return common_failure("StorageRestoreBusy");
 if(!world||!world->catalog->exact_time)return common_failure("ExactTimelineProfileRequired");if(world->common.revision()==std::numeric_limits<Integer>::max())return common_failure("WorldRevisionOverflow");
 world_stage_observation::Scope copy_observation(world_stage_observation::Stage::opening_candidate_copy);
 auto candidate=checkpoint_make_shared<WorldBundle>(*world);copy_observation.stop();if(run_spend_guard){auto observed=candidate->common.begin_wallet_debit_observation(run_spend_guard->wallet_id);if(!observed.ok)return observed;}world_stage_observation::Scope apply_observation(world_stage_observation::Stage::opening_apply);auto opened=open_exact_period_in_candidate(*candidate);apply_observation.stop();if(!opened.ok)return opened;
 Integer gross=0;if(run_spend_guard){gross=candidate->common.observed_wallet_debits();if(gross>run_spend_guard->remaining)return common_failure("RunMoneyBudgetExceeded");candidate->common.clear_wallet_debit_observation();}
 if(plan_run&&run_spend_guard){if(candidate->player_plan.version!=2)return common_failure("PlayerPlanExactOpeningBudgetRequired");auto consumed=player_plan_consume_exact_budget(candidate->player_plan,HalfDayDuration{},gross);if(!consumed.ok)return consumed;}
 ++candidate->common.state().identity.revision;world_stage_observation::Scope validate_observation(world_stage_observation::Stage::opening_validate);auto valid=candidate->validate();validate_observation.stop();if(!valid.ok)return valid;if(run_spend_guard)run_spend_guard->observed=gross;world=std::move(candidate);return common_success();
}
// 建立停车许可并冻结玩家原私人钱包；不在begin隐式提交第一半日。
Outcome<RunProgress> ResidentWorld::begin_run(const BeginRun& request){
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
    if(storage_restore_pending())return fail<RunProgress>("StorageRestoreBusy");
    if(!impl_->world)return fail<RunProgress>("NoWorld");const auto& world=*impl_->world;const auto identity=world.common.state().identity;
    if(!source_matches(identity,request.expected))return fail<RunProgress>("StaleWorld");
    if(!full_matches(identity,request.expected))return fail<RunProgress>("StalePreview");
    if(request.run_id.empty()||request.run_id.size()>256||request.run_id.find('\0')!=StableId::npos||request.max_half_days<1||request.max_gross_outflow<0)return fail<RunProgress>("InvalidRunRequest");
    if(impl_->plan_run&&!terminal(impl_->plan_run->progress.stop))return fail<RunProgress>("RunAlreadyActive");
    if(impl_->run&&!terminal(impl_->run->progress.stop))return fail<RunProgress>("RunAlreadyActive");
    if(std::find(impl_->used_run_ids.begin(),impl_->used_run_ids.end(),request.run_id)!=impl_->used_run_ids.end())return fail<RunProgress>("RunIdAlreadyUsed");
    if(request.actor_id!=world.common.state().metadata.player_actor_id)return fail<RunProgress>("RunPlayerRequired");
    if(request.target){if(const auto* plan=std::get_if<PlanRunDriveTarget>(&*request.target)){if(plan->plan_id!=request.plan_id||request.plan_id.empty()||!request.action_id.empty()||request.resume_saved_scope)return fail<RunProgress>("InvalidPlanRunTarget");}else{const auto& action=std::get<ActionRunDriveTarget>(*request.target);if(action.action_id!=request.action_id||action.action_id.empty()||action.action_kind.empty()||!request.plan_id.empty())return fail<RunProgress>("InvalidActionRunTarget");}}
    if(!request.plan_id.empty())return impl_->begin_plan_run(request);
    const auto* actor=world.common.actor(request.actor_id);if(!actor)return fail<RunProgress>("UnknownActor");const auto* action=world.common.action(actor->action_id);
    if(!action||action->id!=request.action_id||action->actor_id!=request.actor_id)return fail<RunProgress>("RunActionRequired");
    if(request.target){const auto* target=std::get_if<ActionRunDriveTarget>(&*request.target);if(!target||target->action_kind!=action->kind)return fail<RunProgress>("RunActionKindMismatch");}
    if(world.catalog->exact_time&&exact_complete_selected(*world.catalog->exact_time)&&!request.target)return fail<RunProgress>("RunDriveTargetRequired");
    std::optional<ContinuationStartedSource> exact_action_start;
    if(world.catalog->exact_time&&exact_complete_selected(*world.catalog->exact_time)){auto lease=world.complete_exact_time_bindings();if(!lease.ok)return {false,{},std::move(lease.error)};if(!complete_exact_run_kind(lease.value->reads().runtime,action->kind))return fail<RunProgress>("RunActionRequired");auto start=read_complete_exact_continuation_start(world,lease.value->reads(),action->id,action->kind);if(!start.ok)return {false,{},std::move(start.error)};exact_action_start=std::move(start.value);}
    else if(!(bounded_continuation_kind(action->kind)||(world.catalog->exact_time&&action->kind=="service")))return fail<RunProgress>("RunActionRequired");
    const auto* wallet=world.common.wallet(actor->wallet_id);if(!wallet||wallet->owner.kind!=Owner::Kind::actor||wallet->owner.id!=actor->id)return fail<RunProgress>("RunPersonalWalletRequired");
    Impl::ActiveRun run;run.request=request;run.progress.identity=identity;run.progress.run_id=request.run_id;run.action_kind=action->kind;run.wallet_id=actor->wallet_id;run.action_start_tick=action->start_tick;run.started_tick=identity.tick;run.exact_action_started_source=std::move(exact_action_start);
    if(request.resume_saved_scope){
        auto saved=continuation_snapshot();if(!saved.ok)return {false,{},saved.error};
        if(!saved.value)return fail<RunProgress>("ContinuationUnavailable");const auto& previous=*saved.value;
        if(previous.actor_id!=request.actor_id||previous.action_id!=request.action_id||previous.max_half_days-previous.committed_half_days!=request.max_half_days||previous.max_gross_outflow-previous.gross_outflow!=request.max_gross_outflow)return fail<RunProgress>("ContinuationBudgetMismatch");
        run.request.max_half_days=previous.max_half_days;run.request.max_gross_outflow=previous.max_gross_outflow;run.started_tick=previous.started_tick;run.progress.committed_half_days=previous.committed_half_days;run.progress.gross_outflow=previous.gross_outflow;
    }
    if(world.catalog->exact_time){auto maximum=run_budget_units(run.request.max_half_days,world.world_timeline->scale_ref.units_per_half_day);if(!maximum.ok)return {false,{},maximum.error};run.exact_max_units=maximum.value;auto at=read_world_moment(world.common.tick(),*world.world_timeline);if(!at.ok)return {false,{},at.error};run.exact_started_at=at.value;run.progress.exact_source_moment=at.value;run.exact_spent_units=WorkUnits{};run.exact_committed_duration=HalfDayDuration{};
        if(request.resume_saved_scope){auto saved=continuation_snapshot();if(!saved.ok||!saved.value||!saved.value->exact_started_at||!saved.value->exact_committed_duration)return fail<RunProgress>("ExactContinuationRequired");run.exact_started_at=saved.value->exact_started_at;run.exact_committed_duration=saved.value->exact_committed_duration;auto consumed=duration_to_scale_units(*run.exact_committed_duration,world.world_timeline->scale_ref.units_per_half_day);if(!consumed.ok)return {false,{},consumed.error};run.exact_spent_units=consumed.value;}
        run.progress.exact_spent_units=run.exact_spent_units;auto left=subtract_work_units(*run.exact_max_units,*run.exact_spent_units);if(!left.ok)return {false,{},left.error};run.progress.exact_remaining_units=left.value;
    }
    remaining(run);
    auto frozen=snapshot_for(world,run);auto valid=validate_continuation_shape(frozen);if(!valid.ok)return {false,{},valid.error};auto source_stop=read_run_stop(world,frozen);if(!source_stop.ok)return {false,{},std::move(source_stop.error)};run.progress.stop=source_stop.value;if(run.progress.stop==RunStop::awaiting_permit&&run.request.max_gross_outflow>0&&run.progress.remaining_gross_outflow==0)run.progress.stop=RunStop::money_budget_exhausted;
    impl_->used_run_ids.push_back(request.run_id);impl_->restored_continuation.reset();impl_->plan_run.reset();impl_->run=std::move(run);return {true,impl_->run->progress,{}};
}
// Main每次只授一半日；CLI多步许可仍逐次走原submit，并在每个稳定边界核停点。
Outcome<RunProgress> ResidentWorld::resume_run(const ResumeRun& request){
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
    if(storage_restore_pending())return fail<RunProgress>("StorageRestoreBusy");
    if(!impl_->world)return fail<RunProgress>("NoWorld");if(!source_matches(impl_->world->common.state().identity,request.expected))return fail<RunProgress>("StaleWorld");
    if(impl_->plan_run&&impl_->plan_run->progress.run_id==request.run_id)return impl_->resume_plan_run(*this,request);
    if(!impl_->run||impl_->run->progress.run_id!=request.run_id)return fail<RunProgress>("UnknownRun");auto& run=*impl_->run;
    if(request.permit_seq==run.progress.permit_seq&&request.permit_seq>0)return {true,run.progress,{}};
    if(run.progress.permit_seq==std::numeric_limits<Integer>::max()||request.permit_seq!=run.progress.permit_seq+1||request.grant_half_days<1)return fail<RunProgress>("InvalidRunPermit");
    if(!full_matches(impl_->world->common.state().identity,request.expected))return fail<RunProgress>("StalePreview");
    if(terminal(run.progress.stop))return {true,run.progress,{}};
    run.progress.permit_seq=request.permit_seq;run.progress.error.reset();
    if(impl_->world->catalog->exact_time)return impl_->resume_exact_run(*this,request);
    const auto grant=std::min(request.grant_half_days,run.request.max_half_days-run.progress.committed_half_days);
    if(grant==0){run.progress.stop=RunStop::budget_exhausted;return {true,run.progress,{}};}
    for(Integer step=0;step<grant;++step){
    auto stop=continuation_domain_stop(*impl_->world,snapshot_for(*impl_->world,run));
    if(stop==RunStop::awaiting_permit&&!full_matches(impl_->world->common.state().identity,run.progress.identity))stop=RunStop::blocked;
    if(stop!=RunStop::awaiting_permit){run.progress.stop=stop;return {true,run.progress,{}};}
    if(run.progress.committed_half_days>=run.request.max_half_days){run.progress.stop=RunStop::budget_exhausted;return {true,run.progress,{}};}
    auto& world=*impl_->world;CommandEnvelope command;command.world_id=world.common.state().identity.world_id;command.execution_epoch=world.common.state().identity.execution_epoch;command.actor_id=run.request.actor_id;
    const auto* sequence=world.common.state().request_sequences.find(command.actor_id);if(!sequence||*sequence>=2147483647){run.progress.stop=RunStop::failed;run.progress.error=common_failure("InvalidRequestSequence").error;return {true,run.progress,{}};}
    command.request_seq=*sequence+1;command.expected_revision=world.common.revision();command.payload=std::make_shared<const CommandPayload>(CommandPayload{"advance",AdvanceCommand{}});
    auto prepared=preview(command);if(!prepared.ok){run.progress.error=prepared.error;run.progress.stop=failed_stop(prepared.error);return {true,run.progress,{}};}
    Outcome<std::shared_ptr<const CommitResult>> result;Integer spent=0;
    {
        impl_->run_spend_guard=Impl::RunSpendGuard{run.wallet_id,run.request.max_gross_outflow-run.progress.gross_outflow,0};ResetOnExit<Impl::RunSpendGuard> reset{impl_->run_spend_guard};
        result=submit(command);if(result.ok)spent=impl_->run_spend_guard->observed;
    }
    if(!result.ok){run.progress.error=result.error;run.progress.stop=failed_stop(result.error);return {true,run.progress,{}};}
    ++run.progress.committed_half_days;run.progress.gross_outflow+=spent;run.progress.identity=impl_->world->common.state().identity;remaining(run);
    run.progress.stop=continuation_domain_stop(*impl_->world,snapshot_for(*impl_->world,run));
    if(run.progress.stop==RunStop::awaiting_permit){if(run.progress.remaining_half_days==0)run.progress.stop=RunStop::budget_exhausted;else if(run.request.max_gross_outflow>0&&run.progress.remaining_gross_outflow==0)run.progress.stop=RunStop::money_budget_exhausted;}
    if(terminal(run.progress.stop))break;
    }
    return {true,run.progress,{}};
}
// 轮询只返回有限进度；发现人工改动或领域停点就撤销许可，不推进世界。
Outcome<RunProgress> ResidentWorld::poll_run(const RunRef& reference)const{
    if(!impl_->world)return fail<RunProgress>("NoWorld");const auto& current=impl_->world->common.state().identity;
    if(current.world_id!=reference.world_id||current.execution_epoch!=reference.execution_epoch)return fail<RunProgress>("StaleWorld");
    if(impl_->plan_run&&impl_->plan_run->progress.run_id==reference.run_id)return impl_->poll_plan_run(reference);
    if(!impl_->run||impl_->run->progress.run_id!=reference.run_id)return fail<RunProgress>("UnknownRun");auto& run=*impl_->run;
    if(!terminal(run.progress.stop)){auto source_stop=read_run_stop(*impl_->world,snapshot_for(*impl_->world,run));if(!source_stop.ok)return {false,{},std::move(source_stop.error)};run.progress.stop=source_stop.value;if(run.progress.stop==RunStop::awaiting_permit&&!full_matches(current,run.progress.identity))run.progress.stop=RunStop::blocked;}
    return {true,run.progress,{}};
}
// 停止自动驾驶不调用旅行/课程暂停或取消，也不释放领域预留。
Outcome<RunProgress> ResidentWorld::cancel_run(const RunRef& reference){
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());auto progress=poll_run(reference);if(!progress.ok)return progress;if(impl_->plan_run&&impl_->plan_run->progress.run_id==reference.run_id){impl_->plan_run->progress.stop=RunStop::cancelled;return {true,impl_->plan_run->progress,{}};}impl_->run->progress.stop=RunStop::cancelled;return {true,impl_->run->progress,{}};}
// 已结束或换掉的行动没有可恢复许可；原世界checkpoint仍可独立保存。
Outcome<std::optional<ContinuationSnapshot>> ResidentWorld::continuation_snapshot()const{
    if(!impl_->world)return fail<std::optional<ContinuationSnapshot>>("NoWorld");std::optional<ContinuationSnapshot> snapshot;
    if(impl_->plan_run)return {true,std::nullopt,{}};
    if(impl_->run){if(!full_matches(impl_->world->common.state().identity,impl_->run->progress.identity))return {true,std::nullopt,{}};snapshot=snapshot_for(*impl_->world,*impl_->run);}else if(impl_->restored_continuation)snapshot=*impl_->restored_continuation;
    if(!snapshot||!full_matches(impl_->world->common.state().identity,snapshot->source_identity))return {true,std::nullopt,{}};
    auto valid=validate_continuation_snapshot(*impl_->world,*snapshot);if(!valid.ok){if(impl_->world->catalog->exact_time&&impl_->world->common.action(snapshot->action_id))return {false,{},valid.error};return {true,std::nullopt,{}};}return {true,std::move(snapshot),{}};
}
// 冷采纳只安装停驻说明；旧run/permit从不复活，继续必须显式begin新许可。
Status ResidentWorld::restore_continuation(const ContinuationSnapshot& snapshot){
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
    if(storage_restore_pending())return common_failure("StorageRestoreBusy");
    if(!impl_->world)return common_failure("NoWorld");auto valid=validate_continuation_snapshot(*impl_->world,snapshot);if(!valid.ok)return valid;
    auto stopped=checkpoint_make_unique<ContinuationSnapshot>(snapshot);stopped->source_identity=impl_->world->common.state().identity;stopped->stop=RunStop::restored;impl_->run.reset();impl_->plan_run.reset();impl_->run_spend_guard.reset();impl_->restored_continuation.reset();impl_->restored_continuation_memory={};impl_->restored_continuation=std::move(stopped);return common_success();
}
// 原run的精确分派仍逐次使用原preview/submit，预算只按世界实际跨度一次计入。
Outcome<RunProgress> ResidentWorld::Impl::resume_exact_run(ResidentWorld& api,const ResumeRun& permit){
    WorldCheckpointWriteScope checkpoint_write(active_checkpoint_capture.lock());
    if(!active_storage_restore.expired())return fail<RunProgress>("StorageRestoreBusy");
 auto& active=*run;if(!active.exact_max_units||!active.exact_spent_units||!active.exact_started_at)return fail<RunProgress>("ExactRunBudgetRequired");const auto scale=world->world_timeline->scale_ref.units_per_half_day;
 // source identity与其完整M只在所有可能失败的准备后一起发布；无法读M则直接错误，不造offset0。
 auto fail_run=[&](const RuleError& error)->Outcome<RunProgress>{
  active.progress.error=error;active.progress.stop=failed_stop(error);
  auto moment=read_world_moment(world->common.tick(),*world->world_timeline);if(!moment.ok)return {false,{},error};
  if(!active.progress.exact_source_moment)return fail<RunProgress>("ExactRunMomentRequired");
  active.progress.exact_source_moment->half_day_tick=moment.value.half_day_tick;active.progress.exact_source_moment->sub_half_day.swap(moment.value.sub_half_day);active.progress.identity=world->common.state().identity;return {true,active.progress,{}};
 };
 auto refresh=[&]()->Status{
  if(!active.progress.exact_source_moment||!active.progress.exact_spent_units||!active.progress.exact_remaining_units||!active.exact_committed_duration)return common_failure("ExactRunProjectionRequired");
  auto moment=read_world_moment(world->common.tick(),*world->world_timeline);if(!moment.ok)return {false,{},moment.error};
  auto remaining_units=subtract_work_units(*active.exact_max_units,*active.exact_spent_units);if(!remaining_units.ok)return {false,{},remaining_units.error};
  auto duration=scale_units_to_duration(remaining_units.value,scale);if(!duration.ok)return {false,{},duration.error};auto display=ceil_half_day_duration(duration.value);if(!display.ok)return {false,{},display.error};
  auto spent_projection=duration_to_scale_units(*active.exact_committed_duration,scale);if(!spent_projection.ok)return {false,{},spent_projection.error};
  // 上述数值owning结果准备完毕后，只有noexcept值swap/整数写；不重复拷贝身份字符串。
  active.progress.exact_spent_units->value.swap(spent_projection.value.value);active.progress.exact_remaining_units->value.swap(remaining_units.value.value);
  active.progress.committed_half_days=active.request.max_half_days-display.value;active.progress.remaining_half_days=display.value;active.progress.remaining_gross_outflow=active.request.max_gross_outflow-active.progress.gross_outflow;
  active.progress.exact_source_moment->half_day_tick=moment.value.half_day_tick;active.progress.exact_source_moment->sub_half_day.swap(moment.value.sub_half_day);active.progress.identity=world->common.state().identity;return common_success();
 };
 auto left=subtract_work_units(*active.exact_max_units,*active.exact_spent_units);if(!left.ok)return fail_run(left.error);auto granted=run_budget_units(std::min(permit.grant_half_days,active.request.max_half_days),scale);if(!granted.ok)return fail_run(granted.error);auto smaller=compare_fractions(granted.value.value,left.value.value);if(!smaller.ok)return fail_run(smaller.error);const auto allowance=smaller.value<0?granted.value:left.value;auto zero=compare_fractions(allowance.value,CanonicalFraction{});if(!zero.ok)return fail_run(zero.error);if(zero.value==0){active.progress.stop=RunStop::budget_exhausted;return {true,active.progress,{}};}
 // 先撤销失配许可/读真实领域停点，不能拿新世界身份替旧scope授权。
 if(!full_matches(world->common.state().identity,active.progress.identity)){active.progress.stop=RunStop::blocked;auto updated=refresh();if(!updated.ok)return fail_run(updated.error);return {true,active.progress,{}};}
 auto initial_stop=read_run_stop(*world,snapshot_for(*world,active));if(!initial_stop.ok)return fail_run(initial_stop.error);if(initial_stop.value!=RunStop::awaiting_permit){active.progress.stop=initial_stop.value;auto updated=refresh();if(!updated.ok)return fail_run(updated.error);return {true,active.progress,{}};}
 // 许可没有业务due：以实际已用量递减本次grant，每次只构造至多一半日目标，MAXscope不先加日历。
 WorkUnits permit_spent;
 while(true){
  if(!full_matches(world->common.state().identity,active.progress.identity)){active.progress.stop=RunStop::blocked;break;}
  auto domain=read_run_stop(*world,snapshot_for(*world,active));if(!domain.ok)return fail_run(domain.error);if(domain.value!=RunStop::awaiting_permit){active.progress.stop=domain.value;break;}
  if(active.request.max_gross_outflow>0&&active.progress.gross_outflow==active.request.max_gross_outflow){active.progress.stop=RunStop::money_budget_exhausted;break;}
  auto permit_left=subtract_work_units(allowance,permit_spent);if(!permit_left.ok)return fail_run(permit_left.error);auto empty=compare_fractions(permit_left.value.value,CanonicalFraction{});if(!empty.ok)return fail_run(empty.error);if(empty.value==0)break;
  // Opening独立发布，真实收费亦受同一gross预算；开期后重读due，不能使用开期前预测记账。
  if(world->world_timeline->boundary_state==PeriodBoundaryState::ready){Integer gross=0;Status opened;{run_spend_guard=RunSpendGuard{active.wallet_id,active.request.max_gross_outflow-active.progress.gross_outflow,0};ResetOnExit<RunSpendGuard> clear{run_spend_guard};opened=open_exact_period();if(opened.ok)gross=run_spend_guard->observed;}if(!opened.ok)return fail_run(opened.error);active.progress.gross_outflow+=gross;auto updated=refresh();if(!updated.ok)return fail_run(updated.error);continue;}
  auto now=read_world_moment(world->common.tick(),*world->world_timeline);if(!now.ok)return fail_run(now.error);
  auto remaining_duration=scale_units_to_duration(permit_left.value,scale);if(!remaining_duration.ok)return fail_run(remaining_duration.error);auto one=normalize_fraction("1","1");if(!one.ok)return fail_run(one.error);auto more=compare_fractions(remaining_duration.value.value,one.value);if(!more.ok)return fail_run(more.error);auto slice_allowance=more.value>0?HalfDayDuration{one.value}:remaining_duration.value;
  if(now.value.half_day_tick==std::numeric_limits<Integer>::max())return fail_run(common_failure("WorldTickOverflow").error);
  const WorldMoment boundary{now.value.half_day_tick+1,{}};auto until_boundary=elapsed_world_duration(now.value,boundary);if(!until_boundary.ok)return fail_run(until_boundary.error);auto crosses=compare_fractions(slice_allowance.value,until_boundary.value.value);if(!crosses.ok)return fail_run(crosses.error);
  auto upper=crosses.value>=0?Outcome<WorldMoment>{true,boundary,{}}:add_world_duration(now.value,slice_allowance);if(!upper.ok)return fail_run(upper.error);
  CommandEnvelope command;const auto identity=world->common.state().identity;command.world_id=identity.world_id;command.execution_epoch=identity.execution_epoch;command.actor_id=active.request.actor_id;const auto* seq=world->common.state().request_sequences.find(command.actor_id);if(!seq||*seq>=2147483647)return fail_run(common_failure("InvalidRequestSequence").error);command.request_seq=*seq+1;command.expected_revision=identity.revision;command.payload=std::make_shared<const CommandPayload>(CommandPayload{"advance",AdvanceCommand{upper.value}});
  // 真实提交前准备全部预算后值。许可消耗与世界已用时间同次noexcept交换，无按片数收费。
  auto expected_end=prepare_exact_advance_target(*world,upper.value);if(!expected_end.ok)return fail_run(expected_end.error);auto actual=elapsed_world_duration(now.value,expected_end.value);if(!actual.ok)return fail_run(actual.error);auto positive=compare_fractions(actual.value.value,CanonicalFraction{});if(!positive.ok)return fail_run(positive.error);if(positive.value<=0)return fail_run(common_failure("ExactRunPositiveSliceRequired").error);
  auto units=duration_to_scale_units(actual.value,scale);if(!units.ok)return fail_run(units.error);auto next_permit=add_work_units(permit_spent,units.value);if(!next_permit.ok)return fail_run(next_permit.error);auto within=compare_fractions(next_permit.value.value,allowance.value);if(!within.ok)return fail_run(within.error);if(within.value>0)return fail_run(common_failure("ExactRunPermitExceeded").error);
  auto spent=add_work_units(*active.exact_spent_units,units.value);if(!spent.ok)return fail_run(spent.error);auto spent_duration=scale_units_to_duration(spent.value,scale);if(!spent_duration.ok)return fail_run(spent_duration.error);auto prepared=api.preview(command);if(!prepared.ok)return fail_run(prepared.error);
  Outcome<std::shared_ptr<const CommitResult>> committed;Integer gross=0;{run_spend_guard=RunSpendGuard{active.wallet_id,active.request.max_gross_outflow-active.progress.gross_outflow,0};ResetOnExit<RunSpendGuard> clear{run_spend_guard};committed=api.submit(command);if(committed.ok)gross=run_spend_guard->observed;}if(!committed.ok)return fail_run(committed.error);
  active.exact_spent_units->value.swap(spent.value.value);active.exact_committed_duration->value.swap(spent_duration.value.value);permit_spent.value.swap(next_permit.value.value);active.progress.gross_outflow+=gross;auto updated=refresh();if(!updated.ok)return fail_run(updated.error);
 }
 auto updated=refresh();if(!updated.ok)return fail_run(updated.error);if(active.progress.stop==RunStop::awaiting_permit){auto exhausted=compare_fractions(active.progress.exact_remaining_units->value,CanonicalFraction{});if(!exhausted.ok)return fail_run(exhausted.error);if(exhausted.value==0)active.progress.stop=RunStop::budget_exhausted;else if(active.request.max_gross_outflow>0&&active.progress.remaining_gross_outflow==0)active.progress.stop=RunStop::money_budget_exhausted;}return {true,active.progress,{}};
}

} // 同一worker串行执行；队列在稳定边界消费停止，已提交效果不回滚。
