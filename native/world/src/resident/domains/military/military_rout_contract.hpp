// Military10只允许明确两轮受阻规则；原1..9不开新尾，也不让作者自定即时归俘。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryRoutRetreatRule {StableId profile="two_blocked_rounds_v1";Integer blocked_rounds_before_capture=2;};
// 场景只选择原有限战场；不复制强据点/军令或改变地图权源。
struct MilitaryRoutRetreatPolicy {MilitaryRoutRetreatRule rule;std::vector<StableId> battlefield_ids;};
inline constexpr const char* military_rout_module="military_rout_retreat";
inline constexpr std::uint32_t military_rout_checkpoint_feature=8388608u;
struct MilitaryRoutProgress {StableId unit_id;Integer started_round=0,blocked_rounds=0;FieldOrder field_order;};
}
