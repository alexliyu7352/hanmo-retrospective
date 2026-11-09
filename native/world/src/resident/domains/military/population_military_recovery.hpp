// 人口回队只消费真实军事恢复源；不接受数量命令或wounded→fit的任意字符串许可。
#pragma once
#include "population_military.hpp"
#include "military_recovery_rules.hpp"
namespace hanmo::resident {
// 完整原伤批/供给/安全/额度来源；仅c.phase_finish_tick严格单步时可读to。
Status validate_population_military_recovery_source(const PopulationMilitaryReadContext&,const StableId& military_receipt_id);
// LE消费前再核原token、原enrollment、精确wounded→fit及来源顺序，覆盖必须恰等原回队token。
Status validate_population_military_recovered_event(const PopulationMilitaryReadContext&,const PopulationMilitaryFact&);
// 在恢复后立即串原人口/经济候选，不再移动军事名单、不再消费口粮。
Outcome<PopulationMilitaryEffects> reconcile_population_military_recovery(PopulationMilitaryWriteContext&,Integer finish_tick);
}
