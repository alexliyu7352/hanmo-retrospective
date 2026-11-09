#include "resident/kernel/world_timeline.hpp"
// 军籍适配器的人口接缝；普通来源不能通过此入口伪造人数、劳力或供养覆盖。
#pragma once
#include "resident/domains/military/population_military.hpp"
namespace hanmo::resident {
struct OriginalMilitaryPopulationDestination {
 SourceRef population_transfer;MomentOwnerSource original_military_source;
 WorldMoment occurred_at;StableId cohort_id;PopulationStatus status=PopulationStatus::serving;
};
// Actual count writer's destination mapping, proved through its real immutable
// movement/receipt. Assignment is preserved by that original writer, not read
// back from today's cohort. Legacy integer events do not become exact sources.
Outcome<OriginalMilitaryPopulationDestination> read_original_military_population_destination(
 const PopulationMilitaryReadContext&,const SourceRef& actual_population_transfer);
// 当前军事供养必须覆盖原来源的每个活口，所有工资仍沿既有义务结算。
Status validate_local_military_bindings(const PopulationMilitaryReadContext&,const PopulationContinuity&);
// 只证明明确整编政策的未来公库，或已双重落账的旧口粮产权史；不授予当前支付权限。
bool population_military_ration_owner_proven(const PopulationMilitaryReadContext&,const RecurringObligation&);
} // 调用者提供同一候选引用，任何人口写入仍受中央事务回滚约束。
