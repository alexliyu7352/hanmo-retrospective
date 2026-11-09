// 人口逃散只消费Military9真实供给流失源；不接受任意fit→deserted许可。
#pragma once
#include "population_military.hpp"
#include "military_supply_attrition_rules.hpp"
namespace hanmo::resident {
Status validate_population_military_attrition_source(const PopulationMilitaryReadContext&,const StableId& military_receipt_id);
Status validate_population_military_deserted_event(const PopulationMilitaryReadContext&,const PopulationMilitaryFact&);
// 同半日原供给已入账；本桥仅交付原token和劳力转移，不再次发粮。
Outcome<PopulationMilitaryEffects> reconcile_population_military_attrition(PopulationMilitaryWriteContext&,Integer finish_tick);
}
