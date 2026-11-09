// 原contact typed字段同binary visitor；新Road ratio由显式geometry_profile分派。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "movement_contact_codec.hpp"
#include "movement_exact_detail.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <cmath>
#include <set>
namespace hanmo::resident {namespace contact_json {using namespace movement_detail;using V=BoundaryValue;
const V& get(const V& v,const char* name){const auto* x=v.find(name);if(!x)fail("MissingMovementContactField",name);return *x;}
void fields(const V& v,std::initializer_list<const char*> allowed){const auto* d=v.as<V::Dict>();if(!d||d->size()!=allowed.size())fail("MovementContactRecordFieldsMismatch");auto key=allowed.begin();for(const auto& row:*d)if(row.first!=*key++)fail("MovementContactRecordFieldOrderMismatch",row.first);}
void read(const V&,SourceRef&);
Status emit(ProjectionWriter&,std::string_view,const SourceRef&);
void read(const V&,PointI&);
Status emit(ProjectionWriter&,std::string_view,const PointI&);
void read(const V&,WildernessLocation&);
Status emit(ProjectionWriter&,std::string_view,const WildernessLocation&);
void read(const V&,OnRouteLocation&);
Status emit(ProjectionWriter&,std::string_view,const OnRouteLocation&);
void read(const V&,TerrainTravelParams&);
Status emit(ProjectionWriter&,std::string_view,const TerrainTravelParams&);
void read(const V&,RoadTravelParams&);
Status emit(ProjectionWriter&,std::string_view,const RoadTravelParams&);
void read(const V&,RoadTravelMotion&);
Status emit(ProjectionWriter&,std::string_view,const RoadTravelMotion&);
void read(const V&,RoadContactEndpoint&);
Status emit(ProjectionWriter&,std::string_view,const RoadContactEndpoint&);
void read(const V&,RoadContactEndpoints&);
Status emit(ProjectionWriter&,std::string_view,const RoadContactEndpoints&);
void read(const V&,ContactPoint&);
Status emit(ProjectionWriter&,std::string_view,const ContactPoint&);
void read(const V&,ContactSegment&);
Status emit(ProjectionWriter&,std::string_view,const ContactSegment&);
void read(const V&,TravelMotion&);
Status emit(ProjectionWriter&,std::string_view,const TravelMotion&);
void read(const V&,ContactEndpoint&);
Status emit(ProjectionWriter&,std::string_view,const ContactEndpoint&);
void read(const V&,ContactHit&);
Status emit(ProjectionWriter&,std::string_view,const ContactHit&);
void read(const V&,HalfDayDuration&);
Status emit(ProjectionWriter&,std::string_view,const HalfDayDuration&);
void read(const V&,RunningMovementMap&);
Status emit(ProjectionWriter&,std::string_view,const RunningMovementMap&);
void read(const V&,HeldMovementMap&);
Status emit(ProjectionWriter&,std::string_view,const HeldMovementMap&);
void read(const V&,ExactContactMotionWitness&);
Status emit(ProjectionWriter&,std::string_view,const ExactContactMotionWitness&);
void read(const V&,ExactMovementContact&);
Status emit(ProjectionWriter&,std::string_view,const ExactMovementContact&);
void read(const V&,TerrainCostPieceKind&);
Status emit(ProjectionWriter&,std::string_view,const TerrainCostPieceKind&);
void read(const V&,ExactContactGeometryProfile&);
Status emit(ProjectionWriter&,std::string_view,const ExactContactGeometryProfile&);
void read(const V&,StableId&);
Status emit(ProjectionWriter&,std::string_view,const StableId&);
void read(const V&,Integer&);
Status emit(ProjectionWriter&,std::string_view,const Integer&);
void read(const V&,bool&);
Status emit(ProjectionWriter&,std::string_view,const bool&);
void read(const V&,double&);
Status emit(ProjectionWriter&,std::string_view,const double&);
void read(const V&,WorldMoment&);
Status emit(ProjectionWriter&,std::string_view,const WorldMoment&);
void read(const V&,CanonicalFraction&);
Status emit(ProjectionWriter&,std::string_view,const CanonicalFraction&);
template<class T>void read(const V&,std::vector<T>&);template<class T>void read(const V&,std::optional<T>&);template<class...T>void read(const V&,std::variant<T...>&);
template<class T>Status emit(ProjectionWriter&,std::string_view,const std::vector<T>&);template<class T>Status emit(ProjectionWriter&,std::string_view,const std::optional<T>&);template<class...T>Status emit(ProjectionWriter&,std::string_view,const std::variant<T...>&);
void read(const V& v,StableId& x){const auto* p=v.as<StableId>();if(!p)fail("MovementContactScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const StableId& v){return w.field(name,v);}
void read(const V& v,Integer& x){const auto* p=v.as<Integer>();if(!p)fail("MovementContactScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const Integer& v){return w.field(name,v);}
void read(const V& v,bool& x){const auto* p=v.as<bool>();if(!p)fail("MovementContactScalarTypeMismatch");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const bool& v){return w.field(name,v);}
void read(const V& v,double& x){const auto* p=v.as<double>();if(!p)fail("MovementContactScalarTypeMismatch");if(!std::isfinite(*p))fail("NonCanonicalContactBinary64");x=*p;}
Status emit(ProjectionWriter& w,std::string_view name,const double& v){return w.field(name,v);}
void read(const V& v,WorldMoment& x){x=take(exact_fields::decode_json<WorldMoment>(v,[](auto& c,auto& a){return exact_fields::moment(c,a);}));}
Status emit(ProjectionWriter& w,std::string_view name,const WorldMoment& v){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::moment(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
void read(const V& v,CanonicalFraction& x){x=take(exact_fields::decode_json<CanonicalFraction>(v,[](auto& c,auto& a){return exact_fields::fraction(c,a);}));}
Status emit(ProjectionWriter& w,std::string_view name,const CanonicalFraction& v){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,v,[](auto& c,auto& a){return exact_fields::fraction(c,a);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
template<class T>void read(const V& v,std::vector<T>& out){const auto* a=v.as<V::Array>();if(!a||a->size()>CodecVisitor::max_items)fail("MovementContactArrayRequired");out.resize(a->size());for(std::size_t i=0;i<a->size();++i)read((*a)[i],out[i]);}
template<class T>void read(const V& v,std::optional<T>& out){if(v.as<std::monostate>()){out.reset();return;}out.emplace();read(v,*out);}
template<std::size_t I=0,class Variant>bool construct(Variant& out,Integer tag){if constexpr(I<std::variant_size_v<Variant>){if(tag==Integer(I)){out.template emplace<I>();return true;}return construct<I+1>(out,tag);}return false;}
template<class...T>void read(const V& v,std::variant<T...>& out){fields(v,{"variant","value"});Integer tag=0;read(get(v,"variant"),tag);if(!construct(out,tag))fail("InvalidMovementContactVariant");std::visit([&](auto& x){read(get(v,"value"),x);},out);}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::vector<T>& v){return w.sequence(name,v,[&](auto& x,const auto& item){return emit(x,"",item);});}
template<class T>Status emit(ProjectionWriter& w,std::string_view name,const std::optional<T>& v){return v?emit(w,name,*v):w.null(name);}
template<class...T>Status emit(ProjectionWriter& w,std::string_view name,const std::variant<T...>& v){return w.object(name,[&](auto& x){x.field("variant",Integer(v.index()));return std::visit([&](const auto& item){return emit(x,"value",item);},v);});}
void read(const V& v,TerrainCostPieceKind& x){Integer tag=0;read(v,tag);if(tag<0||tag>2)fail("InvalidMovementContactEnum");x=TerrainCostPieceKind(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const TerrainCostPieceKind& v){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidMovementContactEnum");return emit(w,name,tag);}
void read(const V& v,ExactContactGeometryProfile& x){Integer tag=0;read(v,tag);if(tag<0||tag>2)fail("InvalidMovementContactEnum");x=ExactContactGeometryProfile(tag);}
Status emit(ProjectionWriter& w,std::string_view name,const ExactContactGeometryProfile& v){const Integer tag=Integer(v);if(tag<0||tag>2)return w.reject("InvalidMovementContactEnum");return emit(w,name,tag);}
void read(const V& v,SourceRef& x){fields(v,{"domain","kind","id"});read(get(v,"domain"),x.domain);read(get(v,"kind"),x.kind);read(get(v,"id"),x.id);}
Status emit(ProjectionWriter& w,std::string_view name,const SourceRef& v){return w.object(name,[&](auto& x){emit(x,"domain",v.domain);emit(x,"kind",v.kind);emit(x,"id",v.id);return x.status();});}
void read(const V& v,PointI& x){fields(v,{"x","y"});read(get(v,"x"),x.x);read(get(v,"y"),x.y);}
Status emit(ProjectionWriter& w,std::string_view name,const PointI& v){return w.object(name,[&](auto& x){emit(x,"x",v.x);emit(x,"y",v.y);return x.status();});}
void read(const V& v,WildernessLocation& x){fields(v,{"map_id","origin","point"});read(get(v,"map_id"),x.map_id);read(get(v,"origin"),x.origin);read(get(v,"point"),x.point);}
Status emit(ProjectionWriter& w,std::string_view name,const WildernessLocation& v){return w.object(name,[&](auto& x){emit(x,"map_id",v.map_id);emit(x,"origin",v.origin);emit(x,"point",v.point);return x.status();});}
void read(const V& v,OnRouteLocation& x){fields(v,{"edge_id","origin","progress"});read(get(v,"edge_id"),x.edge_id);read(get(v,"origin"),x.origin);read(get(v,"progress"),x.progress);}
Status emit(ProjectionWriter& w,std::string_view name,const OnRouteLocation& v){return w.object(name,[&](auto& x){emit(x,"edge_id",v.edge_id);emit(x,"origin",v.origin);emit(x,"progress",v.progress);return x.status();});}
void read(const V& v,TerrainTravelParams& x){fields(v,{"plan_id","map_id","origin","payer_wallet_id","mode","route_index","budget","travel_credit","money_per_tick","fatigue_per_tick","points","destination","paused","encounter_id","has_encounter_id","has_met_actions","met_actions","field_order"});read(get(v,"plan_id"),x.plan_id);read(get(v,"map_id"),x.map_id);read(get(v,"origin"),x.origin);read(get(v,"payer_wallet_id"),x.payer_wallet_id);read(get(v,"mode"),x.mode);read(get(v,"route_index"),x.route_index);read(get(v,"budget"),x.budget);read(get(v,"travel_credit"),x.travel_credit);read(get(v,"money_per_tick"),x.money_per_tick);read(get(v,"fatigue_per_tick"),x.fatigue_per_tick);read(get(v,"points"),x.points);read(get(v,"destination"),x.destination);read(get(v,"paused"),x.paused);read(get(v,"encounter_id"),x.encounter_id);read(get(v,"has_encounter_id"),x.has_encounter_id);read(get(v,"has_met_actions"),x.has_met_actions);read(get(v,"met_actions"),x.met_actions);read(get(v,"field_order"),x.field_order);}
Status emit(ProjectionWriter& w,std::string_view name,const TerrainTravelParams& v){return w.object(name,[&](auto& x){emit(x,"plan_id",v.plan_id);emit(x,"map_id",v.map_id);emit(x,"origin",v.origin);emit(x,"payer_wallet_id",v.payer_wallet_id);emit(x,"mode",v.mode);emit(x,"route_index",v.route_index);emit(x,"budget",v.budget);emit(x,"travel_credit",v.travel_credit);emit(x,"money_per_tick",v.money_per_tick);emit(x,"fatigue_per_tick",v.fatigue_per_tick);emit(x,"points",v.points);emit(x,"destination",v.destination);emit(x,"paused",v.paused);emit(x,"encounter_id",v.encounter_id);emit(x,"has_encounter_id",v.has_encounter_id);emit(x,"has_met_actions",v.has_met_actions);emit(x,"met_actions",v.met_actions);emit(x,"field_order",v.field_order);return x.status();});}
void read(const V& v,RoadTravelParams& x){fields(v,{"destination","via","path","edge_index","payer_wallet_id","returning","paused","reverse_first","has_terrain","has_paused","has_reverse_first","has_encounter_id","has_met_actions","encounter_id","met_actions","field_order"});read(get(v,"destination"),x.destination);read(get(v,"via"),x.via);read(get(v,"path"),x.path);read(get(v,"edge_index"),x.edge_index);read(get(v,"payer_wallet_id"),x.payer_wallet_id);read(get(v,"returning"),x.returning);read(get(v,"paused"),x.paused);read(get(v,"reverse_first"),x.reverse_first);read(get(v,"has_terrain"),x.has_terrain);read(get(v,"has_paused"),x.has_paused);read(get(v,"has_reverse_first"),x.has_reverse_first);read(get(v,"has_encounter_id"),x.has_encounter_id);read(get(v,"has_met_actions"),x.has_met_actions);read(get(v,"encounter_id"),x.encounter_id);read(get(v,"met_actions"),x.met_actions);read(get(v,"field_order"),x.field_order);}
Status emit(ProjectionWriter& w,std::string_view name,const RoadTravelParams& v){return w.object(name,[&](auto& x){emit(x,"destination",v.destination);emit(x,"via",v.via);emit(x,"path",v.path);emit(x,"edge_index",v.edge_index);emit(x,"payer_wallet_id",v.payer_wallet_id);emit(x,"returning",v.returning);emit(x,"paused",v.paused);emit(x,"reverse_first",v.reverse_first);emit(x,"has_terrain",v.has_terrain);emit(x,"has_paused",v.has_paused);emit(x,"has_reverse_first",v.has_reverse_first);emit(x,"has_encounter_id",v.has_encounter_id);emit(x,"has_met_actions",v.has_met_actions);emit(x,"encounter_id",v.encounter_id);emit(x,"met_actions",v.met_actions);emit(x,"field_order",v.field_order);return x.status();});}
void read(const V& v,RoadTravelMotion& x){fields(v,{"actor_id","action_id","edge_id","origin","start","end","moving","params"});read(get(v,"actor_id"),x.actor_id);read(get(v,"action_id"),x.action_id);read(get(v,"edge_id"),x.edge_id);read(get(v,"origin"),x.origin);read(get(v,"start"),x.start);read(get(v,"end"),x.end);read(get(v,"moving"),x.moving);read(get(v,"params"),x.params);}
Status emit(ProjectionWriter& w,std::string_view name,const RoadTravelMotion& v){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id);emit(x,"action_id",v.action_id);emit(x,"edge_id",v.edge_id);emit(x,"origin",v.origin);emit(x,"start",v.start);emit(x,"end",v.end);emit(x,"moving",v.moving);emit(x,"params",v.params);return x.status();});}
void read(const V& v,RoadContactEndpoint& x){fields(v,{"actor_id","anchor","trace"});read(get(v,"actor_id"),x.actor_id);read(get(v,"anchor"),x.anchor);read(get(v,"trace"),x.trace);}
Status emit(ProjectionWriter& w,std::string_view name,const RoadContactEndpoint& v){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id);emit(x,"anchor",v.anchor);emit(x,"trace",v.trace);return x.status();});}
void read(const V& v,RoadContactEndpoints& x){fields(v,{"first","second"});read(get(v,"first"),x.first);read(get(v,"second"),x.second);}
Status emit(ProjectionWriter& w,std::string_view name,const RoadContactEndpoints& v){return w.object(name,[&](auto& x){emit(x,"first",v.first);emit(x,"second",v.second);return x.status();});}
void read(const V& v,ContactPoint& x){fields(v,{"x","y"});read(get(v,"x"),x.x);read(get(v,"y"),x.y);}
Status emit(ProjectionWriter& w,std::string_view name,const ContactPoint& v){return w.object(name,[&](auto& x){emit(x,"x",v.x);emit(x,"y",v.y);return x.status();});}
void read(const V& v,ContactSegment& x){fields(v,{"from","to","start_cost","end_cost","point_index"});read(get(v,"from"),x.from);read(get(v,"to"),x.to);read(get(v,"start_cost"),x.start_cost);read(get(v,"end_cost"),x.end_cost);read(get(v,"point_index"),x.point_index);}
Status emit(ProjectionWriter& w,std::string_view name,const ContactSegment& v){return w.object(name,[&](auto& x){emit(x,"from",v.from);emit(x,"to",v.to);emit(x,"start_cost",v.start_cost);emit(x,"end_cost",v.end_cost);emit(x,"point_index",v.point_index);return x.status();});}
void read(const V& v,TravelMotion& x){fields(v,{"actor_id","action_id","map_id","origin","params","segments","budget","end_credit","reached_points","end","moving","stationary","stationary_location","cost_piece_kinds","actual_consumed_total","original_point_count"});read(get(v,"actor_id"),x.actor_id);read(get(v,"action_id"),x.action_id);read(get(v,"map_id"),x.map_id);read(get(v,"origin"),x.origin);read(get(v,"params"),x.params);read(get(v,"segments"),x.segments);read(get(v,"budget"),x.budget);read(get(v,"end_credit"),x.end_credit);read(get(v,"reached_points"),x.reached_points);read(get(v,"end"),x.end);read(get(v,"moving"),x.moving);read(get(v,"stationary"),x.stationary);read(get(v,"stationary_location"),x.stationary_location);read(get(v,"cost_piece_kinds"),x.cost_piece_kinds);read(get(v,"actual_consumed_total"),x.actual_consumed_total);read(get(v,"original_point_count"),x.original_point_count);}
Status emit(ProjectionWriter& w,std::string_view name,const TravelMotion& v){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id);emit(x,"action_id",v.action_id);emit(x,"map_id",v.map_id);emit(x,"origin",v.origin);emit(x,"params",v.params);emit(x,"segments",v.segments);emit(x,"budget",v.budget);emit(x,"end_credit",v.end_credit);emit(x,"reached_points",v.reached_points);emit(x,"end",v.end);emit(x,"moving",v.moving);emit(x,"stationary",v.stationary);emit(x,"stationary_location",v.stationary_location);emit(x,"cost_piece_kinds",v.cost_piece_kinds);emit(x,"actual_consumed_total",v.actual_consumed_total);emit(x,"original_point_count",v.original_point_count);return x.status();});}
void read(const V& v,ContactEndpoint& x){fields(v,{"actor_id","anchor","trace"});read(get(v,"actor_id"),x.actor_id);read(get(v,"anchor"),x.anchor);read(get(v,"trace"),x.trace);}
Status emit(ProjectionWriter& w,std::string_view name,const ContactEndpoint& v){return w.object(name,[&](auto& x){emit(x,"actor_id",v.actor_id);emit(x,"anchor",v.anchor);emit(x,"trace",v.trace);return x.status();});}
void read(const V& v,ContactHit& x){fields(v,{"a","b","time_num","time_den","first","second","road"});read(get(v,"a"),x.a);read(get(v,"b"),x.b);read(get(v,"time_num"),x.time_num);read(get(v,"time_den"),x.time_den);read(get(v,"first"),x.first);read(get(v,"second"),x.second);read(get(v,"road"),x.road);}
Status emit(ProjectionWriter& w,std::string_view name,const ContactHit& v){return w.object(name,[&](auto& x){emit(x,"a",v.a);emit(x,"b",v.b);emit(x,"time_num",v.time_num);emit(x,"time_den",v.time_den);emit(x,"first",v.first);emit(x,"second",v.second);emit(x,"road",v.road);return x.status();});}
void read(const V& v,HalfDayDuration& x){fields(v,{"value"});read(get(v,"value"),x.value);}
Status emit(ProjectionWriter& w,std::string_view name,const HalfDayDuration& v){return w.object(name,[&](auto& x){emit(x,"value",v.value);return x.status();});}
void read(const V& v,RunningMovementMap& x){fields(v,{"world_from","cursor_at_from","rate","budget_rate","rate_source"});read(get(v,"world_from"),x.world_from);read(get(v,"cursor_at_from"),x.cursor_at_from);read(get(v,"rate"),x.rate);read(get(v,"budget_rate"),x.budget_rate);read(get(v,"rate_source"),x.rate_source);}
Status emit(ProjectionWriter& w,std::string_view name,const RunningMovementMap& v){return w.object(name,[&](auto& x){emit(x,"world_from",v.world_from);emit(x,"cursor_at_from",v.cursor_at_from);emit(x,"rate",v.rate);emit(x,"budget_rate",v.budget_rate);emit(x,"rate_source",v.rate_source);return x.status();});}
void read(const V& v,HeldMovementMap& x){fields(v,{"held_cursor","actual_hold_source","effective_at","resume_rate","resume_budget_rate","resume_rate_source"});read(get(v,"held_cursor"),x.held_cursor);read(get(v,"actual_hold_source"),x.actual_hold_source);read(get(v,"effective_at"),x.effective_at);read(get(v,"resume_rate"),x.resume_rate);read(get(v,"resume_budget_rate"),x.resume_budget_rate);read(get(v,"resume_rate_source"),x.resume_rate_source);}
Status emit(ProjectionWriter& w,std::string_view name,const HeldMovementMap& v){return w.object(name,[&](auto& x){emit(x,"held_cursor",v.held_cursor);emit(x,"actual_hold_source",v.actual_hold_source);emit(x,"effective_at",v.effective_at);emit(x,"resume_rate",v.resume_rate);emit(x,"resume_budget_rate",v.resume_budget_rate);emit(x,"resume_rate_source",v.resume_rate_source);return x.status();});}
void read(const V& v,ExactContactMotionWitness& x){fields(v,{"unit_ref","basis_ref","original_mapping"});read(get(v,"unit_ref"),x.unit_ref);read(get(v,"basis_ref"),x.basis_ref);read(get(v,"original_mapping"),x.original_mapping);}
Status emit(ProjectionWriter& w,std::string_view name,const ExactContactMotionWitness& v){return w.object(name,[&](auto& x){emit(x,"unit_ref",v.unit_ref);emit(x,"basis_ref",v.basis_ref);emit(x,"original_mapping",v.original_mapping);return x.status();});}
void read(const V& v,ExactMovementContact& x){fields(v,{"id","original_hit","window_from","window_duration","occurred_at","first_prefix_id","second_prefix_id","first_motion_source","second_motion_source","exact_road_fraction","geometry_profile"});read(get(v,"id"),x.id);read(get(v,"original_hit"),x.original_hit);read(get(v,"window_from"),x.window_from);read(get(v,"window_duration"),x.window_duration);read(get(v,"occurred_at"),x.occurred_at);read(get(v,"first_prefix_id"),x.first_prefix_id);read(get(v,"second_prefix_id"),x.second_prefix_id);read(get(v,"first_motion_source"),x.first_motion_source);read(get(v,"second_motion_source"),x.second_motion_source);read(get(v,"exact_road_fraction"),x.exact_road_fraction);read(get(v,"geometry_profile"),x.geometry_profile);}
Status emit(ProjectionWriter& w,std::string_view name,const ExactMovementContact& v){return w.object(name,[&](auto& x){emit(x,"id",v.id);emit(x,"original_hit",v.original_hit);emit(x,"window_from",v.window_from);emit(x,"window_duration",v.window_duration);emit(x,"occurred_at",v.occurred_at);emit(x,"first_prefix_id",v.first_prefix_id);emit(x,"second_prefix_id",v.second_prefix_id);emit(x,"first_motion_source",v.first_motion_source);emit(x,"second_motion_source",v.second_motion_source);emit(x,"exact_road_fraction",v.exact_road_fraction);emit(x,"geometry_profile",v.geometry_profile);return x.status();});}
}
Outcome<ExactMovementContact> decode_exact_movement_contact(const BoundaryValue& v){return movement_detail::guard<ExactMovementContact>([&]{ExactMovementContact out;contact_json::read(v,out);auto actual=movement_detail::take(read_exact_contact_mapped_moment(out));if(!movement_detail::same_moment(actual,out.occurred_at))movement_detail::fail("MovementContactMomentMismatch",out.id);return out;});}
Status emit_exact_movement_contact_projection(ProjectionWriter& w,std::string_view name,const ExactMovementContact& v){auto mapped=read_exact_contact_mapped_moment(v);if(!mapped.ok)return {false,{},mapped.error};auto same=compare_world_moments(mapped.value,v.occurred_at);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("MovementContactMomentMismatch",v.id);return contact_json::emit(w,name,v);}
Outcome<BoundaryValue> export_exact_movement_contact(const ExactMovementContact& v){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(emit_exact_movement_contact_projection(w,"",v));movement_detail::take(w.finish());return out;});}
}
