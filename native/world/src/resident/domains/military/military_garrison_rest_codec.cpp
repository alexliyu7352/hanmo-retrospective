#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include <array>
// 精确字段与递归重键守卫；原实际疲劳历史仍由域冷门核验。
#include "military_garrison_rest_codec.hpp"
#include "military_garrison_rest.hpp"
#include "military_officer_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident::military_garrison_rest_codec {
namespace {

Status emit_body(ProjectionWriter&,const MilitaryGarrisonRestFact&,bool);
template<class T> Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class Alloc>Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,Alloc>&);
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,v);
 else return w.object(key,[&](auto& c){return emit_body(c,v,true);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T>BoundaryValue original_row_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",r);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
template<class T>BoundaryValue public_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& c){return emit_fields(c,r);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
using namespace domain_codec;
Status emit_body(ProjectionWriter& w,const MilitaryGarrisonRestFact& r,bool apply_order){
emit_value(w,"id",r.id);
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"organization_id",r.organization_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"supply_fact_id",r.supply_fact_id);
emit_value(w,"tick",r.tick);
emit_value(w,"training",r.training);
emit_value(w,"fatigue_before",r.fatigue_before);
emit_value(w,"fatigue_after",r.fatigue_after);
if(apply_order)w.order_fields(r.field_order);return w.status();}
V enc(const MilitaryGarrisonRestFact& r){return original_row_json(r);}
void dec(const V& v,MilitaryGarrisonRestFact& r,const StableId& p){boundary::fields(v,{"id","unit_id","garrison_id","organization_id","ownership_id","supply_fact_id","tick","training","fatigue_before","fatigue_after"},{},p);r.field_order=boundary::order(v);
domain_codec::dec(boundary::field(v,"id",p),r.id,p+"/id");
domain_codec::dec(boundary::field(v,"unit_id",p),r.unit_id,p+"/unit_id");
domain_codec::dec(boundary::field(v,"garrison_id",p),r.garrison_id,p+"/garrison_id");
domain_codec::dec(boundary::field(v,"organization_id",p),r.organization_id,p+"/organization_id");
domain_codec::dec(boundary::field(v,"ownership_id",p),r.ownership_id,p+"/ownership_id");
domain_codec::dec(boundary::field(v,"supply_fact_id",p),r.supply_fact_id,p+"/supply_fact_id");
domain_codec::dec(boundary::field(v,"tick",p),r.tick,p+"/tick");
domain_codec::dec(boundary::field(v,"training",p),r.training,p+"/training");
domain_codec::dec(boundary::field(v,"fatigue_before",p),r.fatigue_before,p+"/fatigue_before");
domain_codec::dec(boundary::field(v,"fatigue_after",p),r.fatigue_after,p+"/fatigue_after");
}
Status walk(CodecVisitor& c,MilitaryGarrisonRestFact& r){
c.object("id",[&](auto& child){return domain_codec::visit(child,r.id);});
c.object("unit_id",[&](auto& child){return domain_codec::visit(child,r.unit_id);});
c.object("garrison_id",[&](auto& child){return domain_codec::visit(child,r.garrison_id);});
c.object("organization_id",[&](auto& child){return domain_codec::visit(child,r.organization_id);});
c.object("ownership_id",[&](auto& child){return domain_codec::visit(child,r.ownership_id);});
c.object("supply_fact_id",[&](auto& child){return domain_codec::visit(child,r.supply_fact_id);});
c.object("tick",[&](auto& child){return domain_codec::visit(child,r.tick);});
c.object("training",[&](auto& child){return domain_codec::visit(child,r.training);});
c.object("fatigue_before",[&](auto& child){return domain_codec::visit(child,r.fatigue_before);});
c.object("fatigue_after",[&](auto& child){return domain_codec::visit(child,r.fatigue_after);});
common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
}
Status emit_fields(ProjectionWriter& w,const MilitaryGarrisonRestFact& f,bool apply_order){MilitaryGarrisonRestState s;s.facts={f};auto valid=validate_military_garrison_rest_shape(s);if(!valid.ok)return valid;return emit_body(w,f,apply_order);}
BoundaryValue encode_fact(const MilitaryGarrisonRestFact& f){return public_json(f);}
Outcome<MilitaryGarrisonRestFact> decode_fact(const BoundaryValue& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitaryGarrisonRestFact f;dec(v,f,"military/garrison_rest/fact");MilitaryGarrisonRestState s;s.facts={f};auto valid=validate_military_garrison_rest_shape(s);return valid.ok?Outcome<MilitaryGarrisonRestFact>{true,std::move(f),{}}:Outcome<MilitaryGarrisonRestFact>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_fields(ProjectionWriter& w,const MilitaryGarrisonRestState& s){auto checked=validate_military_garrison_rest_shape(s);if(!checked.ok)return checked;w.sequence("facts",s.facts,[](auto& c,const auto& row){return c.object("",[&](auto& fields){return emit_body(fields,row,true);});});w.order_fields(s.field_order);return w.status();}
BoundaryValue encode(const MilitaryGarrisonRestState& s){return public_json(s);}
Outcome<MilitaryGarrisonRestState> decode(const BoundaryValue& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitaryGarrisonRestState s;boundary::fields(v,{"facts"},{},"military/garrison_rest");s.field_order=boundary::order(v);for(const auto& item:boundary::array(boundary::field(v,"facts","military/garrison_rest"),"facts")){MilitaryGarrisonRestFact f;dec(item,f,"military/garrison_rest/fact");s.facts.push_back(std::move(f));}auto valid=validate_military_garrison_rest_shape(s);return valid.ok?Outcome<MilitaryGarrisonRestState>{true,std::move(s),{}}:Outcome<MilitaryGarrisonRestState>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,MilitaryGarrisonRestState& s){if(!(c.schema_features&military_garrison_rest_checkpoint_feature)||!(c.schema_features&military_command_shock_checkpoint_feature))return c.reject("MilitaryGarrisonRestPrerequisiteRequired");c.sequence("facts",s.facts,[](auto& child,auto& f){return walk(child,f);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto valid=validate_military_garrison_rest_shape(s);return valid.ok?c.status():c.reject(valid.error.original.code,valid.error.original.path);}
}
