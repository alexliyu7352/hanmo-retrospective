// 只组合真实私人生活原路；其他已选 Director 策略仍须各自具名来源。
#pragma once
#include "native_complete_world_sources.hpp"
namespace hanmo::resident {
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_director_read_scope(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeCompleteDomainPrerequisiteOwner &, const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_director_write_scope(
    WorldBundle &, const NativeCompleteWorldCoreSources &,
    NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
    const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
} // namespace hanmo::resident
