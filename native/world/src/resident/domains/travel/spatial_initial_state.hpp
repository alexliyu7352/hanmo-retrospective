// New-world construction only. Decode never calls this initializer.
#pragma once
#include "movement_exact_phase.hpp"
#include "mobile_inventory_adoption.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
namespace hanmo::resident {
// Common/Origin2/creation must already have been initialized from their actual
// locked author inputs. This installs only S1 writer bindings to those sources.
// It does not create people, inventories, army formations, journeys or facts.
Status initialize_exact_spatial_state_in_candidate(const CommonStore&,
 const CommonCatalog&,const TravelState&,const TravelCatalog&,
 const AuthoredActorFoundationState&,const ActorFoundationCatalog&,
 const FamilyCatalog*,const CurrentMomentRead&,const MotionExactAuthorSelection&,
 MovementExactState&,SpatialCarrierState*,GroupMovementExecutionState*,
 TransportRegistrationState*,ExactArithmeticBudget budget={});
}
