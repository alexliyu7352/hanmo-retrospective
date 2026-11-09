// 显式先天学习能力只选择原分段课程的新来源字段，不改变旧Family或ReceiptData版本。
#pragma once
#include "innate_study_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<bool> decode_innate_learning_profile(const BoundaryValue& scenario,const std::vector<StableId>& required_capabilities);
// 标题无目录入口与完整恢复同核依赖、唯一版本；绝不凭未知marker放宽旧Growth字节。
Status validate_innate_learning_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& module_versions);
}
