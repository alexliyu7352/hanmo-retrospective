// 舟战永久事实只记录同场来源/变化；船体、Owner、人货现值仍各只有原唯一权威账。
#pragma once
#include "naval_planning.hpp"
#include "naval_contact.hpp"
#include "naval_repair_model.hpp"
#include "naval_navigation_model.hpp"
namespace hanmo::resident {
// 接敌前原Travel几何输入，折线仍从原journey按route_index取，不保存平行路线。
struct NavalContactApproach {
 StableId actor_id,action_id,plan_id,map_id,origin;PointI start;
 Integer route_index=0,budget=0,travel_credit=0;bool stationary=false,moving=false;FieldOrder field_order;
};
// 对接原Common的唯一序号与原Travel锚点；局部六角位置不能替代世界位置。
struct NavalContactFact {
 StableId id,first_fleet_id,second_fleet_id,battle_id;Integer tick=0,time_num=0,time_den=1;
 WildernessLocation first_anchor,second_anchor;MilitaryMotionSource first_motion,second_motion;
 StableId observed_player_id;NavalContactApproach first_approach,second_approach;
 FieldOrder field_order;
};
struct NavalHullChange { StableId vessel_id;Integer before=0,after=0;FieldOrder field_order; };
// before/after是永久回合证据，不可当新船或新人口输入；冷读须重放同一已提交计划核对。
// 正式计划有独立共同事实；草案不入账，幂等重放不重抽 AI 或增加回合。
struct NavalPlanCommand { StableId battle_id,request_key;NavalTacticalOrders orders;FieldOrder field_order; };
struct NavalPlanFact {
 StableId id,battle_id,request_key,actor_id,army_id,scope,office_id,authority_source;
 Integer tick=0,round=0;NavalTacticalOrders orders;FieldOrder field_order;
};
struct NavalRoundFact {
 StableId id,battle_id;Integer round=0,start_tick=0,tick=0;
 NavalTacticalFrame before,after;NavalTacticalOrders orders;
 StableId rng_seed,rng_before,rng_after,casualty_source_id;std::vector<StableId> supply_source_ids;std::vector<NavalHullChange> hull_changes;
 std::vector<NavalCombatEvent> events;std::vector<NavalCaptureClaim> claims;
 FieldOrder field_order;
};
struct NavalCaptureCommand {
 StableId battle_id,round_source_id,request_key,vessel_id,captor_unit_id;
 // 获胜舰队全体的实际新名单；空船仅留锚，不暗搬原货物。
 std::vector<NavalVesselAssignment> assignments;
};
struct NavalCapturedCargo { StableId source_inventory_id,lot_id,item_id;Integer quantity=0;Owner from_owner,to_owner;FieldOrder field_order; };
struct NavalCaptureFact {
 StableId id,battle_id,round_source_id,request_key,actor_id,vessel_id,from_fleet_id,to_fleet_id,captor_unit_id,authority_source;
 Integer tick=0;Owner from_owner,to_owner;WildernessLocation anchor;
 std::vector<NavalVesselAssignment> before_assignments,after_assignments,from_before_assignments,from_after_assignments;
 std::vector<StableId> interrupted_action_ids;
 std::vector<StableId> released_vessel_ids,prisoner_ids,original_prisoner_ids;
 StableId casualty_source_id;std::vector<NavalCapturedCargo> cargo;std::vector<StableId> emptied_inventory_ids;FieldOrder field_order;
};
// 放弃只关闭本轮接管权，不改变原船主、人货或已结算兵损。
struct NavalClaimDeclineCommand { StableId battle_id,round_source_id,request_key,vessel_id,captor_unit_id; };
struct NavalClaimDeclineFact {
 StableId id,battle_id,round_source_id,request_key,actor_id,vessel_id,captor_unit_id,army_id,formation_source;
 Integer tick=0;FieldOrder field_order;
};
struct NavalContinueCommand { StableId battle_id,round_source_id,request_key; };
// 结算出口记录同一轮所有接管/放弃来源；结束战斗不等于沉船、缴获或强制返营。
struct NavalContinueFact {
 StableId id,battle_id,round_source_id,request_key,actor_id,reason;Integer tick=0;
 NavalTacticalFrame before,after;NavalTacticalOrders ai_orders;
 std::vector<StableId> capture_source_ids,decline_source_ids,unavailable_claim_unit_ids,interrupted_action_ids;
 FieldOrder field_order;
};
struct NavalBattleState {
 std::vector<NavalBattleRecord> battles;std::vector<NavalContactFact> contacts;
 std::vector<NavalPlanFact> plans;std::vector<NavalRoundFact> rounds;std::vector<NavalCaptureFact> captures;
 std::vector<NavalClaimDeclineFact> declines;std::vector<NavalContinueFact> continuations;FieldOrder field_order;
};
// 值语义保证World候选复制同时隔离全部战斗事实，不能用共享指针偷偷写穿旧世界。
struct NavalState : NavalAssetState { NavalBattleState battle;NavalRepairState repairs;FieldOrder field_order;NavalRepairChargeState repair_charges;NavalNavigationState navigation;NavalGunneryState gunnery; };
// 作者部署计划不保存第二份运行舰队；只约束既有军令的NPC下一步意图。
struct NavalFleetPlan { StableId actor_id,order_id;std::vector<StableId> vessel_ids;PointI station;Integer departure_food=0,departure_money=0; };
struct NavalCatalog : NavalAssetCatalog { std::optional<NavalBattlePolicy> combat_policy;std::vector<NavalFleetPlan> fleet_plans;std::optional<NavalRepairPolicy> repair_policy;std::optional<NavalRepairChargePolicy> repair_charge_policy;std::optional<NavalNavigationPolicy> navigation_policy; };
}
