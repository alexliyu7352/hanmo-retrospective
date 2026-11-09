#include "transport_manifest_history.hpp"
#include "group_movement.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <string_view>
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
Integer add_total(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("TransportManifestQuantityOverflow");return a+b;}
using SourceKey=std::tuple<std::string_view,std::string_view,std::string_view>;
SourceKey original_key(const SourceRef& ref){return {ref.domain,ref.kind,ref.id};}
template<class T>using RestoreSet=std::set<T,std::less<T>,CheckpointAllocator<T>>;
}
Status hydrate_transport_entry_manifest_derived_values(
 const CommonStore& common,const SpatialCarrierState& carriers,const MovementExactState& movement,
 GroupMovementExecutionState& executions,const TransportRegistrationState& registrations,
 const NativeTransportInventoryEntryRead& original_entries,
 const TransportManifestHistorySourceRead& history,ExactArithmeticBudget budget){return status([&]{
 if(!original_entries.bound_to(common,carriers,movement,executions,registrations)||!history.bound_to(common,registrations))fail("TransportManifestRestoreCandidateMismatch");
 for(const auto& row:executions.entry_manifests.rows()){
  auto* entry=executions.entry_manifests.find(row.first);
  if(!entry||entry->id!=row.first||entry->manifest.read_stamp)fail("TransportManifestRestoreSourceMismatch",row.first);
  const SourceRef actual_source{"travel","transport_entry_manifest",entry->id};
  // The actual immutable entry owner first checks its original action/group,
  // original register/Army adoption and inventory scope. No derived count is read.
  const auto scope=take(original_entries.read_original_inventory_entry_scope(actual_source));
  if(!same_source(scope.entry_source,actual_source)||!same_moment(scope.occurred_at,entry->occurred_at,budget)||scope.world_id!=common.state().identity.world_id||scope.actual_inventory_ids!=entry->manifest.inventory_refs)fail("TransportManifestRestoreEntryScopeMismatch",entry->id);
  Integer weight=0,persons=0,crew=0;
  RestoreSet<std::string_view> actual_inventories,named;
  for(const auto& id:entry->manifest.named_actor_refs){if(!common.actor(id)||!named.insert(id).second)fail("TransportManifestRestoreActorSourceMismatch",id);persons=add_total(persons,1);}
  for(const auto& inventory:entry->inventory_sources){
   const auto& evidence=inventory.entry_evidence;
   if(!actual_inventories.insert(evidence.inventory_id).second||!same_source(evidence.entry_source,actual_source)||!same_moment(evidence.occurred_at,entry->occurred_at,budget))fail("TransportManifestRestoreInventorySourceMismatch",entry->id);
   take(history.validate_inventory_manifest_history(inventory));
   weight=add_total(weight,take(inventory_entry_evidence_weight(evidence)));
  }
  if((actual_inventories.size()!=scope.actual_inventory_ids.size()||!std::equal(actual_inventories.begin(),actual_inventories.end(),scope.actual_inventory_ids.begin())))fail("TransportManifestRestoreInventoryCoverage",entry->id);
  RestoreSet<SourceKey> populations;
  for(auto& original:entry->population_sources){
   if(original.population_revision<1||!source_valid(original.population_ref)||!populations.insert(original_key(original.population_ref)).second)fail("TransportManifestRestorePopulationSourceMismatch",entry->id);
   auto actual=take(history.read_population_manifest_history(original.population_ref,original.population_revision,entry->occurred_at));
   if(!same_source(actual.source_ref,original.population_ref)||actual.cohort_id.empty()||actual.persons<0||actual.working_crew<0||actual.working_crew>actual.persons)fail("TransportManifestRestorePopulationValueMismatch",entry->id);
   for(const auto& actor:actual.named_actor_ids)if(!named.insert(actor).second)fail("PopulationNamedPersonOverlap",actor);
   persons=add_total(persons,actual.persons);crew=add_total(crew,actual.working_crew);
   original.actual=std::move(actual);
  }
  if(populations.size()!=entry->manifest.civilian_population_refs.size())fail("TransportManifestRestorePopulationCoverage",entry->id);
  for(const auto& ref:entry->manifest.civilian_population_refs)if(!populations.count(original_key(ref)))fail("TransportManifestRestorePopulationCoverage",entry->id);
  if(std::holds_alternative<ArmyCarrierRef>(entry->manifest.carrier)||std::holds_alternative<FleetCarrierRef>(entry->manifest.carrier)||!entry->manifest.military_force_refs.empty()||!entry->manifest.vessel_placements.empty()){
   const auto derived=take(history.read_military_manifest_derived(*entry));
   if(derived.persons<0||derived.working_crew<0||derived.working_crew>derived.persons||derived.weight!=weight||derived.vessels.size()!=entry->manifest.vessel_placements.size())fail("TransportManifestRestoreMilitaryTotalsMismatch",entry->id);
   persons=derived.persons;crew=derived.working_crew;
   RestoreSet<std::string_view> vessels;
   for(const auto& actual:derived.vessels){
    if(actual.vessel_id.empty()||!vessels.insert(actual.vessel_id).second||actual.persons<0||actual.weight<0||actual.maximum_persons<actual.persons||actual.maximum_weight<actual.weight)fail("TransportManifestRestoreVesselTotalsMismatch",entry->id);
    auto found=std::find_if(entry->manifest.vessel_placements.begin(),entry->manifest.vessel_placements.end(),[&](const auto& placement){return placement.vessel_id==actual.vessel_id;});
    if(found==entry->manifest.vessel_placements.end())fail("TransportManifestRestoreVesselSourceRequired",actual.vessel_id);
    found->persons=actual.persons;found->weight=actual.weight;found->maximum_persons=actual.maximum_persons;found->maximum_weight=actual.maximum_weight;
   }
  }
  entry->manifest.persons=persons;entry->manifest.working_crew=crew;entry->manifest.weight=weight;
 }
 // Full original source and crossing coverage, not merely totals equality.
 take(validate_transport_entry_manifest_sources(common,carriers,movement,executions,history,budget));
});}
}
