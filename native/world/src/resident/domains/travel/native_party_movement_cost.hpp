// Original Party membership/role terms at the actual S1 payment cut.
#pragma once
#include "personal_movement_funding.hpp"
#include "resident/domains/interaction/occupancy/party_acceptance_history.hpp"
namespace hanmo::resident {
class NativePartyPersonalMovementCostHistoryRead final:public PersonalMovementPartyCostHistoryRead {
 const CommonStore& common_;const MovementExactState& movement_;
 const InteractionState& interaction_;const InteractionCatalog& catalog_;
 const NativeActorSocialOccupancyRead& occupancy_;ExactArithmeticBudget budget_;
public:
 NativePartyPersonalMovementCostHistoryRead(const CommonStore& common,const MovementExactState& movement,const InteractionState& interaction,const InteractionCatalog& catalog,const NativeActorSocialOccupancyRead& occupancy,ExactArithmeticBudget budget={}):common_(common),movement_(movement),interaction_(interaction),catalog_(catalog),occupancy_(occupancy),budget_(budget){}
 Outcome<Integer> read_original_personal_party_cost(const StableId&,const MomentOwnerSource&,const SourceRef&,const WorldMoment&)const override;
};
}
