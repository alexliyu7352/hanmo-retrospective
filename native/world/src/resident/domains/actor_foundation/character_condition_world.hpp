// Same-WorldBundle owning read lease; no copied body state or cached success.
#pragma once
#include "character_condition_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/exact_time_profile.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeActorSocialOccupancyRead;
class MilitaryFormationSourceRead;
class MilitaryReturnSourceRead;
class NativeCharacterFamilyWorldRead;
struct CharacterConditionWorldDependencies {
  // Borrowed from the same owning complete-world lease. These real domain
  // readers are not command fields and have no permissive substitutes.
  const MilitaryExactState *military_exact = nullptr;
  const NaturalAgeDeathSourceRead *natural_age = nullptr;
  const EnvironmentalSeaDeathSourceRead *environmental_sea = nullptr;
  const GrowthFixedSpatialSources *fixed_spatial = nullptr;
  const NativeActorSocialOccupancyRead *social_occupancy = nullptr;
  const WorkPracticeContributionRead *work_practice = nullptr;
  const BornCharacterSourceRead *born_sources = nullptr;
  const CraftJobReadContext *craft = nullptr;
  const ExactTrainingPeriodRead *military_supply_periods = nullptr;
  const MilitaryFormationSourceRead *military_formations = nullptr;
  const MilitaryReturnSourceRead *military_returns = nullptr;
  // Pure same-World segment application clock, owned before this body lease.
  const FieldSegmentApplicationRead *field_application_clock = nullptr;
  // Real World optional root and early-owned finite absence proof.
  const CharacterInjurySourcesStateV1 *injury_sources = nullptr;
  const CharacterAttributeAbsentSourcesReadV1 *attribute_absence = nullptr;
};
class CharacterConditionWorldRead {
  friend class NativeCharacterFamilyWorldRead;
  struct Impl;
  CheckpointMemoryCharge object_memory_;
  std::unique_ptr<Impl> impl_;
  CharacterConditionWorldRead(CheckpointMemoryCharge, std::unique_ptr<Impl>);
  // 只供同 Complete 构造栈补入真 Condition6 原源，一次绑定后才发布。
  Status bind_original_named_custody(
      const NativeNamedCustodyAvailabilityRead &);

public:
  ~CharacterConditionWorldRead();
  static Outcome<std::unique_ptr<CharacterConditionWorldRead>>
  create(const WorldBundle &, const CompleteExactTimeRuntime &,
         CharacterConditionWorldDependencies);
  // Refreshes the actual phase clock on every call. A returned borrow must not
  // survive a writer, rollback, clone, or change of the current phase.
  Outcome<CharacterConditionSources> sources() const;
  Status validate() const;
};
} // namespace hanmo::resident
