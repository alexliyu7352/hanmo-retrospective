// 舟战计划遵循同一冻结回合：AI先存，玩家只交本人权限内的部队，草案不喂给AI。
#pragma once
#include "naval_combat.hpp"
namespace hanmo::resident {
struct NavalBattlePolicy {
 StableId kind="boarding_ram_v1",battlefield_id;Integer max_rounds=60;
 bool allow_withdrawal=true;Integer ai_surrender_morale=0;
 NavalCombatRules combat;
 // 空值严格保旧AI；具名工法只调整公开局勢下的接近意图，不改同步移动或战果。
 StableId ai_maneuver_policy;
};
struct NavalBattleRecord {
 NavalTacticalFrame frame;NavalTacticalOrders ai_orders,pending_orders;
 StableId rng_seed="0",rng_state="0";Integer created_tick=0,last_tick=0,completed_tick=-1;
 StableId contact_source_id,settlement_source_id,pending_plan_source;std::vector<StableId> round_source_ids;FieldOrder field_order;
};
// 只核政策和既有实船/地形引用，不修改作者或为缺失船型补默认能力。
Status validate_naval_battle_policy(const NavalBattlePolicy&,const NavalAssetCatalog&,const MilitaryBattleCatalog&);
Outcome<NavalTacticalOrders> make_naval_ai(const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalTacticalFrame&,const NavalGunneryPreparation* =nullptr);
// 提交后不能追加/重交本轮；完整序列按unit ID稳定合并，公共回执幂等由World处理。
Outcome<NavalBattleRecord> submit_naval_plan(const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord&,const StableId& actor,const NavalTacticalOrders&,const NavalGunneryPreparation* =nullptr);
Outcome<NavalTacticalOrders> frozen_naval_orders(const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord&,const NavalGunneryPreparation* =nullptr);
}
