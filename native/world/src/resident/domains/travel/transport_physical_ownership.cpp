#include "transport_physical_ownership.hpp"
#include "movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
template<class T,class Parent,class Id,class Key>void children(const OrderedTable<T>& rows,const Parent& parents,Id identity,Key parent_key,bool unique){
 for(const auto& row:rows.rows()){
  if(row.first!=identity(row.second)||row.first.empty()||!parents.find(parent_key(row.second)))fail("PhysicalTransportSourceOwnerRequired",row.first);
  if(unique){Integer matches=0;for(const auto& other:rows.rows())if(parent_key(other.second)==parent_key(row.second))++matches;if(matches!=1)fail("PhysicalTransportSourceOwnerNotUnique",row.first);}
 }
}
template<class T>const StableId& id(const T& v){return v.id;}
}
Status validate_stationary_inventory_motion_roots(
 const MovementExactState& movement,const SpatialCarrierState& carriers,
 const GroupMovementExecutionState& groups,
 bool assignments_selected){return status([&]{
 if(!carriers.transport_units.empty()||!carriers.positions.empty()||
    !carriers.bindings.empty()||!carriers.journeys.empty()||!carriers.steps.empty()||
    !carriers.terminals.empty()||!carriers.crossings.empty()||
    !carriers.journey_purpose_bindings.empty()||!groups.openings.empty()||
    !groups.unit_terms.empty()||!groups.expenses.empty()||!groups.controls.empty()||!groups.military_supply_rebinds.empty()||
    !groups.entry_manifests.empty())fail("UnselectedGroupTransportSources");
 if(!assignments_selected&&!carriers.inventory_bindings.empty())
    fail("UnselectedInventorySpatialAssignments");
 for(const auto& row:carriers.inventory_bindings.rows()){
    const auto* site=std::get_if<StableId>(&row.second.placement);
    if(row.first!=row.second.inventory_id||!site||site->empty()||row.second.revision!=0)
      fail("StationaryInventoryOriginalPlacementRequired",row.first);
 }
 for(const auto& row:movement.prefixes.rows())
    if(std::holds_alternative<GroupMovementExecution>(row.second.execution))
      fail("UnselectedGroupTransportSources",row.first);
 for(const auto& row:movement.units.rows())
    if(std::holds_alternative<GroupMovementExecution>(row.second.execution))
      fail("UnselectedGroupTransportSources",row.first);
 for(const auto& row:movement.spatial_bindings.rows())
    if(std::holds_alternative<CarriedMovementWriter>(row.second.writer))
      fail("UnselectedGroupTransportSources",row.first);
});}
// 注册及释放另借真实根核验，不能用临时空表代替未提供的原来源。
Status validate_stationary_inventory_transport_roots(
 const MovementExactState& movement,const SpatialCarrierState& carriers,
 const GroupMovementExecutionState& groups,const TransportRegistrationState& registrations,
 bool assignments_selected){return status([&]{
 take(validate_stationary_inventory_motion_roots(movement,carriers,groups,assignments_selected));
 if(!registrations.registrations.empty()||!registrations.releases.empty())
    fail("UnselectedGroupTransportSources");
});}
Status validate_transport_physical_source_ownership(const MovementExactState& m,const SpatialCarrierState* carriers,const GroupMovementExecutionState* groups,const TransportRegistrationState* registrations){return status([&]{
 children(m.exertion_bases,m.units,id<TravelExertionBasis>,[](const auto& v)->const StableId&{return v.unit_ref;},false);
 children(m.expenses,m.units,id<MovementExpenseSource>,[](const auto& v)->const StableId&{return v.unit_ref;},true);
 children(m.exertions,m.units,id<TravelExertionSource>,[](const auto& v)->const StableId&{return v.unit_ref;},true);
 children(m.unit_closes,m.units,id<MovementUnitCloseSource>,[](const auto& v)->const StableId&{return v.unit_ref;},true);
 children(m.unit_boundaries,m.units,id<MovementUnitBoundarySource>,[](const auto& v)->const StableId&{return v.unit_ref;},true);
 children(m.basis_transitions,m.units,id<MovementBasisTransitionSource>,[](const auto& v)->const StableId&{return v.unit_ref;},false);
 children(m.personal_controls,m.prefixes,id<PersonalMovementControlSource>,[](const auto& v)->const StableId&{return v.prefix_id;},false);
 children(m.arrivals,m.prefixes,id<MovementArrivalSource>,[](const auto& v)->const StableId&{return v.prefix_id;},true);
 children(m.personal_terminals,m.prefixes,id<PersonalMovementTerminalSource>,[](const auto& v)->const StableId&{return v.prefix_id;},true);
 children(m.contact_installations,m.contacts,id<MovementContactInstallationSource>,[](const auto& v)->const StableId&{return v.contact_ref;},true);
 children(m.contact_continuations,m.contacts,id<MovementContactContinuationSource>,[](const auto& v)->const StableId&{return v.contact_ref;},false);
 children(m.patrol_final_motions,m.units,id<PatrolFinalMotionSource>,[](const auto& v)->const StableId&{return v.unit_ref;},false);
 children(m.patrol_checkpoint_arrivals,m.patrol_final_motions,[](const auto& v)->const StableId&{return v.source_fact_id;},[](const auto& v)->const StableId&{return v.final_motion_source_ref.id;},false);
 for(const auto& row:m.patrol_checkpoint_arrivals.rows())if(row.second.final_motion_source_ref.domain!="travel"||row.second.final_motion_source_ref.kind!="patrol_final_motion")fail("PatrolCheckpointPhysicalOriginalSourceRequired",row.first);
 for(const auto& row:m.patrol_final_motions.rows()){
  bool used=false;for(const auto& arrival:m.patrol_checkpoint_arrivals.rows())if(arrival.second.final_motion_source_ref.id==row.first)used=true;
  if(!used)fail("UnreferencedPatrolPhysicalFinalMotion",row.first);
 }
 if(carriers&&groups&&registrations){
 const auto& s=*carriers;const auto& g=*groups;const auto& r=*registrations;
 children(g.expenses,m.units,id<GroupTransportExpenseSource>,[](const auto& v)->const StableId&{return v.unit_ref;},true);
 children(g.openings,s.journeys,id<GroupMovementOpeningSource>,[](const auto& v)->const StableId&{return v.journey_id;},true);
 children(g.unit_terms,s.journeys,[](const auto& v)->const StableId&{return v.unit_ref;},[](const auto& v)->const StableId&{return v.journey_id;},false);
 children(g.controls,s.journeys,id<GroupMovementControlSource>,[](const auto& v)->const StableId&{return v.journey_id;},false);
 children(g.military_supply_rebinds,s.journeys,id<GroupMilitarySupplyRebindSource>,[](const auto& v)->const StableId&{return v.journey_id;},false);
 for(const auto& row:g.entry_manifests.rows()){
  if(row.first!=row.second.id)fail("TransportEntryManifestKeyMismatch",row.first);
  Integer matches=0;for(const auto& crossing:s.crossings.rows()){const auto& c=crossing.second;const auto& e=row.second;if(execution_key(c.execution)==execution_key(e.execution)&&c.entry_ordinal==e.entry_ordinal&&c.boundary.kind==e.boundary.kind&&c.boundary.id==e.boundary.id&&c.boundary.map_id==e.boundary.map_id&&c.boundary.from_site_id==e.boundary.from_site_id&&c.boundary.to_site_id==e.boundary.to_site_id)++matches;}
  if(matches!=1)fail("TransportEntryPhysicalCrossingOwnerRequired",row.first);
 }
 for(const auto& row:r.registrations.rows())if(row.first!=row.second.id||r.releases.find(row.first)||!s.transport_units.find(row.second.transport_unit_id))fail("TransportRegistrationPhysicalOwnerRequired",row.first);
 for(const auto& row:r.releases.rows())if(row.first!=row.second.id||!s.transport_units.find(row.second.transport_unit_id))fail("TransportReleasePhysicalOwnerRequired",row.first);
 for(const auto& row:s.journey_purpose_bindings.rows())if(row.first!=row.second.journey_id||!s.journeys.find(row.second.journey_id))fail("JourneyPurposePhysicalOwnerRequired",row.first);
 }else{
  if(carriers||groups||registrations)fail("TransportPhysicalGroupRootsNotPaired");
  for(const auto& row:m.prefixes.rows())if(std::holds_alternative<GroupMovementExecution>(row.second.execution))fail("GroupMovementPhysicalRootsRequired",row.first);
  for(const auto& row:m.units.rows())if(std::holds_alternative<GroupMovementExecution>(row.second.execution))fail("GroupMovementPhysicalRootsRequired",row.first);
 }
 for(const auto& row:m.passage_modifier_adoptions.rows()){
  const auto& adoption=row.second;
  if(row.first!=adoption.adoption_id)fail("PassageModifierPhysicalKeyMismatch",row.first);
  bool actual_execution=false;for(const auto& prefix:m.prefixes.rows())if(execution_key(prefix.second.execution)==execution_key(adoption.execution))actual_execution=true;
  if(!actual_execution)fail("PassageModifierPhysicalExecutionRequired",row.first);
 }
 for(const auto& row:m.cargo_handling.terms.rows()){if(row.first!=row.second.terms_id)fail("CargoTermsPhysicalIdentityMismatch",row.first);Integer count=0;for(const auto& e:m.cargo_handling.executions.rows())if(e.second.terms_ref==row.first)++count;if(count!=1)fail("CargoTermsPhysicalExecutionRequired",row.first);}
 for(const auto& row:m.cargo_handling.work_results.rows()){const auto* e=m.cargo_handling.executions.find(row.second.execution_ref);if(row.first!=row.second.result_id||!e||e->work_result_ref!=std::optional<StableId>{row.first}||e->terms_ref!=row.second.terms_ref)fail("CargoWorkPhysicalOwnerRequired",row.first);}
 for(const auto& row:m.cargo_handling.results.rows()){const auto* e=m.cargo_handling.executions.find(row.second.execution_ref);if(row.first!=row.second.result_id||!e||e->terminal_result_ref!=std::optional<StableId>{row.first}||e->terms_ref!=row.second.terms_ref)fail("CargoResultPhysicalOwnerRequired",row.first);}
});}
}
