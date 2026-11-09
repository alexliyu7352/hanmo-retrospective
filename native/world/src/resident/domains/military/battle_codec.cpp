#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
// 既有军事字段的唯一冷编解码；未知字段拒绝，所有嵌套字典保原序但不保第二份值。
#include "battle_world_model.hpp"
#include "battle_codec.hpp"
#include "military_commander.hpp"
#include "military_rout_profile.hpp"
#include "military_rout_retreat.hpp"
#include "military_command_shock_codec.hpp"
#include "military_combat_conditions_codec.hpp"
#include "military_commanded_damage_codec.hpp"
#include "military_command_resources_codec.hpp"
#include "military_tactic_entry_codec.hpp"
#include "military_officer_codec.hpp"
#include <set>
#include <algorithm>
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::battle_codec {
namespace {
template<class T> Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class Alloc> Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,Alloc>&);
template<class T,class Alloc> Status emit_value(ProjectionWriter&,const StableId&,const std::vector<std::pair<StableId,T>,Alloc>&);
template<class T> Status emit_value(ProjectionWriter& w,const StableId& key,const T& value){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,value);
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,key,value);
 else if constexpr(std::is_same_v<T,ReservationRef>)return emit_common_reservation(w,key,value,false,false);
 else if constexpr(std::is_same_v<T,Location>)return emit_common_location(w,key,value);
 else if constexpr(std::is_same_v<T,InteractionContext>)return w.object(key,[&](auto& child){return emit_interaction_context_fields(child,value);});
 else if constexpr(std::is_same_v<T,HexPoint>){const std::array<Integer,2> xy{value.q,value.r};return w.sequence(key,xy,[](auto& child,Integer n){return child.field("",n);});}
 else return w.object(key,[&](auto& child){return emit_fields(child,value);});
}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& child,const auto& row){return emit_value(child,"",row);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<std::pair<StableId,T>,Alloc>& rows){return w.object(key,[&](auto& child){for(const auto& row:rows){auto result=emit_value(child,row.first,row.second);if(!result.ok)return result;}return child.status();});}
template<class Fn>void append_projected_fields(BoundaryValue::Dict& target,Fn emit){BoundaryValue result;BoundaryProjectionWriter sink(result);auto checked=sink.object("",emit);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);auto* rows=result.as<BoundaryValue::Dict>();for(auto& row:*rows)target.push_back(std::move(row));}
template<class T>BoundaryValue encode_projected(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
}
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
using domain_codec::ordered;using domain_codec::order;using domain_codec::visit_order;
V encode(const StableId& s){return V{s};} V encode(Integer x){return V{x};} V encode(bool x){return V{x};}
void decode(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void decode(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}void decode(const V& v,bool& x,const StableId& p){x=boundary::boolean(v,p);}
Status visit(CodecVisitor& c,StableId& x){return c.field("value",x);}Status visit(CodecVisitor& c,Integer& x){return c.field("value",x);}Status visit(CodecVisitor& c,bool& x){return c.field("value",x);}
V encode(const HexPoint& p){return V{A{V{p.q},V{p.r}}};}void decode(const V& v,HexPoint& x,const StableId& p){auto& a=boundary::array(v,p);if(a.size()!=2)boundary::fail("InvalidHexDestination",p);x={boundary::integer(a[0],p),boundary::integer(a[1],p)};}Status visit(CodecVisitor& c,HexPoint& p){c.field("q",p.q);return c.field("r",p.r);}
V encode(const Location& l){return boundary::encode_location(l);}void decode(const V& v,Location& l,const StableId& p){l=boundary::decode_location(v,p);}Status visit(CodecVisitor& c,Location& l){return common_checkpoint::location(c,l);}
template<class T>V encode(const std::vector<T>& rows){A a;for(auto& r:rows)a.push_back(encode(r));return V{std::move(a)};}
template<class T>void decode(const V& v,std::vector<T>& out,const StableId& p){out.clear();for(auto& r:boundary::array(v,p)){T x;decode(r,x,p+"/"+std::to_string(out.size()));out.push_back(std::move(x));}}
template<class T>Status visit(CodecVisitor& c,std::vector<T>& rows){return c.sequence("rows",rows,[](auto& c,auto& x){return visit(c,x);});}
template<class T>V encode(const std::vector<std::pair<StableId,T>>& rows){D d;for(auto& r:rows)d.emplace_back(r.first,encode(r.second));return V{std::move(d)};}
template<class T>void decode(const V& v,std::vector<std::pair<StableId,T>>& out,const StableId& p){out.clear();for(auto& r:boundary::object(v,p)){T x;decode(r.second,x,p+"/"+r.first);out.emplace_back(r.first,std::move(x));}}
template<class T>Status visit(CodecVisitor& c,std::vector<std::pair<StableId,T>>& rows){return c.sequence("pairs",rows,[](auto& c,auto& r){c.field("key",r.first);return visit(c,r.second);});}
// MilitaryOrder的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryOrder& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"action","destination","heading"},{"target_id","tactic_id"},p);
decode(boundary::field(v,"action",p),r.action,p+"/action");
decode(boundary::field(v,"destination",p),r.destination,p+"/destination");
decode(boundary::field(v,"heading",p),r.heading,p+"/heading");
if(auto x=v.find("target_id")){decode(*x,r.target_id,p+"/target_id");}
if(auto x=v.find("tactic_id")){decode(*x,r.tactic_id,p+"/tactic_id");}
}
Status emit_fields(ProjectionWriter& w,const MilitaryOrder& r){w.field("action",r.action);const std::array<Integer,2> coordinates{r.destination.q,r.destination.r};w.sequence("destination",coordinates,[](auto& x,Integer v){return x.field("",v);});w.field("heading",r.heading);if(r.field_order.empty()||domain_codec::had(r.field_order,"target_id"))w.field("target_id",r.target_id);if(r.field_order.empty()||domain_codec::had(r.field_order,"tactic_id"))w.field("tactic_id",r.tactic_id);w.order_fields(r.field_order);return w.status();}
BoundaryValue encode(const MilitaryOrder& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto written=sink.object("",[&](auto& w){return emit_fields(w,r);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);auto finished=sink.finish();if(!finished.ok)boundary::fail(finished.error.original.code,finished.error.original.path);return out;}
Status visit(CodecVisitor& c,MilitaryOrder& r){
c.object("action",[&](auto& c){return visit(c,r.action);});
c.object("destination",[&](auto& c){return visit(c,r.destination);});
c.object("heading",[&](auto& c){return visit(c,r.heading);});
c.object("target_id",[&](auto& c){return visit(c,r.target_id);});
c.object("tactic_id",[&](auto& c){return visit(c,r.tactic_id);});
visit_order(c,r.field_order);return c.status();}
// BattleUnit的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,BattleUnit& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"unit_id","army_id","commander_actor_id","troop_type_id","side","position","heading","morale","tactic_points","statuses","withdrawn","member_ids","fit","wounded","dead","captured","deserted"},{},p);
decode(boundary::field(v,"unit_id",p),r.unit_id,p+"/unit_id");
decode(boundary::field(v,"army_id",p),r.army_id,p+"/army_id");
decode(boundary::field(v,"commander_actor_id",p),r.commander_actor_id,p+"/commander_actor_id");
decode(boundary::field(v,"troop_type_id",p),r.troop_type_id,p+"/troop_type_id");
decode(boundary::field(v,"side",p),r.side,p+"/side");
decode(boundary::field(v,"position",p),r.position,p+"/position");
decode(boundary::field(v,"heading",p),r.heading,p+"/heading");
decode(boundary::field(v,"morale",p),r.morale,p+"/morale");
decode(boundary::field(v,"tactic_points",p),r.tactic_points,p+"/tactic_points");
decode(boundary::field(v,"statuses",p),r.statuses,p+"/statuses");
decode(boundary::field(v,"withdrawn",p),r.withdrawn,p+"/withdrawn");
decode(boundary::field(v,"member_ids",p),r.member_ids,p+"/member_ids");
decode(boundary::field(v,"fit",p),r.fit,p+"/fit");
decode(boundary::field(v,"wounded",p),r.wounded,p+"/wounded");
decode(boundary::field(v,"dead",p),r.dead,p+"/dead");
decode(boundary::field(v,"captured",p),r.captured,p+"/captured");
decode(boundary::field(v,"deserted",p),r.deserted,p+"/deserted");
}
Status emit_fields(ProjectionWriter& w,const BattleUnit& r){
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"commander_actor_id",r.commander_actor_id);
emit_value(w,"troop_type_id",r.troop_type_id);
emit_value(w,"side",r.side);
emit_value(w,"position",r.position);
emit_value(w,"heading",r.heading);
emit_value(w,"morale",r.morale);
emit_value(w,"tactic_points",r.tactic_points);
emit_value(w,"statuses",r.statuses);
emit_value(w,"withdrawn",r.withdrawn);
emit_value(w,"member_ids",r.member_ids);
emit_value(w,"fit",r.fit);
emit_value(w,"wounded",r.wounded);
emit_value(w,"dead",r.dead);
emit_value(w,"captured",r.captured);
emit_value(w,"deserted",r.deserted);
w.order_fields(r.field_order);return w.status();}
V encode(const BattleUnit& r){return encode_projected(r);}
Status visit(CodecVisitor& c,BattleUnit& r){
c.object("unit_id",[&](auto& c){return visit(c,r.unit_id);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("commander_actor_id",[&](auto& c){return visit(c,r.commander_actor_id);});
c.object("troop_type_id",[&](auto& c){return visit(c,r.troop_type_id);});
c.object("side",[&](auto& c){return visit(c,r.side);});
c.object("position",[&](auto& c){return visit(c,r.position);});
c.object("heading",[&](auto& c){return visit(c,r.heading);});
c.object("morale",[&](auto& c){return visit(c,r.morale);});
c.object("tactic_points",[&](auto& c){return visit(c,r.tactic_points);});
c.object("statuses",[&](auto& c){return visit(c,r.statuses);});
c.object("withdrawn",[&](auto& c){return visit(c,r.withdrawn);});
c.object("member_ids",[&](auto& c){return visit(c,r.member_ids);});
c.object("fit",[&](auto& c){return visit(c,r.fit);});
c.object("wounded",[&](auto& c){return visit(c,r.wounded);});
c.object("dead",[&](auto& c){return visit(c,r.dead);});
c.object("captured",[&](auto& c){return visit(c,r.captured);});
c.object("deserted",[&](auto& c){return visit(c,r.deserted);});
visit_order(c,r.field_order);return c.status();}
// MilitaryObjectiveProgress的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryObjectiveProgress& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"hold_rounds","escaped_unit_ids"},{},p);
decode(boundary::field(v,"hold_rounds",p),r.hold_rounds,p+"/hold_rounds");
decode(boundary::field(v,"escaped_unit_ids",p),r.escaped_unit_ids,p+"/escaped_unit_ids");
}
Status emit_fields(ProjectionWriter& w,const MilitaryObjectiveProgress& r){
emit_value(w,"hold_rounds",r.hold_rounds);
emit_value(w,"escaped_unit_ids",r.escaped_unit_ids);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryObjectiveProgress& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryObjectiveProgress& r){
c.object("hold_rounds",[&](auto& c){return visit(c,r.hold_rounds);});
c.object("escaped_unit_ids",[&](auto& c){return visit(c,r.escaped_unit_ids);});
visit_order(c,r.field_order);return c.status();}
// MilitaryBattleResult的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryBattleResult& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"winner_side","reason"},{},p);
decode(boundary::field(v,"winner_side",p),r.winner_side,p+"/winner_side");
decode(boundary::field(v,"reason",p),r.reason,p+"/reason");
}
Status emit_fields(ProjectionWriter& w,const MilitaryBattleResult& r){
emit_value(w,"winner_side",r.winner_side);
emit_value(w,"reason",r.reason);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryBattleResult& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryBattleResult& r){
c.object("winner_side",[&](auto& c){return visit(c,r.winner_side);});
c.object("reason",[&](auto& c){return visit(c,r.reason);});
visit_order(c,r.field_order);return c.status();}
// MilitaryBattleEvent的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryBattleEvent& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"kind"},{"unit_id","target_id","attacker_id","tactic_id","code","captor_unit_id","captor_army_id","reason","morale","casualties","hit_chance","original_casualties","captured","deserted","winner_side","position","captured_ids"},p);
decode(boundary::field(v,"kind",p),r.kind,p+"/kind");
if(auto x=v.find("unit_id")){decode(*x,r.unit_id,p+"/unit_id");}
if(auto x=v.find("target_id")){r.target_id.emplace();decode(*x,*r.target_id,p+"/target_id");}
if(auto x=v.find("attacker_id")){r.attacker_id.emplace();decode(*x,*r.attacker_id,p+"/attacker_id");}
if(auto x=v.find("tactic_id")){r.tactic_id.emplace();decode(*x,*r.tactic_id,p+"/tactic_id");}
if(auto x=v.find("code")){r.code.emplace();decode(*x,*r.code,p+"/code");}
if(auto x=v.find("captor_unit_id")){r.captor_unit_id.emplace();decode(*x,*r.captor_unit_id,p+"/captor_unit_id");}
if(auto x=v.find("captor_army_id")){r.captor_army_id.emplace();decode(*x,*r.captor_army_id,p+"/captor_army_id");}
if(auto x=v.find("reason")){r.reason.emplace();decode(*x,*r.reason,p+"/reason");}
if(auto x=v.find("morale")){r.morale.emplace();decode(*x,*r.morale,p+"/morale");}
if(auto x=v.find("casualties")){r.casualties.emplace();decode(*x,*r.casualties,p+"/casualties");}
if(auto x=v.find("hit_chance")){r.hit_chance.emplace();decode(*x,*r.hit_chance,p+"/hit_chance");}
if(auto x=v.find("original_casualties")){r.original_casualties.emplace();decode(*x,*r.original_casualties,p+"/original_casualties");}
if(auto x=v.find("captured")){r.captured.emplace();decode(*x,*r.captured,p+"/captured");}
if(auto x=v.find("deserted")){r.deserted.emplace();decode(*x,*r.deserted,p+"/deserted");}
if(auto x=v.find("winner_side")){r.winner_side.emplace();decode(*x,*r.winner_side,p+"/winner_side");}
if(auto x=v.find("position")){r.position.emplace();decode(*x,*r.position,p+"/position");}
if(auto x=v.find("captured_ids")){r.captured_ids.emplace();decode(*x,*r.captured_ids,p+"/captured_ids");}
}
// 新事件按原各kind字面构造顺序投影；已载入事件仍以原field_order精确恢复。
Status emit_fields(ProjectionWriter& w,const MilitaryBattleEvent& r){
emit_value(w,"kind",r.kind);
if((!r.unit_id.empty()||domain_codec::had(r.field_order,"unit_id")))emit_value(w,"unit_id",r.unit_id);
if(r.target_id)emit_value(w,"target_id",*r.target_id);
if(r.attacker_id)emit_value(w,"attacker_id",*r.attacker_id);
if(r.tactic_id)emit_value(w,"tactic_id",*r.tactic_id);
if(r.morale)emit_value(w,"morale",*r.morale);
if(r.casualties)emit_value(w,"casualties",*r.casualties);
if(r.hit_chance)emit_value(w,"hit_chance",*r.hit_chance);
if(r.original_casualties)emit_value(w,"original_casualties",*r.original_casualties);
if(r.captured)emit_value(w,"captured",*r.captured);
if(r.deserted)emit_value(w,"deserted",*r.deserted);
if(r.captured_ids)emit_value(w,"captured_ids",*r.captured_ids);
if(r.captor_unit_id)emit_value(w,"captor_unit_id",*r.captor_unit_id);
if(r.captor_army_id)emit_value(w,"captor_army_id",*r.captor_army_id);
if(r.winner_side)emit_value(w,"winner_side",*r.winner_side);
if(r.reason)emit_value(w,"reason",*r.reason);
if(r.code)emit_value(w,"code",*r.code);
if(r.position)emit_value(w,"position",*r.position);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryBattleEvent& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryBattleEvent& r){
c.object("kind",[&](auto& c){return visit(c,r.kind);});
c.object("unit_id",[&](auto& c){return visit(c,r.unit_id);});
c.optional("target_id",r.target_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("attacker_id",r.attacker_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("tactic_id",r.tactic_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("code",r.code,[](auto& c,auto& x){return visit(c,x);});
c.optional("captor_unit_id",r.captor_unit_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("captor_army_id",r.captor_army_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("reason",r.reason,[](auto& c,auto& x){return visit(c,x);});
c.optional("morale",r.morale,[](auto& c,auto& x){return visit(c,x);});
c.optional("casualties",r.casualties,[](auto& c,auto& x){return visit(c,x);});
c.optional("hit_chance",r.hit_chance,[](auto& c,auto& x){return visit(c,x);});
c.optional("original_casualties",r.original_casualties,[](auto& c,auto& x){return visit(c,x);});
c.optional("captured",r.captured,[](auto& c,auto& x){return visit(c,x);});
c.optional("deserted",r.deserted,[](auto& c,auto& x){return visit(c,x);});
c.optional("winner_side",r.winner_side,[](auto& c,auto& x){return visit(c,x);});
c.optional("position",r.position,[](auto& c,auto& x){return visit(c,x);});
c.optional("captured_ids",r.captured_ids,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryMotionSource的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryMotionSource& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"actor_id","action_id","plan_id","stationary","location"},{"route_index"},p);
decode(boundary::field(v,"actor_id",p),r.actor_id,p+"/actor_id");
decode(boundary::field(v,"action_id",p),r.action_id,p+"/action_id");
decode(boundary::field(v,"plan_id",p),r.plan_id,p+"/plan_id");
decode(boundary::field(v,"stationary",p),r.stationary,p+"/stationary");
decode(boundary::field(v,"location",p),r.location,p+"/location");
if(auto x=v.find("route_index")){r.route_index.emplace();decode(*x,*r.route_index,p+"/route_index");}
}
Status emit_fields(ProjectionWriter& w,const MilitaryMotionSource& r){
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"action_id",r.action_id);
emit_value(w,"plan_id",r.plan_id);
emit_value(w,"stationary",r.stationary);
emit_value(w,"location",r.location);
if(r.route_index)emit_value(w,"route_index",*r.route_index);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryMotionSource& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryMotionSource& r){
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("action_id",[&](auto& c){return visit(c,r.action_id);});
c.object("plan_id",[&](auto& c){return visit(c,r.plan_id);});
c.object("stationary",[&](auto& c){return visit(c,r.stationary);});
c.object("location",[&](auto& c){return visit(c,r.location);});
c.optional("route_index",r.route_index,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryActorChange的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryActorChange& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"actor_id","status","captor_army_id"},{"army_id","garrison_id"},p);
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"status",p),r.status,checkpoint_decode_boundary_join({p,"/status"}));
if(auto x=v.find("army_id"))decode(*x,r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));
decode(boundary::field(v,"captor_army_id",p),r.captor_army_id,checkpoint_decode_boundary_join({p,"/captor_army_id"}));
if(auto x=v.find("garrison_id")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<StableId>();r.garrison_id.emplace();decode(*x,*r.garrison_id,checkpoint_decode_boundary_join({p,"/garrison_id"}));}
}
Status emit_fields(ProjectionWriter& w,const MilitaryActorChange& r){w.field("actor_id",r.actor_id);w.field("status",r.status);if(!r.army_id.empty()||domain_codec::had(r.field_order,"army_id"))w.field("army_id",r.army_id);w.field("captor_army_id",r.captor_army_id);if(r.garrison_id)w.field("garrison_id",*r.garrison_id);w.order_fields(r.field_order);return w.status();}
BoundaryValue encode(const MilitaryActorChange& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto written=sink.object("",[&](auto& w){return emit_fields(w,r);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);auto finished=sink.finish();if(!finished.ok)boundary::fail(finished.error.original.code,finished.error.original.path);return out;}
Status visit(CodecVisitor& c,MilitaryActorChange& r){
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("status",[&](auto& c){return visit(c,r.status);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("captor_army_id",[&](auto& c){return visit(c,r.captor_army_id);});
c.optional("garrison_id",r.garrison_id,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryRosterReceipt的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryRosterReceipt& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"fit","wounded","dead","captured","deserted","member_ids","morale","captors"},{},p);
decode(boundary::field(v,"fit",p),r.fit,p+"/fit");
decode(boundary::field(v,"wounded",p),r.wounded,p+"/wounded");
decode(boundary::field(v,"dead",p),r.dead,p+"/dead");
decode(boundary::field(v,"captured",p),r.captured,p+"/captured");
decode(boundary::field(v,"deserted",p),r.deserted,p+"/deserted");
decode(boundary::field(v,"member_ids",p),r.member_ids,p+"/member_ids");
decode(boundary::field(v,"morale",p),r.morale,p+"/morale");
decode(boundary::field(v,"captors",p),r.captors,p+"/captors");
}
Status emit_fields(ProjectionWriter& w,const MilitaryRosterReceipt& r){
emit_value(w,"fit",r.fit);
emit_value(w,"wounded",r.wounded);
emit_value(w,"dead",r.dead);
emit_value(w,"captured",r.captured);
emit_value(w,"deserted",r.deserted);
emit_value(w,"member_ids",r.member_ids);
emit_value(w,"morale",r.morale);
emit_value(w,"captors",r.captors);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryRosterReceipt& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryRosterReceipt& r){
c.field("unit_id",r.unit_id);
c.object("fit",[&](auto& c){return visit(c,r.fit);});
c.object("wounded",[&](auto& c){return visit(c,r.wounded);});
c.object("dead",[&](auto& c){return visit(c,r.dead);});
c.object("captured",[&](auto& c){return visit(c,r.captured);});
c.object("deserted",[&](auto& c){return visit(c,r.deserted);});
c.object("member_ids",[&](auto& c){return visit(c,r.member_ids);});
c.object("morale",[&](auto& c){return visit(c,r.morale);});
c.object("captors",[&](auto& c){return visit(c,r.captors);});
visit_order(c,r.field_order);return c.status();}
// MilitaryCasualtyFact的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryCasualtyFact& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"kind","battle_id","tick","units","actor_changes"},{},p);
if(boundary::get_string(v,"kind",p)!="military_casualties")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"battle_id",p),r.battle_id,p+"/battle_id");
decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
for(auto& row:boundary::object(boundary::field(v,"units",p),p+"/units")){MilitaryRosterReceipt x;decode(row.second,x,p+"/units/"+row.first);x.unit_id=row.first;r.units.push_back(std::move(x));}
decode(boundary::field(v,"actor_changes",p),r.actor_changes,p+"/actor_changes");
}
Status emit_fields(ProjectionWriter& w,const MilitaryCasualtyFact& r){
w.field("kind",StableId("military_casualties"));
emit_value(w,"battle_id",r.battle_id);
emit_value(w,"tick",r.tick);
w.object("units",[&](auto& child){for(const auto& x:r.units){auto checked=emit_value(child,x.unit_id,x);if(!checked.ok)return checked;}return child.status();});
emit_value(w,"actor_changes",r.actor_changes);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryCasualtyFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryCasualtyFact& r){
c.field("id",r.id);
c.object("battle_id",[&](auto& c){return visit(c,r.battle_id);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("units",[&](auto& c){return visit(c,r.units);});
c.object("actor_changes",[&](auto& c){return visit(c,r.actor_changes);});
visit_order(c,r.field_order);return c.status();}
// MilitaryBattleFact的固定字段存在位与原序一起保存，错误原样返回。
void decode(const V& v,MilitaryBattleFact& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"kind","tick","start_tick","phase","session_id","round","army_ids","unit_ids","result","source_id","contact_source_id","winner_side","final"},{},p);
if(boundary::get_string(v,"kind",p)!="military_battle")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
decode(boundary::field(v,"start_tick",p),r.start_tick,p+"/start_tick");
decode(boundary::field(v,"phase",p),r.phase,p+"/phase");
decode(boundary::field(v,"session_id",p),r.session_id,p+"/session_id");
decode(boundary::field(v,"round",p),r.round,p+"/round");
decode(boundary::field(v,"army_ids",p),r.army_ids,p+"/army_ids");
decode(boundary::field(v,"unit_ids",p),r.unit_ids,p+"/unit_ids");
if(!boundary::object(boundary::field(v,"result",p),p+"/result").empty()){r.result.emplace();decode(boundary::field(v,"result",p),*r.result,p+"/result");}
decode(boundary::field(v,"source_id",p),r.source_id,p+"/source_id");
decode(boundary::field(v,"contact_source_id",p),r.contact_source_id,p+"/contact_source_id");
decode(boundary::field(v,"winner_side",p),r.winner_side,p+"/winner_side");
decode(boundary::field(v,"final",p),r.final,p+"/final");
}
Status emit_fields(ProjectionWriter& w,const MilitaryBattleFact& r){
w.field("kind",StableId("military_battle"));
emit_value(w,"phase",r.phase);
emit_value(w,"tick",r.tick);
emit_value(w,"start_tick",r.start_tick);
emit_value(w,"session_id",r.session_id);
emit_value(w,"round",r.round);
emit_value(w,"army_ids",r.army_ids);
emit_value(w,"unit_ids",r.unit_ids);
if(r.result)emit_value(w,"result",*r.result);else w.object("result",[](auto& child){return child.status();});
emit_value(w,"source_id",r.source_id);
emit_value(w,"contact_source_id",r.contact_source_id);
emit_value(w,"winner_side",r.winner_side);
emit_value(w,"final",r.final);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryBattleFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryBattleFact& r){
c.field("id",r.id);
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("start_tick",[&](auto& c){return visit(c,r.start_tick);});
c.object("phase",[&](auto& c){return visit(c,r.phase);});
c.object("session_id",[&](auto& c){return visit(c,r.session_id);});
c.object("round",[&](auto& c){return visit(c,r.round);});
c.object("army_ids",[&](auto& c){return visit(c,r.army_ids);});
c.object("unit_ids",[&](auto& c){return visit(c,r.unit_ids);});
c.optional("result",r.result,[](auto& c,auto& x){return visit(c,x);});
c.object("source_id",[&](auto& c){return visit(c,r.source_id);});
c.object("contact_source_id",[&](auto& c){return visit(c,r.contact_source_id);});
c.object("winner_side",[&](auto& c){return visit(c,r.winner_side);});
c.object("final",[&](auto& c){return visit(c,r.final);});
visit_order(c,r.field_order);return c.status();}
// 完整会话与原空result、rng及单位表的语义对应，不能用纯轮次快照代替世界会话。
void decode(const V& v,MilitaryBattleRecord& r,const StableId& p){
r.field_order=order(v);boundary::fields(v,{"id","kind","battlefield_id","objective_id","objective_unit_ids","phase","round","units","ai_plans","objective_state","rng","last_events","result","created_tick","last_tick","completed_tick","source_id","army_ids","participant_actor_ids","plan_submitted","pending_orders","round_receipt_ids","settlement_receipt_id","applied","controller_actor_id","controller_side","command_scope","command_army_id","command_unit_ids","authority_office_id"},{"rout_retreat_enabled","command_shock","combat_conditions","commanded_damage","command_resources","learned_tactics"},p);if(const auto* selected=v.find("learned_tactics")){auto decoded=military_tactic_entry_codec::decode(*selected);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);r.learned_tactics=std::move(decoded.value);}if(const auto* selected=v.find("command_resources")){auto decoded=military_command_resources_codec::decode(*selected);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);r.command_resources=std::move(decoded.value);}if(const auto* selected=v.find("commanded_damage")){auto decoded=military_commanded_damage_codec::decode(*selected);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);r.commanded_damage=std::move(decoded.value);}if(const auto* condition=v.find("combat_conditions")){auto decoded=military_combat_conditions_codec::decode(*condition);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);r.combat_conditions=std::move(decoded.value);}if(const auto* shock=v.find("command_shock")){auto decoded=military_command_shock_codec::decode(*shock);if(!decoded.ok)boundary::fail(decoded.error.original.code,decoded.error.original.path);r.command_shock=std::move(decoded.value);}if(const auto* selected=v.find("rout_retreat_enabled"))r.rout_retreat_enabled=boundary::boolean(*selected,p+"/rout_retreat_enabled");
#define GET(n) decode(boundary::field(v,#n,p),r.n,p+"/"+#n)
GET(id);GET(kind);GET(battlefield_id);GET(objective_id);GET(objective_unit_ids);GET(phase);GET(round);GET(ai_plans);GET(objective_state);GET(last_events);GET(created_tick);GET(last_tick);GET(completed_tick);GET(source_id);GET(army_ids);GET(participant_actor_ids);GET(plan_submitted);GET(pending_orders);GET(round_receipt_ids);GET(settlement_receipt_id);GET(applied);GET(controller_actor_id);GET(controller_side);GET(command_scope);GET(command_army_id);GET(command_unit_ids);GET(authority_office_id);
#undef GET
for(auto& row:boundary::object(boundary::field(v,"units",p),p+"/units")){BattleUnit u;decode(row.second,u,p+"/units/"+row.first);if(u.unit_id!=row.first)boundary::fail("InvalidMilitaryUnit",p);r.units.push_back(std::move(u));}
auto& rng=boundary::field(v,"rng",p);boundary::fields(rng,{"seed","state"},{},p+"/rng");r.rng_field_order=order(rng);r.rng_seed=boundary::get_string(rng,"seed",p);r.rng_state=boundary::get_string(rng,"state",p);
auto& result=boundary::field(v,"result",p);if(!boundary::object(result,p).empty()){r.result.emplace();decode(result,*r.result,p+"/result");}}
Status emit_fields(ProjectionWriter& w,const MilitaryBattleRecord& r){
#define PUT(n) emit_value(w,#n,r.n)
PUT(id);PUT(kind);PUT(battlefield_id);PUT(objective_id);PUT(objective_unit_ids);PUT(phase);PUT(round);w.object("units",[&](auto& child){for(const auto& u:r.units){auto checked=emit_value(child,u.unit_id,u);if(!checked.ok)return checked;}return child.status();});PUT(ai_plans);PUT(objective_state);w.object("rng",r.rng_field_order,[&](auto& child){child.field("seed",r.rng_seed);return child.field("state",r.rng_state);});PUT(last_events);if(r.result)emit_value(w,"result",*r.result);else w.object("result",[](auto& child){return child.status();});PUT(created_tick);PUT(last_tick);PUT(completed_tick);PUT(source_id);PUT(army_ids);PUT(participant_actor_ids);PUT(plan_submitted);PUT(pending_orders);PUT(round_receipt_ids);PUT(settlement_receipt_id);PUT(applied);PUT(controller_actor_id);PUT(controller_side);PUT(command_scope);PUT(command_army_id);PUT(command_unit_ids);PUT(authority_office_id);if(r.rout_retreat_enabled||domain_codec::had(r.field_order,"rout_retreat_enabled"))PUT(rout_retreat_enabled);if(r.command_shock)w.object("command_shock",[&](auto& child){return military_command_shock_codec::emit_fields(child,*r.command_shock);});if(r.combat_conditions)w.object("combat_conditions",[&](auto& child){return military_combat_conditions_codec::emit_fields(child,*r.combat_conditions);});if(r.commanded_damage)w.object("commanded_damage",[&](auto& child){return military_commanded_damage_codec::emit_fields(child,*r.commanded_damage);});if(r.command_resources)w.object("command_resources",[&](auto& child){return military_command_resources_codec::emit_fields(child,*r.command_resources);});if(r.learned_tactics)w.object("learned_tactics",[&](auto& child){return military_tactic_entry_codec::emit_fields(child,*r.learned_tactics);});
#undef PUT
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryBattleRecord& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryBattleRecord& r){
#define VISIT(n) c.object(#n,[&](auto& c){return visit(c,r.n);})
VISIT(id);VISIT(kind);VISIT(battlefield_id);VISIT(objective_id);VISIT(objective_unit_ids);VISIT(phase);VISIT(round);VISIT(units);VISIT(ai_plans);VISIT(objective_state);VISIT(rng_seed);VISIT(rng_state);VISIT(rng_field_order);VISIT(last_events);c.optional("result",r.result,[](auto& c,auto& x){return visit(c,x);});VISIT(created_tick);VISIT(last_tick);VISIT(completed_tick);VISIT(source_id);VISIT(army_ids);VISIT(participant_actor_ids);VISIT(plan_submitted);VISIT(pending_orders);VISIT(round_receipt_ids);VISIT(settlement_receipt_id);VISIT(applied);VISIT(controller_actor_id);VISIT(controller_side);VISIT(command_scope);VISIT(command_army_id);VISIT(command_unit_ids);VISIT(authority_office_id);if(c.schema_features&military_rout_checkpoint_feature){if(!(c.schema_features&military_supply_attrition_checkpoint_feature))return c.reject("MilitaryRoutPrerequisiteRequired");VISIT(rout_retreat_enabled);}else if(r.rout_retreat_enabled||domain_codec::had(r.field_order,"rout_retreat_enabled"))return c.reject("UnexpectedMilitaryRoutHistory");if(c.schema_features&military_command_shock_checkpoint_feature){if(!(c.schema_features&military_rout_checkpoint_feature))return c.reject("MilitaryCommandShockPrerequisiteRequired");auto restored=c.optional("command_shock",r.command_shock,[](auto& child,auto& value){return military_command_shock_codec::visit(child,value);});if(!restored.ok)return restored;}else if(r.command_shock||domain_codec::had(r.field_order,"command_shock"))return c.reject("UnexpectedMilitaryCommandShock");if(c.schema_features&military_combat_conditions_checkpoint_feature){if(!(c.schema_features&military_garrison_rest_checkpoint_feature))return c.reject("MilitaryCombatConditionPrerequisiteRequired");auto restored=c.optional("combat_conditions",r.combat_conditions,[](auto& child,auto& value){return military_combat_conditions_codec::visit(child,value);});if(!restored.ok)return restored;}else if(r.combat_conditions||domain_codec::had(r.field_order,"combat_conditions"))return c.reject("UnexpectedMilitaryCombatConditions");if(c.schema_features&military_commanded_damage_checkpoint_feature){if(!(c.schema_features&military_combat_conditions_checkpoint_feature))return c.reject("MilitaryCommandedDamagePrerequisiteRequired");auto restored=c.optional("commanded_damage",r.commanded_damage,[](auto& child,auto& value){return military_commanded_damage_codec::visit(child,value);});if(!restored.ok)return restored;}else if(r.commanded_damage||domain_codec::had(r.field_order,"commanded_damage"))return c.reject("UnexpectedMilitaryCommandedDamage");if(c.schema_features&military_command_resources_checkpoint_feature){if(!(c.schema_features&military_commanded_damage_checkpoint_feature))return c.reject("MilitaryCommandResourcesPrerequisiteRequired");auto restored=c.optional("command_resources",r.command_resources,[](auto& child,auto& value){return military_command_resources_codec::visit(child,value);});if(!restored.ok)return restored;}else if(r.command_resources||domain_codec::had(r.field_order,"command_resources"))return c.reject("UnexpectedMilitaryCommandResources");if(c.military_codec_version<0||c.military_codec_version>16)return c.reject("MilitaryTacticEntryCodecVersionMismatch");if((c.military_codec_version==16||c.military_codec_version==17)){if(!(c.schema_features&military_command_resources_checkpoint_feature))return c.reject("MilitaryTacticEntryPrerequisiteRequired");auto restored=c.optional("learned_tactics",r.learned_tactics,[](auto& child,auto& value){return military_tactic_entry_codec::visit(child,value,16);});if(!restored.ok)return restored;}else if(r.learned_tactics||domain_codec::had(r.field_order,"learned_tactics"))return c.reject("UnexpectedMilitaryTacticEntry");
#undef VISIT
visit_order(c,r.field_order);return c.status();}
// 接敌事实可来自原野轨迹或既有围城节点；来源存在位必须保留，不能互相伪装。
void decode(const V& v,MilitaryContactFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","tick","battlefield_id","unit_ids","side_by_army","objective","army_order_ids","army_locations"},{"time_num","time_den","motion_sources","siege_id","node_id","battle_id"},p);if(boundary::get_string(v,"kind",p)!="military_contact")boundary::fail("MilitaryContactSourceRequired",p);
#define GET(n) decode(boundary::field(v,#n,p),r.n,p+"/"+#n)
GET(tick);GET(battlefield_id);GET(unit_ids);GET(side_by_army);GET(army_order_ids);GET(army_locations);
#undef GET
#define OPTIONAL(n) if(auto x=v.find(#n)){r.n.emplace();decode(*x,*r.n,p+"/"+#n);}
OPTIONAL(time_num);OPTIONAL(time_den);OPTIONAL(motion_sources);OPTIONAL(siege_id);OPTIONAL(node_id);OPTIONAL(battle_id);
#undef OPTIONAL
auto& o=boundary::field(v,"objective",p);boundary::fields(o,{"id","unit_ids"},{},p);r.objective_field_order=order(o);decode(boundary::field(o,"id",p),r.objective_id,p);decode(boundary::field(o,"unit_ids",p),r.objective_unit_ids,p);}
Status emit_fields(ProjectionWriter& w,const MilitaryContactFact& r){w.field("kind",StableId("military_contact"));
#define PUT(n) emit_value(w,#n,r.n)
PUT(tick);PUT(battlefield_id);PUT(unit_ids);PUT(side_by_army);w.object("objective",r.objective_field_order,[&](auto& child){child.field("id",r.objective_id);return emit_value(child,"unit_ids",r.objective_unit_ids);});PUT(army_order_ids);PUT(army_locations);
#undef PUT
#define OPTIONAL(n) if(r.n)emit_value(w,#n,*r.n)
OPTIONAL(time_num);OPTIONAL(time_den);OPTIONAL(motion_sources);OPTIONAL(siege_id);OPTIONAL(node_id);OPTIONAL(battle_id);
#undef OPTIONAL
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryContactFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryContactFact& r){
#define VISIT(n) c.object(#n,[&](auto& c){return visit(c,r.n);})
VISIT(id);VISIT(tick);VISIT(battlefield_id);VISIT(unit_ids);VISIT(side_by_army);VISIT(objective_id);VISIT(objective_unit_ids);VISIT(objective_field_order);VISIT(army_order_ids);VISIT(army_locations);
#undef VISIT
#define OPTIONAL(n) c.optional(#n,r.n,[](auto& c,auto& x){return visit(c,x);})
OPTIONAL(time_num);OPTIONAL(time_den);OPTIONAL(motion_sources);OPTIONAL(siege_id);OPTIONAL(node_id);OPTIONAL(battle_id);
#undef OPTIONAL
visit_order(c,r.field_order);return c.status();}
// 静态内容只冷解一次，既有原始纯规则负责有限参数范围与交叉引用验证。
Outcome<MilitaryBattleCatalog> decode_catalog(const V& root){try{MilitaryBattleCatalog c;
auto name=[&](const V& v){if(auto n=v.find("display_name"))return boundary::string(*n,"display_name");auto key=v.find("name_key");if(!key)key=v.find("title_key");if(key){auto texts=root.find("texts");auto text=texts?texts->find(boundary::string(*key,"name_key")):nullptr;if(text)return boundary::string(*text,"texts");}auto id=v.find("id");return id?boundary::string(*id,"id"):StableId{};};
for(auto& row:boundary::object(boundary::field(root,"troop_types","data"),"troop_types")){auto& v=row.second;auto p="troop_types/"+row.first;TroopDefinition t;
#define STR(n) t.n=boundary::get_string(v,#n,p)
STR(id);STR(category);t.has_name_key=v.find("name_key")!=nullptr;t.name=name(v);if(auto n=v.find("battle_portrait_kind"))t.portrait_kind=boundary::string(*n,p);
#undef STR
#define NUM(n) t.n=boundary::get_int(v,#n,p)
NUM(movement);NUM(minimum_range);NUM(maximum_range);NUM(tactic_capacity);NUM(attack);NUM(defense);NUM(accuracy);
#undef NUM
for(auto& cost:boundary::object(boundary::field(v,"movement_costs",p),p))t.movement_costs[cost.first]=boundary::integer(cost.second,p);for(auto& a:boundary::array(boundary::field(v,"tactics",p),p)){MilitaryTactic x;x.id=boundary::get_string(a,"id",p);x.kind=boundary::get_string(a,"kind",p);if(const auto* known=a.find("learned_tactic_id"))x.learned_tactic_id=boundary::string(*known,p);x.name=a.find("name_key")?name(a):(x.kind=="rally"?"鼓舞":x.kind=="charge"?"突击":x.kind=="volley"?"齐射":x.kind=="support"?"协防":x.id);
#define NUM(n) x.n=boundary::get_int(a,#n,p)
NUM(point_cost);NUM(morale_effect);NUM(move_bonus);NUM(status_rounds);NUM(power_bp);NUM(accuracy_bonus);
#undef NUM
t.tactics.push_back(x);}c.troop_types.push_back(std::move(t));}
for(auto& row:boundary::object(boundary::field(root,"military_objectives","data"),"military_objectives")){auto& v=row.second;auto p="military_objectives/"+row.first;BattleObjectiveDefinition o;o.id=boundary::get_string(v,"id",p);o.kind=boundary::get_string(v,"kind",p);if(auto n=v.find("name"))o.name=boundary::string(*n,p);o.side=boundary::get_int(v,"side",p);o.max_rounds=boundary::get_int(v,"max_rounds",p);o.allow_withdrawal=boundary::get_bool(v,"allow_withdrawal",p);if(auto x=v.find("point")){o.point.emplace();decode(*x,*o.point,p);}if(auto x=v.find("hold_rounds"))o.hold_rounds=boundary::integer(*x,p);if(auto x=v.find("required_units"))o.required_units=boundary::integer(*x,p);c.objectives.push_back(o);}
for(auto& row:boundary::object(boundary::field(root,"battlefields","data"),"battlefields")){auto& v=row.second;auto p="battlefields/"+row.first;BattlefieldDefinition b;b.id=boundary::get_string(v,"id",p);b.objective_id=boundary::get_string(v,"objective_id",p);b.name=name(v);if(auto n=v.find("visual_tag"))b.visual_tag=boundary::string(*n,p);for(auto& x:boundary::array(boundary::field(v,"cells",p),p)){HexCell cell;cell.field_order=order(x);cell.q=boundary::get_int(x,"q",p);cell.r=boundary::get_int(x,"r",p);cell.terrain=boundary::get_string(x,"terrain",p);
#define OPT(n,type) if(auto a=x.find(#n))cell.n=boundary::type(*a,p)
OPT(blocks_movement,boolean);OPT(blocks_sight,boolean);OPT(height,integer);OPT(cover,integer);
#undef OPT
b.cells.push_back(cell);}for(auto name:{"spawns","exits"})for(auto& x:boundary::array(boundary::field(v,name,p),p)){HexEntry e;e.field_order=order(x);e.side=boundary::get_int(x,"side",p);decode(boundary::field(x,"position",p),e.position,p);(StableId(name)=="spawns"?b.spawns:b.exits).push_back(e);}if(auto m=v.find("damage_model")){auto& d=b.damage_model;d.present=true;d.field_order=order(*m);d.empty=boundary::object(*m,p).empty();if(!d.empty){d.kind=boundary::get_string(*m,"kind",p);if(d.kind=="legacy_v1")boundary::fields(*m,{"kind"},{},p);else{boundary::fields(*m,{"kind","side_bp","rear_bp","forest_ranged_bp","hill_melee_bp","min_total_bp","max_total_bp"},{},p);
#define NUM(n) d.n=boundary::get_int(*m,#n,p)
NUM(side_bp);NUM(rear_bp);NUM(forest_ranged_bp);NUM(hill_melee_bp);NUM(min_total_bp);NUM(max_total_bp);
#undef NUM
}}}if(const auto* loss=v.find("commander_losses")){
 // 新策略只认具名能力与唯一字段；旧战场没有本字段时不改变原解码顺序或默认值。
 std::set<StableId> fields;for(const auto& f:boundary::object(v,p))if(!fields.insert(f.first).second)boundary::fail("DuplicateMilitaryCommanderField",p+"/"+f.first);fields.clear();for(const auto& f:boundary::object(*loss,p+"/commander_losses"))if(!fields.insert(f.first).second)boundary::fail("DuplicateMilitaryCommanderField",p+"/commander_losses/"+f.first);
 boundary::fields(*loss,{"kind","exposure_bp"},{},p+"/commander_losses");auto caps=boundary::strings(boundary::field(root,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"military_field_commanders_v1")==caps.end())boundary::fail("MilitaryCommanderCapabilityRequired",p);
 b.commander_losses=MilitaryCommanderLossRule{boundary::get_string(*loss,"kind",p),boundary::get_int(*loss,"exposure_bp",p)};auto checked=validate_military_commander_rule(*b.commander_losses);if(!checked.ok)return {false,{},checked.error};
 }if(const auto* rule=v.find("rout_retreat")){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};boundary::fields(*rule,{"profile","blocked_rounds_before_capture"},{},p+"/rout_retreat");auto caps=boundary::strings(boundary::field(root,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"military_rout_retreat_v1")==caps.end())boundary::fail("MilitaryRoutCapabilityRequired",p);b.rout_retreat=MilitaryRoutRetreatRule{boundary::get_string(*rule,"profile",p),boundary::get_int(*rule,"blocked_rounds_before_capture",p)};auto checked=validate_military_rout_rule(*b.rout_retreat,b);if(!checked.ok)return {false,{},checked.error};}c.battlefields.push_back(std::move(b));}return {true,std::move(c),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<MilitaryActorChange> decode_actor_change(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){
 static_assert(std::is_nothrow_move_constructible_v<MilitaryActorChange>);
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","exact_military_return/actor_change");
 try{budget.default_values<MilitaryActorChange>();MilitaryActorChange row;decode(raw,row,checkpoint_decode_boundary_text("exact_military_return/actor_change"));return {true,std::move(row),{}};}
 catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}

}
