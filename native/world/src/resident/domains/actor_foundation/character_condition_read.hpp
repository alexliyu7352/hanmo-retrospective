// 同一Condition链的完整原源重放；不复制当前HP/疲劳表、不从当前值反推初态。
#pragma once
#include "life_condition.hpp"
#include "military_condition.hpp"
#include "model.hpp"
#include "rest_condition.hpp"
#include "travel_condition.hpp"
#include "work_condition.hpp"
namespace hanmo::resident {
// WorldBundle同候选借用组合，actual_now只能来自其受信phase clock/已发布Moment。
class NativeNamedCustodyAvailabilityRead;
class BornCharacterSourceRead;
struct CraftJobReadContext;
class ExactTrainingPeriodRead;
struct GrowthPermanentState;
struct GrowthPermanentCatalog;
struct GrowthRecoveryCatalog;
class WorkPracticeContributionRead;
struct ProfileControlEligibilitySources;
struct DirectorState;
struct DirectorCatalog;
struct PersonalCatalog;
struct FamilyCatalog;
struct EstateRestHistoryContext;
struct EstateState;
struct GrowthFixedSpatialSources;
class GrowthQuietRead;
struct InteractionState;
struct InteractionCatalog;
struct CharacterInjurySourcesStateV1;
struct ResourceGatherConditionContext;
struct PersonalState;
struct FieldBattleWorldState;
class FieldSegmentApplicationRead;
class CharacterAttributeAbsentSourcesReadV1;
struct CharacterConditionSources {
  const CommonStore &common;
  const AuthoredActorFoundationState &state;
  const ActorFoundationCatalog &catalog;
  WorldMoment actual_now;
  const GrowthState *growth = nullptr;
  const GrowthCatalog *growth_catalog = nullptr;
  const CommonCatalog *shared = nullptr;
  const ActionElapsedStore *elapsed = nullptr;
  const TimeScaleRef *scale = nullptr;
  const std::vector<ActionTimeRegistration> *action_registrations = nullptr;
  const AuthoredRestTimeRead *rest_time = nullptr;
  const MovementExactState *movement = nullptr;
  const FamilyDeathState *deaths = nullptr;
  const FamilyLifeCauseSources4 *life_sources = nullptr;
  const MilitaryState *military = nullptr;
  const MilitaryExactState *military_exact = nullptr;
  // Growth训练原PersonalSkill.required_skill_level字段的锁内只读来源；不归身体writer拥有。
  const PersonalCatalog *personal_catalog = nullptr;
  // Original complete creation rules; never reconstructed from the live actor.
  const FamilyCatalog *family_catalog = nullptr;
  const EstateRestHistoryContext *rest_estates = nullptr;
  const GrowthFixedSpatialSources *fixed_spatial = nullptr;
  const ProfileControlEligibilitySources *control = nullptr;
  const DirectorState *director = nullptr;
  const DirectorCatalog *director_catalog = nullptr;
  const GrowthQuietRead *fixed_quiet = nullptr;
  const EstateState *rest_estate_state = nullptr;
  const GrowthPermanentState *permanent_growth = nullptr;
  const GrowthPermanentCatalog *permanent_growth_catalog = nullptr;
  const WorkPracticeContributionRead *work_practice = nullptr;
  const BornCharacterSourceRead *born_sources = nullptr;
  const CraftJobReadContext *craft = nullptr;
  const ExactTrainingPeriodRead *military_supply_periods = nullptr;
  const MilitaryFormationSourceRead *military_formations = nullptr;
  const MilitaryReturnSourceRead *military_returns = nullptr;
  // Same original World roots for immutable nested dialogue-confirm provenance.
  const InteractionState *interaction = nullptr;
  const InteractionCatalog *interaction_catalog = nullptr;
  // Explicit source-proof order; old readers retain their legacy default.
  MilitaryCasualtyHistoryOrder military_history_order =
      MilitaryCasualtyHistoryOrder::legacy_serial;
  // 只由真实同World具名term/closure组合供给，未选必须空，选中缺源硬拒。
  const NativeNamedCustodyAvailabilityRead *named_custody_availability =
      nullptr;
  // source/1 terminal treatment only; borrowed from this actual
  // PreparedCatalog. No optional capability is made mandatory for unrelated
  // Condition reads.
  const GrowthRecoveryCatalog *injury_recovery_catalog = nullptr;
  // 同World借根，不构造副账；source1完整组合必须提供。
  const ResourceGatherConditionContext *gather_condition = nullptr;
  Integer selected_resource_sites =
      0; // actual prepared finite selection, not pointer presence
  const CharacterInjurySourcesStateV1 *injury_sources = nullptr;
  const PersonalState *injury_personal = nullptr;
  const FieldBattleWorldState *injury_field = nullptr;
  // Mandatory for the complete source1/Condition8 finite effect validator.
  // The concrete central owner proves same-World installed-source absence;
  // null never admits neutral effects. No persisted mask or second ledger.
  const CharacterAttributeAbsentSourcesReadV1 *attribute_absence = nullptr;
  // Same World original segment application scope; never a clock made from M.
  const FieldSegmentApplicationRead *injury_field_clock = nullptr;
};
struct CharacterConditionAtHead {
  ActorConditionHead head;
  FrozenBodyScalars body;
  WorldMoment observed_at;
};
// These identity-prefix operations do not validate all receipt payloads. The
// raw owning return retains ordinary strings; callers must provide lifetime
// accounting before claiming the returned capture is budget-owned.
Outcome<CharacterConditionAppliedPrefix>
capture_character_condition_applied_prefix(const CharacterConditionSources &);
Status validate_character_condition_applied_prefix(
    const CharacterConditionSources &, const CharacterConditionAppliedPrefix &);
// Selects the unique maximal actor head within this actual applied prefix,
// then uses the existing verified-prefix chain/source/scalar replay. It is not
// a whole-world reverse-coverage validator or a current-head substitute.
Outcome<CharacterConditionAtHead> read_character_condition_at_applied_prefix(
    const CharacterConditionSources &, const StableId &actor_id,
    const CharacterConditionAppliedPrefix &,
    const WorldMoment &actual_boundary);
Status
validate_complete_character_conditions(const CharacterConditionSources &);
Outcome<CharacterConditionAtHead>
read_character_condition_at_head(const CharacterConditionSources &,
                                 const ActorConditionHead &,
                                 const WorldMoment &actual_boundary);
// 对实际已应用来源取该时刻全部前驱；历史业务已冻结head时仍用at_head反验。
Outcome<CharacterConditionAtHead>
read_character_condition_at_moment(const CharacterConditionSources &,
                                   const StableId &actor_id,
                                   const WorldMoment &actual_boundary);
Outcome<CharacterConditionAtHead>
read_current_character_condition(const CharacterConditionSources &,
                                 const StableId &actor_id);
// Current acceptance capture only: both roots must be the actual candidate and
// its trusted clock must equal sources.actual_now. Reuses the original verified
// prefix, including every applied cause, without whole-world reverse coverage.
// The returned pointer is the actual current row, not a copied source DTO or
// historical Common-cut proof. Retain its value in the real accepting source
// before any candidate mutation, rollback, root replacement or phase change.
// Original source callbacks must remain acyclic; full publication gates remain
// mandatory. This borrowed result does not close replay-allocation accounting.
Outcome<const ActorConditionHead *>
read_verified_current_character_condition_head(
    const CharacterConditionSources &, const CommonStore &actual_common,
    const AuthoredActorFoundationState &actual_foundation,
    const StableId &actor_id, const CurrentMomentRead &actual_clock);
} // namespace hanmo::resident
namespace hanmo::resident {
// 单条变更的历史payload只借原表；先核完整唯一cause链，不把今日head当当次结果。
Outcome<const ConditionReceiptV2 *>
read_verified_character_condition_change(const CharacterConditionSources &,
                                         const ConditionReceiptRefV2 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Original actor prefix and each actual cause, without requiring that unrelated
// actors in a just-created multi-person source have already been consumed.
// This is not the whole-world reverse-coverage validation used at publication.
Outcome<const ConditionReceiptV2 *>
read_character_condition_receipt_provenance(const CharacterConditionSources &,
                                            const ConditionReceiptRefV2 &);
} // namespace hanmo::resident
