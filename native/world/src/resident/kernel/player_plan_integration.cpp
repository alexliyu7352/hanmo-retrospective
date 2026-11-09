// 同源新增/授权不信caller条款；原业务回执形成后才登记步骤与实际预算。
#include "player_plan_integration.hpp"
#include "resident/domains/player_plan/plan_commands.hpp"
#include "resident/domains/player_plan/plan_codec.hpp"
#include "resident/domains/player_plan/plan_lifecycle.hpp"
#include "resident/domains/player_plan/plan_time2.hpp"
#include "exact_time_profile.hpp"
#include "resident/domains/player_plan/plan_sources.hpp"
namespace hanmo::resident {
// 外部可用的固定操作名单不含内部execute，防止把控制器权限借给普通command包。
bool player_plan_command_kind(const StableId& kind){return kind=="player_plan_add"||kind=="player_plan_remove"||kind=="player_plan_reorder"||kind=="player_plan_clear"||kind=="player_plan_pause"||kind=="player_plan_authorize"||kind=="player_plan_budget"||kind=="player_plan_step";}
Outcome<PlayerPlanCommand> decode_player_plan_command(const StableId& kind,const BoundaryValue& raw){try{
    using namespace boundary;
    if(kind=="player_plan_step")return {false,{},common_failure("PlayerPlanInternalCommandRequired").error};
    if(kind=="player_plan_budget"){fields(raw,{"max_additional_ticks","max_gross_spend"},{},"player_plan_budget");return {true,{kind,PlayerPlanApproveBudget{get_int(raw,"max_additional_ticks","player_plan_budget"),get_int(raw,"max_gross_spend","player_plan_budget")}},{}};}
    auto edit=decode_player_plan_edit(kind,raw);if(!edit.ok)return {false,{},edit.error};return {true,{kind,std::move(edit.value)},{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
// 预览在只读世界和私有计划副本上验证，不扣费、不预留、不消耗原ID/RNG。
Outcome<PlayerPlanQuote> preview_player_plan_command(const WorldBundle& w,const StableId& actor,const PlayerPlanCommand& raw){
    if(!w.player_plan_enabled())return {false,{},common_failure("PlayerPlanProfileRequired",{},ErrorKind::unsupported_capability).error};
    if(actor!=w.common.state().metadata.player_actor_id||!w.common.actor(actor))return {false,{},common_failure("PlayerPlanActorRequired").error};
    PlayerPlanQuote quote;quote.command=raw;
    if(const auto* execute=std::get_if<PlayerPlanInternalStep>(&raw.data)){
        auto execution=prepare_player_plan_execution(w,actor,w.player_plan);if(!execution.ok)return {false,{},execution.error};
        if(execution.value.plan_id!=execute->plan_id||execution.value.step_id!=execute->step_id)return {false,{},common_failure("StalePlayerPlan").error};
        quote.execution=std::make_shared<const PlayerPlanExecution>(std::move(execution.value));return {true,std::move(quote),{}};
    }
    auto state=w.player_plan;
    if(const auto* budget=std::get_if<PlayerPlanApproveBudget>(&raw.data)){auto valid=player_plan_set_budget(state,w.common.tick(),budget->ticks,budget->spend);if(!valid.ok)return {false,{},valid.error};return {true,std::move(quote),{}};}
    auto& edit=std::get<PlayerPlanEdit>(quote.command.data);
    if(const auto* add=std::get_if<PlayerPlanAppend>(&edit)){
        auto known=prepare_player_plan_source(w,actor,add->data,add->authorized);if(!known.ok)return {false,{},known.error};
        if(add->authorized&&(!add->terms||!known.value.terms||!same_player_plan_terms(*add->terms,*known.value.terms)))return {false,{},common_failure("PlayerPlanQuotedTermsChanged").error};
        quote.terms=known.value.terms;edit=std::move(known.value);
    }else if(const auto* auth=std::get_if<PlayerPlanAuthorize>(&edit)){
        const PlayerPlanStep* step=nullptr;if(state.current)for(const auto& candidate:state.current->steps)if(candidate.id==auth->step_id)step=&candidate;
        if(!step)return {false,{},common_failure("UnknownPlayerPlanStep").error};
        if(auth->authorized&&(!auth->terms||!step->terms||!same_player_plan_terms(*auth->terms,*step->terms)))return {false,{},common_failure("PlayerPlanQuotedTermsChanged").error};
        auto canonical=*auth;canonical.terms=step->terms;edit=std::move(canonical);quote.terms=step->terms;
    }
    CommonStore scratch=w.common;auto valid=edit_player_plan(scratch,state,actor,edit);if(!valid.ok)return {false,{},valid.error};return {true,std::move(quote),{}};
}
// apply复用原业务candidate；实际付款在共同观察器中由提交尾段统一核预算。
// 同候选计划执行只借真实外层捕获；内层失败原样返回，不变成计划成功。
Outcome<ReceiptDetails> apply_player_plan_command(WorldBundle& w,const StableId& actor,const PlayerPlanQuote& quote,CompleteExactCommandCapture* capture){
    if(quote.execution)return apply_player_plan_execution(w,w.player_plan,actor,*quote.execution,capture);
    if(const auto* budget=std::get_if<PlayerPlanApproveBudget>(&quote.command.data)){auto valid=player_plan_set_budget(w.player_plan,w.common.tick(),budget->ticks,budget->spend);if(!valid.ok)return {false,{},valid.error};}
    else{auto valid=edit_player_plan(w.common,w.player_plan,actor,std::get<PlayerPlanEdit>(quote.command.data));if(!valid.ok)return {false,{},valid.error};}
    return {true,ReceiptDetails{Done{},{}},{}};
}
// 只保当前八步的最多16个真实边界回执，仍在原64槽内淘汰其它最旧结果。
bool player_plan_pins_result(const PlayerPlanState& state,const StableId& id){if(state.current)for(const auto& step:state.current->steps)if(step.start_result_id==std::optional<StableId>(id)||step.completion_result_id==std::optional<StableId>(id))return true;return false;}
// 此调用在原result已入当前候选之后；合法继任不继承前任玩家意向。
Status finish_player_plan_commit(WorldBundle& w,const StableId& actor,const PlayerPlanExecution* execution,const ReceiptDetails& details,const StableId& result){
    if(!w.player_plan_enabled())return common_success();
    if(w.player_plan.current&&w.player_plan.current->actor_id!=w.common.state().metadata.player_actor_id){w.player_plan.current.reset();return common_success();}
    if(!w.player_plan.current)return execution?common_failure("StalePlayerPlan"):common_success();
    const auto observer=std::holds_alternative<AdvanceDetails>(details.data)?w.player_plan.current->actor_id:actor;
    auto observed=reconcile_player_plan_commit(w,w.player_plan,observer,execution,details,result);if(!observed.ok)return observed;
    if(execution&&w.player_plan.version==2){if(!execution->exact_source_moment||!w.world_timeline)return common_failure("PlayerPlanExactSourceRequired");auto now=read_world_moment(w.common.tick(),*w.world_timeline);if(!now.ok)return {false,{},now.error};auto elapsed=elapsed_world_duration(*execution->exact_source_moment,now.value);if(!elapsed.ok)return {false,{},elapsed.error};auto one=normalize_fraction("1","1");if(!one.ok)return {false,{},one.error};auto maximum=compare_fractions(elapsed.value.value,one.value);if(!maximum.ok)return {false,{},maximum.error};auto positive=compare_fractions(elapsed.value.value,CanonicalFraction{});if(!positive.ok)return {false,{},positive.error};if(maximum.value>0||(execution->kind==PlayerPlanExecutionKind::advance?positive.value<=0:positive.value!=0))return common_failure("PlayerPlanInvalidStepClock");return player_plan_consume_exact_budget(w.player_plan,elapsed.value,w.common.observed_wallet_debits());}
    if(execution){const auto elapsed=w.common.tick()-execution->source.tick;if(elapsed<0||elapsed>1)return common_failure("PlayerPlanInvalidStepClock");return player_plan_consume_budget(w.player_plan,elapsed,w.common.observed_wallet_debits());}
    return common_success();
}
}
