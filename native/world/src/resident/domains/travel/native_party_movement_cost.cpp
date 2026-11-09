#include "native_party_movement_cost.hpp"
#include "movement_result_read.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {using namespace movement_detail;
Outcome<Integer> NativePartyPersonalMovementCostHistoryRead::read_original_personal_party_cost(const StableId& actor,const MomentOwnerSource& basis_source,const SourceRef& actual_payment,const WorldMoment& charged_at)const{return guard<Integer>([&]{
 if(!occupancy_.party_history_bound_to(common_,interaction_,catalog_))fail("PersonalPartyHistoryCandidateMismatch",actor);
 if(basis_source.source_schema!=1||basis_source.source_ref.domain!="travel"||basis_source.source_ref.kind!="travel_exertion_basis"||actual_payment.domain!="travel"||actual_payment.kind!="movement_expense")fail("PersonalPartyActualChargeSourceRequired",actor);
 const auto* basis=movement_.exertion_bases.find(basis_source.source_ref.id);const auto* paid=movement_.expenses.find(actual_payment.id);const auto* unit=basis?movement_.units.find(basis->unit_ref):nullptr;
 const auto* execution=basis?std::get_if<PersonalMovementExecution>(&basis->execution):nullptr;
 const auto* head=common_.state().facts.find(actual_payment.id);
 if(!basis||!unit||!paid||!execution||!basis->original_action||basis->original_action->actor_id!=actor||basis->original_action->id!=execution->action_id||paid->unit_ref!=unit->id||execution_key(paid->execution)!=execution_key(basis->execution)||unit->effort_basis_refs.empty()||!same_source(unit->effort_basis_refs.front(),basis_source.source_ref)||!head||head->domain!=actual_payment.domain||head->kind!=actual_payment.kind||!same_moment(paid->occurred_at,charged_at,budget_))fail("PersonalPartyActualChargeSourceRequired",actor);
 const auto opened=take(read_movement_source_moment(movement_,basis_source.source_ref,1,budget_));if(take(compare_world_moments(opened,charged_at,budget_))>0)fail("PersonalPartyChargeBeforeOpening",actual_payment.id);
 const auto result=take(read_original_party_travel_cost_before_source(common_,interaction_,catalog_,occupancy_,actor,actual_payment,charged_at));if(result.cost_per_tick<0)fail("InvalidOriginalPartyTravelCost",actual_payment.id);return result.cost_per_tick;
});}
}
