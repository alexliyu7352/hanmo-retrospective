// C9.10 / V420. Actual Care changes the original dependency relation at the
// actual handover place. S4 owns H, acceptance, coverage and its completion.
#pragma once
#include "travel_dependent_state.hpp"
#include "actor_spatial_history.hpp"
#include "resident/domains/family/care_handover_execution.hpp"
namespace hanmo::resident {
struct CareDependentPlacementRead {
 const CareDependentBindingChange* original=nullptr;
 MomentOwnerSource placement_result;
};
struct CareDependentPlacementContext {
 const CommonStore& common;const CommonCatalog& catalog;
 const MovementExactState& movement;const TravelDependentState& dependents;
 const SpatialCarrierState& carriers;const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actors;
 const SpatialCarrierSources& spatial;const CareHandoverSourceContext& handovers;
 const FamilyCatalog* family_catalog=nullptr;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const BirthPlacementRead* births=nullptr;
 const MilitaryActionPlacementRead* military_actions=nullptr;
 Integer selected_version=2;ExactArithmeticBudget budget{};
};
class NativeCareDependentPlacementRead final {
 CareDependentPlacementContext context_;
public:
 explicit NativeCareDependentPlacementRead(CareDependentPlacementContext x):context_(x){}
 bool bound_to(const CommonStore&)const noexcept;
 bool bound_to(const CommonStore&,const TravelDependentState&)const noexcept;
 // Source-local H/acceptance plus the actual S1 adoption cut and independently
 // reconstructed original positions. Allows the real same-candidate pending
 // S4 Completed; publication additionally requires validate_stable_sources.
 Outcome<CareDependentPlacementRead> read_placement_result(const MomentOwnerSource&)const;
 Status validate_stable_sources()const;
};
// Full original two-root closure also supports Care-only worlds with authored
// children. Missing Birth is legal only when no actual Birth result needs it.
Status validate_travel_dependents(const CommonStore&,const TravelDependentState&,
 Integer selected_version,const BirthPlacementRead* births,
 const NativeCareDependentPlacementRead* care_handovers,ExactArithmeticBudget={});
struct CareDependentPlacementApplication {MomentOwnerSource placement_result;bool already_applied=false;};
// The currently concrete branch is a true, co-located Settlement handover.
// No desired place, Location payload, dependency or zero-cost claim is accepted
// from a caller. Transport/custody results require their original owner writer.
// This branch preserves Location and does not manufacture a spatial mutation
// or source head. Its full binding before/after and place live only in S1.
Outcome<CareDependentPlacementApplication> adopt_care_settlement_handover_in_candidate(
 CommonStore&,TravelDependentState&,const CareDependentPlacementContext&,
 const MomentOwnerSource& actual_handover_execution);
}
