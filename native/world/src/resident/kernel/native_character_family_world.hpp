// One same-candidate owner for original body, age, death history and Family
// people. External professional readers must outlive this owner; no outer
// factory is called.
#pragma once
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/born_character_source.hpp"
#include "resident/domains/family/family_continuation_people4.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include "resident/domains/family/native_continuation_sources.hpp"
#include "resident/domains/growth/native_work_practice_source.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
class NativeCompleteWorldSources;
struct WorldBundle;
class NativeMilitaryTrainingPeriodRead;
class MilitaryFormationSourceRead;
class MilitaryReturnSourceRead;
struct NativeCharacterFamilyDependencies {
  const GrowthFixedSpatialSources &spatial;
  const EnvironmentalSeaDeathSourceRead *sea = nullptr;
  const BornCharacterSourceRead *born = nullptr;
  const CraftJobReadContext *craft = nullptr;
  // Both borrow the actual original owners retained by the same complete
  // lease. The period adapter is the concrete original economic Opening read.
  const NativeMilitaryTrainingPeriodRead *military_supply_periods = nullptr;
  const MilitaryFormationSourceRead *military_formations = nullptr;
  const MilitaryReturnSourceRead *military_returns = nullptr;
  // Original optional relation owners, retained by the surrounding complete
  // graph. A selected relation never receives an invented empty context.
  const DiscipleControlRelationContext *disciple = nullptr;
  const BiologicalControlDecisionContext *biological = nullptr;
  const CollateralControlDecisionContext *collateral = nullptr;
  const BirthPlacementRead *birth_placement = nullptr;
  const BirthDueBornSourceRead *birth_results = nullptr;
  const FieldSegmentApplicationRead *field_application_clock = nullptr;
  const CharacterInjurySourcesStateV1 *injury_sources = nullptr;
  const CharacterAttributeAbsentSourcesReadV1 *attribute_absence = nullptr;
};
// 只给同候选 Complete 私有装配阶段补入真实关系来源；不是外部可变 setter。
struct NativeCharacterFamilyRelationSources {
  const DiscipleControlRelationContext *disciple = nullptr;
  const BiologicalControlDecisionContext *biological = nullptr;
  const CollateralControlDecisionContext *collateral = nullptr;
};
class NativeCharacterFamilyWorldRead final : public CheckpointAllocatedObject {
  friend class NativeCompleteWorldSources;
  const WorldBundle &world_;
  const CompleteExactTimeRuntime &runtime_;
  NativeCharacterFamilyDependencies dependencies_;
  WorldCurrentMomentRead clock_;
  OriginalCalendarRead calendar_;
  ProfileControlEligibilitySources control_;
  // The explicit original-Birth route owns the real source closure. These
  // precede every consumer. Access.people is bound once, only after people_
  // exists and before any source read or public return.
  std::optional<BirthArrangementAccess> birth_access_;
  std::optional<BornCharacterSourceContext> birth_context_;
  std::optional<NativeBornCharacterSourceRead> born_;
  LifeAgeSources ages_;
  std::optional<NativeLifeAgeExactRead> age_;
  NativeWorkPracticeContributionRead work_;
  std::unique_ptr<CharacterConditionWorldRead> body_;
  std::optional<FamilyLifeHistoryRead4> life_;
  std::optional<FamilyContinuationPeopleRead4> people_;
  std::optional<FamilyOriginV2Sources> origins_;
  std::optional<NativeContinuationOriginalSourceRead> continuation_;
  bool relations_bound_ = false;
  NativeCharacterFamilyWorldRead(const WorldBundle &,
                                 const CompleteExactTimeRuntime &,
                                 NativeCharacterFamilyDependencies,
                                 OriginalCalendarRead, bool original_birth,
                                 bool defer_relations);
  // 只有唯一 Complete owner 可持有这个尚未发布的中间对象。
  static Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
  create_for_complete_assembly(const WorldBundle &,
                               const CompleteExactTimeRuntime &,
                               NativeCharacterFamilyDependencies,
                               bool original_birth);
  // people 已有真实地址后只绑定一次，随后才允许运行完整公开校验。
  void bind_relation_sources(NativeCharacterFamilyRelationSources);
  // 唯一 Complete 友元的真实晚源闭合；不得向调用方暴露可变 Body。
  Status bind_original_named_custody(
      const NativeNamedCustodyAvailabilityRead &);
  static Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
  create_impl(const WorldBundle &, const CompleteExactTimeRuntime &,
              NativeCharacterFamilyDependencies, bool original_birth,
              bool defer_relations = false);

public:
  NativeCharacterFamilyWorldRead(const NativeCharacterFamilyWorldRead &) =
      delete;
  NativeCharacterFamilyWorldRead &
  operator=(const NativeCharacterFamilyWorldRead &) = delete;
  NativeCharacterFamilyWorldRead(NativeCharacterFamilyWorldRead &&) = delete;
  NativeCharacterFamilyWorldRead &
  operator=(NativeCharacterFamilyWorldRead &&) = delete;
  static Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
  create(const WorldBundle &, const CompleteExactTimeRuntime &,
         NativeCharacterFamilyDependencies);
  // Explicit Birth4 composition, not a pointer-selected profile or fallback.
  // dependencies.born must be null; the same owner supplies the original
  // pre-downstream reader to age, body and the actual Family people closure.
  static Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
  create_with_original_birth(const WorldBundle &,
                             const CompleteExactTimeRuntime &,
                             NativeCharacterFamilyDependencies);
  // Borrowed only while this owner and its World/providers remain alive.
  // A creation source is not the full BirthDue/S5/Care result source.
  const BornCharacterSourceContext *original_birth_context() const noexcept {
    return birth_context_ ? &*birth_context_ : nullptr;
  }
  const BornCharacterSourceRead *born() const noexcept {
    return dependencies_.born;
  }
  // Construction only binds real original sources. It does not capture P10 or
  // validate a candidate-only command event before its actual result is sealed.
  Status validate_bindings() const;
  Status validate_original_sources() const;
  const CharacterConditionWorldRead &body() const noexcept { return *body_; }
  const FamilyContinuationPeopleRead4 &people() const noexcept {
    return *people_;
  }
  const FamilyLifeHistoryRead4 &life() const noexcept { return *life_; }
  const ProfileControlEligibilitySources &control() const noexcept {
    return control_;
  }
  const NativeContinuationOriginalSourceRead *continuation() const noexcept {
    return continuation_ ? &*continuation_ : nullptr;
  }
  const WorkPracticeContributionRead &work_practice() const noexcept {
    return work_;
  }
  const MilitaryFormationSourceRead *military_formations() const noexcept {
    return dependencies_.military_formations;
  }
  const MilitaryReturnSourceRead *military_returns() const noexcept {
    return dependencies_.military_returns;
  }
  const NativeMilitaryTrainingPeriodRead *
  military_supply_periods() const noexcept {
    return dependencies_.military_supply_periods;
  }
  const LifeAgeSources &ages() const noexcept { return ages_; }
  const LifeAgeExactReadLease *life_age() const noexcept {
    return age_ ? &*age_ : nullptr;
  }
};
} // namespace hanmo::resident
