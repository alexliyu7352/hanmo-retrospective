// 既有围城的有限节点、材料、会战来源与战略结果；观察只借真实领域投影，不生成士兵或公物。
#pragma once
#include "battle_model.hpp"
#include "military_siege_engineering.hpp"
namespace hanmo::resident {
struct SiegeMaterial { StableId item_id;Integer quantity=0;FieldOrder field_order; };
struct SiegeNode { StableId id,kind,name,battlefield_id;Integer max_hp=0; };
struct SiegeEdge { StableId from,to,blocker_id; };
struct SiegeRuleDefinition { Integer hold_ticks=0,blockade_units=0,starvation_ticks=0,max_ticks=0,gate_damage=0,repair_amount=0,engine_bonus=0,engine_ticks=0,surrender_morale=0,wall_defense_bp=10000;StableId engine_item_id;std::vector<SiegeMaterial> engine_materials,repair_materials; };
struct SiegeTopology { StableId id,name,attacker_entry_id,defender_entry_id,inner_id;std::vector<SiegeNode> nodes;std::vector<SiegeEdge> edges;std::vector<StableId> supply_node_ids;SiegeRuleDefinition rules; };
struct SiegeEngineUse { StableId inventory_id,item_id;Integer quantity=1;FieldOrder field_order; };
struct SiegeOrder { StableId action;std::optional<StableId> node_id;std::optional<SiegeEngineUse> engine_use;FieldOrder field_order; };
using SiegeOrders=std::vector<std::pair<StableId,SiegeOrder>>;
struct SiegeEngineWork { StableId node_id;Integer remaining=0;FieldOrder field_order; };
struct SiegeBattleRequest { StableId source_id,node_id,battlefield_id;std::vector<StableId> unit_ids;std::vector<std::pair<StableId,StableId>> origin_nodes;FieldOrder field_order; };
struct SiegeResult { StableId kind;Integer winner_side=-1,tick=0;std::optional<StableId> terms,reason;FieldOrder field_order; };
struct SiegeEvent { StableId kind;std::optional<StableId> unit_id,node_id,stronghold_id,campaign_id,terms,defender_actor_id,reason;std::optional<Integer> damage,remaining_hp;std::optional<std::vector<StableId>> army_ids,battle_source_ids;FieldOrder field_order; };
struct SiegeRecord { StableId id,campaign_id,stronghold_id,topology_id,phase="active";Integer started_tick=0,last_tick=0,round=1;std::vector<std::pair<StableId,Integer>> armies;std::vector<std::pair<StableId,StableId>> unit_nodes;std::vector<std::pair<StableId,Integer>> node_hp;Integer inner_hold_ticks=0,unsupplied_ticks=0;bool blockaded=false;std::vector<std::pair<StableId,SiegeEngineWork>> engine_work;std::vector<std::pair<StableId,SiegeBattleRequest>> pending_nodes;SiegeOrders ai_plans;std::vector<StableId> seen_receipt_ids,withdrawn_army_ids;std::optional<SiegeResult> result;std::vector<SiegeEvent> last_events;bool plan_submitted=false;SiegeOrders pending_orders;FieldOrder field_order; };
struct SiegeObservedArmy { StableId id,organization_id,commander_actor_id,inventory_id;Location location; };
struct SiegeObservedUnit { StableId id,army_id;Integer fit_count=0,morale=0;bool available=false,engaged=false; };
struct SiegeFoodSource { StableId inventory_id,food_item_id,organization_id,recipient_army_id; };
// 纯规则仅需要这些固定回执字段；原完整永久事实仍由军事世界状态保存。
struct SiegeObservedReceipt { StableId id,kind,phase,source_id,army_id,stronghold_id,item_id,to_inventory_id,recipient_army_id;Integer tick=0,quantity=0,winner_side=-1;Owner recipient_owner; };
using SiegeGoods=std::vector<std::pair<StableId,std::vector<std::pair<StableId,Integer>>>>;
struct SiegeObservation { Integer tick=0;StableId world_map_id,controller_actor_id,target_settlement_id;PointI target_point;std::vector<SiegeObservedArmy> armies;std::vector<SiegeObservedUnit> units;SiegeGoods public_goods;std::vector<std::pair<StableId,std::vector<StableId>>> control_sets;Integer defender_food=0;std::vector<SiegeFoodSource> defender_food_sources;bool defender_commander_out=true;std::vector<SiegeObservedReceipt> receipts;std::vector<std::pair<StableId,std::vector<StableId>>> side_allies;StableId defender_actor_id;std::vector<StableId> authorized_actor_ids;Integer willingness=0;bool negotiated_integration=false;
 // 仅原同候选工程绑定器设置的临时资格；不进入旧SiegeRecord/codec。
 std::optional<MilitarySiegeEngineeringPreparation> engineering_preparation; };
struct SiegeOrderQuote { StableId action,node_id;std::vector<SiegeMaterial> costs;std::optional<SiegeEngineUse> engine_use;std::optional<Integer> damage;bool under_attack=false; };
struct SiegeAssetEffect { StableId kind,army_id,item_id;Integer quantity=0;StableId source_id; };
struct SiegeResolution { SiegeRecord siege;std::vector<SiegeAssetEffect> asset_effects;std::vector<SiegeBattleRequest> battle_requests;std::vector<SiegeEvent> events; };
struct SiegeStrategicCommand { StableId kind,army_id,terms;std::vector<StableId> unexpected_fields;FieldOrder field_order; };
struct SiegeCreateRequest { StableId id,campaign_id,stronghold_id;std::vector<StableId> attacker_army_ids,defender_army_ids; };
// 世界观察和军议入口复用原有效单位/守点谓词，不把节点会战占用当作守军消失。
bool siege_unit_available(const SiegeRecord&,const SiegeObservation&,const StableId& unit);
bool siege_side_at(const SiegeRecord&,const SiegeObservation&,const StableId& node,Integer side);
// 原纯规则逐项返回失败；材料占用只在本轮候选账，不另建持久预约。
Status validate_siege_topology(const SiegeTopology&,const MilitaryBattleCatalog&,const std::vector<StableId>& item_ids);
Status validate_siege(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&);
Outcome<SiegeRecord> create_siege(const SiegeCreateRequest&,const SiegeTopology&,const SiegeObservation&);
Outcome<SiegeOrderQuote> preview_siege_order(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const StableId& unit,const SiegeOrder&);
Status validate_siege_orders(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeOrders&);
Outcome<SiegeOrders> make_siege_ai(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&);
Outcome<SiegeResolution> resolve_siege(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeOrders&,Integer finish_tick);
Outcome<SiegeRecord> join_siege(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& army,Integer side,const StableId& receipt);
Outcome<SiegeRecord> finish_siege_battle(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& node,const StableId& receipt);
Outcome<SiegeResolution> strategic_siege(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeStrategicCommand&);
Outcome<SiegeResolution> terminate_siege(const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& reason);
}
