// Internal original planner sharing; callers cannot pass a replacement wallet.
#pragma once
#include "resident/domains/commerce/personal_transport_funding.hpp"
namespace hanmo::resident::movement_funded_plan_detail {
Outcome<AcceptedTaskPersonalMovementFunding> accepted_basis(const TaskPersonalMovementFundingContext&,const SourceRef&,const StableId& actor_id,const TravelDestination&,const StableId& pending_action_id);
Outcome<TravelQuote> terrain(const TaskPersonalMovementFundingContext&,const SourceRef&,const StableId&,const TravelDestination&,const std::vector<StableId>&,const StableId& pending_action_id);
Outcome<TravelQuote> road(const TaskPersonalMovementFundingContext&,const SourceRef&,const StableId&,const TravelDestination&,const std::vector<StableId>&,const StableId& pending_action_id);
}
