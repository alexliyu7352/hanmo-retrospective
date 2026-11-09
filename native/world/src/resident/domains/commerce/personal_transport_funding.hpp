// The real MC1 accepted plan consumes S1's original personal WalletDelta source.
// This reader never creates a fee, obligation, payee, balance or movement plan.
#pragma once
#include "self_transport_funding.hpp"
#include "resident/domains/travel/personal_movement_funding.hpp"
#include "resident/domains/travel/movement_action_time.hpp"
namespace hanmo::resident {
struct TaskPersonalMovementFundingContext {
 const SelfTransportFundingContext& funding;const CommonCatalog& common_catalog;
 const TravelState& travel;const MovementExactState& movement;
 const TravelCatalog& travel_catalog;const TravelServices& travel_services;
 const NativeMovementActionTimeRead& movement_actions;
 const PersonalMovementPartyCostHistoryRead* party_history=nullptr;
 ExactArithmeticBudget arithmetic_budget{};
};
struct AcceptedTaskPersonalMovementFunding {
 SourceRef accepted_plan,funding_source;StableId payer_wallet_id;
 WorldMoment accepted_at,actual_at,finite_until;
 const SelfTransportFundingTerms* terms=nullptr;
};
// Source-free movement quote input, but the funding/Task plan must already be
// genuinely accepted. The actual clock is read from the bound context.
Outcome<AcceptedTaskPersonalMovementFunding> read_accepted_task_personal_movement_funding(
 const TaskPersonalMovementFundingContext&,const SourceRef& actual_accepted_plan,
 const StableId& carrier_actor_id,const MovementAnchor& actual_destination);
class NativeTaskPersonalMovementFundingRead final:public PersonalMovementFundingRead {
 const TaskPersonalMovementFundingContext& x_;
public:
 explicit NativeTaskPersonalMovementFundingRead(const TaskPersonalMovementFundingContext&x):x_(x){}
 bool bound_to(const CommonRead&,const MovementExactState&)const noexcept override;
 Status validate_original_personal_execution_funding(const StableId& action_id,const std::optional<MomentOwnerSource>&)const override;
 Status validate_new_personal_unit_cost(const PersonalMovementUnitCost&)const override;
 Status validate_recorded_personal_unit_cost(const MovementExpenseSource&)const override;
};
// Whole-world reverse coverage is separate from each actual payment callback.
// No recursive call to Task's full validator or S1's full expense validator.
Status validate_task_personal_movement_funding(const TaskPersonalMovementFundingContext&);
}
