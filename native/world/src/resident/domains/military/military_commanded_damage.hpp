// 原05§5.8整数公式纯报价；不抽随机、不授军权、不写人物或兵员。
#pragma once
#include "battle_model.hpp"
#include "military_commanded_damage_model.hpp"
namespace hanmo::resident {
Status validate_military_commanded_damage_rule(const MilitaryCommandedDamageRule&);
Outcome<StableId> military_commanded_skill(const StableId& troop_category);
Outcome<Integer> military_commanded_strength(Integer leadership,Integer skill_level,Integer training);
// 基础值不提前按目标人数裁掉，原完整倍率合并后才执行最终目标上限。
Outcome<MilitaryCommandedDamageQuote> military_commanded_damage_base(Integer attacking_fit,Integer defending_fit,Integer attack_leadership,Integer attack_level,Integer attack_training,Integer defense_leadership,Integer defense_level,Integer defense_training);
Outcome<MilitaryCommandedDamageQuote> military_commanded_guarded_damage_base(Integer attacking_fit,Integer defending_fit,Integer attack_leadership,Integer attack_level,Integer attack_training,Integer defense_leadership,Integer defense_level,Integer defense_training);
// 只核固定帧的有限形状；真实军权在早门、完整人物经验在晚门分别证明。
Status validate_military_commanded_damage_shape(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const MilitaryCommandedDamageState&);
Outcome<MilitaryCommandedDamageQuote> preview_military_commanded_attack_base(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& attacking_unit,const StableId& defending_unit);
// BA02真实固守仅增本轮防御强度25%；只由显式Military16战法执行选择。
Outcome<MilitaryCommandedDamageQuote> preview_military_commanded_guarded_attack_base(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& attacking,const StableId& defending);

}
