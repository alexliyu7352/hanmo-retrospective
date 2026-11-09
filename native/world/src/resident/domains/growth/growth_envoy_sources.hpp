// 本人往返交涉人物卡只读原 Task / 政治报告事实，不创建另一份完成账。
#pragma once
#include "resident/domains/tasks/envoy_task_state.hpp"

namespace hanmo::resident {
struct GrowthEnvoyRoundTripEvidence {
    StableId actor_id,task_id,contract_id,acceptance_fact_id,mission_authority_fact_id,proposal_id;
    StableId delivery_fact_id,presentation_fact_id,completion_fact_id,report_fact_id,scenario_id;
    WorldId world_id{};
    Integer accepted_tick=0,delivered_tick=0,completed_tick=0;
};
// 前提：输入属于同一已完整验证的世界；read 必须借该世界的真实呈递及返报来源。
Outcome<std::optional<GrowthEnvoyRoundTripEvidence>> first_growth_envoy_round_trip(
    const CommonState&,const EnvoyTaskState&,const TaskEnvoyRead&,const StableId& actor_id);
// 仅S4内部canonical取得/冷验证使用；读真实任意人物成果，公开页另核观察者。
Outcome<std::optional<GrowthEnvoyRoundTripEvidence>> read_verified_growth_envoy_round_trip(
    const CommonState&,const EnvoyTaskState&,const TaskEnvoyRead&,const StableId& actor_id);
}
