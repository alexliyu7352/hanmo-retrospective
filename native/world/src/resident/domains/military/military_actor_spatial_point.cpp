#include "military_actor_spatial_point.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool has(const std::vector<StableId>& a,const StableId& id){return std::find(a.begin(),a.end(),id)!=a.end();}
bool same_writer(const BattleMovementWriter& a,const BattleMovementWriter& b){return a.session_id==b.session_id&&a.generation==b.generation&&a.handoff_id==b.handoff_id;}
void add_source(std::vector<SourceRef>& out,const SourceRef& ref){if(!source_valid(ref))fail("MilitaryActorSpatialOriginalSourceRequired",ref.id);for(const auto& v:out)if(same_source(v,ref))return;out.push_back(ref);}
void header(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("MilitaryActorSpatialHeaderRequired",ref.id);}
}
Outcome<MilitaryActorSpatialPointView> read_actual_military_actor_spatial_point(const MilitaryActorSpatialPointContext& x,const StableId& actor,const BattleMovementWriter& requested){return guard<MilitaryActorSpatialPointView>([&]{
 const auto now=take(x.clock.read_current_moment());if(!x.common.actor(actor)||requested.session_id.empty()||requested.handoff_id.empty())fail("MilitaryActorSpatialIdentityRequired",actor);
 const auto* binding=x.movement.spatial_bindings.find("actor:"+actor);if(!binding||binding->subject_kind!="actor"||binding->subject_id!=actor)fail("MilitaryActorSpatialBindingRequired",actor);
 SpatialWriterBinding effective=*binding;if(const auto* carried=std::get_if<CarriedMovementWriter>(&binding->writer))effective=take(read_resolved_spatial_writer(x.movement,carried->carrier_ref));
 const auto* writer=std::get_if<BattleMovementWriter>(&effective.writer);if(!writer||!same_writer(*writer,requested))fail("MilitaryActorSpatialWriterMismatch",actor);
 const auto* handoff=x.exact.spatial_handoffs.handoffs.find(writer->handoff_id);const auto* entry=handoff?std::get_if<BattleMovementWriter>(&handoff->to_binding.writer):nullptr;const auto* chart=handoff?x.exact.spatial_handoffs.charts.find(handoff->chart_ref):nullptr;
 const SourceRef handoff_ref{"military","military_spatial_handoff",writer->handoff_id};header(x.common,handoff_ref);
 if(!handoff||!entry||!same_writer(*entry,*writer)||!chart||chart->session_ref!=writer->session_id||take(compare_world_moments(handoff->at,now,x.budget))>0||!same_source(effective.binding_source,handoff_ref))fail("MilitaryActorSpatialHandoffMismatch",actor);
 const auto actual_entry=take(x.handoffs.read_actual_military_entry(handoff->entry_or_exit_source_ref));
 if(actual_entry.session_ref!=writer->session_id||actual_entry.generation!=writer->generation||spatial_carrier_key(actual_entry.carrier)!=spatial_carrier_key(handoff->carrier_ref)||!same_source(actual_entry.entry_source,handoff->entry_or_exit_source_ref)||!same_moment(actual_entry.occurred_at,handoff->at,x.budget)||!has(actual_entry.actual_named_actor_ids,actor))fail("MilitaryActorActualEntryRequired",actor);
 take(validate_military_spatial_handoffs(x.common,x.movement,x.exact.local_motion,x.exact.spatial_handoffs,x.handoffs,x.budget));
 const auto* session=take(read_dynamic_military_session_current(x.common,x.military,x.exact,x.sessions,writer->session_id,writer->generation,now,x.budget));
 const auto* initialized=x.exact.session_state_sources.find(session->initialization_source_ref.id);const auto* initial=initialized?std::get_if<DynamicSessionInitialized>(&initialized->payload):nullptr;
 if(!initial||initialized->session_ref!=session->session_ref||initialized->generation!=session->generation||(!same_source(initial->entry_or_contact_source_ref,actual_entry.entry_source)&&!same_source(initial->entry_or_contact_source_ref,actual_entry.world_contact_source)))fail("MilitaryActorSessionEntryMismatch",actor);
 std::vector<StableId> unit_ids;std::set<StableId> unique;
 for(const auto& row:initial->unit_entry_rows){const auto original=take(x.sessions.read_actual_unit_entry(row.entry_snapshot_ref));if(original.unit_ref!=row.unit_ref||military_local_body_key(original.body_ref)!=military_local_body_key(row.body_ref)||!unique.insert(row.unit_ref).second)fail("MilitaryActorUnitEntryMismatch",row.unit_ref);unit_ids.push_back(row.unit_ref);}
 if(unit_ids.empty())fail("MilitaryActorOriginalUnitSourceRequired",actor);
 // This fold reads all original changes, including real casualty2 sources. It
 // never chooses a body from the mutable MilitaryBattleRecord.units projection.
 const auto rosters=take(read_military_rosters_exact(x.common,x.military,x.catalog,x.exact,unit_ids,now,{},x.population,x.population_catalog,x.budget,x.formations,x.returns,x.periods));
 const DynamicMilitaryUnitEntry* selected=nullptr;const MilitaryUnitRecord* actual_unit=nullptr;
 for(const auto& row:initial->unit_entry_rows){const MilitaryUnitRecord* unit=nullptr;for(const auto& candidate:rosters.units)if(candidate.id==row.unit_ref)unit=&candidate;if(!unit)fail("MilitaryActorUnitHistoryRequired",row.unit_ref);
  if(unit->commander_actor_id!=actor&&!has(unit->member_ids,actor))continue;
  if(has(unit->captured,actor)||has(unit->deserted,actor)||has(unit->dead,actor))fail("MilitaryActorDispositionPlacementRequired",actor);
  if(selected)fail("AmbiguousMilitaryActorLocalBody",actor);selected=&row;actual_unit=unit;
 }
 if(!selected||!actual_unit)fail("MilitaryActorOriginalBodyBindingRequired",actor);
 if(const auto* army=std::get_if<ArmyCarrierRef>(&handoff->carrier_ref);army&&actual_unit->army_id!=army->army_id)fail("MilitaryActorOriginalArmyBodyMismatch",actor);
 if(!std::holds_alternative<MilitaryLocalUnitRef>(selected->body_ref))fail("MilitaryActorAboardPlacementSourceRequired",actor);
 bool current_unit=false,current_body=false;for(const auto& row:session->unit_tactical_states)if(row.unit_ref==selected->unit_ref){if(military_local_body_key(row.body_ref)!=military_local_body_key(selected->body_ref)||!same_source(row.entry_snapshot_ref,selected->entry_snapshot_ref))fail("MilitaryActorBodyHandoffSourceRequired",actor);current_unit=true;}for(const auto& row:session->body_tactical_states)if(military_local_body_key(row.local_body_ref)==military_local_body_key(selected->body_ref))current_body=true;
 if(!current_unit||!current_body)fail("MilitaryActorCurrentBodyRequired",actor);
 const auto key=military_local_body_key(selected->body_ref);bool entered_body=false;for(const auto& body:actual_entry.actual_bodies)if(military_local_body_key(body)==key)entered_body=true;if(!entered_body)fail("MilitaryActorBodyNotInActualEntry",actor);
 const auto* cursor=x.exact.local_motion.cursors.find(key);const auto* basis=cursor?x.exact.local_motion.bases.find(cursor->basis_ref):nullptr;
 if(!cursor||!basis||cursor->phase==LocalMotionPhase::closed||basis->session_ref!=writer->session_id||basis->chart_ref!=chart->id||military_local_body_key(basis->local_body_ref)!=key||military_local_body_key(cursor->local_body_ref)!=key||execution_key(cursor->execution_ref)!=execution_key(basis->execution_ref)||spatial_carrier_key(basis->carrier_ref)!=spatial_carrier_key(handoff->carrier_ref)||take(compare_world_moments(cursor->last_settled_at,now,x.budget))>0||(cursor->phase==LocalMotionPhase::moving&&!same_moment(cursor->last_settled_at,now,x.budget)))fail("MilitaryActorLocalCursorRequired",actor);
 take(validate_local_motion_basis(x.exact.local_motion,*basis,x.local_motion,x.budget));take(x.local_motion.validate_original_local_cursor(*basis,*cursor));
 const auto local=take(read_local_motion_position(x.exact.local_motion,*cursor,x.budget));const auto world=take(military_chart_to_world(*chart,local.current,x.budget));
 MilitaryActorSpatialPointView out;out.actor_id=actor;out.session_ref=writer->session_id;out.generation=writer->generation;out.carrier=handoff->carrier_ref;out.body=selected->body_ref;out.actual_handoff=handoff;out.actual_session=session;out.actual_basis=basis;out.actual_cursor=cursor;out.actual_chart=chart;out.local_position=local.current;out.world_position=world;out.observed_at=now;
 add_source(out.original_sources,handoff_ref);add_source(out.original_sources,actual_entry.entry_source);add_source(out.original_sources,session->initialization_source_ref);add_source(out.original_sources,selected->entry_snapshot_ref);add_source(out.original_sources,basis->intent_source_ref);add_source(out.original_sources,basis->unit_or_vessel_capability_source_ref);add_source(out.original_sources,cursor->phase_source_ref);
 for(const auto& source:rosters.trace.authored_unit_sources)add_source(out.original_sources,source);for(const auto& source:rosters.trace.original_changes)add_source(out.original_sources,source.source_ref);if(source_valid(local.topology_anchor_source_ref))add_source(out.original_sources,local.topology_anchor_source_ref);
 out.known_source_schemas.push_back({handoff_ref,2});out.known_source_schemas.push_back({session->initialization_source_ref,2});
 for(const auto& source:rosters.trace.authored_unit_sources)out.known_source_schemas.push_back({source,1});
 for(const auto& source:rosters.trace.original_changes)out.known_source_schemas.push_back({source.source_ref,source.source_schema});
 return out;
});}
Outcome<ActorSpatialLocationView> NativeBattleActorSpatialRead::read_battle_actor_location(const StableId& actor,const BattleMovementWriter& writer)const{return guard<ActorSpatialLocationView>([&]{
 const auto actual=take(read_actual_military_actor_spatial_point(context_,actor,writer));
 if(!actual.actual_chart||!actual.actual_handoff||!actual.actual_cursor||!actual.actual_basis)fail("MilitaryActorSpatialSourceSchemaReadRequired",actor);
 const auto* unit=std::get_if<MilitaryLocalUnitRef>(&actual.body);const auto* person=context_.common.actor(actor);
 if(!unit||!person)fail("MilitaryActorOriginalBodyBindingRequired",actor);
 // Common remains the semantic InArmy/Carried identity. It is never a local
 // position sampler, and the exact chart point is not rounded back into it.
 const auto* army=std::get_if<ArmyCarrierRef>(&actual.carrier);const auto* location=std::get_if<InArmyLocation>(&person->location);
 if(!army||!location||location->army_id!=army->army_id)fail("MilitaryActorOriginalLocationIdentityRequired",actor);
 ActorSpatialLocationView out;out.actor_id=actor;out.location_snapshot=person->location;
 out.actual_anchor=ActualMilitaryActorPosition{actual.session_ref,actual.generation,unit->unit_id,actual.actual_chart,actual.world_position,actual.observed_at};
 for(const auto& source:actual.original_sources){Integer schema=0;for(const auto& known:actual.known_source_schemas)if(same_source(known.source_ref,source)){if(schema&&schema!=known.source_schema)fail("MilitaryActorSpatialSourceSchemaConflict",source.id);schema=known.source_schema;}if(!schema){if(!context_.source_schemas||!context_.source_schemas->bound_to(context_.common,context_.exact))fail("MilitaryActorSpatialSourceSchemaReadRequired",source.id);schema=take(context_.source_schemas->read_actual_spatial_source_schema(source));}if(schema<1)fail("MilitaryActorSpatialSourceSchemaRequired",source.id);out.actual_location_sources.push_back({source,schema});}
 if(out.actual_location_sources.empty())fail("MilitaryActorSpatialSourceHeadRequired",actor);
 return out;
});}

}
