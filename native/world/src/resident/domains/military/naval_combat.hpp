// 实船战斗候选：原近战兵损、船体伤损与接管根据同一轮提交，产权仍交World唯一事务。
#pragma once
#include "naval_tactics.hpp"
#include "naval_gunnery_model.hpp"
namespace hanmo::resident {
// 冲撞数值必须由具名作者显式提供；没有定义的船型不能凭空获得撞击能力。
struct NavalRamDefinition { StableId hull_id;Integer target_damage=0,self_damage=0; };
// 显式作者参数只缩放本轮实际兵损暴露；死亡/受伤比例直接来自原兵损，不另外掷暴击。
struct NavalCommanderLossRule { StableId kind;Integer exposure_bp=0; };
struct NavalCombatRules { StableId profile="boarding_ram_v1";std::vector<NavalRamDefinition> ram;std::optional<NavalCommanderLossRule> commander_losses;std::optional<NavalTacticalNavigationPolicy> tactical_navigation;std::optional<NavalGunneryPolicy> gunnery; };
struct NavalCombatEvent {
 StableId kind,unit_id,target_unit_id,code;
 Integer casualties=0,hull_damage=0,self_damage=0,hit_chance=0;FieldOrder field_order;
};
// 接管候选只来自本轮真实登船胜负或本人显式投降，不能由失能状态推导。
struct NavalCaptureClaim {
 StableId unit_id,captor_unit_id,captor_army_id,kind;Integer round=0;
 std::vector<StableId> survivor_ids;FieldOrder field_order;
};
struct NavalCombatResult {
 NavalAssetState assets;NavalTacticalFrame frame;
 StableId rng_seed,rng_state;
 std::vector<NavalTacticalEvent> maneuvers;std::vector<NavalCombatEvent> events;
 std::vector<NavalCaptureClaim> capture_claims;
};
// 只返回完整候选；晚随机失败也不写原船、原兵员、钱货或调用者随机状态。
Status validate_naval_combat_rules(const NavalCombatRules&,const NavalAssetCatalog&);
Outcome<NavalCombatResult> resolve_naval_combat(const NavalAssetState&,const NavalAssetCatalog&,const NavalCombatRules&,const MilitaryBattleCatalog&,const NavalTacticalFrame&,const NavalTacticalOrders&,const StableId& rng_seed,const StableId& rng_state,const MilitaryBattleRandom&,const NavalGunneryPreparation* =nullptr);
}
