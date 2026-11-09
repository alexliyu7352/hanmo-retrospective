#include "spatial_initial_state.hpp"
#include "movement_exact_detail.hpp"
namespace hanmo::resident {namespace {
using namespace movement_detail;
void empty(const MovementExactState& s){
 if(!s.local_moves.empty()||!s.passage_modifier_adoptions.empty()||!s.patrol_final_motions.empty()||!s.patrol_checkpoint_arrivals.empty()||
 !s.cargo_handling.terms.empty()||!s.cargo_handling.executions.empty()||!s.cargo_handling.work_results.empty()||!s.cargo_handling.results.empty()||
 !s.personal_controls.empty()||!s.contact_continuations.empty()||!s.unit_boundaries.empty()||!s.contact_installations.empty()||!s.personal_terminals.empty()||
 !s.prefixes.empty()||!s.units.empty()||!s.spatial_bindings.empty()||!s.contacts.empty()||!s.credit_dispositions.empty()||!s.arrivals.empty()||!s.expenses.empty()||!s.exertions.empty()||!s.exertion_bases.empty()||!s.unit_closes.empty()||!s.basis_transitions.empty())fail("SpatialInitialStateNotEmpty");
}
void empty(const SpatialCarrierState& s){if(!s.transport_units.empty()||!s.positions.empty()||!s.bindings.empty()||!s.journeys.empty()||!s.steps.empty()||!s.terminals.empty()||!s.inventory_bindings.empty()||!s.crossings.empty()||!s.journey_purpose_bindings.empty())fail("SpatialCarrierInitialStateNotEmpty");}
void empty(const GroupMovementExecutionState& s){if(!s.openings.empty()||!s.unit_terms.empty()||!s.expenses.empty()||!s.controls.empty()||!s.entry_manifests.empty())fail("GroupExecutionInitialStateNotEmpty");}
void empty(const TransportRegistrationState& s){if(!s.registrations.empty()||!s.releases.empty())fail("TransportRegistrationInitialStateNotEmpty");}
}
Status initialize_exact_spatial_state_in_candidate(const CommonStore& common,const CommonCatalog& shared,
 const TravelState& travel,const TravelCatalog& catalog,const AuthoredActorFoundationState& foundation,
 const ActorFoundationCatalog& actors,const FamilyCatalog* family,const CurrentMomentRead& clock,
 const MotionExactAuthorSelection& selection,MovementExactState& movement,SpatialCarrierState* carriers,
 GroupMovementExecutionState* groups,TransportRegistrationState* registrations,ExactArithmeticBudget budget){return movement_detail::status([&]{
 using namespace movement_detail;
 if((selection.travel_schema!=4&&selection.travel_schema!=5)||(selection.group_schema!=0&&selection.group_schema!=4&&selection.group_schema!=5&&selection.group_schema!=6)||
 (selection.manifest_binding_schema!=0&&selection.manifest_binding_schema!=2)||
 (selection.cargo_handling_schema!=0&&selection.cargo_handling_schema!=1)||
 (selection.patrol_checkpoint_schema!=0&&selection.patrol_checkpoint_schema!=2))fail("UnsupportedExactSpatialInitialSelection");
 const bool group=selection.group_schema!=0||selection.manifest_binding_schema!=0;
 if(group&&(!carriers||!groups||!registrations))fail("SpatialInitialGroupRootsRequired");
 // 暂定静置箱也由原 Manifest2 唯一拥有；不因此产生 transport unit 或旅程。
 if(selection.group_schema&&!selection.manifest_binding_schema)fail("SpatialInitialGroupSelectionRequired");
 empty(movement);if(carriers)empty(*carriers);if(groups)empty(*groups);if(registrations)empty(*registrations);
 if(!travel.actions.empty()||!travel.road_actions.empty()||!travel.rescues.empty()||!travel.journey_facts.empty())fail("ExistingTravelRequiresExactMigrationSource");
 for(const auto& row:travel.itineraries.rows())if(row.second.travelling)fail("ExistingTravelRequiresExactMigrationSource",row.first);
 for(const auto& row:common.state().actions.rows())if(row.second.kind=="travel"||row.second.kind=="cargo_handling")fail("ExistingTravelRequiresExactMigrationSource",row.first);
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="travel")fail("ExistingTravelRequiresExactMigrationSource",row.first);
 const auto now=take(clock.read_current_moment());take(encode_world_moment(now,budget));
 if(now.half_day_tick!=common.tick()||compare(now.sub_half_day,CanonicalFraction{},budget)!=0)fail("SpatialInitialWholeWorldMomentRequired");
 // Validate every original placement first. A loaded position or unsupported
 // initial carried/army/battle placement cannot be silently reset to a town.
 std::vector<std::pair<StableId,OriginalMovementAnchor>> placements;
 for(const auto& row:common.state().actors.rows()){
  const auto& actor=row.second;OriginalMovementAnchor placement;
  if(const auto* authored=actors.actors.find(row.first)){
   take(read_authored_character_origin_v2(common.state(),foundation,actors,row.first));
   const auto* binding=foundation.actor_start_context.actor_domain_bindings.find(row.first);
   if(!binding||authored->placement.kind!=AuthoredStartPlacement::Kind::at_place||binding->placement_ref.kind!=AuthoredStartPlacement::Kind::at_place||binding->placement_ref.place_ref!=authored->placement.place_ref)fail("ActorOriginalPlacementRequired",row.first);
   const auto* place=shared.places.find(authored->placement.place_ref);if(!place)fail("ActorOriginalPlaceRequired",row.first);
   placement={SettlementLocation{place->settlement_id,place->id},{"actor_foundation","scenario_placement",row.first}};
  }else{
   if(!family)fail("ActorInitialPlacementOwnerRequired",row.first);
   auto created=take(read_created_character_initial_placement_v2(common,foundation,actors,*family,shared,row.first,now));
   if(created.actor_id!=row.first||created.source_schema!=2||!same_moment(created.created_at,now,budget))fail("CreatedSpatialInitialSourceRequired",row.first);
   placement={created.location,created.actual_creation_source};
  }
  if(!take(same_actual_movement_position(placement,OriginalMovementAnchor{actor.location,{"common","actor_projection",row.first}},budget)))fail("ActorInitialPlacementProjectionMismatch",row.first);
  placements.emplace_back(row.first,std::move(placement));
 }
 if(!catalog.mobile_inventory_definitions.empty()){
  if(!group||!carriers||!registrations)fail("MobileInventoryGroupSelectionRequired");
  take(initialize_mobile_inventory_bindings_in_candidate(common,shared,catalog,*carriers,*registrations,clock));
 }
 for(auto& row:placements){const auto key="actor:"+row.first;const auto source=row.second.actual_source;
  if(!movement.spatial_bindings.insert(key,SpatialWriterBinding{"actor",row.first,StableMovementWriter{std::move(row.second)},source}))fail("DuplicateInitialSpatialWriter",row.first);
 }
 take(validate_movement_exact_world(common,travel,movement,budget));
});}
}
