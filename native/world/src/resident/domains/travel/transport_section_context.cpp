#include "transport_section_detail.hpp"
#include "movement_exact_detail.hpp"
#include "transport_physical_fact_headers.hpp"
namespace hanmo::resident {
using namespace movement_detail;
namespace {
bool original_travel_empty(const TravelState& s){return s.actions.empty()&&s.road_actions.empty()&&s.rescues.empty()&&s.journey_facts.empty()&&s.itineraries.empty();}
bool manifest_roots_empty(const TransportPhysicalCodecRoots& r){return (!r.carriers||(r.carriers->inventory_bindings.empty()&&r.carriers->crossings.empty()))&&(!r.registrations||(r.registrations->registrations.empty()&&r.registrations->releases.empty()))&&(!r.executions||r.executions->entry_manifests.empty());}
void selected_roots(const TransportPhysicalCodecRoots& roots,const TransportPhysicalCodecSelection& selected){
 if(selected.travel_version!=4&&selected.travel_version!=5)fail("UnsupportedTravelPhysicalVersion");
 if(selected.group_transport_version!=0&&selected.group_transport_version!=4&&selected.group_transport_version!=5&&selected.group_transport_version!=6&&selected.group_transport_version!=7)fail("UnsupportedGroupTransportPhysicalVersion");
 if(selected.transport_manifest_bindings_version!=0&&selected.transport_manifest_bindings_version!=2)fail("UnsupportedTransportManifestPhysicalVersion");
 const bool group=selected.group_transport_version!=0,manifest=selected.transport_manifest_bindings_version!=0;
 // 暂定静置组合只借原 Manifest2；group 已选仍必须原成对，不能缺失业务段。
 if(group&&!manifest)fail("TransportPhysicalGroupManifestSelectionNotPaired");
 if((group||manifest)&&!selected.carrier_location_enabled)fail("TransportPhysicalCarrierLocationSelectionRequired");
 if(group||manifest){if(!roots.carriers||!roots.executions||!roots.registrations||!roots.payments)fail("TransportPhysicalOriginalRootsRequired");}
 else if(roots.carriers||roots.executions||roots.registrations)fail("UnselectedTransportPhysicalRoots");
 if(!manifest&&!manifest_roots_empty(roots))fail("TransportManifestPhysicalSectionRequired");
 if(!group&&manifest)take(validate_stationary_inventory_transport_roots(
     roots.movement,*roots.carriers,*roots.executions,*roots.registrations,true));
}
void validate_final_structure(TransportPhysicalCodecContext::State& state){
 const auto& roots=state.roots;
 selected_roots(roots,state.selected);
 take(validate_transport_physical_source_ownership(roots.movement,roots.carriers,roots.executions,roots.registrations));
 take(validate_transport_physical_fact_headers(state.common,roots));
 if(roots.payments){
  auto payments=take(index_transport_payment_physical_owners(state.common,roots.movement,roots.carriers,roots.executions,*roots.payments,state.budget));
  if(state.reading){
   if(state.decoded_payment_owners.size()!=roots.payments->payments.size())fail("TransportPaymentPhysicalDecodeCoverageMismatch");
   for(const auto& original:state.decoded_payment_owners){
    if(original.payment_index>=roots.payments->payments.size())fail("TransportPaymentPhysicalDecodeOrdinalMismatch");
    const auto& payment=roots.payments->payments[original.payment_index];const auto* owner=payments.find_payment(payment.id);
    const std::string_view encoded_key(original.owner_key.data(),original.owner_key.size());
    if(!owner||!owner->owner_id||owner->owner!=original.owner||std::string_view(*owner->owner_id)!=encoded_key)fail("TransportPaymentPhysicalOwnerMismatch",payment.id);
   }
  }else state.writing_payments=std::move(payments);
 }else{
  if(!state.decoded_payments.empty()||!state.decoded_payment_owners.empty())fail("TransportPaymentOriginalRootRequired");
  auto check=[&](const auto& refs){for(const auto& ref:refs)if(ref.domain=="local_economy"&&ref.kind=="transport_expense_paid")fail("TransportPaymentOriginalRootRequired",ref.id);};
  for(const auto& row:roots.movement.cargo_handling.terms.rows())check(row.second.actual_start_cost_sources);
  for(const auto& row:roots.movement.cargo_handling.work_results.rows())check(row.second.cost_source_refs);
 }
 auto originals=take(index_original_movement_bases(roots.movement,state.budget));
 if(state.reading){
  if(originals.rows.size()!=state.decoded_bases.size())fail("MovementPhysicalBasisCoverageMismatch");
  for(const auto& original:state.decoded_bases.rows()){const auto* restored=originals.find(original.first);if(!restored||!take(same_original_movement_basis_payload(*restored,original.second,state.budget)))fail("MovementPhysicalBasisRestoreMismatch",original.first);}
 }else state.writing_bases=std::move(originals);
}
}
TransportPhysicalCodecContext::State::State(const CommonStore& c,TransportPhysicalCodecRoots r,const TransportPhysicalCodecSelection& s,bool decode,ExactArithmeticBudget b,MovementBasisDecodeAllocation* allocation)
 :common(c),roots(r),selected(s),record_selection{s.travel_version,s.group_transport_version?s.group_transport_version:4,s.common_resources,s.carrier_location_enabled,s.typed_reservations,s.domain_activity_reservations},budget(std::move(b)),original_basis_allocation(allocation),reading(decode){}
Outcome<MovementBasis> TransportPhysicalCodecContext::State::copy_original_basis(const StableId& id){return guard<MovementBasis>([&]{
 if(!reading||!original_basis_allocation)fail("MovementBasisDecodeAllocationRequired",id);
 const auto* basis=decoded_bases.find(id);if(!basis)fail("MovementPhysicalOriginalBasisRequired",id);
 take(original_basis_allocation->reserve_original_basis_copy(*basis));
 return *basis;
});}
Status TransportPhysicalCodecContext::State::adopt_travel_rows(){return status([&]{
 auto& m=roots.movement;
 #define ADOPT(MEMBER,PENDING) take(transport_physical_table::adopt(m.MEMBER,PENDING,#MEMBER))
 ADOPT(spatial_bindings,spatial_bindings);ADOPT(prefixes,prefixes);ADOPT(units,units);
 ADOPT(exertion_bases,effort_bases);ADOPT(expenses,expenses);ADOPT(unit_closes,closes);
 ADOPT(exertions,exertions);ADOPT(unit_boundaries,boundaries);ADOPT(basis_transitions,transitions);
 ADOPT(personal_controls,personal_controls);ADOPT(arrivals,arrivals);ADOPT(personal_terminals,personal_terminals);
 ADOPT(patrol_final_motions,patrol_final_motions);ADOPT(patrol_checkpoint_arrivals,patrol_checkpoint_arrivals);
 ADOPT(credit_dispositions,credits);ADOPT(contacts,contacts);ADOPT(contact_installations,installations);
 ADOPT(contact_continuations,continuations);ADOPT(passage_modifier_adoptions,modifiers);
 ADOPT(cargo_handling.terms,handling_terms);ADOPT(cargo_handling.executions,handling_executions);
 ADOPT(cargo_handling.work_results,handling_work);ADOPT(cargo_handling.results,handling_results);
 #undef ADOPT
 if(roots.executions)take(transport_physical_table::adopt(roots.executions->expenses,group_expenses,"group_expenses"));
 else if(!group_expenses.values.empty())fail("GroupExpenseOriginalRootRequired");
});}
Status TransportPhysicalCodecContext::State::adopt_group_rows(){return status([&]{
 if(!roots.carriers||!roots.executions)fail("GroupTransportOriginalRootsRequired");
 auto& s=*roots.carriers;auto& g=*roots.executions;
 #define ADOPT(ROOT,MEMBER,PENDING) take(transport_physical_table::adopt(ROOT.MEMBER,PENDING,#MEMBER))
 ADOPT(s,transport_units,transport_units);ADOPT(s,positions,positions);ADOPT(s,journeys,journeys);
 ADOPT(s,bindings,group_bindings);ADOPT(s,steps,steps);ADOPT(s,terminals,terminals);
 ADOPT(s,journey_purpose_bindings,purposes);ADOPT(g,openings,openings);ADOPT(g,unit_terms,unit_terms);ADOPT(g,controls,group_controls);ADOPT(g,military_supply_rebinds,military_supply_rebinds);
 #undef ADOPT
});}
Status TransportPhysicalCodecContext::State::adopt_manifest_rows(){return status([&]{
 if(!roots.carriers||!roots.executions||!roots.registrations)fail("TransportManifestOriginalRootsRequired");
 take(transport_physical_table::adopt(roots.carriers->inventory_bindings,assignments,"assignments"));
 take(transport_physical_table::adopt(roots.carriers->crossings,crossings,"boundary_crossing_receipts"));
 take(transport_physical_table::adopt(roots.registrations->registrations,registrations,"registrations"));
 take(transport_physical_table::adopt(roots.registrations->releases,releases,"releases"));
 take(transport_physical_table::adopt(roots.executions->entry_manifests,entry_manifests,"entry_manifests"));
});}
TransportPhysicalCodecContext::TransportPhysicalCodecContext(std::unique_ptr<State> s):state_(std::move(s)){}
TransportPhysicalCodecContext::~TransportPhysicalCodecContext()=default;
TransportPhysicalCodecContext::State& TransportPhysicalCodecContext::state()noexcept{return *state_;}
Outcome<std::unique_ptr<TransportPhysicalCodecContext>> make_transport_physical_codec_context(const CommonStore& common,TransportPhysicalCodecRoots roots,const TransportPhysicalCodecSelection& selected,bool reading,ExactArithmeticBudget budget,MovementBasisDecodeAllocation* allocation){return guard<std::unique_ptr<TransportPhysicalCodecContext>>([&]{
 selected_roots(roots,selected);
 if(reading){if(!allocation)fail("MovementBasisDecodeAllocationRequired");if(!original_travel_empty(roots.travel))fail("PhysicalTravelRootAlreadyPopulated");if(roots.payments&&!roots.payments->payments.empty())fail("PhysicalTransportPaymentRootAlreadyPopulated");}
 auto state=std::make_unique<TransportPhysicalCodecContext::State>(common,roots,selected,reading,budget,allocation);
 if(!reading)validate_final_structure(*state);
 return std::make_unique<TransportPhysicalCodecContext>(std::move(state));
});}
Outcome<std::unique_ptr<TransportPhysicalCodecContext>> make_transport_physical_section_write_context(const CommonStore& common,TransportPhysicalCodecRoots roots,const TransportPhysicalCodecSelection& selected,TransportPhysicalSection section,ExactArithmeticBudget budget){return guard<std::unique_ptr<TransportPhysicalCodecContext>>([&]{
 selected_roots(roots,selected);
 switch(section){
  case TransportPhysicalSection::travel:break;
  case TransportPhysicalSection::group_transport:if(!selected.group_transport_version)fail("UnselectedGroupTransportPhysicalSection");break;
  case TransportPhysicalSection::transport_manifest_bindings:if(selected.transport_manifest_bindings_version!=2)fail("UnselectedTransportManifestPhysicalSection");break;
  default:fail("UnknownTransportPhysicalSection");
 }
 auto state=std::make_unique<TransportPhysicalCodecContext::State>(common,roots,selected,false,budget,nullptr);
 state->write_section=section;
 validate_final_structure(*state);
 return std::make_unique<TransportPhysicalCodecContext>(std::move(state));
});}
Status finish_transport_physical_codec(TransportPhysicalCodecContext& context){return status([&]{
 auto& state=context.state();if(state.finished)fail("TransportPhysicalCodecAlreadyFinished");
 if(state.write_section){
  if(state.reading)fail("TransportPhysicalCodecModeMismatch");
  if(state.travel_seen!=(*state.write_section==TransportPhysicalSection::travel)||state.group_seen!=(*state.write_section==TransportPhysicalSection::group_transport)||state.manifest_seen!=(*state.write_section==TransportPhysicalSection::transport_manifest_bindings))fail("TransportPhysicalSingleWriteCoverageMismatch");
 }else if(!state.travel_seen||state.group_seen!=(state.selected.group_transport_version!=0)||state.manifest_seen!=(state.selected.transport_manifest_bindings_version!=0))fail("TransportPhysicalSectionCoverageMismatch");
 if(state.reading&&state.roots.payments){
  if(!state.roots.payments->payments.empty())fail("PhysicalTransportPaymentRootAlreadyPopulated");
  const auto pending_resource=state.decoded_payments.get_allocator().resource();
  const auto original_resource=state.roots.payments->payments.get_allocator().resource();
  if(bool(pending_resource)!=bool(original_resource)||(pending_resource&&!pending_resource->shares_job(*original_resource)))fail("CheckpointCandidateMemoryJobMismatch","transport_payments");
  state.roots.payments->payments=std::move(state.decoded_payments);
 }
 validate_final_structure(state);state.finished=true;
});}
}
