// 批次以真实伤亡事实和同事实前名册证明；当前wounded池或自存起始人数都不是其替代。
#pragma once
#include "military_recovery_model.hpp"
#include "military_roster_history.hpp"
namespace hanmo::resident {
// access仅借原Military名册重放，不能回调本恢复总验；恢复事件在原名册叶只施加严格前后变化。
Outcome<MilitaryWoundBatch> query_military_wound_batch(
 const CommonStore&,const MilitaryState&,const MilitaryAccess&,
 const StableId& casualty_fact_id,const StableId& unit_id);
// 只统计该批次已实际提交的合格半日及回队来源；边界必须是真Common事实，空仅当前。
Outcome<MilitaryRecoveryBatchProgress> query_military_recovery_progress(
 const CommonStore&,const MilitaryRecoveryState&,const MilitaryWoundBatch&,
 const StableId& before_fact_id={});
// preclock限定单步上限，供原人口伤员回队同候选使用，非任意未来查询。
Outcome<MilitaryRecoveryBatchProgress> query_military_recovery_progress(
 const CommonStore&,const MilitaryRecoveryState&,const MilitaryWoundBatch&,const StableId& before_fact_id,const DynamicStepInput&);
// 只核整数额度与原批次身份；可恢复人员名单须另经同候选安全驻地/伤员与具名人物伤势读。
Outcome<std::vector<StableId>> quote_military_recovery_members(
 const MilitaryRecoveryBatchProgress&,const std::vector<StableId>& eligible_wounded_ids,
 Integer qualified_half_days_after);
}
