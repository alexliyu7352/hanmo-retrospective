#include "household_transport_funding.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{value.error};}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
void header(const CommonStore& c,const SourceRef& ref){const auto* f=c.state().facts.find(ref.id);if(!f||f->domain!=ref.domain||f->kind!=ref.kind)fail("HouseholdTransportOriginalHeaderRequired",ref.id);}
bool same_control(const HouseholdProvisionControlProof& a,const HouseholdProvisionControlProof& b){return std::tie(a.actor_id,a.household_group_id,a.fact_id,a.source_role_id,a.operation,a.source_revision,a.effective_from_tick,a.through_tick)==std::tie(b.actor_id,b.household_group_id,b.fact_id,b.source_role_id,b.operation,b.source_revision,b.effective_from_tick,b.through_tick);}
bool same_initiator(const HouseholdPolicyGroupInitiator& i,const HouseholdPurchaseExpenseAuthority& a){return i.accepted_household_policy_ref==a.accepted_policy_ref&&i.policy_revision==a.policy_revision&&i.purchase_plan_ref==a.purchase_plan_ref&&i.purchase_ref==a.purchase_ref&&i.phase==a.phase;}

}
Outcome<TransportExpenseAllowance> NativeHouseholdTransportExpenseAuthorityRead::read_transport_expense_allowance(const TransportExpenseAuthority& value,const ActualTransportExpense& cost,TransportExpenseAuthorityCheck check)const{
 try{
  const auto* a=std::get_if<HouseholdPurchaseExpenseAuthority>(&value);
  if(!a)fail("UnsupportedHouseholdExpenseAuthority");
  if(a->phase!="approach"&&a->phase!="delivery"&&a->phase!="return")fail("UnknownHouseholdTransportPhase",a->phase);
  const auto* plan=x_.state.plans.find(a->purchase_plan_ref);const auto* policy=x_.state.policies.find(a->accepted_policy_ref);
  if(!plan||!policy||a->household_id!=plan->household_id||policy->household_id!=a->household_id||policy->revision!=a->policy_revision||plan->accepted_policy_revision!=a->policy_revision||!same_source_ref(plan->accepted_policy_ref,policy->acceptance_source_ref)||plan->accepted_policy_ref.id!=a->accepted_policy_ref||!same_control(plan->controller_authority_ref,policy->controller_authority_ref))fail("HouseholdTransportPolicyMismatch",a->purchase_plan_ref);
  header(x_.common,{"local_economy","household_purchase_planned",plan->plan_id});header(x_.common,policy->acceptance_source_ref);
  take(x_.controls.validate_accepted(policy->controller_authority_ref,policy->accepted_tick));
  const auto* buyer=std::get_if<CohortHouseholdBuyer>(&plan->execution_choice);
  if(!buyer||!policy->allow_cohort_buyer||buyer->workers!=1||!same_source_ref(buyer->accepted_household_policy_ref,policy->acceptance_source_ref))fail("HouseholdTransportCohortExecutorRequired",plan->plan_id);
  const auto* selection=x_.population.selections.find(buyer->population_selection_ref);
  if(!selection||selection->purpose!=PopulationSelectionPurpose::household_purchase||selection->selected_persons!=1||selection->selected_labor_eligible!=1||selection->owner_business_ref.id!=plan->plan_id)fail("HouseholdTransportSelectionMismatch",plan->plan_id);
  const auto* journey=carriers_.journeys.find(cost.journey_id);const auto* initiator=journey?std::get_if<HouseholdPolicyGroupInitiator>(&journey->initiator):nullptr;
  const auto* carrier=journey?std::get_if<TransportUnitCarrierRef>(&journey->carrier):nullptr;
  const auto* unit=carrier?carriers_.transport_units.find(carrier->transport_unit_ref.transport_unit_id):nullptr;
  const auto* registration=unit?registrations_.registrations.find(unit->registration_source_ref.id):nullptr;
  if(!journey||!initiator||!same_initiator(*initiator,*a)||!same_transport_expense_authority(journey->expense_authority,value)||!unit||!registration||registration->transport_unit_id!=unit->transport_unit_id||unit->registration_source_ref.domain!="travel"||unit->registration_source_ref.kind!="transport_registration")fail("HouseholdTransportAcceptedJourneyRequired",cost.journey_id);
  header(x_.common,unit->registration_source_ref);
  const auto* registered_initiator=std::get_if<HouseholdPolicyGroupInitiator>(&registration->accepted_scope.initiator);
  const auto actual_subject=take(read_registered_transport_subject(x_.common,x_.economy,x_.population,*registration));
  if(!registered_initiator||registered_initiator->accepted_household_policy_ref!=a->accepted_policy_ref||registered_initiator->policy_revision!=a->policy_revision||registered_initiator->purchase_plan_ref!=a->purchase_plan_ref||actual_subject.kind!=SubjectKind::cohort||journey->sponsor.kind!=SubjectKind::cohort||actual_subject.id!=journey->sponsor.id||std::find(registration->accepted_scope.accepted_selection_ids.begin(),registration->accepted_scope.accepted_selection_ids.end(),selection->id)==registration->accepted_scope.accepted_selection_ids.end())fail("HouseholdTransportRegistrationScopeMismatch",cost.journey_id);
  const auto* materialization=x_.population.materializations.find(selection->id);
  if(!materialization||materialization->result_cohort_ref.cohort_id!=journey->sponsor.id)fail("HouseholdTransportMaterializationRequired",cost.journey_id);
  // The registered one-worker interval and original accepted plan bound every leg.
  if(cmp(registration->occurred_at,journey->started_at)>0||cmp(journey->started_at,cost.occurred_at)>0||cost.occurred_at.half_day_tick<policy->valid_from_tick||cmp(cost.occurred_at,{plan->delivery_deadline,{}})>0||plan->delivery_deadline>policy->valid_until_tick||registration->accepted_scope.labor_until_tick<plan->delivery_deadline)fail("HouseholdTransportOutsideAcceptedWindow",cost.journey_id);
  // The original S1 opening and original S5 service/cargo sources establish
  // the accepted phase, including finite return without a successful purchase.
  take(validate_household_transport_phase_adoption({x_,carriers_,movement_,registrations_,executions_,spatial_},*journey));
  if(check==TransportExpenseAuthorityCheck::new_charge){
   if(cmp(cost.occurred_at,x_.now)>0)fail("FutureHouseholdTransportCharge",cost.journey_id);
   // Withdrawal does not erase an already accepted finite return responsibility.
   bool closed_at_charge=false;if(policy->closed_source_ref){const auto* next=x_.state.policies.find(policy->closed_source_ref->id);if(!next||!same_source_ref(next->acceptance_source_ref,*policy->closed_source_ref))fail("HouseholdPolicyClosureSourceRequired",policy->id);closed_at_charge=cost.occurred_at.half_day_tick>=next->accepted_tick;}
   if(closed_at_charge&&a->phase!="return")fail("HouseholdPolicyNoNewTransportExpense",policy->id);
   if(!closed_at_charge){auto authority=take(x_.controls.current(policy->controller_actor_id,policy->household_id,household_purchase_operation,cost.occurred_at.half_day_tick));if(!authority||!same_control(*authority,policy->controller_authority_ref))fail("HouseholdTransportAuthorityChanged",policy->id);}
  }
  auto usage=take(read_household_purchase_allowance(x_,policy->acceptance_source_ref,policy->revision));
  Integer paid=usage.transport_gross_paid;
  if(check==TransportExpenseAuthorityCheck::existing_charge){const auto* original=take(read_transport_payment(x_.transport_expenses,cost.source_ref,cost.source_schema));if(!same_transport_expense_authority(original->request.authority,value)||original->expense.amount!=cost.amount||paid<cost.amount)fail("HouseholdTransportHistoricalChargeMismatch",original->id);paid-=cost.amount;}
  const auto* home=x_.economy.households.find(plan->household_id);const auto* wallet=home?x_.common.wallet(home->wallet_id):nullptr;
  if(!wallet||!owner_equal(wallet->owner,{Owner::Kind::household,plan->household_id,{}})||journey->payer_wallet_id!=wallet->id)fail("HouseholdTransportWalletMismatch",plan->plan_id);
  const auto limit=std::min(policy->transport_cap_qian,policy->total_cap_qian-usage.merchandise_gross_paid);
  if(limit<0||paid<0||paid>limit||cost.amount>limit-paid)fail("HouseholdTransportAllowanceExceeded",policy->id);
  if(!receiver_)return {false,{},common_failure("OriginalTransportReceiverSourceRequired",cost.source_ref.id,ErrorKind::unsupported_capability).error};
  if(!receiver_->bound_to(x_.common))return {false,{},common_failure("OriginalTransportReceiverCandidateMismatch",cost.source_ref.id,ErrorKind::internal).error};
  const auto receiver=take(receiver_->read_original_transport_receiver(cost));if(receiver.creditor.id.empty()||!valid_source_ref(receiver.receiver_source.source_ref)||receiver.receiver_source.source_schema<1)fail("HouseholdTransportReceiverSourceMismatch",cost.source_ref.id);
  return {true,{wallet->id,policy->acceptance_source_ref.id,policy->revision,limit,paid,journey->sponsor,receiver.creditor},{}};
 }catch(const Failure& e){return {false,{},e.error};}
}
Outcome<TransportExpenseResponsibility> NativeHouseholdTransportExpenseAuthorityRead::read_original_transport_responsibility(const TransportExpenseAuthority&value,const ActualTransportExpense&cost,TransportExpenseAuthorityCheck check)const{try{
 const auto allowance=take(read_transport_expense_allowance(value,cost,check));
 const auto&a=std::get<HouseholdPurchaseExpenseAuthority>(value);const auto&policy=*x_.state.policies.find(a.accepted_policy_ref);const auto&plan=*x_.state.plans.find(a.purchase_plan_ref);
 const auto&journey=*carriers_.journeys.find(cost.journey_id);const auto&carrier=std::get<TransportUnitCarrierRef>(journey.carrier);const auto&unit=*carriers_.transport_units.find(carrier.transport_unit_ref.transport_unit_id);
 const auto receiver=take(receiver_->read_original_transport_receiver(cost));
 if(receiver.creditor.kind!=allowance.beneficiary.kind||receiver.creditor.id!=allowance.beneficiary.id)fail("HouseholdTransportReceiverSourceChanged",cost.source_ref.id);
 return {true,{allowance.principal,receiver.creditor,allowance.payer_wallet_id,{policy.acceptance_source_ref,1},receiver.receiver_source,{{policy.acceptance_source_ref,1},{{"local_economy","household_purchase_planned",plan.plan_id},1},{unit.registration_source_ref,1}}},{}};
}catch(const Failure&e){return {false,{},e.error};}}
}
