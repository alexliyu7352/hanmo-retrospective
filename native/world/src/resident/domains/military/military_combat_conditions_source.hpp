// 真实接敌前原条件读与M10永久入口复验；不把现在疲劳回填旧轮次。
#pragma once
#include "military_combat_conditions.hpp"
#include "military_conditions.hpp"
namespace hanmo::resident {
Outcome<MilitaryCombatConditionState> prepare_military_combat_condition_entry(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryBattleRecord&);
Status validate_military_combat_condition_entry(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryRoutEntryFact&,const MilitaryCombatConditionState&,const DynamicStepInput* phase=nullptr);
}
