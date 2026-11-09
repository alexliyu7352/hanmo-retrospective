// 原PlayerPlan预算的Time2量纲适配，消费与原command效果共候选发布。
#include "plan_time2.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
// 原算术技术分类原样传播，不转换成业务超时。
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void fail(const char* code){throw Failure{common_failure(code,"player_plan/budget").error};}
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(const Failure& f){return {false,{},f.error};}catch(const std::bad_alloc&){return {false,{},common_failure("PlayerPlanTimeResourceExhausted","player_plan",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("PlayerPlanTimeResourceExhausted","player_plan",ErrorKind::internal).error};}}
CanonicalFraction maximum(const PlayerPlanBudget& b,ExactArithmeticBudget budget){if(b.max_additional_ticks<1)fail("InvalidPlayerPlanBudget");return take(normalize_fraction(std::to_string(b.max_additional_ticks),"1",budget));}
}
// /1保旧整数规则；/2精确累计和floor投影必须相互一致，档案不能借版本清空消耗。
Status validate_player_plan_time_budget(const PlayerPlanState& state,ExactArithmeticBudget budget){return guard<Done>([&]{
 if(state.version!=player_plan_version&&state.version!=player_plan_time2_version)fail("PlayerPlanVersionMismatch");if(!state.current)return Done{};const auto& p=*state.current;
 if(p.approved_scope_revision<0||(state.version==1&&p.approved_scope_revision!=0))fail("InvalidPlayerPlanScopeRevision");
 if(!p.budget){if(state.version==2&&p.approved_scope_revision!=0)fail("UnapprovedPlayerPlanScopeRevision");return Done{};}const auto& b=*p.budget;
 if(state.version==1){if(b.exact_used_duration)fail("UnexpectedPlayerPlanExactBudget");return Done{};}
 if(!b.exact_used_duration||p.approved_scope_revision<1)fail("MissingPlayerPlanExactBudget");
 if(take(compare_fractions(b.exact_used_duration->value,maximum(b,budget),budget))>0)fail("PlayerPlanTimeBudgetExceeded");
 if(take(floor_fraction(b.exact_used_duration->value,budget))!=b.used_ticks)fail("PlayerPlanTimeProjectionMismatch");return Done{};
});}
// 只读同一批准上限减实际消耗；零/分数均保真，不能重新批准ceil后的额度。
Outcome<HalfDayDuration> read_player_plan_remaining_duration(const PlayerPlanState& state,ExactArithmeticBudget budget){return guard<HalfDayDuration>([&]{take(validate_player_plan_time_budget(state,budget));if(state.version!=2||!state.current||!state.current->budget)fail("PlayerPlanExactBudgetRequired");const auto& b=*state.current->budget;return HalfDayDuration{take(subtract_fractions(maximum(b,budget),b.exact_used_duration->value,budget))};});}
// 所有可能失败的精确计算先完成；最后仅交换数值并写原整数，无半份消费。
Status player_plan_consume_exact_budget(PlayerPlanState& state,const HalfDayDuration& elapsed,Integer spend,ExactArithmeticBudget budget){return guard<Done>([&]{auto remaining=take(read_player_plan_remaining_duration(state,budget));auto& b=*state.current->budget;if(spend<0||b.gross_spend<0||b.gross_spend>b.max_gross_spend)fail("InvalidPlayerPlanBudget");if(spend>b.max_gross_spend-b.gross_spend)fail("PlayerPlanSpendBudgetExceeded");if(take(compare_fractions(elapsed.value,remaining.value,budget))>0)fail("PlayerPlanTimeBudgetExceeded");auto sum=take(add_fractions(b.exact_used_duration->value,elapsed.value,budget));auto projected=take(floor_fraction(sum,budget));b.exact_used_duration->value.swap(sum);b.used_ticks=projected;b.gross_spend+=spend;return Done{};});}
// 合法旧整数累计精确嵌入；初次scope仅来自已有真实预算，不重置钱或扩大授权。
Outcome<PlayerPlanState> migrate_validated_player_plan_time2(const PlayerPlanState& original,ExactArithmeticBudget budget){return guard<PlayerPlanState>([&]{if(original.version!=1)fail("PlayerPlanMigrationVersionMismatch");take(validate_player_plan_shape(original));auto result=original;result.version=2;if(result.current){auto& plan=*result.current;plan.approved_scope_revision=plan.budget?1:0;if(plan.budget)plan.budget->exact_used_duration=HalfDayDuration{take(normalize_fraction(std::to_string(plan.budget->used_ticks),"1",budget))};}take(validate_player_plan_shape(result));return result;});}
}
