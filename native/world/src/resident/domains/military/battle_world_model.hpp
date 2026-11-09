#include "resident/domains/military/siege_model.hpp"
// 军事接敌及逐轮永久账；共用人产由原MilitaryState权威保存，回执不反向覆盖现世界。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
struct MilitaryActorChange { StableId actor_id,status,captor_army_id;std::optional<StableId> garrison_id;StableId army_id;FieldOrder field_order; };
struct MilitaryMotionSource { StableId actor_id,action_id,plan_id;bool stationary=false;Location location;std::optional<Integer> route_index;FieldOrder field_order; };
struct MilitaryContactFact { StableId id,battlefield_id,objective_id;Integer tick=0;std::vector<StableId> unit_ids,objective_unit_ids;std::vector<std::pair<StableId,Integer>> side_by_army;std::vector<std::pair<StableId,StableId>> army_order_ids;std::vector<std::pair<StableId,Location>> army_locations;std::optional<Integer> time_num,time_den;std::optional<std::vector<std::pair<StableId,MilitaryMotionSource>>> motion_sources;std::optional<StableId> siege_id,node_id,battle_id;FieldOrder objective_field_order,field_order; };
struct MilitaryBattleFact { StableId id,phase="completed";Integer tick=0,start_tick=0,round=0;StableId session_id;std::vector<StableId> army_ids,unit_ids;std::optional<MilitaryBattleResult> result;StableId source_id,contact_source_id;Integer winner_side=-1;bool final=false;FieldOrder field_order; };
struct MilitaryRosterReceipt { StableId unit_id;std::vector<StableId> fit,wounded,dead,captured,deserted,member_ids;Integer morale=0;std::vector<std::pair<StableId,StableId>> captors;FieldOrder field_order; };
struct MilitaryCasualtyFact { StableId id,battle_id;Integer tick=0;std::vector<MilitaryRosterReceipt> units;std::vector<MilitaryActorChange> actor_changes;FieldOrder field_order; };
struct MilitaryBattlePlan { StableId battle_id;MilitaryOrders orders;bool orders_valid=true;FieldOrder field_order; };
struct MilitaryBattleClose { StableId battle_id;FieldOrder field_order; };
struct SiegeStart { StableId campaign_id;FieldOrder field_order; };
struct SiegeJoin { StableId siege_id,campaign_id;FieldOrder field_order; };
struct SiegePlan { StableId siege_id;SiegeOrders orders;bool orders_valid=true;FieldOrder field_order; };
struct SiegeStrategy { StableId siege_id;SiegeStrategicCommand command;bool command_valid=true;FieldOrder field_order; };
struct SiegeClose { StableId siege_id;FieldOrder field_order; };
struct MilitaryBattleStep { std::vector<StableId> completed_battle_ids,round_fact_ids;std::vector<MilitaryActorChange> actor_changes;
 // 每次原settle唯一真实casualty，保与本次actor_changes各组相同的执行顺序。
 std::vector<StableId> casualty_source_ids; };
}
