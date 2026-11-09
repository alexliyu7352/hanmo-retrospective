#include "inventory_spatial_history.hpp"
#include "group_movement.hpp"
#include "movement_result_read.hpp"
#include "movement_exact_detail.hpp"
#include "../military/military_spatial_handoff.hpp"
#include <tuple>
#include <algorithm>
#include <set>
namespace hanmo::resident {namespace {
using namespace movement_detail;
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
std::size_t order(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind)fail("InventoryHistoricalObservationRequired",ref.id);const auto& ids=common.state().facts.order();auto found=std::find(ids.begin(),ids.end(),ref.id);if(found==ids.end())fail("InventoryHistoricalObservationRequired",ref.id);return std::size_t(found-ids.begin());}
void original_sources(const FixedInventoryPlacement& p){if(p.inventory_id.empty()||p.original_sources.empty())fail("FixedInventoryOriginalSourceRequired",p.inventory_id);std::set<std::tuple<StableId,StableId,StableId>> seen;for(const auto& ref:p.original_sources)if(!source_valid(ref.source_ref)||ref.source_schema<1||!seen.emplace(ref.source_ref.domain,ref.source_ref.kind,ref.source_ref.id).second)fail("FixedInventoryOriginalSourceRequired",p.inventory_id);}
// 只有原三种目录位置声明没有 Common 发生头；动态开店等必须实际先于观察点。
void fixed_sources_before(const CommonStore& common,const FixedInventoryPlacement& placement,const SourceRef& observation){
 const auto cut=order(common,observation);
 for(const auto& source:placement.original_sources){const auto& ref=source.source_ref;
  const bool catalog=source.source_schema==1&&
   ((ref.domain=="local_economy_catalog"&&ref.kind=="cohort_home_inventory")||
    (ref.domain=="estate_catalog"&&ref.kind=="estate_inventory")||
    (ref.domain=="growth_catalog"&&ref.kind=="shop_inventory"));
  if(!catalog&&order(common,ref)>=cut)fail("FixedInventoryCreationNotBeforeObservation",placement.inventory_id);
 }
}
std::vector<StableId> fixed_ids(const FixedInventoryPlacementRead& source){auto ids=take(source.enumerate_original_fixed_inventory_ids());std::set<StableId> seen;for(const auto& id:ids)if(id.empty()||!seen.insert(id).second)fail("DuplicateFixedInventoryPlacement",id);return ids;}
MovementAnchor carrier_at(const InventorySpatialHistoryContext& c,const TransportUnitRef& carrier,const SourceRef& observation,const WorldMoment& at,std::vector<MomentOwnerSource>& refs){
 const auto& x=c.mobile;const auto cut=order(x.common,observation);const auto* unit=x.carriers.transport_units.find(carrier.transport_unit_id);if(!unit||(unit->subject.kind!=carrier.subject.kind||unit->subject.id!=carrier.subject.id))fail("InventoryHistoricalCarrierRequired",carrier.transport_unit_id);
 const auto* registration=x.registrations.registrations.find(unit->registration_source_ref.id);if(!registration||registration->transport_unit_id!=unit->transport_unit_id)fail("InventoryHistoricalRegistrationRequired",unit->transport_unit_id);
 // 原登记历史不能在业务 provider 缺席时降级成可用位置。
 if(!x.original_business)fail("MobileInventoryOriginalBusinessRequired",registration->id);
 take(x.original_business->validate_registration_authority(registration->accepted_scope));
 MovementAnchor result=registration->departure_anchor;WorldMoment latest=registration->occurred_at;std::size_t latest_order=order(x.common,unit->registration_source_ref);SourceRef latest_source=unit->registration_source_ref;
 auto visible=[&](const WorldMoment& when,const SourceRef& ref){const auto ordinal=order(x.common,ref);const auto cmp=take(compare_world_moments(when,at,x.budget));if(ordinal<cut){if(cmp>0)fail("InventorySpatialCausalMomentMismatch",ref.id);return true;}if(cmp<0)fail("InventorySpatialOmittedEarlierMovement",ref.id);return false;};
 if(!visible(latest,latest_source))fail("InventoryHistoricalRegistrationRequired",latest_source.id);
 auto adopt=[&](const WorldMoment& when,const MovementAnchor& anchor,const SourceRef& ref){if(!visible(when,ref))return;const auto cmp=take(compare_world_moments(when,latest,x.budget));const auto ordinal=order(x.common,ref);if(cmp>0||(cmp==0&&ordinal>latest_order)){result=anchor;latest=when;latest_order=ordinal;latest_source=ref;}else if(cmp==0&&ordinal==latest_order&&!take(same_actual_movement_position(result,anchor,x.budget)))fail("InventoryHistoricalPositionAmbiguous",ref.id);};
 auto belongs=[&](const MovementUnitState& motion){const auto* e=std::get_if<GroupMovementExecution>(&motion.execution);const auto* p=e?std::get_if<TransportUnitCarrierRef>(&e->carrier):nullptr;return p&&p->transport_unit_ref.transport_unit_id==carrier.transport_unit_id;};
 for(const auto& row:x.movement.units.rows())if(belongs(row.second)){
  const auto& motion=row.second;
  for(const auto& ref:motion.effort_basis_refs){const auto* basis=x.movement.exertion_bases.find(ref.id);if(!basis||ref.domain!="travel"||ref.kind!="travel_exertion_basis"||basis->unit_ref!=motion.id)fail("InventoryHistoricalMovementBasisRequired",ref.id);take(read_movement_source_moment(x.movement,ref,1,x.budget));MovementPrefixState prefix;prefix.basis=basis->actual_basis;prefix.cursor=basis->opened_cursor;prefix.lifecycle=MovementActive{};adopt(basis->opened_at,take(read_movement_position(prefix,x.budget)),ref);}
  if(const auto* close=x.movement.unit_closes.find(motion.id+":close")){const SourceRef ref{"travel","movement_unit_close",close->id};take(read_movement_source_moment(x.movement,ref,1,x.budget));adopt(close->occurred_at,close->actual_anchor,ref);}
 }
 for(const auto& row:x.movement.arrivals.rows()){const auto* motion=x.movement.units.find(row.second.unit_ref);if(!motion||!belongs(*motion))continue;const SourceRef ref{"travel","movement_arrival",row.first};take(read_verified_movement_arrival(x.movement,ref,1,x.budget));adopt(row.second.occurred_at,row.second.actual_anchor,ref);}
 for(const auto& row:c.group_executions.controls.rows()){const auto* journey=x.carriers.journeys.find(row.second.journey_id);const auto* bound=journey?std::get_if<TransportUnitCarrierRef>(&journey->carrier):nullptr;if(bound&&bound->transport_unit_ref.transport_unit_id==carrier.transport_unit_id)adopt(row.second.occurred_at,row.second.actual_anchor,{"travel","group_movement_control",row.first});}
 for(const auto& row:x.movement.units.rows())if(belongs(row.second))for(const auto& activity:row.second.activity_spans){take(make_moment_span(activity.span.from,activity.span.to,x.budget));if(take(compare_world_moments(activity.span.from,at,x.budget))<0&&take(compare_world_moments(at,activity.span.to,x.budget))<0)fail("InventoryHandoverDuringMovement",carrier.transport_unit_id);if(take(compare_world_moments(activity.span.to,at,x.budget))<=0&&take(compare_world_moments(activity.span.to,latest,x.budget))>0)fail("InventoryStationarySourceRequired",carrier.transport_unit_id);}
 if(c.military_handoffs)for(const auto& row:c.military_handoffs->handoffs.rows()){const auto* bound=std::get_if<TransportUnitCarrierRef>(&row.second.carrier_ref);if(bound&&bound->transport_unit_ref.transport_unit_id==carrier.transport_unit_id&&visible(row.second.at,{"military","military_spatial_handoff",row.first}))fail("InventoryMilitaryLocalHistoryRequired",carrier.transport_unit_id);}
 else for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_spatial_handoff")fail("InventoryMilitaryLocalHistoryRequired",carrier.transport_unit_id);
 refs.push_back({latest_source,1});return result;
}
}
Status FixedInventoryPlacementRead::validate_original_fixed_inventory_creation(
 const CommonStore& common,const CommonCatalog& shared,const StableId& inventory)const{
 if(!common.inventory(inventory)||!shared.required_inventories.find(inventory))
  return common_failure("FixedInventoryOriginalSourceRequired",inventory);
 return common_success();
}
Outcome<InventoryHistoricalSpatialPoint> read_inventory_spatial_anchor_before_source(const InventorySpatialHistoryContext& c,const StableId& inventory,const SourceRef& observation,const WorldMoment& at){return movement_detail::guard<InventoryHistoricalSpatialPoint>([&]{using namespace movement_detail;const auto& x=c.mobile;order(x.common,observation);take(encode_world_moment(at,x.budget));if(!x.common.inventory(inventory)||take(compare_world_moments(at,take(x.clock.read_current_moment()),x.budget))>0)fail("InventoryHistoricalObservationRequired",inventory);
 const auto fixed=fixed_ids(c.fixed);const bool stationary=contains(fixed,inventory);bool mobile=false;for(const auto& row:x.travel_catalog.mobile_inventory_definitions.rows())if(row.second.inventory_id==inventory)mobile=true;StableId actor;for(const auto& row:x.common.state().actors.rows())if(row.second.inventory_id==inventory){if(!actor.empty())fail("InventoryInTwoActorBodies",inventory);actor=row.first;}
 if(Integer(stationary)+Integer(mobile)+Integer(!actor.empty())!=1)fail("InventoryOriginalPlacementAmbiguous",inventory);
 InventoryHistoricalSpatialPoint out;out.inventory_id=inventory;out.observed_at=at;out.observation_source=observation;
 if(stationary){take(c.fixed.validate_original_fixed_inventory_creation(x.common,x.common_catalog,inventory));auto original=take(c.fixed.read_original_fixed_inventory_placement(inventory));original_sources(original);if(original.inventory_id!=inventory)fail("FixedInventoryOriginalSourceRequired",inventory);fixed_sources_before(x.common,original,observation);for(const auto& row:x.registrations.registrations.rows())if(contains(row.second.accepted_scope.inventory_ids,inventory))fail("FixedInventoryWasCarried",inventory);out.actual_anchor=take(read_original_spatial_site_anchor(x.common_catalog,x.travel_catalog,original.original_site_ref));out.original_sources=std::move(original.original_sources);return out;}
 if(mobile){auto original=take(read_mobile_inventory_binding_before_source(x,inventory,observation,at));out.actual_anchor=original.stationary_or_departure_anchor;out.original_sources.push_back(original.actual_binding_source);if(original.carrying_unit)out.actual_anchor=carrier_at(c,*original.carrying_unit,observation,at,out.original_sources);return out;}
 const SourceRef accepted=c.actor_catalog.actors.find(actor)?SourceRef{"actor_foundation","scenario_placement",actor}:SourceRef{"family","actor_creation",actor};auto original=take(read_actor_spatial_location_before_source(x.common,x.common_catalog,x.movement,x.carriers,x.registrations,c.foundation,c.actor_catalog,x.spatial_sources,actor,accepted,observation,at,c.military_handoffs,x.budget,c.family_catalog));out.actual_anchor=std::move(original.actual_anchor);out.original_sources=std::move(original.original_sources);return out;
});}
Outcome<StorageReachability> read_storage_reachability_before_source(const InventorySpatialHistoryContext& c,const StableId& from,const StableId& to,const SourceRef& observation,const WorldMoment& at,const SourceRef& handling){return movement_detail::guard<StorageReachability>([&]()->StorageReachability{using namespace movement_detail;if(from==to)fail("SameInventoryTransfer");if(!source_valid(handling))fail("CargoHandlingRuleRequired");auto a=take(read_inventory_spatial_anchor_before_source(c,from,observation,at));auto b=take(read_inventory_spatial_anchor_before_source(c,to,observation,at));if(take(same_actual_movement_position(a.actual_anchor,b.actual_anchor,c.mobile.budget)))return SameSiteReachability{a.actual_anchor,handling};return JourneyRequiredReachability{a.actual_anchor,b.actual_anchor,{}};});}
Status validate_original_fixed_inventory_bindings(const CommonStore& common,const CommonCatalog& shared,const TravelCatalog& travel,const SpatialCarrierState& carriers,const TransportRegistrationState& registrations,const FixedInventoryPlacementRead& sources){return movement_detail::status([&]{using namespace movement_detail;for(const auto& id:fixed_ids(sources)){
 if(!common.inventory(id))fail("FixedInventoryOriginalSourceRequired",id);take(sources.validate_original_fixed_inventory_creation(common,shared,id));const auto original=take(sources.read_original_fixed_inventory_placement(id));original_sources(original);if(original.inventory_id!=id)fail("FixedInventoryOriginalSourceRequired",id);take(read_original_spatial_site_anchor(shared,travel,original.original_site_ref));
 const auto* current=carriers.inventory_bindings.find(id);const auto* site=current?std::get_if<StableId>(&current->placement):nullptr;if(!current||!site||*site!=original.original_site_ref.id||current->revision!=0||!same_source(current->source_ref,original.original_sources.front().source_ref))fail("FixedInventoryBindingSourceMismatch",id);
 for(const auto& row:registrations.registrations.rows())if(contains(row.second.accepted_scope.inventory_ids,id))fail("FixedInventoryWasCarried",id);
 for(const auto& actor:common.state().actors.rows())if(actor.second.inventory_id==id)fail("FixedInventoryConflictsWithActor",id);for(const auto& mobile:travel.mobile_inventory_definitions.rows())if(mobile.second.inventory_id==id)fail("FixedInventoryConflictsWithMobile",id);
 }});}
Status install_original_fixed_inventory_bindings_in_candidate(const CommonStore& common,const CommonCatalog& shared,const TravelCatalog& travel,SpatialCarrierState& carriers,const TransportRegistrationState& registrations,const FixedInventoryPlacementRead& sources){return movement_detail::status([&]{using namespace movement_detail;if(!registrations.registrations.empty()||!registrations.releases.empty()||!carriers.journeys.empty())fail("FixedInventoryInitialConstructionRequired");const auto ids=fixed_ids(sources);std::vector<InventorySpatialBinding> pending;
 for(const auto& id:ids){if(!common.inventory(id)||!shared.required_inventories.find(id)||carriers.inventory_bindings.find(id))fail("FixedInventoryInitialConstructionRequired",id);for(const auto& actor:common.state().actors.rows())if(actor.second.inventory_id==id)fail("FixedInventoryConflictsWithActor",id);for(const auto& mobile:travel.mobile_inventory_definitions.rows())if(mobile.second.inventory_id==id)fail("FixedInventoryConflictsWithMobile",id);auto original=take(sources.read_original_fixed_inventory_placement(id));original_sources(original);if(original.inventory_id!=id)fail("FixedInventoryOriginalSourceRequired",id);take(read_original_spatial_site_anchor(shared,travel,original.original_site_ref));pending.push_back({id,original.original_site_ref.id,original.original_sources.front().source_ref,0});}
 for(auto& binding:pending){const auto id=binding.inventory_id;if(!carriers.inventory_bindings.insert(id,std::move(binding)))fail("DuplicateFixedInventoryPlacement",id);}
});}
}
