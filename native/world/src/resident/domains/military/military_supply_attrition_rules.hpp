// 原供给后同候选落匿名逃散账；入口不另消费军粮、不更换军官、不写Common人物。
#pragma once
#include "military_supply_attrition.hpp"
namespace hanmo::resident {
Status validate_military_supply_attrition_shape(const MilitarySupplyAttritionState&);
Status validate_military_supply_attrition_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const DynamicStepInput* phase=nullptr);
Outcome<std::vector<StableId>> step_military_supply_attrition(CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const DynamicStepInput&);
// 只在完整军事源已验后给原回合copy同步这一供给；extra morale是原Supply -8之后补足-10。
Status apply_military_supply_attrition_to_battle(const MilitaryState&,BattleUnit&,const StableId& supply_fact_id,Integer tick);
// 新野外策略明确借原目录；旧签名保持原士气下限。
Status apply_military_supply_attrition_to_battle(const MilitaryState&,const MilitaryCatalog&,BattleUnit&,const StableId& supply_fact_id,Integer tick);
}
