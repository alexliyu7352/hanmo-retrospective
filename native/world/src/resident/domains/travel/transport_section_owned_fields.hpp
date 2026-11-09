#pragma once
#include "transport_section_detail.hpp"
namespace hanmo::resident::transport_section_fields {
template<class C,class T,class Allocator>Status append_original_row(C& c,std::vector<T,Allocator>& rows,T&& value,const StableId& kind){if constexpr(std::is_nothrow_move_constructible_v<T>)return c.append_decoded_value(rows,std::move(value),kind);else return c.append_decoded_value(rows,std::move(value),kind,transport_decoded_transfer::OriginalTransportDecodedRowTransfer{});}
template<ProjectionRecordShape Shape,class T,class Allocator>Status append_original_row(ProjectionRecordArchive<Shape>& c,std::vector<T,Allocator>&,T&&,const StableId& kind){return c.reject("PhysicalDecodeOnProjectionWriter",kind);}
template<class C,class T,class Visit,class Filter>
Status optional_child(C& c,const StableId& name,OrderedTable<T>& table,transport_physical_table::DecodedRows<T>& pending,Visit visit,Filter filter){
 using Row=std::pair<StableId,T>;
 if(c.reading()){
  std::optional<Row> row;
  auto read=c.optional(name,row,[&](auto& a,Row& v){a.field("key",v.first);return a.object("fields",[&](auto& b){return visit(b,v.first,v.second);});});
  if(!read.ok)return read;
  if(row){if(row->first.empty()||!filter(row->first,row->second))return c.reject("PhysicalTransportOptionalOwnerMismatch",name);return append_original_row(c,pending.values,std::move(*row),name);}
  return c.status();
 }
 transport_physical_table::retain(c,table);
 std::optional<const Row*> found;
 for(const auto& row:table.rows())if(filter(row.first,row.second)){if(found)return c.reject("DuplicatePhysicalOptionalSource",name);found=&row;}
 return c.optional(name,found,[&](auto& a,const Row*& p){a.field("key",const_cast<StableId&>(p->first));return a.object("fields",[&](auto& b){return visit(b,p->first,const_cast<T&>(p->second));});});
}
template<class C>Status payments(C& c,const StableId& name,State& state,TransportPaymentPhysicalOwner owner,const StableId& owner_id){
 if(c.reading()){
  const auto begin=state.decoded_payments.size();
  auto read=transport_physical_table::append_rows(c,name,state.decoded_payments,[&](auto& a,TransportPaymentReceipt& receipt){return value(a,receipt,state.record_selection);});
  if(!read.ok)return read;
  if(!state.roots.payments&&state.decoded_payments.size()!=begin)return c.reject("TransportPaymentOriginalRootRequired",owner_id);
  for(auto n=begin;n<state.decoded_payments.size();++n)if(state.decoded_payments[n].id.empty()||(n>begin&&state.decoded_payments[n-1].id>=state.decoded_payments[n].id))return c.reject("InvalidCanonicalTransportPaymentOrder",owner_id);
  for(auto n=begin;n<state.decoded_payments.size();++n){State::DecodedPaymentOwner actual;actual.payment_index=n;actual.owner=owner;actual.owner_key.assign(owner_id.begin(),owner_id.end());state.decoded_payment_owners.emplace_back(std::move(actual));}
  return c.status();
 }
 CheckpointVector<const TransportPaymentReceipt*> rows;
 for(const auto& binding:state.writing_payments.bindings)if(binding.owner==owner&&binding.owner_id&&*binding.owner_id==owner_id)rows.push_back(binding.payment);
 return c.sequence(name,rows,[&](auto& a,const TransportPaymentReceipt*& row){return value(a,const_cast<TransportPaymentReceipt&>(*row),state.record_selection);});
}
template<class C>Status movement_unit(C& c,const StableId& key,MovementUnitState& row,State& state){
 if(c.reading())row.id=key;else if(row.id!=key)return c.reject("MovementUnitKeyMismatch",key);
 auto& selection=state.record_selection;
 field(c,"execution",row.execution,selection);field(c,"unit_ordinal",row.ordinal,selection);
 field(c,"active_elapsed",row.active_elapsed,selection);field(c,"terrain_budget",row.budget,selection);
 field(c,"payment",row.payment,selection);field(c,"effort_basis_refs",row.effort_basis_refs,selection);
 field(c,"naval_finalized_source",row.naval_finalized_source,selection);field(c,"status",row.status,selection);
 field(c,"activity_spans",row.activity_spans,selection);
 auto& m=state.roots.movement;
 auto same_unit=[&](const auto&,const auto& child){return child.unit_ref==key;};
 children(c,"effort_basis_sources",m.exertion_bases,state.effort_bases,[&](auto& a,const StableId& id,TravelExertionBasis& v){auto status=movement_physical_fields::exertion_basis(a,v,selection,&state);if(status.ok&&id!=v.id)return a.reject("MovementEffortKeyMismatch",id);return status;},same_unit);
 c.object("expense_source",[&](auto& a){
  optional_child(a,"movement_expense",m.expenses,state.expenses,[&](auto& b,const auto& id,auto& v){auto status=value(b,v,selection);if(status.ok&&id!=v.id)return b.reject("MovementExpenseKeyMismatch",id);return status;},same_unit);
  auto& group=state.roots.executions?state.roots.executions->expenses:state.absent_group_expenses;
  optional_child(a,"group_expense",group,state.group_expenses,[&](auto& b,const auto& id,auto& v){if(!state.roots.executions)return b.reject("GroupExpenseOriginalRootRequired",id);auto status=value(b,v,selection);if(status.ok&&id!=v.id)return b.reject("GroupExpenseKeyMismatch",id);return status;},same_unit);
  return payments(a,"payment_receipts",state,TransportPaymentPhysicalOwner::movement_unit,key);
 });
 optional_child(c,"unit_close_source",m.unit_closes,state.closes,[&](auto& a,const auto& id,auto& v){auto status=value(a,v,selection);if(status.ok&&id!=v.id)return a.reject("MovementCloseKeyMismatch",id);return status;},same_unit);
 optional_child(c,"exertion_source",m.exertions,state.exertions,[&](auto& a,const auto& id,auto& v){auto status=value(a,v,selection);if(status.ok&&id!=v.id)return a.reject("TravelExertionKeyMismatch",id);return status;},same_unit);
 optional_child(c,"unit_boundary_source",m.unit_boundaries,state.boundaries,[&](auto& a,const auto& id,auto& v){auto status=value(a,v,selection);if(status.ok&&id!=v.id)return a.reject("MovementBoundaryKeyMismatch",id);return status;},same_unit);
 auto transitions=children(c,"basis_transition_sources",m.basis_transitions,state.transitions,[&](auto& a,const auto& id,auto& v){auto status=value(a,v,selection);if(status.ok&&id!=v.id)return a.reject("MovementTransitionKeyMismatch",id);return status;},same_unit);
 if(!transitions.ok)return transitions;
 return children(c,"patrol_final_motion_sources",m.patrol_final_motions,state.patrol_final_motions,[&](auto& a,const StableId& motion_id,PatrolFinalMotionSource& motion){
  auto actual=value(a,motion,selection);if(!actual.ok)return actual;
  if(motion.id!=motion_id)return a.reject("PatrolFinalMotionPhysicalKeyMismatch",motion_id);
  return children(a,"checkpoint_arrivals",m.patrol_checkpoint_arrivals,state.patrol_checkpoint_arrivals,[&](auto& b,const StableId& arrival_id,PatrolCheckpointArrival2& arrival){
   auto original=value(b,arrival,selection);if(original.ok&&arrival.source_fact_id!=arrival_id)return b.reject("PatrolCheckpointPhysicalKeyMismatch",arrival_id);return original;
  },[&](const StableId&,const PatrolCheckpointArrival2& arrival){const auto& source=arrival.final_motion_source_ref;return source.domain=="travel"&&source.kind=="patrol_final_motion"&&source.id==motion_id;});
 },same_unit);
}
template<class C>Status movement_prefix(C& c,const StableId& key,MovementPrefixState& row,State& state){
 if(c.reading())row.id=key;else if(row.id!=key)return c.reject("MovementPrefixKeyMismatch",key);
 auto checked=movement_physical_fields::prefix_core(c,row,state.record_selection,&state,state.budget);if(!checked.ok)return checked;
 auto& m=state.roots.movement;auto same_prefix=[&](const auto&,const auto& child){return child.prefix_id==key;};
 auto fields=[&](auto& a,const StableId& id,auto& v){auto result=value(a,v,state.record_selection);if(result.ok&&v.id!=id)return a.reject("MovementSourceKeyMismatch",id);return result;};
 children(c,"personal_control_sources",m.personal_controls,state.personal_controls,fields,same_prefix);
 children(c,"arrival_sources",m.arrivals,state.arrivals,fields,same_prefix);
 return optional_child(c,"personal_terminal_source",m.personal_terminals,state.personal_terminals,fields,same_prefix);
}
template<class C>Status contact(C& c,const StableId& key,ExactMovementContact& row,State& state){
 auto checked=value(c,row,state.record_selection);if(!checked.ok)return checked;if(row.id!=key)return c.reject("MovementContactKeyMismatch",key);
 auto same_contact=[&](const auto&,const auto& child){return child.contact_ref==key;};
 auto fields=[&](auto& a,const StableId& id,auto& v){auto result=value(a,v,state.record_selection);if(result.ok&&v.id!=id)return a.reject("ContactSourceKeyMismatch",id);return result;};
 auto& m=state.roots.movement;
 optional_child(c,"installation_source",m.contact_installations,state.installations,fields,same_contact);
 return children(c,"continuation_sources",m.contact_continuations,state.continuations,fields,same_contact);
}
}
