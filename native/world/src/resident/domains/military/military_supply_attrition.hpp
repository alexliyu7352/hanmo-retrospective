// 连续缺粮只由原半日供给和当时军籍证明；此纯读不制造战斗伤亡或自动处置具名军官。
#pragma once
#include "military_roster_history.hpp"
#include "military_supply_attrition_model.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct MilitaryUnitSupplyRun {
 WorldIdentity source_identity;
 StableId unit_id,current_supply_id,previous_supply_id;
 Integer observed_tick=0,consecutive_shortage_half_days=0,fit_count=0,desertion_quota=0;
 std::vector<StableId> shortage_supply_ids,fit_ids,eligible_fit_ids;
};
// 精确current_supply_id必须是本单位实际受给头；军团编散不以今天所在处改写旧供给。
// 无连续缺粮的合法读返回quota0；坏源/缺原人口或名册读明确错误。
Outcome<MilitaryUnitSupplyRun> query_military_unit_supply_run(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const MilitarySupplyAttritionPolicy&,const StableId& unit_id,const StableId& current_supply_id,
 const DynamicStepInput* phase=nullptr);
}
