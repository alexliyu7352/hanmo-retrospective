// 准备来源JSON递归拒重名；typed/JSON共用字段形状门，完整因果仍由原域历史核验。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "naval_gunnery_codec.hpp"
#include "resident/storage/common_projection.hpp"
#include <array>
#include "naval_gunnery_source.hpp"
#include "naval_json_guard.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::naval_gunnery_codec {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// The original JSON field writer is also the borrowed no-tree writer.
Status emit_value(ProjectionWriter& w,const StableId& key,const StableId& x){return w.field(key,x);}
Status emit_value(ProjectionWriter& w,const StableId& key,Integer x){return w.field(key,x);}
Status emit_value(ProjectionWriter& w,const StableId& key,bool x){return w.field(key,x);}
Status emit_record_fields(ProjectionWriter&,const NavalGunneryCaptain&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalGunneryCaptain& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
Status emit_record_fields(ProjectionWriter&,const NavalGunneryPreparation&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalGunneryPreparation& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
Status emit_record_fields(ProjectionWriter&,const NavalGunneryState&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalGunneryState& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T>V projected_value(const T& value){V out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}

V enc(const StableId& x){return V{x};}V enc(Integer x){return V{x};}
void dec(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void dec(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}
Status walk(CodecVisitor& v,StableId& x){return v.field("value",x);}Status walk(CodecVisitor& v,Integer& x){return v.field("value",x);}
V enc(const NavalGunneryCaptain&);void dec(const V&,NavalGunneryCaptain&,const StableId&);Status walk(CodecVisitor&,NavalGunneryCaptain&);
V enc(const NavalGunneryPreparation&);void dec(const V&,NavalGunneryPreparation&,const StableId&);Status walk(CodecVisitor&,NavalGunneryPreparation&);
V enc(const NavalGunneryState&);void dec(const V&,NavalGunneryState&,const StableId&);Status walk(CodecVisitor&,NavalGunneryState&);
template<class T>V enc(const std::vector<T>& rows){A a;for(const auto& r:rows)a.push_back(enc(r));return V{std::move(a)};}
template<class T>void dec(const V& v,std::vector<T>& rows,const StableId& p){for(const auto& x:boundary::array(v,p)){T r;dec(x,r,p);rows.push_back(std::move(r));}}
template<class T>Status walk(CodecVisitor& v,std::vector<T>& rows){return v.sequence("rows",rows,[&](auto& child,auto& row){return walk(child,row);});}
// NavalGunneryCaptain字段完整且有序，不能藏旧value或省略技艺来源。
Status emit_record_fields(ProjectionWriter& w,const NavalGunneryCaptain& r){emit_value(w,"unit_id",r.unit_id);emit_value(w,"vessel_id",r.vessel_id);emit_value(w,"fleet_id",r.fleet_id);emit_value(w,"army_id",r.army_id);emit_value(w,"captain_actor_id",r.captain_actor_id);emit_value(w,"authored_actor_id",r.authored_actor_id);emit_value(w,"experience",r.experience);emit_value(w,"level",r.level);emit_value(w,"causal_fact_ids",r.causal_fact_ids);w.order_fields(r.field_order);return w.status();}
V enc(const NavalGunneryCaptain& r){return projected_value(r);}
void dec(const V& v,NavalGunneryCaptain& r,const StableId& p){boundary::fields(v,{"unit_id","vessel_id","fleet_id","army_id","captain_actor_id","authored_actor_id","experience","level","causal_fact_ids"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"unit_id",p),r.unit_id,p+"/unit_id");dec(boundary::field(v,"vessel_id",p),r.vessel_id,p+"/vessel_id");dec(boundary::field(v,"fleet_id",p),r.fleet_id,p+"/fleet_id");dec(boundary::field(v,"army_id",p),r.army_id,p+"/army_id");dec(boundary::field(v,"captain_actor_id",p),r.captain_actor_id,p+"/captain_actor_id");dec(boundary::field(v,"authored_actor_id",p),r.authored_actor_id,p+"/authored_actor_id");dec(boundary::field(v,"experience",p),r.experience,p+"/experience");dec(boundary::field(v,"level",p),r.level,p+"/level");dec(boundary::field(v,"causal_fact_ids",p),r.causal_fact_ids,p+"/causal_fact_ids");}
Status walk(CodecVisitor& c,NavalGunneryCaptain& r){c.object("unit_id",[&](auto& v){return walk(v,r.unit_id);});c.object("vessel_id",[&](auto& v){return walk(v,r.vessel_id);});c.object("fleet_id",[&](auto& v){return walk(v,r.fleet_id);});c.object("army_id",[&](auto& v){return walk(v,r.army_id);});c.object("captain_actor_id",[&](auto& v){return walk(v,r.captain_actor_id);});c.object("authored_actor_id",[&](auto& v){return walk(v,r.authored_actor_id);});c.object("experience",[&](auto& v){return walk(v,r.experience);});c.object("level",[&](auto& v){return walk(v,r.level);});c.object("causal_fact_ids",[&](auto& v){return walk(v,r.causal_fact_ids);});common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
// NavalGunneryPreparation字段完整且有序，不能藏旧value或省略技艺来源。
Status emit_record_fields(ProjectionWriter& w,const NavalGunneryPreparation& r){emit_value(w,"id",r.id);emit_value(w,"battle_id",r.battle_id);emit_value(w,"contact_source_id",r.contact_source_id);emit_value(w,"tick",r.tick);emit_value(w,"captains",r.captains);w.order_fields(r.field_order);return w.status();}
V enc(const NavalGunneryPreparation& r){return projected_value(r);}
void dec(const V& v,NavalGunneryPreparation& r,const StableId& p){boundary::fields(v,{"id","battle_id","contact_source_id","tick","captains"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"id",p),r.id,p+"/id");dec(boundary::field(v,"battle_id",p),r.battle_id,p+"/battle_id");dec(boundary::field(v,"contact_source_id",p),r.contact_source_id,p+"/contact_source_id");dec(boundary::field(v,"tick",p),r.tick,p+"/tick");dec(boundary::field(v,"captains",p),r.captains,p+"/captains");}
Status walk(CodecVisitor& c,NavalGunneryPreparation& r){c.object("id",[&](auto& v){return walk(v,r.id);});c.object("battle_id",[&](auto& v){return walk(v,r.battle_id);});c.object("contact_source_id",[&](auto& v){return walk(v,r.contact_source_id);});c.object("tick",[&](auto& v){return walk(v,r.tick);});c.object("captains",[&](auto& v){return walk(v,r.captains);});common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
// NavalGunneryState字段完整且有序，不能藏旧value或省略技艺来源。
Status emit_record_fields(ProjectionWriter& w,const NavalGunneryState& r){emit_value(w,"preparations",r.preparations);w.order_fields(r.field_order);return w.status();}
V enc(const NavalGunneryState& r){return projected_value(r);}
void dec(const V& v,NavalGunneryState& r,const StableId& p){boundary::fields(v,{"preparations"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"preparations",p),r.preparations,p+"/preparations");}
Status walk(CodecVisitor& c,NavalGunneryState& r){c.object("preparations",[&](auto& v){return walk(v,r.preparations);});common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
}
Status emit_fields(ProjectionWriter& w,const NavalGunneryState& state){auto checked=validate_naval_gunnery_shape(state);if(!checked.ok)return checked;return emit_record_fields(w,state);}
BoundaryValue encode(const NavalGunneryState& state){V out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,state);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Outcome<NavalGunneryState> decode(const BoundaryValue& v){auto unique=validate_naval_json_unique_keys(v,"naval/gunnery");if(!unique.ok)return {false,{},unique.error};try{NavalGunneryState s;dec(v,s,"naval/gunnery");auto checked=validate_naval_gunnery_shape(s);if(!checked.ok)return {false,{},checked.error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& v,NavalGunneryState& s){if(v.naval_codec_version!=6)return v.reject("NavalGunneryProfileRequired");auto checked=walk(v,s);if(!checked.ok)return checked;checked=validate_naval_gunnery_shape(s);return checked.ok?v.status():v.reject(checked.error.original.code,checked.error.original.path);}
}
