// Final V travel record ownership. These fields replace only the new exact
// physical branches; original travel/3 fields remain with their original codec.
#pragma once
#include "movement_record_codec_fields.hpp"
#include "movement_physical_basis.hpp"
#include "movement_exact_detail.hpp"
namespace hanmo::resident::movement_physical_fields {
using movement_record_fields::field;
// The explicit four-way tag is the physical selection. A missing field or an
// old progress=0/0 pair never selects the continuous input domain.
enum class BasisTag:Integer {original_road=0,original_terrain=1,continued_road=2,continued_terrain=3};
inline BasisTag basis_tag(const MovementBasis& basis){return std::visit([](const auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,RoadMovementBasis>)return v.continued?BasisTag::continued_road:BasisTag::original_road;else return v.continued?BasisTag::continued_terrain:BasisTag::original_terrain;},basis);}
template<class C>Status basis(C& c,MovementBasis& value,const MovementRecordCodecSelection& selection){
 Integer tag=static_cast<Integer>(basis_tag(value));c.field("kind",tag);
 if(tag<0||tag>3)return c.reject("InvalidPhysicalMovementBasisTag");
 const bool terrain=tag==1||tag==3,continued=tag>=2;
 if(c.reading()&&c.status().ok){if(terrain)value.emplace<TerrainCostBasis>();else value.emplace<RoadMovementBasis>();}
 if(!c.status().ok)return c.status();
 if(terrain){
  auto& original=std::get<TerrainCostBasis>(value);if(c.reading())original.continued=continued;
  field(c,"basis_ref",original.basis_ref,selection);
  field(c,"start_anchor",original.start_anchor,selection);field(c,"end_anchor",original.end_anchor,selection);
  field(c,"catalog_rule_ref",original.catalog_rule_ref,selection);field(c,"original_unit_ref",original.original_unit_ref,selection);
  StableId decoded_cost_basis_ref;auto& cost_basis_ref=c.reading()?decoded_cost_basis_ref:original.basis_ref;
  field(c,"terrain_cost_basis_ref",cost_basis_ref,selection);
  if(c.status().ok&&(cost_basis_ref.empty()||cost_basis_ref!=original.basis_ref))return c.reject("TerrainCostBasisOriginalPairMismatch",original.basis_ref);
 }else{
  auto& original=std::get<RoadMovementBasis>(value);if(c.reading())original.continued=continued;
  field(c,"basis_ref",original.basis_ref,selection);field(c,"original_inputs",original.original_inputs,selection);
  field(c,"start_anchor",original.start_anchor,selection);field(c,"end_anchor",original.end_anchor,selection);
  field(c,"exact_start_progress",original.exact_start_progress,selection);field(c,"exact_end_progress",original.exact_end_progress,selection);
  field(c,"catalog_rule_ref",original.catalog_rule_ref,selection);field(c,"original_unit_ref",original.original_unit_ref,selection);
 }
 return c.status();
}
// All original terrain params, exact budget, binary64 geometry pieces and the
// original closing branch have one physical owner, paired to the four-way basis.
template<class C>Status terrain_cost_basis(C& c,TerrainCostBasis& value,const MovementRecordCodecSelection& selection){
 field(c,"basis_ref",value.basis_ref,selection);field(c,"original_inputs",value.original_inputs,selection);
 field(c,"parameter_limit",value.parameter_limit,selection);field(c,"pieces",value.pieces,selection);
 field(c,"actual_consumed_total",value.actual_consumed_total,selection);field(c,"reached_points",value.reached_points,selection);
 field(c,"original_point_count",value.original_point_count,selection);field(c,"original_end_credit",value.original_end_credit,selection);
 return field(c,"original_closing_policy",value.original_closing_policy,selection);
}
// Only immutable values are requested. The outer decoded-candidate owner must
// account for each actual deep copy before assigning it to the original roots.
class BasisRestoreRead {
public:virtual ~BasisRestoreRead()=default;
 virtual Outcome<MovementBasis> copy_original_basis(const StableId&)=0;
};
inline Outcome<CanonicalFraction> consumed_ratio(const MovementPrefixState& prefix,ExactArithmeticBudget budget){return movement_detail::guard<CanonicalFraction>([&]{using namespace movement_detail;const auto limit=basis_limit(prefix.basis,budget);if(compare(limit,{},budget)<=0||compare(prefix.cursor,limit,budget)>0)fail("MovementPhysicalCursorRangeMismatch",prefix.id);return div(prefix.cursor,limit,budget);});}
inline Outcome<CanonicalFraction> original_map_cursor(const MovementPrefixState& prefix,ExactArithmeticBudget budget){return movement_detail::guard<CanonicalFraction>([&]{using namespace movement_detail;if(const auto* held=std::get_if<HeldMovementMap>(&prefix.activity_world_map))return held->held_cursor;const auto& running=std::get<RunningMovementMap>(prefix.activity_world_map);return add(running.cursor_at_from,mul(running.rate,take(elapsed_world_duration(running.world_from,prefix.last_settled_at,budget)).value,budget),budget);});}
template<class C>Status prefix_core(C& c,MovementPrefixState& value,const MovementRecordCodecSelection& selection,BasisRestoreRead* originals,ExactArithmeticBudget budget={}){
 field(c,"execution",value.execution,selection);
 StableId decoded_basis_ref;auto& basis_ref=c.reading()?decoded_basis_ref:std::visit([](auto& original)->StableId&{return original.basis_ref;},value.basis);
 field(c,"basis",basis_ref,selection);
 StableId decoded_geometry_cost_basis_ref;auto& geometry_cost_basis_ref=c.reading()?decoded_geometry_cost_basis_ref:basis_ref;
 field(c,"geometry_cost_basis_ref",geometry_cost_basis_ref,selection);
 if(c.status().ok&&c.reading()){
  if(!originals)return c.reject("MovementPhysicalOriginalBasisReaderRequired");
  auto original=originals->copy_original_basis(basis_ref);if(!original.ok)return {false,{},original.error};
  value.basis=std::move(original.value);
 }
 if(!c.status().ok)return c.status();
 if(basis_ref.empty()||geometry_cost_basis_ref!=basis_ref)return c.reject("MovementPhysicalGeometryBasisMismatch",value.id);
 std::optional<ExactTerrainCost> terrain_cursor;
 if(!c.reading()&&std::holds_alternative<TerrainCostBasis>(value.basis))terrain_cursor=ExactTerrainCost{value.cursor};
 field(c,"terrain_cursor",terrain_cursor,selection);field(c,"activity_world_map",value.activity_world_map,selection);
 CanonicalFraction ratio;
 if(!c.reading()){auto expected=consumed_ratio(value,budget);if(!expected.ok)return {false,{},expected.error};ratio=std::move(expected.value);}
 field(c,"consumed_ratio",ratio,selection);field(c,"last_settled_at",value.last_settled_at,selection);
 field(c,"unit_ref",value.unit_ref,selection);field(c,"lifecycle",value.lifecycle,selection);
 if(!c.status().ok)return c.status();
 const bool terrain=std::holds_alternative<TerrainCostBasis>(value.basis);
 if(terrain!=terrain_cursor.has_value())return c.reject("MovementPhysicalTerrainCursorBranchMismatch",value.id);
 auto mapped=original_map_cursor(value,budget);if(!mapped.ok)return {false,{},mapped.error};
 if(c.reading())value.cursor=terrain?terrain_cursor->value:std::move(mapped.value);
 else if(terrain){auto equal=compare_fractions(terrain_cursor->value,value.cursor,budget);if(!equal.ok)return {false,{},equal.error};if(equal.value)return c.reject("MovementPhysicalTerrainCursorMismatch",value.id);}
 auto expected=consumed_ratio(value,budget);if(!expected.ok)return {false,{},expected.error};
 auto equal=compare_fractions(expected.value,ratio,budget);if(!equal.ok)return {false,{},equal.error};
 if(equal.value)return c.reject("MovementPhysicalConsumedRatioMismatch",value.id);
 return c.status();
}
template<class C>Status exertion_basis(C& c,TravelExertionBasis& value,const MovementRecordCodecSelection& selection,BasisRestoreRead* originals){
 field(c,"id",value.id,selection);field(c,"unit_ref",value.unit_ref,selection);field(c,"execution",value.execution,selection);
 field(c,"original_action",value.original_action,selection);field(c,"opened_at",value.opened_at,selection);
 field(c,"actual_participants",value.actual_participants,selection);field(c,"original_fatigue_per_unit",value.original_fatigue_per_unit,selection);
 field(c,"original_rule_source",value.original_rule_source,selection);
 StableId decoded_basis_ref;auto& basis_ref=c.reading()?decoded_basis_ref:std::visit([](auto& original)->StableId&{return original.basis_ref;},value.actual_basis);field(c,"basis_ref",basis_ref,selection);
 if(c.status().ok&&c.reading()){if(!originals)return c.reject("MovementPhysicalOriginalBasisReaderRequired");auto basis=originals->copy_original_basis(basis_ref);if(!basis.ok)return {false,{},basis.error};value.actual_basis=std::move(basis.value);}
 field(c,"opened_cursor",value.opened_cursor,selection);
 if(selection.travel_version==5)field(c,"accepted_funding_plan",value.accepted_funding_plan,selection);
 else if(selection.travel_version!=4||value.accepted_funding_plan)return c.reject("FundedPersonalOpeningRequiresTravel5",value.id);
 return c.status();
}
// Entry manifest persists only original references/entry inventory evidence.
// Totals and derived population read values are restored by the one pure hydrate.
template<class C>Status entry_manifest(C& c,TransportEntryManifestSource& value,const MovementRecordCodecSelection& selection){
 field(c,"id",value.id,selection);field(c,"execution",value.execution,selection);field(c,"entry_ordinal",value.entry_ordinal,selection);
 field(c,"boundary",value.boundary,selection);field(c,"occurred_at",value.occurred_at,selection);
 c.object("manifest",[&](auto& archive){auto& v=value.manifest;
  if(v.read_stamp)return archive.reject("TransportManifestHotStampNotPersistent",value.id);
  field(archive,"carrier",v.carrier,selection);field(archive,"manifest_revision",v.manifest_revision,selection);field(archive,"composition_revision",v.composition_revision,selection);field(archive,"at_tick",v.at_tick,selection);
  field(archive,"actual_anchor",v.actual_anchor,selection);field(archive,"source_refs",v.source_refs,selection);field(archive,"named_actor_refs",v.named_actor_refs,selection);field(archive,"vehicle_refs",v.vehicle_refs,selection);field(archive,"inventory_refs",v.inventory_refs,selection);field(archive,"civilian_population_refs",v.civilian_population_refs,selection);field(archive,"military_force_refs",v.military_force_refs,selection);
  return archive.sequence("vessel_placements",v.vessel_placements,[&](auto& x,VesselManifestPlacement& p){field(x,"vessel_id",p.vessel_id,selection);field(x,"placement_source",p.placement_source,selection);field(x,"named_actor_ids",p.named_actor_ids,selection);field(x,"inventory_ids",p.inventory_ids,selection);return field(x,"population_sources",p.population_sources,selection);});
 });
 field(c,"inventory_sources",value.inventory_sources,selection);
 return c.sequence("population_sources",value.population_sources,[&](auto& x,TransportPopulationManifest& p){field(x,"population_ref",p.population_ref,selection);return field(x,"population_revision",p.population_revision,selection);});
}
}
