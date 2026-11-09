// Transient, same-candidate consumers. No stored owner or replacement history.
#pragma once
#include "military_march_source.hpp"
#include "native_military_world_scope.hpp"
namespace hanmo::resident {
Status release_native_field_military_action_in_candidate(
    WorldBundle &, const NativeMilitaryWorldConstructionContext &,
    ExactPhaseWriteContext &, const StableId &actual_action);
Status adopt_native_field_formation_locations_in_candidate(
    WorldBundle &, const NativeMilitaryWorldConstructionContext &,
    ExactPhaseWriteContext &, const DynamicMilitaryActionFact2 &,
    Integer first_actual_location_sequence);
// Only the actual CampaignDepart command path. Calendar return requests are
// NOT departures and cannot be converted to this call by changing destination.
Outcome<StableId> start_native_field_campaign_departure_in_candidate(
    WorldBundle &, const NativeMilitaryWorldConstructionContext &,
    ExactPhaseWriteContext &, const GroupMovementContext &,
    const CurrentReadStampRead &, const MilitaryMarchAuthorSelection &,
    const StableId &actual_campaign, const StableId &issuing_actor);
} // namespace hanmo::resident
