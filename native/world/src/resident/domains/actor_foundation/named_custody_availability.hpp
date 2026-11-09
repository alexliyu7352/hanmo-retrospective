// 原具名俘获/关闭的唯一 availability 采用；军务不给布尔目标或自报前驱。
#pragma once
#include "military_availability.hpp"
namespace hanmo::resident {
Outcome<MilitaryAvailabilityApplication> apply_verified_named_custody_capture(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const SourceRef &actual_term,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
Outcome<MilitaryAvailabilityApplication> apply_verified_named_custody_release(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const SourceRef &actual_closure,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
} // namespace hanmo::resident
