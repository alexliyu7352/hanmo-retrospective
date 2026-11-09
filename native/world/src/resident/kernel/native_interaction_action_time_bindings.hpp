// 原交互行动的有限装配叶，只引用同一个 complete graph 已拥有的三个真实来源。
// 本叶不构造缺失的专业域，也不取得外层 complete owner 或改写领域状态。
#pragma once

#include "native_action_time_sources.hpp"

namespace hanmo::resident {
struct NativeCompleteWorldCharacterSources;

// 为 runtime 实际选中的交互原域创建拥有自身的转发适配器。
// 原 Family/Interaction 根由同一 complete lease 保持至适配器析构之后；
// 适配器及其 shared_ptr 不得逃逸该 lease。未选择交互时返回空登记集，
// 选择失配时失败；其他专业域仍由各自原构造器负责，不在此伪造来源。
Outcome<CheckpointVector<CompiledActionTimeOwnerBinding>>
make_native_interaction_action_time_bindings(
    const NativeCompleteWorldCharacterSources &);
} // namespace hanmo::resident
