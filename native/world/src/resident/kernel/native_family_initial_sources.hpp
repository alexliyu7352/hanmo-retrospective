// 同一调用栈拥有原身份和初态年龄读包，不替代完整生命周期年检来源。
#pragma once
#include "original_calendar.hpp"
#include "resident/domains/family/family_origin_v2.hpp"
#include "resident/domains/family/life_age.hpp"
#include "world_read_stamp.hpp"

namespace hanmo::resident {
struct WorldBundle;
class NativeFamilyInitialSourcesLease final : public CheckpointAllocatedObject {
  WorldCurrentMomentRead clock_;
  OriginalCalendarRead calendar_;
  ProfileControlEligibilitySources control_;
  FamilyOriginV2Sources origins_;
  LifeAgeSources initial_age_;
  NativeFamilyInitialSourcesLease(const WorldBundle &, OriginalCalendarRead);

public:
  // 只有未选 Complete 时为空；已选而缺原根必须显式失败。
  static Outcome<std::unique_ptr<NativeFamilyInitialSourcesLease>>
  create(const WorldBundle &);
  NativeFamilyInitialSourcesLease(const NativeFamilyInitialSourcesLease &) =
      delete;
  NativeFamilyInitialSourcesLease &
  operator=(const NativeFamilyInitialSourcesLease &) = delete;
  const FamilyOriginV2Sources &origins() const noexcept { return origins_; }
  const LifeAgeSources &initial_age() const noexcept { return initial_age_; }
};
} // namespace hanmo::resident
