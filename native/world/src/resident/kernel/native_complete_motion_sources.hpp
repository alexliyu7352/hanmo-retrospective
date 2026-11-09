// 同候选个人陆路及明选 Field Army 的真实 owning lease；不补空商业、装卸或巡逻
// provider。
#pragma once
#include "native_complete_world_sources.hpp"
namespace hanmo::resident {
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_motion_read_scope(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeCompleteDomainPrerequisiteOwner &, const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_motion_write_scope(
    WorldBundle &, const NativeCompleteWorldCoreSources &,
    NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
    const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
} // namespace hanmo::resident
