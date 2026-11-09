#pragma once
#include "household_transport_registration.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
// C21.5 is a projection of the accepted Plan, original work/retail/cargo and
// actual S1 registration. It owns no second transport, population or balance.
struct HouseholdPurchaseTransportContext {
 const HouseholdPurchaseContext& purchase;const SpatialCarrierState& carriers;
 const MovementExactState& movement;const TransportRegistrationState& registrations;
 const GroupMovementExecutionState& executions;const SpatialCarrierSources& spatial;
};
struct HouseholdPurchaseTransportRead {
 StableId plan_id;std::optional<StableId> purchase_id;StableId phase,household_id,executor_cohort_id,population_selection_ref;
 SourceRef accepted_policy_ref;Integer policy_revision=0;SourceRef policy_acceptance_source;
 MovementAnchor origin_anchor;std::vector<MovementAnchor> permitted_destinations;MomentSpan finite_window;
 std::vector<CargoSelection> cargo_lot_refs;StableId custody_ref;SourceRef funding_source_ref;
 std::vector<SourceRef> original_phase_sources;
};
Outcome<HouseholdPurchaseTransportRead> read_household_purchase_transport(
 const HouseholdPurchaseTransportContext&,const TransportRegistrationSource&,
 const TransportUnitState&,const HouseholdPolicyGroupInitiator&,const SubjectRef&,const WorldMoment&);
// Cold/payment adoption uses the real S1 opening cut. It never reads today's
// purchase phase or today's cargo location as the old journey's authorization.
Status validate_household_transport_phase_adoption(const HouseholdPurchaseTransportContext&,
 const GroupMovementJourney&);
// Route preparation still belongs to S1. It must compare its actual terminal
// anchor to this original allowed set, not merely compare settlement names.
Status validate_household_transport_journey_request(const HouseholdPurchaseTransportContext&,
 const StartGroupJourney&,const TransportUnitState&,const TransportManifestRead&,
 const MovementAnchor& actual_prepared_destination);
}
