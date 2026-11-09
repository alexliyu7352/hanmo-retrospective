#include "resident/kernel/catalog_section_observation.hpp"
#include "military_body_footprint.hpp"
#include "naval_vessel_identity.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;
Status validate_military_body_footprint_catalog(const MilitaryBodyFootprintCatalog2& c,
 const MilitaryBattleCatalog& battle,const MilitarySpatialOriginalSourceRead& spatial,
 const NavalAssetCatalog* naval,ExactArithmeticBudget b){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 80, "MilitaryBodyFootprintCatalog2", "validate_military_body_footprint_catalog", &c);
return status([&]{
 std::set<std::tuple<StableId,std::size_t,StableId>> selections;
 for(const auto& row:c.definitions.rows()){
  const auto& v=row.second;
  if(row.first!=v.id||v.id.empty()||v.version<1||v.geometry_ref.empty())
   fail("MilitaryBodyFootprintDefinitionRequired",row.first);
  const auto geometry=take(spatial.read_original_spatial_definition(v.geometry_ref));
  if(geometry.id!=v.geometry_ref)fail("MilitaryBodyFootprintGeometryMismatch",v.id);
  take(validate_military_spatial_geometry_definition(geometry,b));
  const auto body_id=std::visit([&](const auto& original)->StableId{
   using T=std::decay_t<decltype(original)>;std::size_t matches=0;
   if constexpr(std::is_same_v<T,MilitaryTroopFootprintRef>){
    for(const auto& troop:battle.troop_types)if(troop.id==original.troop_type_id)++matches;
    if(matches!=1||original.troop_type_id.empty())fail("MilitaryFootprintOriginalTroopRequired",v.id);
    return original.troop_type_id;
   }else{
    if(!naval)fail("MilitaryFootprintOriginalHullCatalogRequired",v.id);
    for(const auto& hull:naval->hulls)if(hull.id==original.hull_id)++matches;
    if(matches!=1||original.hull_id.empty())fail("MilitaryFootprintOriginalHullRequired",v.id);
    return original.hull_id;
   }
  },v.original_body_definition);
  if(!selections.emplace(v.geometry_ref,v.original_body_definition.index(),body_id).second)
   fail("AmbiguousMilitaryBodyFootprintSelection",v.id);
  take(validate_military_convex_footprint(v.offsets,b));
 }
});}
Outcome<const MilitaryBodyFootprintDefinition2*> read_military_body_footprint_definition(
 const MilitaryBodyFootprintCatalog2& c,const SourceRef& source,Integer schema,
 ExactArithmeticBudget b){return guard<const MilitaryBodyFootprintDefinition2*>([&]{
 if(source.domain!="military_catalog"||source.kind!="body_footprint"||schema!=2)
  fail("MilitaryBodyFootprintSourceRequired",source.id);
 const auto* row=c.definitions.find(source.id);
 if(!row||row->id!=source.id||row->version<1||row->geometry_ref.empty())
  fail("MilitaryBodyFootprintSourceRequired",source.id);
 take(validate_military_convex_footprint(row->offsets,b));return row;
});}
static Outcome<MilitarySessionBodyFootprintsRead> read_body_footprints(
 const MilitaryBodyFootprintReadContext& x,const StableId& session_id,Integer generation,
 const WorldMoment& upper,bool land_only){return guard<MilitarySessionBodyFootprintsRead>([&]{
 const auto& m=x.motion;const auto b=m.budget;
 take(validate_military_body_footprint_catalog(x.footprints,x.military_catalog.battle_catalog,
     m.spatial,x.naval_catalog,b));
 auto motion=take(predict_actual_military_session_motion(m,session_id,generation,upper));
 std::vector<StableId> unit_ids;
 for(const auto& body:motion.bodies){
  const auto* unit=std::get_if<MilitaryLocalUnitRef>(&body.actual_body->local_body_ref);
  if(!unit){if(land_only)fail("MilitaryVesselFootprintHullHistoryRequired",session_id);continue;}
  unit_ids.push_back(unit->unit_id);
 }
 MilitarySessionBodyFootprintsRead out;out.actual_session=motion.actual_session;
 out.common_window=motion.common_window;
 MilitaryRosterHistoryRead rosters;
 if(!unit_ids.empty())rosters=take(read_military_rosters_exact(m.common,m.military,x.military_catalog,
     m.exact,unit_ids,motion.common_window.from,{},x.population,x.population_catalog,b,
     x.formations,x.returns,x.periods));
 for(auto& body:motion.bodies){
  MilitaryActualBodyFootprintRead actual;StableId original_definition_id;
  const bool is_unit=std::holds_alternative<MilitaryLocalUnitRef>(body.actual_body->local_body_ref);
  if(is_unit){
   const auto& unit_id=std::get<MilitaryLocalUnitRef>(body.actual_body->local_body_ref).unit_id;
   const MilitaryUnitRecord* roster=nullptr;
   for(const auto& unit:rosters.units)if(unit.id==unit_id){
    if(roster)fail("MilitaryFootprintDuplicateOriginalUnit",unit_id);roster=&unit;
   }
   if(!roster)fail("MilitaryFootprintOriginalRosterRequired",unit_id);
   const DynamicMilitaryUnitTacticalState* entry=nullptr;
   for(const auto& unit:motion.actual_session->unit_tactical_states)
    if(military_local_body_key(unit.body_ref)==military_local_body_key(body.actual_body->local_body_ref)){
     if(entry||unit.unit_ref!=unit_id)fail("MilitaryFootprintOriginalBodyMembershipRequired",unit_id);
     entry=&unit;
    }
   if(!entry)fail("MilitaryFootprintOriginalBodyMembershipRequired",unit_id);
   const auto original_entry=take(m.sessions.read_actual_unit_entry(entry->entry_snapshot_ref));
   if(original_entry.unit_ref!=unit_id||
      military_local_body_key(original_entry.body_ref)!=military_local_body_key(body.actual_body->local_body_ref))
    fail("MilitaryFootprintOriginalEntryMismatch",unit_id);
   original_definition_id=roster->troop_type_id;
   actual.original_entry_snapshot=entry->entry_snapshot_ref;
   for(const auto& source:rosters.trace.authored_unit_sources)
    actual.original_roster_sources.push_back({source,1});
   for(const auto& source:rosters.trace.original_changes)
    actual.original_roster_sources.push_back({source.source_ref,source.source_schema});
  }else{
   const auto& vessel_id=std::get<MilitaryLocalVesselRef>(body.actual_body->local_body_ref).vessel_id;
   if(!x.naval_catalog||!x.naval_assets)fail("MilitaryVesselOriginalAssetIdentityRequired",vessel_id);
   actual.original_vessel=take(read_original_naval_vessel_identity(*x.naval_assets,*x.naval_catalog,vessel_id));
   original_definition_id=actual.original_vessel->hull_id;
  }
  const auto& basis=*body.motion.actual_basis;
  const MilitaryBodyFootprintDefinition2* selected=nullptr;
  for(const auto& row:x.footprints.definitions.rows()){
   const auto& definition=row.second;
   const auto* troop=std::get_if<MilitaryTroopFootprintRef>(&definition.original_body_definition);
   const auto* hull=std::get_if<MilitaryHullFootprintRef>(&definition.original_body_definition);
   const bool matches=is_unit?(troop&&troop->troop_type_id==original_definition_id):
       (hull&&hull->hull_id==original_definition_id);
   if(matches&&definition.geometry_ref==basis.geometry_definition_ref){
    if(selected)fail("AmbiguousMilitaryBodyFootprintSelection",basis.id);selected=&definition;
   }
  }
  if(!selected)fail("AuthoredMilitaryBodyFootprintRequired",basis.id);
  actual.actual_definition=selected;
  actual.original_shape={{"military_catalog","body_footprint",selected->id},2};
  actual.oriented_offsets=take(orient_military_body_footprint(selected->offsets,
      body.actual_body->heading,b));
  actual.actual_motion=std::move(body);out.bodies.push_back(std::move(actual));
 }
 return out;
});}
namespace {
WorldMoment actual_contact_parameter_moment(const MilitarySessionBodyFootprintsRead& inputs,
 const CanonicalFraction& parameter,ExactArithmeticBudget b){
 if(compare(parameter,{},b)<0||compare(parameter,integer(1,b),b)>0)
  fail("MilitaryContactParameterOutsideActualPiece",inputs.actual_session->session_ref);
 const auto duration=take(elapsed_world_duration(inputs.common_window.from,inputs.common_window.to,b));
 return take(add_world_duration(inputs.common_window.from,
     HalfDayDuration{mul(duration.value,parameter,b)},b));
}
template<class Clear>
bool retain_actual_earliest_gate(const WorldMoment& at,std::optional<WorldMoment>& earliest,
 Clear&& clear,ExactArithmeticBudget b){
 const auto order=earliest?take(compare_world_moments(at,*earliest,b)):-1;
 if(order>0)return false;
 if(order<0){earliest=at;clear();}
 return true;
}
template<class Emit>
void collect_actual_body_contact_candidates(const MilitarySessionBodyFootprintsRead& inputs,
 Emit&& emit,ExactArithmeticBudget b){
 struct WorldBody {MilitarySpatialSegment centers;std::vector<MilitarySpatialPoint> offsets;};
 std::vector<WorldBody> world;world.reserve(inputs.bodies.size());
 const MilitarySpatialChart2* common_chart=nullptr;
 for(const auto& body:inputs.bodies){
  const auto& chart=*body.actual_motion.actual_chart;
  if(common_chart&&(chart.world_geometry_ref!=common_chart->world_geometry_ref||
      chart.world_length_unit_ref!=common_chart->world_length_unit_ref))
   fail("MilitaryContactCommonWorldGeometryRequired",inputs.actual_session->session_ref);
  common_chart=&chart;WorldBody actual;
  actual.centers={take(military_chart_to_world(chart,body.actual_motion.motion.centers.from,b)),
      take(military_chart_to_world(chart,body.actual_motion.motion.centers.to,b))};
  actual.offsets.reserve(body.oriented_offsets.size());
  for(const auto& offset:body.oriented_offsets)
   actual.offsets.push_back(take(military_chart_vector_to_world(chart,offset,b)));
  world.push_back(std::move(actual));
 }
 for(std::size_t first=0;first<world.size();++first)
  for(std::size_t second=first+1;second<world.size();++second){
   const auto contact=take(intersect_military_convex_body_motion(world[first].offsets,
       world[first].centers,world[second].offsets,world[second].centers,b));
   if(contact.initially_penetrating)
    fail("MilitaryActualEntryColocationProofRequired",inputs.actual_session->session_ref);
   if(!contact.first_inward_contact)continue;
   if(!contact.closed_contact)fail("MilitaryBodyContactWindowRequired",inputs.actual_session->session_ref);
   emit(*contact.first_inward_contact,MilitaryBodyContactPairPrediction{
       first,second,*contact.closed_contact,*contact.first_inward_contact});
  }
}
template<class Emit>
void collect_actual_domain_contact_candidates(const MilitarySessionBodyFootprintsRead& inputs,
 Emit&& emit,ExactArithmeticBudget b){
 for(std::size_t n=0;n<inputs.bodies.size();++n){
  const auto& body=inputs.bodies[n];
  const auto contact=take(intersect_military_convex_domain_motion(body.oriented_offsets,
      body.actual_motion.motion.centers,body.actual_motion.actual_chart->domain_polygon,b));
  if(contact.initially_outside)
   fail("MilitaryActualPortalBodyPassageRequired",inputs.actual_session->session_ref);
  if(!contact.first_outward_contact)continue;
  if(!contact.closed_inside_window)
   fail("MilitaryBodyDomainContactWindowRequired",inputs.actual_session->session_ref);
  emit(*contact.first_outward_contact,MilitaryBodyDomainContactPrediction{
      n,*contact.closed_inside_window,*contact.first_outward_contact});
 }
}
}
static Outcome<MilitaryBodyContactPrediction> predict_body_contacts(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,
 const WorldMoment& upper,bool land_only){return guard<MilitaryBodyContactPrediction>([&]{
 const auto b=x.motion.budget;MilitaryBodyContactPrediction out;
 out.original_inputs=take(read_body_footprints(x,session,generation,upper,land_only));
 collect_actual_body_contact_candidates(out.original_inputs,[&](const auto& parameter,auto candidate){
  const auto at=actual_contact_parameter_moment(out.original_inputs,parameter,b);
  if(retain_actual_earliest_gate(at,out.first_contact_at,[&]{out.simultaneous_pairs.clear();},b))
   out.simultaneous_pairs.push_back(std::move(candidate));
 },b);
 return out;
});}
Outcome<MilitarySessionBodyFootprintsRead> read_actual_land_session_body_footprints(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,const WorldMoment& upper){
 return read_body_footprints(x,session,generation,upper,true);
}
Outcome<MilitarySessionBodyFootprintsRead> read_actual_session_body_footprints(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,const WorldMoment& upper){
 return read_body_footprints(x,session,generation,upper,false);
}
Outcome<MilitaryLandBodyContactPrediction> predict_actual_land_body_contacts(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,const WorldMoment& upper){
 return predict_body_contacts(x,session,generation,upper,true);
}
Outcome<MilitaryBodyContactPrediction> predict_actual_military_body_contacts(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,const WorldMoment& upper){
 return predict_body_contacts(x,session,generation,upper,false);
}
Outcome<MilitarySessionDomainContactPrediction> predict_actual_military_domain_contacts(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,
 const WorldMoment& upper){return guard<MilitarySessionDomainContactPrediction>([&]{
 const auto b=x.motion.budget;MilitarySessionDomainContactPrediction out;
 out.original_inputs=take(read_actual_session_body_footprints(x,session,generation,upper));
 collect_actual_domain_contact_candidates(out.original_inputs,[&](const auto& parameter,auto candidate){
  const auto at=actual_contact_parameter_moment(out.original_inputs,parameter,b);
  if(retain_actual_earliest_gate(at,out.first_contact_at,[&]{out.simultaneous_boundaries.clear();},b))
   out.simultaneous_boundaries.push_back(std::move(candidate));
 },b);
 return out;
});}
Outcome<MilitarySessionSpatialGatePrediction> predict_actual_military_body_domain_and_edge_gates(
 const MilitaryBodyFootprintReadContext& x,const StableId& session,Integer generation,
 const WorldMoment& upper){return guard<MilitarySessionSpatialGatePrediction>([&]{
 const auto b=x.motion.budget;MilitarySessionSpatialGatePrediction out;
 out.original_inputs=take(read_actual_session_body_footprints(x,session,generation,upper));
 const auto& inputs=out.original_inputs;
 auto retain=[&](const WorldMoment& at){return retain_actual_earliest_gate(at,out.first_gate_at,[&]{
  out.simultaneous_pairs.clear();out.simultaneous_boundaries.clear();
  out.completed_edge_body_ordinals.clear();
 },b);};
 // The actual session motion reader has already cut every body at the earliest
 // edge due. A requested upper bound alone is never an edge completion.
 for(std::size_t n=0;n<inputs.bodies.size();++n){
  const auto& due=inputs.bodies[n].actual_motion.motion.actual_edge_due;
  if(due&&same_moment(*due,inputs.common_window.to,b)&&retain(*due))
   out.completed_edge_body_ordinals.push_back(n);
 }
 collect_actual_body_contact_candidates(inputs,[&](const auto& parameter,auto candidate){
  if(retain(actual_contact_parameter_moment(inputs,parameter,b)))
   out.simultaneous_pairs.push_back(std::move(candidate));
 },b);
 collect_actual_domain_contact_candidates(inputs,[&](const auto& parameter,auto candidate){
  if(retain(actual_contact_parameter_moment(inputs,parameter,b)))
   out.simultaneous_boundaries.push_back(std::move(candidate));
 },b);
 return out;
});}
}
