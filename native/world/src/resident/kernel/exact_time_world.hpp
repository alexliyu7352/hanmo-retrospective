// 同一WorldBundle的精确首链接头；调用者仍为原submit/run，不提供平行世界或driver。
#pragma once
#include "world_timeline.hpp"
namespace hanmo::resident {
struct WorldBundle;struct AdvanceDetails;
// 显式空macro首profile严格拒绝所有尚需宏观规则的已选策略/对象，不隐藏旧完整世界。
Status validate_exact_rest_world_scope(const WorldBundle&);
// 完整配对状态与原Action/owner源反验；没有任何provider默认成功。
Status validate_exact_time_world(const WorldBundle&);
// Ready内部开期只改变实际协调状态；此首profile没有已选供给provider，不生成空供给回执。
Status open_exact_period_in_candidate(WorldBundle&);
// 纯预览不分ID或开期，完整精确上界受原一次许可限制。
Outcome<WorldMoment> prepare_exact_advance_target(const WorldBundle&,const std::optional<WorldMoment>& requested_to);
// 取请求/下一宏观界/原Action真实due之最早点；最后正片和closing统一候选。
Outcome<AdvanceDetails> advance_exact_world(WorldBundle&,const std::optional<WorldMoment>& requested_to);
}
