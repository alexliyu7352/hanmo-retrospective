#pragma once
#include "growth_fixed_service_model.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
// Called after the actual boundary allocation in the original uncommitted command
// candidate. The source is read from the unique Family chain, never supplied by UI.
Outcome<LifePlayerActorRoute> capture_growth_player_route(const ProfileControlEligibilitySources&,const GrowthState&,const StableId& actual_boundary_id,const StableId& actor_id);
// Reads an already adopted original NPC command only when the actual actor is not
// the historical player. The Director result must exist in this same candidate.
Outcome<LifeActorRoute> capture_growth_actor_route(const CharacterConditionSources&,const GrowthState&,const StableId& actual_boundary_id,const LifeActualCommand&);
Status validate_growth_fixed_control_inputs(const CharacterConditionSources&,const GrowthState&);
}
