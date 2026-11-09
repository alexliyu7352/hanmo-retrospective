// 同候选真实 S5 前序组合，不重造生产/家户/资金来源。
#pragma once
#include "native_complete_world_sources.hpp"
namespace hanmo::resident {
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_economic_read_scope(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeCompleteDomainPrerequisiteOwner &, const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_economic_write_scope(
    WorldBundle &, const NativeCompleteWorldCoreSources &,
    NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
    const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
} // namespace hanmo::resident
