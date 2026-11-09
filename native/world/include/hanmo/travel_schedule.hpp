// 旧travel_schedule的同步纯提案合同；没有世界、ECS、安装或执行命令权限。
#pragma once
#include "hanmo/projection_types.hpp"
namespace hanmo {
// 原travel形状的纯值数据，Bridge另机械添加kind及params包装。
struct TravelScheduleProposal { std::string actor_id,destination; std::int64_t next_index=0; };
// 只有完整成功批次可用；scope仅回显调用者的临时同步标记，不是世界身份。
struct TravelScheduleBatch {
    std::string scope_token;
    std::int64_t evaluated_count=0;
    std::vector<TravelScheduleProposal> proposals;
};
// 从有界严格JSON执行原行程过滤及一次目的地推进；失败返回空结果且不留状态。
Result<TravelScheduleBatch> travel_schedule_proposals(const std::string& bytes);
} // 调用者仍负责原preview、begin与候选世界发布。
