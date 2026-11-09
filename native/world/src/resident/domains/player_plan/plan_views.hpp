// 预测只合成当前已知估计；原任务/课程owner提供真截止，未知耗时绝不填零。
#pragma once
#include "plan_model.hpp"
#include <functional>
namespace hanmo::resident {
struct TaskState;struct TaskCatalog;struct GrowthState;
struct PlayerPlanEstimate { std::optional<Integer> remaining_ticks;StableId source,unknown_reason; };
struct PlayerKnownCommitment { PlayerCommitmentRef ref;StableId status;std::optional<Integer> due_tick;StableId required_boundary="completion",place_id,label;std::optional<Integer> estimated_completion_tick; };
struct PlayerPlanProjectedStep { StableId step_id;std::optional<Integer> start_tick,completion_tick;StableId estimate_source,unknown_reason; };
struct PlayerPlanConflict { StableId step_id;PlayerCommitmentRef commitment;Integer due_tick=0;std::optional<Integer> estimated_completion_tick;bool known_conflict=false,unknown=false; };
struct PlayerPlanProjection { std::vector<PlayerPlanProjectedStep> steps;std::vector<PlayerKnownCommitment> commitments;std::vector<PlayerPlanConflict> conflicts; };
using PlayerPlanEstimateRead=std::function<PlayerPlanEstimate(const PlayerPlanStep&)>;
Outcome<std::vector<PlayerKnownCommitment>> known_player_commitments(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const GrowthState&,const StableId& actor_id);
Outcome<PlayerPlanProjection> project_player_plan(const PlayerPlanState&,Integer tick,const std::vector<PlayerKnownCommitment>&,const PlayerPlanEstimateRead&);
BoundaryValue encode_player_plan_view(const PlayerPlanState&,const PlayerPlanProjection&);
} // 不因风险提示禁止合法动作，不替代原领域截止与履约结果。
