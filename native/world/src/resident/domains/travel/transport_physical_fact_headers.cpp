#include "transport_physical_fact_headers.hpp"
#include "transport_physical_ownership.hpp"
#include "movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
// One finite source-header registry, shared by the forward and reverse reads.
// No row is inferred from a kind string in a decoded JSON payload.
#define K4_MOVEMENT_HEADS(X) \
 X(movement_prefix,prefixes,id) X(movement_unit,units,id) \
 X(movement_expense,expenses,id) X(travel_exertion_basis,exertion_bases,id) \
 X(movement_unit_close,unit_closes,id) X(travel_exertion,exertions,id) \
 X(movement_unit_boundary,unit_boundaries,id) X(movement_basis_transition,basis_transitions,id) \
 X(personal_movement_control,personal_controls,id) X(personal_movement_terminal,personal_terminals,id) \
 X(movement_arrival,arrivals,id) X(terrain_credit_disposition,credit_dispositions,id) \
 X(movement_contact,contacts,id) X(movement_contact_installation,contact_installations,id) \
 X(movement_contact_continuation,contact_continuations,id) \
 X(patrol_final_motion,patrol_final_motions,id) X(patrol_checkpoint_arrival,patrol_checkpoint_arrivals,source_fact_id) \
 X(cargo_handling_terms,cargo_handling.terms,terms_id) X(cargo_handling_execution,cargo_handling.executions,execution_id) \
 X(cargo_handling_work_result,cargo_handling.work_results,result_id) X(cargo_handling_result,cargo_handling.results,result_id)
#define K4_CARRIER_HEADS(X) X(group_journey,journeys,journey_id) X(group_journey_step,steps,id) X(boundary_crossing,crossings,receipt_id)
#define K4_EXECUTION_HEADS(X) X(group_movement_opening,openings,id) X(group_transport_expense,expenses,id) X(group_movement_control,controls,id) X(group_military_supply_rebind,military_supply_rebinds,id) X(transport_entry_manifest,entry_manifests,id)
#define K4_REGISTRATION_HEADS(X) X(transport_registration,registrations,id) X(transport_release,releases,id)
template<class F>void original_tables(const TransportPhysicalCodecRoots& r,F&& visit){
 #define ROW(K,T,I) visit("travel",#K,r.movement.T,[](const auto& row)->const StableId&{return row.I;});
 K4_MOVEMENT_HEADS(ROW)
 #undef ROW
 if(r.carriers){
  #define ROW(K,T,I) visit("travel",#K,r.carriers->T,[](const auto& row)->const StableId&{return row.I;});
  K4_CARRIER_HEADS(ROW)
  #undef ROW
 }
 if(r.executions){
  #define ROW(K,T,I) visit("travel",#K,r.executions->T,[](const auto& row)->const StableId&{return row.I;});
  K4_EXECUTION_HEADS(ROW)
  #undef ROW
 }
 if(r.registrations){
  #define ROW(K,T,I) visit("travel",#K,r.registrations->T,[](const auto& row)->const StableId&{return row.I;});
  K4_REGISTRATION_HEADS(ROW)
  #undef ROW
 }
}
void original_head(const CommonStore& c,const StableId& id,std::string_view domain,std::string_view kind){
 const auto* actual=c.state().facts.find(id);
 if(id.empty()||!actual||actual->id!=id||actual->domain!=domain||actual->kind!=kind)fail("TransportPhysicalOriginalFactHeaderRequired",id);
}
void validate_one(const CommonStore& c,const TransportPhysicalCodecRoots& roots,const FactRecord& header){
 if(!owns_transport_physical_fact_kind(header.domain,header.kind))fail("TransportPhysicalFactKindNotOwned",header.id);
 original_head(c,header.id,header.domain,header.kind);
 std::size_t matches=0;
 original_tables(roots,[&](std::string_view domain,std::string_view kind,const auto& table,auto identity){
  if(header.domain!=domain||header.kind!=kind)return;
  const auto* row=table.find(header.id);if(!row)return;
  if(identity(*row)!=header.id)fail("TransportPhysicalFactPayloadIdentityMismatch",header.id);
  ++matches;
 });
 if(header.domain=="local_economy"&&header.kind=="transport_expense_paid"&&roots.payments)
  for(const auto& row:roots.payments->payments)if(row.id==header.id)++matches;
 if(matches!=1)fail("TransportPhysicalFactPayloadCoverageMismatch",header.id);
}
}
bool owns_transport_physical_fact_kind(std::string_view domain,std::string_view kind)noexcept{
 if(domain=="local_economy")return kind=="transport_expense_paid";
 if(domain!="travel")return false;
 #define KIND(K,T,I) if(kind==#K)return true;
 K4_MOVEMENT_HEADS(KIND) K4_CARRIER_HEADS(KIND) K4_EXECUTION_HEADS(KIND) K4_REGISTRATION_HEADS(KIND)
 #undef KIND
 return false;
}
Status validate_transport_physical_fact_header(const CommonStore& c,const TransportPhysicalCodecRoots& r,const FactRecord& h){return status([&]{validate_one(c,r,h);});}
Status validate_transport_physical_fact_headers(const CommonStore& c,const TransportPhysicalCodecRoots& r){return status([&]{
 take(validate_transport_physical_source_ownership(r.movement,r.carriers,r.executions,r.registrations));
 original_tables(r,[&](std::string_view domain,std::string_view kind,const auto& table,auto identity){for(const auto& row:table.rows()){
  if(row.first!=identity(row.second))fail("TransportPhysicalFactPayloadIdentityMismatch",row.first);
  original_head(c,row.first,domain,kind);
 }});
 if(r.payments)for(const auto& row:r.payments->payments)original_head(c,row.id,"local_economy","transport_expense_paid");
 for(const auto& row:c.state().facts.rows())if(owns_transport_physical_fact_kind(row.second.domain,row.second.kind)){
  if(row.first!=row.second.id)fail("TransportPhysicalCommonFactKeyMismatch",row.first);
  validate_one(c,r,row.second);
 }
});}
#undef K4_MOVEMENT_HEADS
#undef K4_CARRIER_HEADS
#undef K4_EXECUTION_HEADS
#undef K4_REGISTRATION_HEADS
}
