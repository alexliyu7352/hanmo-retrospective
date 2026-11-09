// 玩家计划边界只登记已采用的原操作；调用者负责和同一世界候选一起提交。
#include "plan_lifecycle.hpp"
#include "plan_time2.hpp"
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 稳定ID查找不重排步骤，也不凭显示下标改绑目标。
PlayerPlanStep* find_step(PlayerPlanState& state,const StableId& id){if(!state.current)return nullptr;for(auto& step:state.current->steps)if(step.id==id)return &step;return nullptr;}
// 已经启动但尚无完成回执的行动不得从意向编辑中移除。
bool running(const PlayerPlanStep& step){return step.started_tick&&!step.completed_tick;}
// 授权只适用于明确普通工作或课程；执行时仍须原领域重报价。
bool authorizable(const PlayerPlanStepData& data){return std::holds_alternative<PlayerPlanWork>(data)||std::holds_alternative<PlayerPlanCourse>(data);}
// 同一计划和下一步骤是唯一合法执行边界，旧回调不能跨到新列表。
Outcome<PlayerPlanStep*> boundary_step(PlayerPlanState& state,const StableId& plan,const StableId& step){if(!state.current||state.current->id!=plan)return {false,nullptr,common_failure("StalePlayerPlan").error};auto* found=find_step(state,step);if(!found)return {false,nullptr,common_failure("UnknownPlayerPlanStep").error};return {true,found,{}};}
}
// 步骤类型由variant确定，不接收任意执行命令名。
StableId player_plan_step_kind(const PlayerPlanStepData& data){static const char* kinds[]={"move_local","travel","work","course","rest","continue_action","task_reminder"};return kinds[data.index()];}
// 阶段名是固定投影词表，未知枚举不会映射为可执行状态。
StableId player_plan_phase_name(PlayerPlanStepPhase phase){switch(phase){case PlayerPlanStepPhase::queued:return "queued";case PlayerPlanStepPhase::active:return "active";case PlayerPlanStepPhase::awaiting_confirmation:return "awaiting_confirmation";case PlayerPlanStepPhase::completed:return "completed";case PlayerPlanStepPhase::blocked:return "blocked";}return "invalid";}
// 比较全部实质条款，任何价格、来源或资格规则变化都需要重新授权。
bool same_player_plan_terms(const PlayerPlanTerms& a,const PlayerPlanTerms& b){return std::tie(a.provider_actor_id,a.role_id,a.place_id,a.payer_wallet_id,a.recipient_wallet_id,a.mode,a.skill_id,a.qualification_key,a.challenge_kind,a.challenge_id,a.challenge_mode,a.settlement_policy,a.challenge_source_sha256,a.duration,a.cost,a.wage,a.hp_gain,a.fatigue_delta,a.xp_limit,a.target_level)==std::tie(b.provider_actor_id,b.role_id,b.place_id,b.payer_wallet_id,b.recipient_wallet_id,b.mode,b.skill_id,b.qualification_key,b.challenge_kind,b.challenge_id,b.challenge_mode,b.settlement_policy,b.challenge_source_sha256,b.duration,b.cost,b.wage,b.hp_gain,b.fatigue_delta,b.xp_limit,b.target_level);}
// 按既定顺序寻找首个未成功完成步骤，不跳过阻塞目标。
const PlayerPlanStep* next_player_plan_step(const PlayerPlanState& state){if(state.current)for(const auto& step:state.current->steps)if(step.phase!=PlayerPlanStepPhase::completed)return &step;return nullptr;}
// 可写查找借用同一个计划，不复制或重新绑定步骤。
PlayerPlanStep* next_player_plan_step(PlayerPlanState& state){return const_cast<PlayerPlanStep*>(next_player_plan_step(static_cast<const PlayerPlanState&>(state)));}
// 编辑先在计划副本验证，ID沿共同分配器产生且不触及资源或原行动。
Outcome<StableId> edit_player_plan(CommonStore& common,PlayerPlanState& state,const StableId& actor,const PlayerPlanEdit& edit){
    const auto fail=[](const StableId& code)->Outcome<StableId>{return {false,{},common_failure(code).error};};
    if(actor!=common.state().metadata.player_actor_id||!common.actor(actor))return fail("PlayerPlanActorRequired");
    auto candidate=state;bool scope_changed=false;if(candidate.current&&candidate.current->actor_id!=actor)return fail("PlayerPlanActorMismatch");StableId changed;
    if(const auto* add=std::get_if<PlayerPlanAppend>(&edit)){
        scope_changed=true;
        if(candidate.current&&candidate.current->steps.size()>=player_plan_max_steps)return fail("PlayerPlanStepLimit");
        if(add->authorized&&(!authorizable(add->data)||!add->terms))return fail("PlayerPlanAuthorizationRequired");
        PlayerPlanStep step;step.id="pending_step";step.data=add->data;step.source=add->source;step.authorized=add->authorized;step.terms=add->terms;
        if(!candidate.current)candidate.current=PlayerPlan{"pending_plan",actor,common.tick(),{},"",{}};
        candidate.current->steps.push_back(step);auto checked=validate_player_plan_shape(candidate);if(!checked.ok)return {false,{},checked.error};
        if(candidate.current->id=="pending_plan"){auto id=common.next_id("player_plan");if(!id.ok)return {false,{},id.error};candidate.current->id=id.value;}
        auto id=common.next_id("plan_step");if(!id.ok)return {false,{},id.error};candidate.current->steps.back().id=id.value;changed=id.value;
    }else if(std::holds_alternative<PlayerPlanClear>(edit)){
        if(candidate.current){auto& steps=candidate.current->steps;scope_changed=std::any_of(steps.begin(),steps.end(),[](const auto& step){return !running(step);});const bool active=std::any_of(steps.begin(),steps.end(),running);if(active){steps.erase(std::remove_if(steps.begin(),steps.end(),[](const auto& step){return !running(step);}),steps.end());candidate.current->stop_reason="FuturePlanCleared";}else candidate.current.reset();}
    }else if(std::holds_alternative<PlayerPlanPause>(edit)){player_plan_pause(candidate,"PlayerPlanPaused");}
    else{
        if(!candidate.current)return fail("NoPlayerPlan");
        if(const auto* remove=std::get_if<PlayerPlanRemove>(&edit)){auto* step=find_step(candidate,remove->step_id);if(!step)return fail("UnknownPlayerPlanStep");if(running(*step))return fail("PlayerPlanActionStillActive");scope_changed=true;changed=step->id;auto& steps=candidate.current->steps;steps.erase(std::remove_if(steps.begin(),steps.end(),[&](const auto& row){return row.id==remove->step_id;}),steps.end());if(steps.empty())candidate.current.reset();}
        else if(const auto* reorder=std::get_if<PlayerPlanReorder>(&edit)){auto& steps=candidate.current->steps;if(reorder->step_ids.size()!=steps.size())return fail("InvalidPlayerPlanOrder");std::set<StableId> seen;std::vector<PlayerPlanStep> sorted;for(std::size_t i=0;i<reorder->step_ids.size();++i){auto* step=find_step(candidate,reorder->step_ids[i]);if(!step||!seen.insert(step->id).second)return fail("InvalidPlayerPlanOrder");if((step->started_tick||steps[i].started_tick)&&step->id!=steps[i].id)return fail("PlayerPlanStartedOrderFixed");if(step->id!=steps[i].id)scope_changed=true;sorted.push_back(*step);}steps=std::move(sorted);}
        else if(const auto* auth=std::get_if<PlayerPlanAuthorize>(&edit)){auto* step=find_step(candidate,auth->step_id);if(!step)return fail("UnknownPlayerPlanStep");if(step->started_tick)return fail("PlayerPlanStepAlreadyStarted");if(!authorizable(step->data))return fail("PlayerPlanNotAuthorizable");if(auth->authorized&&!auth->terms)return fail("PlayerPlanAuthorizationRequired");scope_changed=step->authorized!=auth->authorized||(auth->terms&&(!step->terms||!same_player_plan_terms(*step->terms,*auth->terms)));step->authorized=auth->authorized;if(auth->terms)step->terms=auth->terms;step->phase=PlayerPlanStepPhase::queued;step->stop_reason.clear();changed=step->id;}
    }
    if(candidate.current&&!std::holds_alternative<PlayerPlanClear>(edit)&&!std::holds_alternative<PlayerPlanPause>(edit))candidate.current->stop_reason="PlayerPlanEdited";
    if(candidate.version==2&&candidate.current&&scope_changed&&candidate.current->budget){if(candidate.current->approved_scope_revision==std::numeric_limits<Integer>::max())return fail("PlayerPlanScopeRevisionOverflow");++candidate.current->approved_scope_revision;}
    auto checked=validate_player_plan_shape(candidate);if(!checked.ok)return {false,{},checked.error};state=std::move(candidate);return {true,changed,{}};
}
// 登记本次真实启动回执，重复同一边界无副作用，其他边界拒绝。
Status player_plan_record_started(PlayerPlanState& state,const StableId& plan,const StableId& step_id,const std::optional<StableId>& action,const StableId& result,Integer tick){
    auto found=boundary_step(state,plan,step_id);if(!found.ok)return {false,{},found.error};auto& step=*found.value;
    if(step.started_tick){if(step.action_id==action&&step.start_result_id==result&&*step.started_tick==tick)return common_success();return common_failure("PlayerPlanStartAlreadyRecorded",step_id);}
    if(next_player_plan_step(state)!=&step||result.empty()||tick<state.current->created_tick||(action&&action->empty()))return common_failure("InvalidPlayerPlanStart",step_id);
    if(!action&&!std::holds_alternative<PlayerPlanMove>(step.data))return common_failure("PlayerPlanActionReferenceRequired",step_id);
    step.action_id=action;step.start_result_id=result;step.started_tick=tick;step.phase=PlayerPlanStepPhase::active;step.stop_reason.clear();state.current->stop_reason.clear();return common_success();
}
// 完成或终止目标只引用原回执，不由空闲状态或预计收益推断。
Status player_plan_record_completed(PlayerPlanState& state,const StableId& plan,const StableId& step_id,const std::optional<StableId>& action,const StableId& result,Integer tick,bool success,const StableId& code){
    auto found=boundary_step(state,plan,step_id);if(!found.ok)return {false,{},found.error};auto& step=*found.value;
    if(step.completed_tick){if(step.action_id==action&&step.completion_result_id==result&&*step.completed_tick==tick&&((step.phase==PlayerPlanStepPhase::completed)==success)&&step.stop_reason==code)return common_success();return common_failure("PlayerPlanCompletionAlreadyRecorded",step_id);}
    if(!step.started_tick){if(action||!std::holds_alternative<PlayerPlanMove>(step.data))return common_failure("PlayerPlanStartRequired",step_id);auto started=player_plan_record_started(state,plan,step_id,action,result,tick);if(!started.ok)return started;}
    if(step.action_id!=action||result.empty()||tick<*step.started_tick)return common_failure("InvalidPlayerPlanCompletion",step_id);
    step.completion_result_id=result;step.completed_tick=tick;step.phase=success?PlayerPlanStepPhase::completed:PlayerPlanStepPhase::blocked;step.stop_reason=code;if(!success)state.current->stop_reason=code.empty()?"PlayerPlanActionFailed":code;return common_success();
}
// 阻塞原因仅暂停下一步，当前已接受行动保留原占用和引用。
Status player_plan_stop(PlayerPlanState& state,const StableId& step_id,const StableId& reason,bool confirmation){auto* step=find_step(state,step_id);if(!step)return common_failure("UnknownPlayerPlanStep");if(reason.empty())return common_failure("PlayerPlanStopReasonRequired");if(!step->started_tick)step->phase=confirmation?PlayerPlanStepPhase::awaiting_confirmation:PlayerPlanStepPhase::blocked;step->stop_reason=reason;state.current->stop_reason=reason;return common_success();}
// 手动操作或恢复仅记录停止原因，不撤销任何领域承诺。
void player_plan_pause(PlayerPlanState& state,const StableId& reason){if(state.current)state.current->stop_reason=reason;}
// /1保原批次重批规则；/2只改明确上限并保留已耗金额/时间，逐步进度不重授额度。
Status player_plan_set_budget(PlayerPlanState& state,Integer tick,Integer ticks,Integer spend){
 if(!state.current)return common_failure("NoPlayerPlan");if(tick<state.current->created_tick||ticks<1||spend<0)return common_failure("InvalidPlayerPlanBudget");
 auto& plan=*state.current;
 if(state.version==2){auto valid=validate_player_plan_shape(state);if(!valid.ok)return valid;
  const bool changed=!plan.budget||plan.budget->max_additional_ticks!=ticks||plan.budget->max_gross_spend!=spend;
  if(changed&&plan.approved_scope_revision==std::numeric_limits<Integer>::max())return common_failure("PlayerPlanScopeRevisionOverflow");
  PlayerPlanBudget next=plan.budget.value_or(PlayerPlanBudget{});next.max_additional_ticks=ticks;next.max_gross_spend=spend;next.approved_tick=tick;if(!next.exact_used_duration)next.exact_used_duration=HalfDayDuration{};
  auto maximum=normalize_fraction(std::to_string(ticks),"1");if(!maximum.ok)return {false,{},maximum.error};auto within=compare_fractions(next.exact_used_duration->value,maximum.value);if(!within.ok)return {false,{},within.error};if(within.value>0||next.gross_spend>spend)return common_failure("PlayerPlanBudgetBelowConsumed");
  plan.budget=std::move(next);if(changed)++plan.approved_scope_revision;
 }else if(state.version==1)plan.budget=PlayerPlanBudget{ticks,spend,0,0,tick,{}};else return common_failure("PlayerPlanVersionMismatch");
 plan.stop_reason.clear();for(auto& step:plan.steps){if(!step.started_tick){step.phase=PlayerPlanStepPhase::queued;step.stop_reason.clear();}else if(!step.completed_tick)step.stop_reason.clear();}return common_success();
}
// 先核两个剩余额，再同时登记时间和毛支出，拒绝不留下部分消耗。
Status player_plan_consume_budget(PlayerPlanState& state,Integer ticks,Integer spend){if(state.version!=1)return common_failure("PlayerPlanExactBudgetRequired");if(!state.current||!state.current->budget)return common_failure("PlayerPlanBudgetRequired");auto& b=*state.current->budget;if(ticks<0||spend<0||b.used_ticks>b.max_additional_ticks||b.gross_spend>b.max_gross_spend)return common_failure("InvalidPlayerPlanBudget");if(ticks>b.max_additional_ticks-b.used_ticks)return common_failure("PlayerPlanTimeBudgetExceeded");if(spend>b.max_gross_spend-b.gross_spend)return common_failure("PlayerPlanSpendBudgetExceeded");b.used_ticks+=ticks;b.gross_spend+=spend;return common_success();}
} // 编辑和停止只影响计划驱动，没有任何钱包、库存或原承诺变更。
