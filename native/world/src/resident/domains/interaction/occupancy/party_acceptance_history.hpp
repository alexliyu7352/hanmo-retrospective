#pragma once
#include "actor_social_occupancy.hpp"
namespace hanmo::resident {
struct PartyTravelCostHistory {Integer cost_per_tick=0;std::vector<SourceRef> original_membership_sources;};
Status validate_party_membership_acceptance_sources(const CommonStore&,const InteractionState&,const InteractionCatalog&);
// S1 validates the actual original charge/source M first. This reads the real
// Party member acceptances before that Common cut, never current Party.terms.
Outcome<PartyTravelCostHistory> read_original_party_travel_cost_before_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const NativeActorSocialOccupancyRead&,const StableId&actual_leader,const SourceRef&actual_charge_source,const WorldMoment&charged_at);
}
