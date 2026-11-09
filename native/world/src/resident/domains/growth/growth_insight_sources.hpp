// 本人见闻来源只读实际完成事实；不授身份、不重算训练或再发成长。
#pragma once
#include "resident/domains/military/military_refs.hpp"

namespace hanmo::resident {
struct GrowthCampTrainingExperience {
    StableId fact_id,actor_id,garrison_id,place_id,unit_id,scenario_id;
    WorldId world_id{};
    Integer started_tick=0,completed_tick=0;
};
// 前提：同一世界候选已经通过完整Common/Military验证。
// 本函数只选择本人真实参训的最早完成事实；教官任职来源不成为本人的任职。
Outcome<std::optional<GrowthCampTrainingExperience>> first_growth_camp_training_experience(
    const CommonState&,const MilitaryState&,const StableId& actor_id);
// 内部取得owner与冷验证读真实参与者；公开页仍调用原本人入口。
Outcome<std::optional<GrowthCampTrainingExperience>> read_verified_growth_camp_training_experience(
    const CommonState&,const MilitaryState&,const StableId& actor_id);
}
