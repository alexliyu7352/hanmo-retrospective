#include "resident/storage/common_store.hpp"
// 安民效果在实际劳动完成时冻结；任务终结、支出与民心应用仍须各自真实来源。
#include "politics_governance_contract.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
namespace hanmo::resident {
Status validate_appeasement_rule(const AppeasementRule& rule){
    if(rule.id.empty()||rule.version.empty()||rule.name_key.empty()||rule.grain_item_id.empty()||rule.duration_ticks<1||rule.cash_amount<1||rule.grain_amount<1||rule.base_support_gain<0||rule.base_support_gain>100||rule.support_cap<1||rule.support_cap>100)return common_failure("InvalidAppeasementRule",rule.id);
    return common_success();
}
Outcome<GovernanceOutcomePlan> freeze_appeasement_outcome(const CommonStore& common,const AppeasementRule& rule,const StableId& executor,Integer tick,const DynamicStepInput* phase){
    auto valid=validate_appeasement_rule(rule);if(!valid.ok)return {false,{},valid.error};
    Integer boundary=common.tick();if(phase){if(phase->from_tick!=common.tick()||phase->from_tick<0||phase->from_tick==INT64_MAX||phase->to_tick!=phase->from_tick+1)return {false,{},common_failure("GovernancePhaseBoundaryMismatch").error};boundary=phase->to_tick;}
    if(tick!=boundary)return {false,{},common_failure("GovernanceWorkCompletionTickMismatch").error};
    const auto* actor=common.state().actors.find(executor);if(!actor||!actor->alive)return {false,{},common_failure("GovernanceExecutorUnavailable",executor).error};
    const auto xp=growth_skill_xp(actor->skills,"rites"),level=growth_skill_level(xp);
    if(xp<0||level<0||level>4)return {false,{},common_failure("GovernanceRitesQualificationUnknown",executor).error};
    return {true,{rule.id,rule.version,executor,tick,xp,level,rule.base_support_gain+level,rule.support_cap},{}};
}
Status validate_governance_outcome_plan(const AppeasementRule& rule,const GovernanceOutcomePlan& plan){
    auto valid=validate_appeasement_rule(rule);if(!valid.ok)return valid;
    if(plan.rule_id!=rule.id||plan.rule_version!=rule.version||plan.executor_actor_id.empty()||plan.work_completed_tick<0||plan.rites_xp<0||plan.rites_level!=growth_skill_level(plan.rites_xp)||plan.rites_level<0||plan.rites_level>4||plan.support_delta!=rule.base_support_gain+plan.rites_level||plan.support_cap!=rule.support_cap)return common_failure("InvalidGovernanceOutcomePlan",plan.rule_id);
    return common_success();
}
}
