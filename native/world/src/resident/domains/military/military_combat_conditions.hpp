// 原03§3.3四档与05§5.8攻击疲劳系数；只报价和校验，不抽随机或改人员。
#pragma once
#include "battle_model.hpp"
#include "military_combat_conditions_model.hpp"
namespace hanmo::resident {
Status validate_military_combat_condition_rule(const MilitaryCombatConditionRule&);
Outcome<Integer> military_combat_fatigue_efficiency(Integer fatigue);
Status validate_military_combat_condition_shape(const MilitaryBattleRecord&,const MilitaryCombatConditionState&);
// 显式快照外为空即旧中性；快照存在时必须精确匹配该unit，缺行不是默认100%。
Outcome<Integer> military_combat_attack_efficiency(const MilitaryBattleRecord&,const StableId& unit_id);
}
