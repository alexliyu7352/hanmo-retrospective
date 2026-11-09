// 舰载普通攻击借原命中/兵损算法；新策略与真实本场技能来源缺一不可。
#pragma once
#include "naval_gunnery_model.hpp"
#include "naval_combat.hpp"
namespace hanmo::resident {
struct NavalGunneryQuote {
 StableId preparation_source_id,captain_actor_id;Integer naval_level=0,hull_damage=0;
 MilitaryAttackQuote personnel;
};
// 同步结算只返回本次实际损失增量；调用者并入原round一次伤亡分配与唯一船体账。
struct NavalGunneryResult {
 StableId rng_seed,rng_state;std::vector<NavalCombatEvent> events;
 std::map<StableId,Integer> hull_damage,casualties,morale;
};
Status validate_naval_gunnery_policy(const NavalGunneryPolicy&,const NavalAssetCatalog&);
// current frame可为后续回合，船长必须仍为本场该原绑定；成长只读准备来源，不读今日XP。
Outcome<NavalGunneryQuote> preview_naval_gunnery(const NavalAssetState&,const NavalAssetCatalog&,
 const NavalGunneryPolicy&,const MilitaryBattleCatalog&,const NavalGunneryPreparation&,
 const NavalTacticalFrame&,const StableId& unit_id,const StableId& target_unit_id);
// requests只取原完整同轮命令经同步移动后的攻击意图；每unit恰最多一次，稳定排序消耗原RNG。
Outcome<NavalGunneryResult> resolve_naval_gunnery(const NavalAssetState&,const NavalAssetCatalog&,
 const NavalGunneryPolicy&,const MilitaryBattleCatalog&,const NavalGunneryPreparation&,
 const NavalTacticalFrame&,const std::vector<std::pair<StableId,StableId>>& requests,
 const StableId& rng_seed,const StableId& rng_state,const MilitaryBattleRandom&);
}
