// 同候选早期空间与晚关系的真实拥有体；未实现专业选择明确拒绝，不注入空来源。
#pragma once
#include "native_complete_world_sources.hpp"
namespace hanmo::resident {
// 同一私有 Complete 构造栈的真实 Core 一次绑定；不接受跨 World 或未知 owner。
Status bind_native_complete_family_original_core(
    NativeCompleteFamilyPrerequisiteOwner &, const NativeCompleteWorldCoreSources &);
Outcome<std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>>
make_native_complete_family_prerequisites(
    const NativeCompleteWorldEarlySources &);
}
