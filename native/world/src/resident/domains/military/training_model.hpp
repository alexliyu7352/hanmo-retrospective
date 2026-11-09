// 驻军训练保存唯一实际课时、公共预约和军阵题面绑定；部队条件不使用人物疲劳替代。
#pragma once
#include "drill_model.hpp"
#include "resident/domains/travel/travel_model.hpp"
namespace hanmo::resident {
struct TrainingGroupSource { StableId kind="abstract_training",unit_id,troop_type_id,roster_sha256;std::vector<StableId> group_ids;FieldOrder field_order; };
struct TrainingDrillTerms { StableId kind="drill",challenge_id,mode;Integer tactics_xp=0;DrillSnapshot actor_snapshot;StableId source_sha256;TrainingGroupSource group_source;FieldOrder field_order; };
struct TrainingDrillInstance { StableId origin_seed,origin_state;FieldOrder origin_field_order;DrillState challenge;StableId phase="pending";Integer frozen_tick=-1;std::optional<Integer> performance_bp;FieldOrder field_order; };
struct MilitaryTrainingTerms { StableId actor_id,office_id,authority_source,unit_id,garrison_id,organization_id,ownership_id,place_id,commander_actor_id,troop_type_id;std::vector<StableId> fit_ids,participants;StableId skill_id;Integer skill_xp=0,skill_level=0,gain_total=0,training_before=0,fatigue_before=0,fatigue_per_tick=0;StableId payer_wallet_id,recipient_wallet_id;Integer duration=4,fee=150;std::optional<TrainingDrillTerms> drill;FieldOrder field_order; };
struct MilitaryTrainingLesson { Integer tick=0;StableId supply_fact_id;Integer fee_paid=0,training_before=0,training_after=0,fatigue_before=0,fatigue_after=0;FieldOrder field_order; };
struct MilitaryTrainingFact { StableId id,phase="started",actor_id;Integer start_tick=0,completed_tick=-1;bool success=false;MilitaryTrainingTerms terms;std::vector<MilitaryTrainingLesson> lessons;StableId code;std::optional<TrainingDrillInstance> drill;FieldOrder field_order; };
struct MilitaryTrainingAction { StableId action_id;MilitaryTrainingTerms training;FieldOrder field_order; };
struct MilitaryTrain { StableId office_id,unit_id;std::optional<StableId> challenge_mode;bool input_valid=true;FieldOrder field_order; };
struct MilitaryTrainCancel { StableId action_id;bool input_valid=true;FieldOrder field_order; };
struct MilitaryDrillCommand { StableId kind,action_id,group_id;std::optional<Integer> expected_step;std::optional<HexPoint> target;std::optional<DrillPending> expected_pending;bool input_valid=true;FieldOrder field_order; };
struct MilitaryTrainingReceipt { StableId unit_id;Integer provided_half_days=0,fee_paid=0,training_gain=0,released_unprovided_fee=0;FieldOrder field_order; };
struct MilitaryTrainingStep { bool completed=false,suspended=false,applied=false; };
// 行军收据引用同一旅行执行，不复制计划或共同人物状态。
struct MilitaryMarchUnit { StableId unit_id;Integer training=0,fatigue_before=0,fatigue_after=0;FieldOrder field_order; };
struct MilitaryMarchFact { StableId id,rule="terrain_march_v1";Integer tick=0;StableId army_id,formation_source,commander_actor_id,supply_fact_id;TerrainExecution movement;FieldOrder movement_field_order;std::optional<TerrainExecution> uncut_execution;FieldOrder uncut_field_order;StableId contact_source_id;std::optional<std::pair<Integer,Integer>> contact_time;FieldOrder contact_time_field_order;std::vector<MilitaryMarchUnit> units;FieldOrder field_order; };
struct MilitaryConditionProfile { Integer training=0,fatigue=0; };
}
