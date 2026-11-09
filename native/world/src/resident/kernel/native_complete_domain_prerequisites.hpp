// 中央有限拥有装配；只接已经具备原实现的域，不用空provider代表尚缺的业务。
#pragma once
#include "native_complete_world_sources.hpp"
namespace hanmo::resident {
Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>>
make_native_complete_domain_read_prerequisites(
    const NativeCompleteWorldCoreSources &);
Outcome<std::unique_ptr<NativeCompleteDomainPrerequisiteOwner>>
make_native_complete_domain_write_prerequisites(
    const NativeCompleteWorldCoreSources &, WorldBundle &,
    CompleteExactWorldBindingOwner &);
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_household_read_scope(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeCompleteDomainPrerequisiteOwner &, const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_household_write_scope(
    WorldBundle &, const NativeCompleteWorldCoreSources &,
    NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
    const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_political_read_scope(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeCompleteDomainPrerequisiteOwner &, const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_political_write_scope(
    WorldBundle &, const NativeCompleteWorldCoreSources &,
    NativeCompleteDomainPrerequisiteOwner &, CompleteExactWorldBindingOwner &,
    const WorldMoment &, ExactPhaseWriteContext *, const DynamicStepInput *);
} // namespace hanmo::resident
