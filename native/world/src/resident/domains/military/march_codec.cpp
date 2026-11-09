#include "resident/domains/military/military_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/storage/common_projection.hpp"
#include <array>
// 原HM-TERRAIN-EXECUTION/1逐字段编码；空uncut/contact_time保空字典而不补伪执行。
#include "march_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::training_codec {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
Status emit_point(ProjectionWriter& w,const StableId& name,const PointI& p){const std::array<Integer,2> xy{p.x,p.y};return w.sequence(name,xy,[](auto& c,Integer n){return c.field("",n);});}
PointI read_point(const V& v,const StableId& p){auto& a=boundary::array(v,p);if(a.size()!=2)boundary::fail("InvalidMilitaryMarchExecution",p);return {boundary::integer(a[0],p),boundary::integer(a[1],p)};}
Status emit_execution(ProjectionWriter& w,const TerrainExecution& e,const FieldOrder& fields){w.field("format",StableId("HM-TERRAIN-EXECUTION/1"));w.field("actor_id",e.actor_id);w.field("action_id",e.action_id);w.field("action_start_tick",e.action_start_tick);w.field("source_tick",e.source_tick);w.field("plan_id",e.plan_id);w.field("map_id",e.map_id);emit_point(w,"from_point",e.from_point);emit_point(w,"to_point",e.to_point);w.field("route_index_before",e.route_index_before);w.field("route_index_after",e.route_index_after);w.field("travel_credit_before",e.travel_credit_before);w.field("travel_credit_after",e.travel_credit_after);emit_common_location(w,"result_location",e.result_location);w.order_fields(fields);return w.status();}
void read_execution(const V& v,TerrainExecution& e,FieldOrder& fields,const StableId& p){boundary::fields(v,{"format","actor_id","action_id","action_start_tick","source_tick","plan_id","map_id","from_point","to_point","route_index_before","route_index_after","travel_credit_before","travel_credit_after","result_location"},{},p);if(boundary::get_string(v,"format",p)!="HM-TERRAIN-EXECUTION/1")boundary::fail("InvalidMilitaryMarchExecution",p);fields=domain_codec::order(v);
#define STR(n) e.n=boundary::get_string(v,#n,p)
STR(actor_id);STR(action_id);STR(plan_id);STR(map_id);
#undef STR
#define NUM(n) e.n=boundary::get_int(v,#n,p)
NUM(action_start_tick);NUM(source_tick);NUM(route_index_before);NUM(route_index_after);NUM(travel_credit_before);NUM(travel_credit_after);
#undef NUM
e.from_point=read_point(boundary::field(v,"from_point",p),p);e.to_point=read_point(boundary::field(v,"to_point",p),p);e.result_location=boundary::decode_location(boundary::field(v,"result_location",p),p);}
Status visit_execution(CodecVisitor& c,TerrainExecution& e){
#define FIELD(n) c.field(#n,e.n)
FIELD(actor_id);FIELD(action_id);FIELD(plan_id);FIELD(map_id);FIELD(action_start_tick);FIELD(source_tick);FIELD(route_index_before);FIELD(route_index_after);FIELD(travel_credit_before);FIELD(travel_credit_after);
#undef FIELD
c.field("from_x",e.from_point.x);c.field("from_y",e.from_point.y);c.field("to_x",e.to_point.x);c.field("to_y",e.to_point.y);return c.object("location",[&](auto& c){return common_checkpoint::location(c,e.result_location);});}
}
void decode(const BoundaryValue& v,MilitaryMarchFact& f,const StableId& p){boundary::fields(v,{"kind","rule","tick","army_id","formation_source","commander_actor_id","supply_fact_id","movement","uncut_execution","contact_source_id","contact_time","units"},{},p);if(boundary::get_string(v,"kind",p)!="military_march")boundary::fail("InvalidMilitaryMarchFact",p);f.field_order=domain_codec::order(v);f.rule=boundary::get_string(v,"rule",p);f.tick=boundary::get_int(v,"tick",p);
#define STR(n) f.n=boundary::get_string(v,#n,p)
STR(army_id);STR(formation_source);STR(commander_actor_id);STR(supply_fact_id);STR(contact_source_id);
#undef STR
read_execution(boundary::field(v,"movement",p),f.movement,f.movement_field_order,p+"/movement");auto& uncut=boundary::field(v,"uncut_execution",p);if(!boundary::object(uncut,p).empty()){f.uncut_execution.emplace();read_execution(uncut,*f.uncut_execution,f.uncut_field_order,p+"/uncut_execution");}auto& time=boundary::field(v,"contact_time",p);if(!boundary::object(time,p).empty()){boundary::fields(time,{"num","den"},{},p);f.contact_time_field_order=domain_codec::order(time);f.contact_time=std::make_pair(boundary::get_int(time,"num",p),boundary::get_int(time,"den",p));}for(auto& row:boundary::object(boundary::field(v,"units",p),p)){MilitaryMarchUnit u;decode(row.second,u,p+"/units/"+row.first);u.unit_id=row.first;f.units.push_back(u);}}
Status emit_fields(ProjectionWriter& w,const MilitaryMarchFact& f){w.field("kind",StableId("military_march"));w.field("rule",f.rule);w.field("tick",f.tick);w.field("army_id",f.army_id);w.field("formation_source",f.formation_source);w.field("commander_actor_id",f.commander_actor_id);w.field("supply_fact_id",f.supply_fact_id);w.object("movement",[&](auto& c){return emit_execution(c,f.movement,f.movement_field_order);});w.object("uncut_execution",[&](auto& c){return f.uncut_execution?emit_execution(c,*f.uncut_execution,f.uncut_field_order):c.status();});w.field("contact_source_id",f.contact_source_id);w.object("contact_time",[&](auto& c){if(f.contact_time){c.field("num",f.contact_time->first);c.field("den",f.contact_time->second);c.order_fields(f.contact_time_field_order);}return c.status();});w.object("units",[&](auto& c){for(const auto& unit:f.units)c.object(unit.unit_id,[&](auto& row){return emit_fields(row,unit);});return c.status();});w.order_fields(f.field_order);return w.status();}
BoundaryValue encode(const MilitaryMarchFact& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto check=sink.object("",[&](auto& c){return emit_fields(c,value);});if(check.ok)check=sink.finish();if(!check.ok)boundary::fail(check.error.original.code,check.error.original.path);return out;}
Status visit(CodecVisitor& c,MilitaryMarchFact& f){
#define FIELD(n) c.field(#n,f.n)
FIELD(id);FIELD(rule);FIELD(tick);FIELD(army_id);FIELD(formation_source);FIELD(commander_actor_id);FIELD(supply_fact_id);FIELD(contact_source_id);
#undef FIELD
c.object("movement",[&](auto& c){return visit_execution(c,f.movement);});c.object("movement_order",[&](auto& c){return domain_codec::visit_order(c,f.movement_field_order);});c.optional("uncut_execution",f.uncut_execution,[](auto& c,auto& e){return visit_execution(c,e);});c.object("uncut_order",[&](auto& c){return domain_codec::visit_order(c,f.uncut_field_order);});c.optional("contact_time",f.contact_time,[](auto& c,auto& p){c.field("num",p.first);return c.field("den",p.second);});c.object("time_order",[&](auto& c){return domain_codec::visit_order(c,f.contact_time_field_order);});c.sequence("units",f.units,[](auto& c,auto& u){return visit(c,u);});return domain_codec::visit_order(c,f.field_order);}
}
namespace hanmo::resident::march_codec {
bool decode_fact(const StableId& kind,const BoundaryValue& value,const StableId& id,MilitaryState& s){auto rule=value.find("rule");if(kind!="military_march"&&(!rule||!rule->as<StableId>()||*rule->as<StableId>()!="terrain_march_v1"))return false;MilitaryMarchFact f;training_codec::decode(value,f,"facts/"+id);f.id=id;s.march_facts.push_back(f);return true;}
Status emit_fact_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& f:s.march_facts)w.object(f.id,[&](auto& c){return training_codec::emit_fields(c,f);});return w.status();}
void append_state(BoundaryValue::Dict& facts,const MilitaryState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto check=sink.object("",[&](auto& c){return emit_fact_fields(c,s);});if(check.ok)check=sink.finish();if(!check.ok)boundary::fail(check.error.original.code,check.error.original.path);for(auto& row:*out.as<BoundaryValue::Dict>())facts.push_back(std::move(row));}
Status visit_state(CodecVisitor& c,MilitaryState& s){return c.sequence("march_facts",s.march_facts,[](auto& c,auto& f){return training_codec::visit(c,f);});}
}
