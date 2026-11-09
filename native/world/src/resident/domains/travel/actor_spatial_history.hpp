#pragma once
#include "actor_spatial_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "group_transport_registration.hpp"
#include "../actor_foundation/authored_origin.hpp"
namespace hanmo::resident {
struct MilitarySpatialHandoffState;
class NamedMilitaryCustodyPlacementRead;
class FieldBattleExitPlacementRead;
struct FamilyCatalog;
class BirthPlacementRead;
class MilitaryActionPlacementRead;
struct ActorStationaryLocationSpan {
  StableId actor_id;
  SourceRef accepted_presence_source;
  MomentSpan span;
  MovementAnchor actual_anchor;
  std::vector<MomentOwnerSource> original_sources;
  SourceRef actual_causal_position_source;
};
// Named service owns no carrier. Read the retained actual motion/registration
// chain, including later history; the current Common projection is never a span
// proof. A selected military handoff domain must supply its original state,
// even for cold reads.
Outcome<ActorStationaryLocationSpan> read_actor_stationary_location_span(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const SourceRef &accepted_presence_source, const MomentSpan &,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);
struct ActorHistoricalSpatialPoint {
  StableId actor_id;
  WorldMoment observed_at;
  SourceRef observation_source;
  MovementAnchor actual_anchor;
  std::vector<MomentOwnerSource> original_sources;
};
// The caller owns and validates the observation source's exact M. This reader
// uses its real Common append boundary to distinguish later same-M transitions.
Outcome<ActorHistoricalSpatialPoint> read_actor_spatial_location_before_source(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const SourceRef &accepted_presence_source,
    const SourceRef &actual_observation_source,
    const WorldMoment &actual_observation_at,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);

// The original adopting owner validates that this prefix was captured at its
// actual M. This finite read verifies the retained Common prefix and selects
// the latest real placement within it; it does not authorize a caller-selected
// cut.
using ActorSpatialActualFactPrefix = CommonActualFactPrefix;
Outcome<ActorSpatialLocationView>
read_actor_spatial_location_at_actual_fact_prefix(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const WorldMoment &actual_at,
    const ActorSpatialActualFactPrefix &original_adoption_prefix,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);

// FSL and other internal owner boundaries retain the actual immutable location
// sources already captured at their accepted M. Later same-M events are
// allowed; omitted earlier-M position changes reject the frozen head as stale.
// This read does not require or create a synthetic Common observation fact.
Outcome<ActorSpatialLocationView> read_actor_spatial_location_at_source_head(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const std::vector<ActualSpatialSourceRef> &frozen_actual_sources,
    const WorldMoment &actual_at,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);

// Pure comparison of two already adopted immutable heads at the same M. Both
// heads are first re-read through the complete actor history. Aliases resolve
// to the actual placement-producing Common source, not lexical IDs or a prefix
// ID.
Outcome<Integer> compare_actor_spatial_source_heads_at_moment(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const std::vector<ActualSpatialSourceRef> &first,
    const std::vector<ActualSpatialSourceRef> &second, const WorldMoment &,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);

struct ActorSpatialHeadChange {
  WorldMoment occurred_at;
  MovementAnchor actual_anchor;
  std::vector<ActualSpatialSourceRef> immutable_source_head;
};
// Actual immutable head changes in the open/closed causal interval (from,to].
// Does not emit continuous activity slices. Same-M aliases are resolved through
// their actual placement producer; returned canonical heads may name that
// producer instead of its contact/control alias.
Outcome<std::vector<ActorSpatialHeadChange>> read_actor_spatial_head_changes(
    const CommonStore &, const CommonCatalog &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const AuthoredActorFoundationState &, const ActorFoundationCatalog &,
    const SpatialCarrierSources &, const StableId &actor_id,
    const std::vector<ActualSpatialSourceRef> &from_head,
    const WorldMoment &from_at,
    const std::vector<ActualSpatialSourceRef> &to_head,
    const WorldMoment &to_at,
    const MilitarySpatialHandoffState *military_handoffs = nullptr,
    ExactArithmeticBudget budget = {},
    const FamilyCatalog *family_catalog = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);

// Field 尾借同 World 原入场/实际成员交接；没有 reader 时任何真实 Field
// 历史头均明确拒绝，不把暂停行军的旧 anchor 当在战静置位置。

// Compiled source mapping for this selected exact-history owner. This is a kind
// registry only; the history read still verifies the actual actor/source/M.
Outcome<Integer> actor_spatial_history_source_schema(const SourceRef &);

} // namespace hanmo::resident
