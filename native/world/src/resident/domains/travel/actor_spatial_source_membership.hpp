#pragma once
#include "actor_spatial_history.hpp"
namespace hanmo::resident {
// Only the immutable causal heads emitted by at_actual_fact_prefix. Supporting
// aliases, mutable prefixes and unrelated participation sources are not heads.
bool is_actor_spatial_head_source_kind(const SourceRef&)noexcept;
struct ActorSpatialHeadSourceReadContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const MovementExactState& movement;const SpatialCarrierState& carriers;
 const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actors;
 const SpatialCarrierSources& spatial;
 const FamilyCatalog* family_catalog=nullptr;const BirthPlacementRead* births=nullptr;
 ExactArithmeticBudget budget{};const MilitaryActionPlacementRead* military_actions=nullptr;
};
// Valid source for a different actor => false. Missing/malformed source, wrong
// schema or unselected source owner => error. This only classifies the original
// source's actual participants; at_source_head and stationary_span still prove
// the adopted M/cut and full service window. It grants no new participation.
Outcome<bool> read_actor_spatial_head_source_membership(
 const ActorSpatialHeadSourceReadContext&,const StableId& actor_id,
 const MomentOwnerSource& actual_head_source);
// Source-local occurrence, never an arbitrary interpolation time. A valid
// source belonging to another actor returns null; malformed or missing sources
// still fail. Basis occurrence is opened_at, even if it moved for longer.
Outcome<std::optional<WorldMoment>> read_actor_spatial_head_source_occurrence(
 const ActorSpatialHeadSourceReadContext&,const StableId& actor_id,
 const MomentOwnerSource& actual_head_source);
struct ActorSpatialLocationTransitionRead {
 StableId actor_id;MomentOwnerSource actual_source;WorldMoment occurred_at;
 ActorSpatialLocationView before,after;
 bool actual_position_changed=false;
};
// Real ordinary placement before/after this source's own Common append cut.
// This does not interpret Care permission or coverage. Same-position basis,
// hold or close is reported as unchanged. Initial insertion, active transport
// or tactical history needs its original specialized owner; never a projection
// fallback. No caller-supplied M or retained duplicate position is accepted.
Outcome<std::optional<ActorSpatialLocationTransitionRead>>
 read_actor_spatial_location_transition_at_source(
 const ActorSpatialHeadSourceReadContext&,const StableId& actor_id,
 const MomentOwnerSource& actual_head_source,
 const MilitarySpatialHandoffState* military_handoffs=nullptr);
// A shared participation pool may contain an older shared head and a newer
// actor-specific head. Validate every source, then select by its real occurrence
// and same-M Common append order (authored initial placement is ordinal zero).
// No source may postdate adoption. The selected head still passes the complete
// retained actor history; omitted earlier changes remain errors.
Outcome<ActorSpatialLocationView> read_actor_spatial_location_from_head_pool(
 const ActorSpatialHeadSourceReadContext&,const StableId& actor_id,
 const std::vector<MomentOwnerSource>& actual_source_pool,const WorldMoment& actual_from,
 const MilitarySpatialHandoffState* military_handoffs=nullptr);
}
