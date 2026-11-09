// 旧military_schedule的同步纯提案合同；不持世界、军权、安装实例或命令执行入口。
#pragma once
#include "hanmo/projection_types.hpp"
namespace hanmo {
// kind仅由同库固定旧分支产生，params保Godot原类型且不接受输入命令模板。
struct MilitaryScheduleProposal { std::string actor_id,kind; detail::Value::Dict params; };
// 成功才有完整原序数组；临时scope不代表持久世界身份。
struct MilitaryScheduleBatch {
    std::string scope_token;
    std::int64_t evaluated_count=0;
    std::vector<MilitaryScheduleProposal> proposals;
};
// 有界严格JSON只读计算旧军政一步提案；失败丢弃全部局部结果，不影响后续调用。
Result<MilitaryScheduleBatch> military_schedule_proposals(const std::string& bytes);
} // 原Kernel继续唯一负责预览、子候选和正式发布。
