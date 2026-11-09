// Financial allowance for the original personal movement debit. No fee or
// payer is accepted from a caller, and this interface never moves money.
#pragma once
#include "movement_exact_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
class CurrentMomentRead;class NativeMovementActionTimeRead;
struct PersonalMovementFirstPositivePrefix {MomentSpan selected_span;};
struct PersonalMovementZeroContact {SourceRef actual_contact;};
using PersonalMovementActualStart=std::variant<PersonalMovementFirstPositivePrefix,PersonalMovementZeroContact>;
struct PersonalMovementCostRequest {StableId prefix_id;PersonalMovementActualStart actual_start;};
struct PersonalMovementUnitCost {
 StableId action_id,actor_id,prefix_id,unit_ref,payer_wallet_id;Integer amount=0;
 StableId original_cost_rule_ref;MomentOwnerSource unit_basis;
 WorldMoment occurred_at;PersonalMovementActualStart actual_start;
};
Outcome<PersonalMovementUnitCost> read_unconsumed_personal_unit_cost(const CommonRead&,const TravelState&,const MovementExactState&,const TravelCatalog&,const TravelServices&,const PersonalMovementCostRequest&,const CurrentMomentRead&,ExactArithmeticBudget budget={});
class PersonalMovementFundingRead {public:virtual ~PersonalMovementFundingRead()=default;
 virtual bool bound_to(const CommonRead&,const MovementExactState&)const noexcept=0;
 virtual Status validate_original_personal_execution_funding(const StableId& action_id,const std::optional<MomentOwnerSource>& actual_accepted_plan)const=0;
 virtual Status validate_new_personal_unit_cost(const PersonalMovementUnitCost&)const=0;
 // The source has already passed S1 original unit/start/debit validation. This
 // adds the original accepted business/funding/window/gross-cap proof.
 virtual Status validate_recorded_personal_unit_cost(const MovementExpenseSource&)const=0;
};
// The original party owner proves its retained terms at the payment cut. It
// cannot replace this with today's party or the recorded amount itself.
class PersonalMovementPartyCostHistoryRead {public:virtual ~PersonalMovementPartyCostHistoryRead()=default;
 virtual Outcome<Integer> read_original_personal_party_cost(const StableId& actor_id,const MomentOwnerSource& original_unit_basis,const SourceRef& actual_payment,const WorldMoment& charged_at)const=0;
};
Status validate_personal_movement_funding_sources(const CommonStore&,const MovementExactState&,const NativeMovementActionTimeRead&,Integer selected_travel_version,Integer selected_funding_version,const PersonalMovementFundingRead*);
Status validate_personal_movement_recorded_cost(const CommonStore&,const MovementExactState&,const TravelCatalog&,const MovementExpenseSource&,const PersonalMovementPartyCostHistoryRead*,ExactArithmeticBudget budget={});
}
