#include "household_transport_registration.hpp"
#include "household_transport_read.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{value.error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t order(const CommonStore& c,const SourceRef& ref){std::size_t n=0;for(const auto& row:c.state().facts.rows()){if(row.first==ref.id){if(row.second.domain!=ref.domain||row.second.kind!=ref.kind||row.second.id!=ref.id)fail("HouseholdTransportSourceMismatch",ref.id);return n;}++n;}fail("HouseholdTransportSourceMissing",ref.id);}
bool same_control(const HouseholdProvisionControlProof&a,const HouseholdProvisionControlProof&b){return std::tie(a.actor_id,a.household_group_id,a.fact_id,a.source_role_id,a.operation,a.source_revision,a.effective_from_tick,a.through_tick)==std::tie(b.actor_id,b.household_group_id,b.fact_id,b.source_role_id,b.operation,b.source_revision,b.effective_from_tick,b.through_tick);}
const HouseholdPurchasePlan& plan(const HouseholdPurchaseContext&x,const SourceRef& source){
 const auto* p=source.domain=="local_economy"&&source.kind=="household_purchase_planned"?x.state.plans.find(source.id):nullptr;
 if(!p||p->plan_id!=source.id||x.selected_household_version!=3||!p->mobile_custody_adoption||!x.mobile_custody)fail("HouseholdTransportOriginalMobilePlanRequired",source.id);
 order(x.common,source);take(validate_household_mobile_custody(x,*x.mobile_custody,*p));return *p;
}
const CohortHouseholdBuyer& original_buyer(const HouseholdPurchaseContext&x,const HouseholdPurchasePlan&p){
 const auto* b=std::get_if<CohortHouseholdBuyer>(&p.execution_choice);const auto* selection=b?x.population.selections.find(b->population_selection_ref):nullptr;
 if(!b||b->workers!=1||!selection||selection->id!=b->population_selection_ref||selection->source.cohort_id!=b->cohort_id||selection->purpose!=PopulationSelectionPurpose::household_purchase||selection->partition_policy!=PopulationPartitionPolicy::labor_eligible_only||selection->selected_persons!=1||selection->selected_labor_eligible!=1||!same_source_ref(selection->owner_business_ref,{"local_economy","household_purchase_planned",p.plan_id})||!same_source_ref(selection->release_gate,selection->owner_business_ref)||!same_source_ref(b->accepted_household_policy_ref,p.accepted_policy_ref))fail("HouseholdTransportSelectedBuyerRequired",p.plan_id);
 const auto* a=x.economy.labor_allocations.find(b->labor_allocation_id);
 if(!a||a->id!=b->labor_allocation_id||a->cohort_id!=b->cohort_id||a->activity_or_production_id!=p.plan_id||a->commitment_id!=p.plan_id||a->from_tick!=selection->created_tick||order(x.common,{"local_economy","household_buyer_labor_reserved",a->id})<=order(x.common,selection->owner_business_ref))fail("HouseholdTransportOriginalBuyerLaborRequired",p.plan_id);
 take(read_population_revision_source(x.economy,x.population,selection->source));return *b;
}
TransportRegistrationAuthority accepted_scope(const HouseholdPurchaseContext&x,const SourceRef&source){
 const auto& p=plan(x,source);const auto& buyer=original_buyer(x,p);
 const auto* policy=x.state.policies.find(p.accepted_policy_ref.id);
 if(!policy||!policy->allow_cohort_buyer||policy->household_id!=p.household_id||policy->revision!=p.accepted_policy_revision||!same_source_ref(policy->acceptance_source_ref,p.accepted_policy_ref)||!same_control(policy->controller_authority_ref,p.controller_authority_ref)||!same_source_ref(p.mobile_custody_adoption->plan_source,source))fail("HouseholdTransportOriginalPolicyMismatch",p.plan_id);
 TransportRegistrationAuthority out;out.business_source=source;out.subject={SubjectKind::cohort,buyer.cohort_id};out.initiator=HouseholdPolicyGroupInitiator{p.accepted_policy_ref.id,p.accepted_policy_revision,p.plan_id,{},"approach"};out.accepted_scope_revision=p.terms_version;out.labor_until_tick=p.delivery_deadline;out.accepted_selection_ids={buyer.population_selection_ref};out.inventory_ids={p.mobile_custody_adoption->actual_view.inventory_id};out.custody_ref=p.plan_id;
 if(p.mobile_custody_adoption->actual_view.vehicle_ref)out.vehicle_refs={*p.mobile_custody_adoption->actual_view.vehicle_ref};return out;
}
bool equal(const TransportRegistrationAuthority&a,const TransportRegistrationAuthority&b){const auto*x=std::get_if<HouseholdPolicyGroupInitiator>(&a.initiator);const auto*y=std::get_if<HouseholdPolicyGroupInitiator>(&b.initiator);return x&&y&&same_source_ref(a.business_source,b.business_source)&&a.subject.kind==b.subject.kind&&a.subject.id==b.subject.id&&std::tie(x->accepted_household_policy_ref,x->policy_revision,x->purchase_plan_ref,x->purchase_ref,x->phase)==std::tie(y->accepted_household_policy_ref,y->policy_revision,y->purchase_plan_ref,y->purchase_ref,y->phase)&&std::tie(a.accepted_scope_revision,a.labor_until_tick,a.accepted_selection_ids,a.named_participants,a.vehicle_refs,a.inventory_ids,a.custody_ref)==std::tie(b.accepted_scope_revision,b.labor_until_tick,b.accepted_selection_ids,b.named_participants,b.vehicle_refs,b.inventory_ids,b.custody_ref);}
bool same_refs(const std::vector<SourceRef>&a,const std::vector<SourceRef>&b){if(a.size()!=b.size())return false;for(const auto&r:a)if(std::count_if(b.begin(),b.end(),[&](const auto&v){return same_source_ref(r,v);})!=1)return false;for(std::size_t i=0;i<a.size();++i)for(std::size_t j=0;j<i;++j)if(same_source_ref(a[i],a[j]))return false;return true;}
}
Outcome<TransportRegistrationAuthority> NativeHouseholdTransportRegistrationBusinessRead::read_registration_authority(const SourceRef&source)const{try{
 auto out=accepted_scope(x_,source);const auto&p=*x_.state.plans.find(source.id);const auto&b=std::get<CohortHouseholdBuyer>(p.execution_choice);const auto*policy=x_.state.policies.find(p.accepted_policy_ref.id);
 if(cmp(x_.now,take(spatial_.current_moment()))||policy->closed_source_ref||x_.now.half_day_tick<policy->valid_from_tick||x_.now.half_day_tick>=policy->valid_until_tick||x_.now.half_day_tick>p.latest_start_tick||cmp(p.mobile_custody_adoption->actual_view.adopted_at,x_.now)>0)fail("HouseholdTransportCurrentAcceptanceUnavailable",p.plan_id);
 auto authority=take(x_.controls.current(policy->controller_actor_id,p.household_id,household_purchase_operation,x_.now.half_day_tick));if(!authority||!same_control(*authority,p.controller_authority_ref))fail("HouseholdTransportCurrentAuthorityRequired",p.plan_id);
 const auto*selection=x_.population.selections.find(b.population_selection_ref);const auto*a=x_.economy.labor_allocations.find(b.labor_allocation_id);
 if(selection->state!=PopulationSelectionPhase::reserved||x_.population.materializations.find(selection->id)||a->workers!=1||a->from_tick>x_.now.half_day_tick||a->through_tick!=p.delivery_deadline)fail("HouseholdTransportBuyerAlreadyMaterialized",p.plan_id);
 const auto*inventory=x_.common.inventory(out.inventory_ids.front());if(!inventory||!owner_equal(inventory->owner,{Owner::Kind::household,p.household_id,{}}))fail("HouseholdTransportCurrentInventoryTitleRequired",p.plan_id);
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
Status NativeHouseholdTransportRegistrationBusinessRead::validate_registration_authority(const TransportRegistrationAuthority&scope)const{try{if(!equal(scope,accepted_scope(x_,scope.business_source)))fail("HouseholdTransportAcceptedScopeMismatch",scope.business_source.id);return common_success();}catch(const Failure&e){return {false,{},e.error};}}
Status NativeHouseholdTransportRegistrationBusinessRead::validate_registered_initiator(const TransportRegistrationSource&r,const TransportUnitState&u,const GroupInitiator&i,const SubjectRef&s,const WorldMoment&at)const{
 const auto* h=std::get_if<HouseholdPolicyGroupInitiator>(&i);if(!h)return common_failure("HouseholdTransportInitiatorRequired",u.transport_unit_id);
 auto scope=validate_registration_authority(r.accepted_scope);if(!scope.ok)return scope;
 auto read=read_household_purchase_transport({x_,carriers_,movement_,registrations_,executions_,spatial_},r,u,*h,s,at);return read.ok?common_success():Status{false,{},read.error};
}
Outcome<std::vector<StableId>> NativeHouseholdTransportRegistrationBusinessRead::read_releasable_labor_allocations(const StableId&id,const SourceRef&source)const{try{const auto&p=plan(x_,source);const auto&b=original_buyer(x_,p);if(id!=b.population_selection_ref)fail("HouseholdTransportReleaseSelectionMismatch",id);return {true,{b.labor_allocation_id},{}};}catch(const Failure&e){return {false,{},e.error};}}
Status NativeHouseholdTransportRegistrationBusinessRead::validate_transport_handover(const TransportUnitReleaseSource&release,const TransportUnitState&unit)const{try{
 const auto*registration=unit.registration_source_ref.domain=="travel"&&unit.registration_source_ref.kind=="transport_registration"?registrations_.registrations.find(unit.registration_source_ref.id):nullptr;
 if(!registration||registration->transport_unit_id!=unit.transport_unit_id||release.transport_unit_id!=unit.transport_unit_id||release.released_composition.transport_unit_id!=unit.transport_unit_id||release.composition_revision!=unit.composition_revision||cmp(release.occurred_at,registration->occurred_at)<0||cmp(release.occurred_at,x_.now)>0)fail("HouseholdTransportHandoverRegistrationRequired",unit.transport_unit_id);
 take(validate_registration_authority(registration->accepted_scope));const auto&p=plan(x_,registration->accepted_scope.business_source);const auto&b=original_buyer(x_,p);
 if(!same_source_ref(release.accepted_handover_source,registration->accepted_scope.business_source)||release.receiver_custody_ref!=p.plan_id||unit.inventory_ids!=registration->accepted_scope.inventory_ids||unit.vehicle_refs!=registration->accepted_scope.vehicle_refs||!release.participant_release_refs.empty()||!release.vehicle_release_refs.empty()||!release.obligation_disposition_refs.empty())fail("HouseholdTransportFiniteReturnScopeMismatch",unit.transport_unit_id);
 if(!take(same_spatial_anchor(registration->departure_anchor,release.actual_anchor))||!take(same_spatial_anchor(take(spatial_.original_site_anchor(release.handover_site_id)),release.actual_anchor)))fail("HouseholdTransportNotReturnedHome",unit.transport_unit_id);
 const auto actual_subject=take(read_registered_transport_subject(x_.common,x_.economy,x_.population,*registration));if(unit.subject.kind!=actual_subject.kind||unit.subject.id!=actual_subject.id)fail("HouseholdTransportReturnBuyerMismatch",unit.transport_unit_id);
 const auto*materialized=x_.population.materializations.find(b.population_selection_ref);if(!materialized||!materialized->result_cohort_ref.transport_binding_id||release.population_binding_ids!=std::vector<StableId>{*materialized->result_cohort_ref.transport_binding_id})fail("HouseholdTransportReturnPopulationMismatch",unit.transport_unit_id);
 const SourceRef release_source{"travel","transport_release",release.id};const auto* released_header=x_.common.state().facts.find(release.id);const auto release_cut=released_header?order(x_.common,release_source):x_.common.state().facts.size();if(!released_header&&cmp(release.occurred_at,x_.now))fail("HouseholdTransportActualReturnMomentRequired",release.id);
 std::vector<SourceRef> deliveries;
 for(const auto&row:x_.state.purchases.rows())if(row.second.plan_ref==p.plan_id){const auto&purchase=row.second;const auto*a=x_.state.accepted_terms.find(purchase.accepted_terms_ref.id);if(!a)fail("HouseholdTransportPurchaseSourceMissing",row.first);
  if(order(x_.common,a->source)>=release_cut||cmp(a->accepted_at,release.occurred_at)>0)fail("HouseholdTransportPurchaseAfterReturn",row.first);
  // Original terminal responsibility stays in S5. Releasing the transporter
// never drops a live service, held cargo, debt, or refunds that are still due.
  const auto cargo=take(read_household_purchase_cargo(x_,row.first));
  if(purchase.state!=HouseholdPurchasePhase::received||!cargo.unreceived_cargo.empty()||cargo.received_quantity!=cargo.purchased_quantity)fail("HouseholdTransportActualDeliveryRequired",row.first);
  for(const auto&h:x_.state.handovers.rows())if(h.second.purchase_id==row.first&&h.second.to_inventory_id==a->plan.destination_inventory){if(order(x_.common,{"local_economy","household_purchase_delivered",h.first})>=release_cut||cmp(h.second.occurred_at,release.occurred_at)>0)fail("HouseholdTransportDeliveryAfterReturn",h.first);deliveries.push_back({"local_economy","household_purchase_delivered",h.first});}
 }
 if(!same_refs(release.cargo_handover_refs,deliveries))fail("HouseholdTransportDeliveryCoverageMismatch",unit.transport_unit_id);
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
}
