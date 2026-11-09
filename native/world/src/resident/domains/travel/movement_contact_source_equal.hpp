// 从同一typed字段声明生成完整接触源比较；只读、不分配。
#pragma once
#include "movement_exact.hpp"
namespace hanmo::resident::movement_contact_detail {
inline bool same(const PointI&,const PointI&);
inline bool same(const WildernessLocation&,const WildernessLocation&);
inline bool same(const OnRouteLocation&,const OnRouteLocation&);
inline bool same(const TerrainTravelParams&,const TerrainTravelParams&);
inline bool same(const RoadTravelParams&,const RoadTravelParams&);
inline bool same(const RoadTravelMotion&,const RoadTravelMotion&);
inline bool same(const RoadContactEndpoint&,const RoadContactEndpoint&);
inline bool same(const RoadContactEndpoints&,const RoadContactEndpoints&);
inline bool same(const ContactPoint&,const ContactPoint&);
inline bool same(const ContactSegment&,const ContactSegment&);
inline bool same(const TravelMotion&,const TravelMotion&);
inline bool same(const ContactEndpoint&,const ContactEndpoint&);
inline bool same(const ContactHit&,const ContactHit&);
template<class T> bool same(const T& a,const T& b){return a==b;}
template<class T> bool same(const std::vector<T>& a,const std::vector<T>& b);
template<class T> bool same(const std::optional<T>& a,const std::optional<T>& b);
template<class... T> bool same(const std::variant<T...>& a,const std::variant<T...>& b);
template<class T> bool same(const std::vector<T>& a,const std::vector<T>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same(a[i],b[i]))return false;return true;}
template<class T> bool same(const std::optional<T>& a,const std::optional<T>& b){return a.has_value()==b.has_value()&&(!a||same(*a,*b));}
template<class... T> bool same(const std::variant<T...>& a,const std::variant<T...>& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& x){return same(x,std::get<std::decay_t<decltype(x)>>(b));},a);}
inline bool same(const PointI& a,const PointI& b){return same(a.x,b.x)&&same(a.y,b.y);}
inline bool same(const WildernessLocation& a,const WildernessLocation& b){return same(a.map_id,b.map_id)&&same(a.origin,b.origin)&&same(a.point,b.point);}
inline bool same(const OnRouteLocation& a,const OnRouteLocation& b){return same(a.edge_id,b.edge_id)&&same(a.origin,b.origin)&&same(a.progress,b.progress);}
inline bool same(const TerrainTravelParams& a,const TerrainTravelParams& b){return same(a.plan_id,b.plan_id)&&same(a.map_id,b.map_id)&&same(a.origin,b.origin)&&same(a.payer_wallet_id,b.payer_wallet_id)&&same(a.mode,b.mode)&&same(a.route_index,b.route_index)&&same(a.budget,b.budget)&&same(a.travel_credit,b.travel_credit)&&same(a.money_per_tick,b.money_per_tick)&&same(a.fatigue_per_tick,b.fatigue_per_tick)&&same(a.points,b.points)&&same(a.destination,b.destination)&&same(a.paused,b.paused)&&same(a.encounter_id,b.encounter_id)&&same(a.has_encounter_id,b.has_encounter_id)&&same(a.has_met_actions,b.has_met_actions)&&same(a.met_actions,b.met_actions)&&same(a.field_order,b.field_order);}
inline bool same(const RoadTravelParams& a,const RoadTravelParams& b){return same(a.destination,b.destination)&&same(a.via,b.via)&&same(a.path,b.path)&&same(a.edge_index,b.edge_index)&&same(a.payer_wallet_id,b.payer_wallet_id)&&same(a.returning,b.returning)&&same(a.paused,b.paused)&&same(a.reverse_first,b.reverse_first)&&same(a.has_terrain,b.has_terrain)&&same(a.has_paused,b.has_paused)&&same(a.has_reverse_first,b.has_reverse_first)&&same(a.has_encounter_id,b.has_encounter_id)&&same(a.has_met_actions,b.has_met_actions)&&same(a.encounter_id,b.encounter_id)&&same(a.met_actions,b.met_actions)&&same(a.field_order,b.field_order);}
inline bool same(const RoadTravelMotion& a,const RoadTravelMotion& b){return same(a.actor_id,b.actor_id)&&same(a.action_id,b.action_id)&&same(a.edge_id,b.edge_id)&&same(a.origin,b.origin)&&same(a.start,b.start)&&same(a.end,b.end)&&same(a.moving,b.moving)&&same(a.params,b.params);}
inline bool same(const RoadContactEndpoint& a,const RoadContactEndpoint& b){return same(a.actor_id,b.actor_id)&&same(a.anchor,b.anchor)&&same(a.trace,b.trace);}
inline bool same(const RoadContactEndpoints& a,const RoadContactEndpoints& b){return same(a.first,b.first)&&same(a.second,b.second);}
inline bool same(const ContactPoint& a,const ContactPoint& b){return same(a.x,b.x)&&same(a.y,b.y);}
inline bool same(const ContactSegment& a,const ContactSegment& b){return same(a.from,b.from)&&same(a.to,b.to)&&same(a.start_cost,b.start_cost)&&same(a.end_cost,b.end_cost)&&same(a.point_index,b.point_index);}
inline bool same(const TravelMotion& a,const TravelMotion& b){return same(a.actor_id,b.actor_id)&&same(a.action_id,b.action_id)&&same(a.map_id,b.map_id)&&same(a.origin,b.origin)&&same(a.params,b.params)&&same(a.segments,b.segments)&&same(a.budget,b.budget)&&same(a.end_credit,b.end_credit)&&same(a.reached_points,b.reached_points)&&same(a.end,b.end)&&same(a.moving,b.moving)&&same(a.stationary,b.stationary)&&same(a.stationary_location,b.stationary_location)&&same(a.cost_piece_kinds,b.cost_piece_kinds)&&same(a.actual_consumed_total,b.actual_consumed_total)&&same(a.original_point_count,b.original_point_count);}
inline bool same(const ContactEndpoint& a,const ContactEndpoint& b){return same(a.actor_id,b.actor_id)&&same(a.anchor,b.anchor)&&same(a.trace,b.trace);}
inline bool same(const ContactHit& a,const ContactHit& b){return same(a.a,b.a)&&same(a.b,b.b)&&same(a.time_num,b.time_num)&&same(a.time_den,b.time_den)&&same(a.first,b.first)&&same(a.second,b.second)&&same(a.road,b.road);}
}
