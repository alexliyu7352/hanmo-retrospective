#include "movement_physical_basis.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <cstring>
#include <tuple>
namespace hanmo::resident {namespace physical_basis {
using namespace movement_detail;
const StableId& id(const MovementBasis& b){return std::visit([](const auto& x)->const StableId&{return x.basis_ref;},b);}
bool same(const PointI& a,const PointI& b){return a.x==b.x&&a.y==b.y;}
bool same(const TravelDestination& a,const TravelDestination& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<StableId>(&a))return *x==std::get<StableId>(b);return same(std::get<PointI>(a),std::get<PointI>(b));}
bool same(const Location& a,const Location& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);if constexpr(std::is_same_v<T,SettlementLocation>)return x.settlement_id==y.settlement_id&&x.place_id==y.place_id;else if constexpr(std::is_same_v<T,WildernessLocation>)return x.map_id==y.map_id&&x.origin==y.origin&&same(x.point,y.point);else if constexpr(std::is_same_v<T,FollowingLocation>)return x.party_id==y.party_id;else if constexpr(std::is_same_v<T,InArmyLocation>)return x.army_id==y.army_id;else if constexpr(std::is_same_v<T,OnRouteLocation>)return x.edge_id==y.edge_id&&x.origin==y.origin&&x.progress==y.progress;else return x.carrier_kind==y.carrier_kind&&x.carrier_id==y.carrier_id;},a);}
bool same(const MovementAnchor& a,const MovementAnchor& z,ExactArithmeticBudget budget){if(a.index()!=z.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(z);if constexpr(std::is_same_v<T,OriginalMovementAnchor>)return same(x.location,y.location)&&same_source(x.actual_source,y.actual_source);else if constexpr(std::is_same_v<T,RoadFractionalAnchor>)return x.edge_id==y.edge_id&&x.origin==y.origin&&!compare(x.progress,y.progress,budget)&&same_source(x.actual_source,y.actual_source);else return x.map_id==y.map_id&&x.origin==y.origin&&x.x_bits==y.x_bits&&x.y_bits==y.y_bits&&same_source(x.actual_prefix_close_source,y.actual_prefix_close_source)&&x.geometry_profile_ref==y.geometry_profile_ref;},a);}
bool same(const TerrainTravelParams& a,const TerrainTravelParams& b){
 if(std::tie(a.plan_id,a.map_id,a.origin,a.payer_wallet_id,a.mode,a.route_index,a.budget,a.travel_credit,a.money_per_tick,a.fatigue_per_tick,a.paused,a.encounter_id,a.has_encounter_id,a.has_met_actions,a.met_actions,a.field_order)!=std::tie(b.plan_id,b.map_id,b.origin,b.payer_wallet_id,b.mode,b.route_index,b.budget,b.travel_credit,b.money_per_tick,b.fatigue_per_tick,b.paused,b.encounter_id,b.has_encounter_id,b.has_met_actions,b.met_actions,b.field_order)||a.points.size()!=b.points.size()||!same(a.destination,b.destination))return false;
 for(std::size_t i=0;i<a.points.size();++i)if(!same(a.points[i],b.points[i]))return false;return true;
}
bool same(const RoadTravelParams& a,const RoadTravelParams& b){
 if(std::tie(a.via,a.path,a.edge_index,a.payer_wallet_id,a.returning,a.paused,a.reverse_first,a.has_terrain,a.has_paused,a.has_reverse_first,a.has_encounter_id,a.has_met_actions,a.encounter_id,a.met_actions,a.field_order)!=std::tie(b.via,b.path,b.edge_index,b.payer_wallet_id,b.returning,b.paused,b.reverse_first,b.has_terrain,b.has_paused,b.has_reverse_first,b.has_encounter_id,b.has_met_actions,b.encounter_id,b.met_actions,b.field_order)||bool(a.destination)!=bool(b.destination))return false;
 return !a.destination||same(*a.destination,*b.destination);
}
bool same(const RoadTravelMotion& a,const RoadTravelMotion& b){return std::tie(a.actor_id,a.action_id,a.edge_id,a.origin,a.start,a.end,a.moving)==std::tie(b.actor_id,b.action_id,b.edge_id,b.origin,b.start,b.end,b.moving)&&same(a.params,b.params);}
bool bits(double a,double b){std::uint64_t x=0,y=0;std::memcpy(&x,&a,sizeof x);std::memcpy(&y,&b,sizeof y);return x==y;}
bool same(const ContactSegment& a,const ContactSegment& b){return bits(a.from.x,b.from.x)&&bits(a.from.y,b.from.y)&&bits(a.to.x,b.to.x)&&bits(a.to.y,b.to.y)&&bits(a.start_cost,b.start_cost)&&bits(a.end_cost,b.end_cost)&&a.point_index==b.point_index;}
bool same(const RoadMovementBasis& a,const RoadMovementBasis& b,ExactArithmeticBudget budget){return a.basis_ref==b.basis_ref&&a.continued==b.continued&&a.catalog_rule_ref==b.catalog_rule_ref&&a.original_unit_ref==b.original_unit_ref&&same(a.original_inputs,b.original_inputs)&&same(a.start_anchor,b.start_anchor,budget)&&same(a.end_anchor,b.end_anchor,budget)&&!compare(a.exact_start_progress,b.exact_start_progress,budget)&&!compare(a.exact_end_progress,b.exact_end_progress,budget);}
bool same(const TerrainCostBasis& a,const TerrainCostBasis& b,ExactArithmeticBudget budget){
 if(std::tie(a.basis_ref,a.actual_consumed_total,a.reached_points,a.original_point_count,a.original_end_credit,a.original_closing_policy,a.continued,a.catalog_rule_ref,a.original_unit_ref)!=std::tie(b.basis_ref,b.actual_consumed_total,b.reached_points,b.original_point_count,b.original_end_credit,b.original_closing_policy,b.continued,b.catalog_rule_ref,b.original_unit_ref)||!same(a.original_inputs,b.original_inputs)||!same(a.start_anchor,b.start_anchor,budget)||!same(a.end_anchor,b.end_anchor,budget)||compare(a.parameter_limit.value,b.parameter_limit.value,budget)||a.pieces.size()!=b.pieces.size())return false;
 for(std::size_t i=0;i<a.pieces.size();++i){const auto& x=a.pieces[i];const auto& y=b.pieces[i];if(x.kind!=y.kind||!same(x.segment,y.segment)||compare(x.cost_from,y.cost_from,budget)||compare(x.cost_to,y.cost_to,budget))return false;}return true;
}
}
const MovementBasis* MovementPhysicalBasisIndex::find(const StableId& key)const noexcept{const auto it=std::lower_bound(rows.begin(),rows.end(),key,[](const MovementBasis* row,const StableId& value){return physical_basis::id(*row)<value;});return it!=rows.end()&&physical_basis::id(**it)==key?*it:nullptr;}
Outcome<bool> same_original_movement_basis_payload(const MovementBasis& a,const MovementBasis& b,ExactArithmeticBudget budget){return movement_detail::guard<bool>([&]{if(a.index()!=b.index())return false;return std::visit([&](const auto& x){return physical_basis::same(x,std::get<std::decay_t<decltype(x)>>(b),budget);},a);});}
Outcome<MovementPhysicalBasisIndex> index_original_movement_bases(const MovementExactState& state,ExactArithmeticBudget budget){return movement_detail::guard<MovementPhysicalBasisIndex>([&]{using namespace movement_detail;MovementPhysicalBasisIndex out;
 auto append=[&](const MovementBasis& b){if(physical_basis::id(b).empty())fail("MovementBasisIdentityRequired");const auto it=std::lower_bound(out.rows.begin(),out.rows.end(),physical_basis::id(b),[](const MovementBasis* row,const StableId& key){return physical_basis::id(*row)<key;});if(it!=out.rows.end()&&physical_basis::id(**it)==physical_basis::id(b)){if(!take(same_original_movement_basis_payload(**it,b,budget)))fail("MovementBasisOwnedPayloadConflict",physical_basis::id(b));}else out.rows.insert(it,&b);};
 for(const auto& row:state.prefixes.rows()){if(row.first!=row.second.id)fail("MovementPrefixKeyMismatch",row.first);append(row.second.basis);}
 for(const auto& row:state.exertion_bases.rows()){if(row.first!=row.second.id)fail("MovementEffortBasisKeyMismatch",row.first);append(row.second.actual_basis);}
 for(const auto& row:state.units.rows())for(const auto& span:row.second.activity_spans)if(!out.find(span.basis_ref))fail("MovementActivityBasisRequired",span.basis_ref);
 for(const auto& row:state.basis_transitions.rows())if(!out.find(row.second.old_basis_ref)||!out.find(row.second.new_basis_ref))fail("MovementTransitionBasisRequired",row.first);
 for(const auto* b:out.rows){const auto& unit=std::visit([](const auto& x)->const StableId&{return x.original_unit_ref;},*b);if(!state.units.find(unit))fail("MovementBasisOriginalUnitRequired",unit);}
 return out;
});}
}
