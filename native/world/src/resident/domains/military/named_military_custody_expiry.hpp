// 期满只在原 S0 到期事件的完整候选内采用；不新建时间表或另一份在押集合。
#pragma once
#include "named_military_custody_release_read.hpp"
#include "resident/domains/actor_foundation/named_custody_availability.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
struct NamedMilitaryCustodyExpiryContext {
  const NamedMilitaryCustodyReleaseContext &sources;
  const CharacterConditionSources &conditions;
  const CurrentReadStampRead &reads;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
};
struct NamedMilitaryCustodyExpiryApplication {
  StableId closure_id;
  std::vector<BoundSpatialLocationWrite> location_writes;
  MilitaryAvailabilityApplication availability;
  bool already_applied = false;
};
// 只接受同根真实 term ID，不接受调用方给的落点、恢复布尔值或伪造的期限。
// 外层拥有原 phase_current 与完整 World 回滚；任何失败不能发布局部修改。
Outcome<NamedMilitaryCustodyExpiryApplication> expire_named_military_custody_in_candidate(
    CommonStore &, MilitaryState &, AuthoredActorFoundationState &, MovementExactState &,
    const NamedMilitaryCustodyExpiryContext &, const StableId &term_id);
} // namespace hanmo::resident
