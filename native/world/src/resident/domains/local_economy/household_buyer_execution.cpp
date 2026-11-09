#include "household_buyer_execution.hpp"
#include "household_mobile_custody.hpp"
#include "household_purchase_contract.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const StableId& code,const StableId& id={}) {throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{r.error};return std::move(r.value);}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
SourceRef plan_ref(const HouseholdPurchasePlan& p){return {"local_economy","household_purchase_planned",p.plan_id};}
std::size_t order(const CommonStore& common,const SourceRef& source){
 std::size_t i=0;for(const auto& row:common.state().facts.rows()){
  if(row.first==source.id){if(row.second.domain!=source.domain||row.second.kind!=source.kind)fail("HouseholdBuyerOriginalSourceMismatch",source.id);return i;}++i;
 }fail("HouseholdBuyerOriginalSourceMissing",source.id);
}
bool adopted(const CommonStore& c,const SourceRef& source,const WorldMoment& occurred,
             const WorldMoment& at,const SourceRef* before){
 const auto ordinal=order(c,source);const auto comparison=cmp(occurred,at);
 if(before){const auto cut=order(c,*before);if(comparison<0&&ordinal>=cut)fail("HouseholdBuyerSourceOrderMismatch",source.id);if(comparison>0&&ordinal<cut)fail("HouseholdBuyerSourceOrderMismatch",source.id);return comparison<=0&&ordinal<cut;}
 return comparison<=0;
}
bool same_buyer(const HouseholdBuyer& a,const HouseholdBuyer& b){
 if(a.index()!=b.index())return false;
 if(const auto* n=std::get_if<NamedHouseholdBuyer>(&a)){const auto& m=std::get<NamedHouseholdBuyer>(b);return n->actor_id==m.actor_id&&bool(n->accepted_service_ref)==bool(m.accepted_service_ref)&&(!n->accepted_service_ref||same_source_ref(*n->accepted_service_ref,*m.accepted_service_ref));}
 const auto& n=std::get<CohortHouseholdBuyer>(a);const auto& m=std::get<CohortHouseholdBuyer>(b);
 return std::tie(n.cohort_id,n.population_selection_ref,n.labor_allocation_id,n.workers)==std::tie(m.cohort_id,m.population_selection_ref,m.labor_allocation_id,m.workers)&&same_source_ref(n.accepted_household_policy_ref,m.accepted_household_policy_ref);
}
bool same_plan_scope(const HouseholdPurchasePlan& a,const HouseholdPurchasePlan& b){
 if(!same_household_mobile_custody(a.mobile_custody_adoption,b.mobile_custody_adoption))return false;
 const auto& x=a.controller_authority_ref;const auto& y=b.controller_authority_ref;
 if(std::tie(a.plan_id,a.household_id,a.spend_cap_qian,a.known_market_ref,a.destination_inventory,a.latest_start_tick,a.delivery_deadline,a.terms_version,a.accepted_policy_revision)!=std::tie(b.plan_id,b.household_id,b.spend_cap_qian,b.known_market_ref,b.destination_inventory,b.latest_start_tick,b.delivery_deadline,b.terms_version,b.accepted_policy_revision)||!same_source_ref(a.need_or_policy_ref,b.need_or_policy_ref)||!same_source_ref(a.accepted_policy_ref,b.accepted_policy_ref)||std::tie(x.actor_id,x.household_group_id,x.fact_id,x.source_role_id,x.operation,x.source_revision,x.effective_from_tick,x.through_tick)!=std::tie(y.actor_id,y.household_group_id,y.fact_id,y.source_role_id,y.operation,y.source_revision,y.effective_from_tick,y.through_tick)||bool(a.approach_travel_ref)!=bool(b.approach_travel_ref)||(a.approach_travel_ref&&!same_source_ref(*a.approach_travel_ref,*b.approach_travel_ref))||a.allowed_items.size()!=b.allowed_items.size())return false;
 for(std::size_t i=0;i<a.allowed_items.size();++i)if(a.allowed_items[i].item_id!=b.allowed_items[i].item_id||a.allowed_items[i].quantity_cap!=b.allowed_items[i].quantity_cap)return false;
 return true;
}
struct Sources {
 const PopulationSelection* selection=nullptr;
 const PopulationSelectionResult* materialization=nullptr;
 const PopulationTransportBinding* binding=nullptr;
 const TransportRegistrationSource* registration=nullptr;
 const TransportUnitReleaseSource* release=nullptr;
};
Sources read_sources(const HouseholdBuyerExecutionContext& x,const HouseholdPurchasePlan& p){
 const auto* buyer=std::get_if<CohortHouseholdBuyer>(&p.execution_choice);
 if(!buyer||buyer->workers!=1||!same_source_ref(buyer->accepted_household_policy_ref,p.accepted_policy_ref))fail("HouseholdBuyerSelectionMismatch",p.plan_id);
 order(x.common,plan_ref(p));
 const auto* selected=x.population.selections.find(buyer->population_selection_ref);
 const auto* initial=x.economy.labor_allocations.find(buyer->labor_allocation_id);
 if(!selected||selected->id!=buyer->population_selection_ref||selected->source.cohort_id!=buyer->cohort_id||selected->purpose!=PopulationSelectionPurpose::household_purchase||selected->selected_persons!=1||selected->selected_labor_eligible!=1||selected->partition_policy!=PopulationPartitionPolicy::labor_eligible_only||!same_source_ref(selected->owner_business_ref,plan_ref(p))||!same_source_ref(selected->release_gate,plan_ref(p))||!initial||initial->id!=buyer->labor_allocation_id||initial->cohort_id!=buyer->cohort_id||initial->activity_or_production_id!=p.plan_id||initial->commitment_id!=p.plan_id||initial->from_tick!=selected->created_tick)fail("HouseholdBuyerOriginalReservationMismatch",p.plan_id);
 const SourceRef labor{"local_economy","household_buyer_labor_reserved",initial->id};
 if(order(x.common,labor)<=order(x.common,plan_ref(p)))fail("HouseholdBuyerOriginalReservationMismatch",p.plan_id);
 Sources out;out.selection=selected;out.materialization=x.population.materializations.find(selected->id);
 if(!out.materialization){
  if(selected->state!=PopulationSelectionPhase::reserved||initial->workers!=1||initial->through_tick!=p.delivery_deadline)fail("HouseholdBuyerReservationHistoryRequired",p.plan_id);
  return out;
 }
 const auto& materialized=*out.materialization;
 if(selected->state!=PopulationSelectionPhase::materialized||materialized.selection_id!=selected->id||materialized.source_cohort_ref.cohort_id!=selected->source.cohort_id||materialized.transfer_ref!=selected->result_transfer_ref||!materialized.result_cohort_ref.transport_binding_id)fail("HouseholdBuyerMaterializationMismatch",selected->id);
 out.binding=x.population.transport_bindings.find(*materialized.result_cohort_ref.transport_binding_id);
 if(!out.binding||out.binding->transport_cohort_id!=materialized.result_cohort_ref.cohort_id||out.binding->source_population_transfer_ref!=materialized.transfer_ref||out.binding->labor_allocation_refs.size()!=1||!same_source_ref(materialized.location_binding_ref,{"travel","population_transport_position",out.binding->binding_id}))fail("HouseholdBuyerMaterializationBindingMismatch",selected->id);
 if(!x.transport_registrations)fail("HouseholdTransportRegistrationReadRequired",selected->id);
 const auto& r=out.binding->registration_source_ref;
 out.registration=r.domain=="travel"&&r.kind=="transport_registration"?x.transport_registrations->registrations.find(r.id):nullptr;
 if(!out.registration||out.registration->id!=r.id||out.registration->transport_unit_id!=out.binding->transport_unit_id||out.binding->created_tick!=out.registration->occurred_at.half_day_tick)fail("HouseholdTransportRegistrationSourceMismatch",r.id);
 order(x.common,r);
 const auto& registration=*out.registration;const auto& scope=registration.accepted_scope;
 const auto* initiator=std::get_if<HouseholdPolicyGroupInitiator>(&scope.initiator);
 if(!same_source_ref(scope.business_source,plan_ref(p))||scope.accepted_scope_revision!=p.terms_version||scope.labor_until_tick!=p.delivery_deadline||scope.accepted_selection_ids!=std::vector<StableId>{selected->id}||!initiator||initiator->accepted_household_policy_ref!=p.accepted_policy_ref.id||initiator->policy_revision!=p.accepted_policy_revision||initiator->purchase_plan_ref!=p.plan_id||initiator->purchase_ref||initiator->phase!="approach"||registration.selections.size()!=1||registration.selections.front().selection_id!=selected->id||registration.selections.front().release_allocation_ids!=std::vector<StableId>{initial->id})fail("HouseholdTransportAcceptedPurposeMismatch",r.id);
 if(cmp(registration.occurred_at,{selected->created_tick,{}})<0||order(x.common,r)<=order(x.common,labor))fail("HouseholdTransportRegistrationOrderMismatch",r.id);
 const auto original=take(read_population_revision_source(x.economy,x.population,materialized.result_cohort_ref));
 if(original.headcount!=1||original.labor_capacity!=1)fail("HouseholdBuyerMaterializedCountMismatch",selected->id);
 const auto* actual=x.economy.labor_allocations.find(out.binding->labor_allocation_refs.front());
 if(!actual||actual->cohort_id!=out.binding->transport_cohort_id||actual->activity_or_production_id!=out.binding->transport_unit_id||actual->commitment_id!=r.id||actual->from_tick!=registration.occurred_at.half_day_tick)fail("HouseholdTransportLaborSourceMismatch",r.id);
 const Integer initial_workers=initial->from_tick<registration.occurred_at.half_day_tick?1:0;
 const Integer initial_until=initial_workers?std::min(p.delivery_deadline,registration.occurred_at.half_day_tick-1):p.delivery_deadline;
 if(initial->workers!=initial_workers||initial->through_tick!=initial_until)fail("HouseholdBuyerReservationTransitionMismatch",initial->id);
 for(const auto& release:x.population.releases)if(release.binding_id==out.binding->binding_id){
  if(out.release||release.actual_handover_source.domain!="travel"||release.actual_handover_source.kind!="transport_release")fail("HouseholdTransportReleaseMismatch",out.binding->binding_id);
  out.release=x.transport_registrations->releases.find(release.actual_handover_source.id);
  if(!out.release||out.release->id!=release.actual_handover_source.id||out.release->transport_unit_id!=out.binding->transport_unit_id||out.release->occurred_at.half_day_tick!=release.effective_tick||std::count(out.release->population_binding_ids.begin(),out.release->population_binding_ids.end(),out.binding->binding_id)!=1||cmp(out.release->occurred_at,registration.occurred_at)<0||order(x.common,release.actual_handover_source)<=order(x.common,r))fail("HouseholdTransportReleaseMismatch",out.binding->binding_id);
 }
 if(bool(out.release)!=(out.binding->state==PopulationTransportPhase::released))fail("HouseholdTransportReleaseCoverage",out.binding->binding_id);
 const Integer workers=out.release&&actual->from_tick>=out.release->occurred_at.half_day_tick?0:1;
 const Integer until=out.release&&workers?std::min(scope.labor_until_tick,out.release->occurred_at.half_day_tick-1):scope.labor_until_tick;
 if(actual->workers!=workers||actual->through_tick!=until)fail("HouseholdTransportLaborHistoryMismatch",actual->id);
 return out;
}
HouseholdBuyer resolve(const HouseholdBuyerExecutionContext& x,const HouseholdPurchasePlan& p,const WorldMoment& at,const SourceRef* before){
 take(encode_world_moment(at));if(before&&order(x.common,plan_ref(p))>=order(x.common,*before))fail("HouseholdBuyerAdoptionOrderMismatch",p.plan_id);
 if(std::holds_alternative<NamedHouseholdBuyer>(p.execution_choice))return p.execution_choice;
 const auto sources=read_sources(x,p);auto buyer=std::get<CohortHouseholdBuyer>(p.execution_choice);
 if(at.half_day_tick<sources.selection->created_tick||at.half_day_tick>p.delivery_deadline)fail("HouseholdBuyerOutsideAcceptedWindow",p.plan_id);
 if(sources.registration&&adopted(x.common,sources.binding->registration_source_ref,sources.registration->occurred_at,at,before)){
  if(sources.release&&adopted(x.common,{"travel","transport_release",sources.release->id},sources.release->occurred_at,at,before))fail("HouseholdBuyerTransportAlreadyReleased",p.plan_id);
  buyer.cohort_id=sources.binding->transport_cohort_id;buyer.labor_allocation_id=sources.binding->labor_allocation_refs.front();
 }
 return buyer;
}
}
Outcome<HouseholdBuyer> read_household_buyer_for_adoption(const HouseholdBuyerExecutionContext& x,const HouseholdPurchasePlan& p,const WorldMoment& at,const SourceRef* before){return guard<HouseholdBuyer>([&]{return resolve(x,p,at,before);});}
Status validate_household_buyer_adoption(const HouseholdBuyerExecutionContext& x,const HouseholdPurchasePlan& original,const HouseholdPurchaseAcceptedTerms& a){return guard<Done>([&]{
 if(!same_plan_scope(a.plan,original)||!same_buyer(resolve(x,original,a.accepted_at,&a.source),a.plan.execution_choice))fail("HouseholdAcceptedBuyerMismatch",a.source.id);
 return Done{};
});}
Status validate_household_buyer_labor_span(const HouseholdBuyerExecutionContext& x,const HouseholdPurchasePlan& original,const HouseholdPurchaseAcceptedTerms& a,const MomentSpan& span){return guard<Done>([&]{
 if(cmp(span.from,span.to)>=0||cmp(span.from,a.accepted_at)<0||cmp(span.to,{original.delivery_deadline,{}})>0)fail("HouseholdBuyerLaborWindowMismatch",a.source.id);
 if(!same_plan_scope(a.plan,original)||!same_buyer(resolve(x,original,a.accepted_at,&a.source),a.plan.execution_choice))fail("HouseholdAcceptedBuyerMismatch",a.source.id);
 const auto* buyer=std::get_if<CohortHouseholdBuyer>(&a.plan.execution_choice);if(!buyer)fail("CohortHouseholdBuyerRequired",a.source.id);
 const auto sources=read_sources(x,original);
 if(buyer->labor_allocation_id==std::get<CohortHouseholdBuyer>(original.execution_choice).labor_allocation_id){
  if(sources.registration&&cmp(span.to,sources.registration->occurred_at)>0)fail("HouseholdOriginalBuyerLaborAlreadyHandedOff",a.source.id);
 }else{
  if(!sources.registration||buyer->labor_allocation_id!=sources.binding->labor_allocation_refs.front()||cmp(span.from,sources.registration->occurred_at)<0||(sources.release&&cmp(span.to,sources.release->occurred_at)>0))fail("HouseholdTransportBuyerLaborNotCovered",a.source.id);
 }
 return Done{};
});}
}
