#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include <array>
// 原字段顺序与实际token列表逐项保存；重键、缺项、越界和旧feature夹带全部拒绝。
#include "military_supply_attrition_codec.hpp"
#include "military_supply_attrition_rules.hpp"
#include "military_officer_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident::military_supply_attrition_codec {
namespace {

Status emit_body(ProjectionWriter&,const MilitarySupplyAttritionFact&,bool);
template<class T> Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class Alloc>Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,Alloc>&);
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,v);
 else return w.object(key,[&](auto& c){return emit_body(c,v,true);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T>BoundaryValue original_row_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",r);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
template<class T>BoundaryValue public_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& c){return emit_fields(c,r);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
using namespace domain_codec;
Status emit_body(ProjectionWriter& w,const MilitarySupplyAttritionFact& r,bool apply_order){
emit_value(w,"id",r.id);
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"supply_fact_id",r.supply_fact_id);
emit_value(w,"previous_supply_fact_id",r.previous_supply_fact_id);
emit_value(w,"tick",r.tick);
emit_value(w,"consecutive_shortage_half_days",r.consecutive_shortage_half_days);
emit_value(w,"fit_before",r.fit_before);
emit_value(w,"eligible_fit_before",r.eligible_fit_before);
emit_value(w,"deserted_ids",r.deserted_ids);
if(apply_order)w.order_fields(r.field_order);return w.status();}
V enc(const MilitarySupplyAttritionFact& r){return original_row_json(r);}
void dec(const V& v,MilitarySupplyAttritionFact& r,const StableId& p){boundary::fields(v,{"id","unit_id","supply_fact_id","previous_supply_fact_id","tick","consecutive_shortage_half_days","fit_before","eligible_fit_before","deserted_ids"},{},p);r.field_order=boundary::order(v);
domain_codec::dec(boundary::field(v,"id",p),r.id,p+"/id");
domain_codec::dec(boundary::field(v,"unit_id",p),r.unit_id,p+"/unit_id");
domain_codec::dec(boundary::field(v,"supply_fact_id",p),r.supply_fact_id,p+"/supply_fact_id");
domain_codec::dec(boundary::field(v,"previous_supply_fact_id",p),r.previous_supply_fact_id,p+"/previous_supply_fact_id");
domain_codec::dec(boundary::field(v,"tick",p),r.tick,p+"/tick");
domain_codec::dec(boundary::field(v,"consecutive_shortage_half_days",p),r.consecutive_shortage_half_days,p+"/consecutive_shortage_half_days");
domain_codec::dec_list(boundary::field(v,"fit_before",p),r.fit_before,p+"/fit_before");
domain_codec::dec_list(boundary::field(v,"eligible_fit_before",p),r.eligible_fit_before,p+"/eligible_fit_before");
domain_codec::dec_list(boundary::field(v,"deserted_ids",p),r.deserted_ids,p+"/deserted_ids");
}
Status walk(CodecVisitor& c,MilitarySupplyAttritionFact& r){
c.object("id",[&](auto& child){return domain_codec::visit(child,r.id);});
c.object("unit_id",[&](auto& child){return domain_codec::visit(child,r.unit_id);});
c.object("supply_fact_id",[&](auto& child){return domain_codec::visit(child,r.supply_fact_id);});
c.object("previous_supply_fact_id",[&](auto& child){return domain_codec::visit(child,r.previous_supply_fact_id);});
c.object("tick",[&](auto& child){return domain_codec::visit(child,r.tick);});
c.object("consecutive_shortage_half_days",[&](auto& child){return domain_codec::visit(child,r.consecutive_shortage_half_days);});
common_checkpoint::strings(c,"fit_before",r.fit_before);
common_checkpoint::strings(c,"eligible_fit_before",r.eligible_fit_before);
common_checkpoint::strings(c,"deserted_ids",r.deserted_ids);
common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
}
Status emit_fields(ProjectionWriter& w,const MilitarySupplyAttritionFact& f,bool apply_order){MilitarySupplyAttritionState s;s.facts={f};auto checked=validate_military_supply_attrition_shape(s);if(!checked.ok)return checked;return emit_body(w,f,apply_order);}
BoundaryValue encode_fact(const MilitarySupplyAttritionFact& f){return public_json(f);}
Outcome<MilitarySupplyAttritionFact> decode_fact(const BoundaryValue& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitarySupplyAttritionFact f;dec(v,f,"military/supply_attrition/fact");MilitarySupplyAttritionState s;s.facts={f};auto checked=validate_military_supply_attrition_shape(s);return checked.ok?Outcome<MilitarySupplyAttritionFact>{true,std::move(f),{}}:Outcome<MilitarySupplyAttritionFact>{false,{},checked.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_fields(ProjectionWriter& w,const MilitarySupplyAttritionState& s){auto checked=validate_military_supply_attrition_shape(s);if(!checked.ok)return checked;w.sequence("facts",s.facts,[](auto& c,const auto& row){return c.object("",[&](auto& fields){return emit_body(fields,row,true);});});w.order_fields(s.field_order);return w.status();}
BoundaryValue encode(const MilitarySupplyAttritionState& s){return public_json(s);}
Outcome<MilitarySupplyAttritionState> decode(const BoundaryValue& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitarySupplyAttritionState s;boundary::fields(v,{"facts"},{},"military/supply_attrition");s.field_order=boundary::order(v);for(const auto& item:boundary::array(boundary::field(v,"facts","military/supply_attrition"),"facts")){MilitarySupplyAttritionFact f;dec(item,f,"military/supply_attrition/fact");s.facts.push_back(std::move(f));}auto checked=validate_military_supply_attrition_shape(s);return checked.ok?Outcome<MilitarySupplyAttritionState>{true,std::move(s),{}}:Outcome<MilitarySupplyAttritionState>{false,{},checked.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,MilitarySupplyAttritionState& s){if(!(c.schema_features&military_supply_attrition_checkpoint_feature)||!(c.schema_features&military_recovery_checkpoint_feature))return c.reject("MilitarySupplyAttritionPrerequisiteRequired");c.sequence("facts",s.facts,[](auto& child,auto& f){return walk(child,f);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto checked=validate_military_supply_attrition_shape(s);return checked.ok?c.status():c.reject(checked.error.original.code,checked.error.original.path);}
}
