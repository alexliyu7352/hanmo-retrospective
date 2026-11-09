#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include <array>
// 新JSON入口递归拒重键；typed visitor直接遍历原Battle结构，不把边界字典当热源。
#include "military_rout_codec.hpp"
#include "military_rout_profile.hpp"
#include "military_officer_codec.hpp"
#include "battle_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::military_rout_codec {
// 供下方 JSON 模板查找既有校验入口；独立事实默认保留原字段顺序。
Status emit_fields(ProjectionWriter &, const MilitaryRoutEntryFact &,
                   bool apply_order = true);
Status emit_fields(ProjectionWriter &, const MilitaryRoutRoundFact &,
                   bool apply_order = true);
Status emit_fields(ProjectionWriter &, const MilitaryRoutState &);

namespace {

Status emit_body(ProjectionWriter&,const MilitaryRoutProgress&,bool);
Status emit_body(ProjectionWriter&,const MilitaryRoutEntryFact&,bool);
Status emit_body(ProjectionWriter&,const MilitaryRoutRoundFact&,bool);
template<class T> Status emit_value(ProjectionWriter&,const StableId&,const T&);
template<class T,class Alloc>Status emit_value(ProjectionWriter&,const StableId&,const std::vector<T,Alloc>&);
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,v);
 else if constexpr(std::is_same_v<T,MilitaryBattleRecord>)return w.object(key,[&](auto& c){return battle_codec::emit_fields(c,v);});
 else return w.object(key,[&](auto& c){return emit_body(c,v,true);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T>BoundaryValue original_row_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",r);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
template<class T>BoundaryValue public_json(const T& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& c){return emit_fields(c,r);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
Status emit_body(ProjectionWriter& w,const MilitaryRoutProgress& r,bool apply_order){
emit_value(w,"unit_id",r.unit_id);
emit_value(w,"started_round",r.started_round);
emit_value(w,"blocked_rounds",r.blocked_rounds);
if(apply_order)w.order_fields(r.field_order);return w.status();}
V enc(const MilitaryRoutProgress& r){return original_row_json(r);}
void dec(const V& v,MilitaryRoutProgress& r,const StableId& p){boundary::fields(v,{"unit_id","started_round","blocked_rounds"},{},p);r.field_order=boundary::order(v);
domain_codec::dec(boundary::field(v,"unit_id",p),r.unit_id,p+"/unit_id");
domain_codec::dec(boundary::field(v,"started_round",p),r.started_round,p+"/started_round");
domain_codec::dec(boundary::field(v,"blocked_rounds",p),r.blocked_rounds,p+"/blocked_rounds");
}
Status walk(CodecVisitor& c,MilitaryRoutProgress& r){
c.field("unit_id",r.unit_id);
c.field("started_round",r.started_round);
c.field("blocked_rounds",r.blocked_rounds);
common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
Status emit_body(ProjectionWriter& w,const MilitaryRoutEntryFact& r,bool apply_order){
emit_value(w,"id",r.id);
emit_value(w,"tick",r.tick);
emit_value(w,"initial",r.initial);
if(apply_order)w.order_fields(r.field_order);return w.status();}
V enc(const MilitaryRoutEntryFact& r){return original_row_json(r);}
void dec(const V& v,MilitaryRoutEntryFact& r,const StableId& p){boundary::fields(v,{"id","tick","initial"},{},p);r.field_order=boundary::order(v);
domain_codec::dec(boundary::field(v,"id",p),r.id,p+"/id");
domain_codec::dec(boundary::field(v,"tick",p),r.tick,p+"/tick");
battle_codec::decode(boundary::field(v,"initial",p),r.initial,p+"/initial");
}
Status walk(CodecVisitor& c,MilitaryRoutEntryFact& r){
c.field("id",r.id);
c.field("tick",r.tick);
c.object("initial",[&](auto& child){return battle_codec::visit(child,r.initial);});
common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
Status emit_body(ProjectionWriter& w,const MilitaryRoutRoundFact& r,bool apply_order){
emit_value(w,"id",r.id);
emit_value(w,"battle_id",r.battle_id);
emit_value(w,"battle_round_fact_id",r.battle_round_fact_id);
emit_value(w,"casualty_fact_id",r.casualty_fact_id);
emit_value(w,"tick",r.tick);
emit_value(w,"round",r.round);
emit_value(w,"resolution_kind",r.resolution_kind);
emit_value(w,"termination_reason",r.termination_reason);
emit_value(w,"before",r.before);
emit_value(w,"after",r.after);
w.object("damage_sources",[&](auto& c){for(const auto& row:r.damage_sources)c.field(row.first,row.second);return c.status();});
emit_value(w,"progress_before",r.progress_before);
emit_value(w,"progress_after",r.progress_after);
if(apply_order)w.order_fields(r.field_order);return w.status();}
V enc(const MilitaryRoutRoundFact& r){return original_row_json(r);}
void dec(const V& v,MilitaryRoutRoundFact& r,const StableId& p){boundary::fields(v,{"id","battle_id","battle_round_fact_id","casualty_fact_id","tick","round","resolution_kind","termination_reason","before","after","damage_sources","progress_before","progress_after"},{},p);r.field_order=boundary::order(v);
domain_codec::dec(boundary::field(v,"id",p),r.id,p+"/id");
domain_codec::dec(boundary::field(v,"battle_id",p),r.battle_id,p+"/battle_id");
domain_codec::dec(boundary::field(v,"battle_round_fact_id",p),r.battle_round_fact_id,p+"/battle_round_fact_id");
domain_codec::dec(boundary::field(v,"casualty_fact_id",p),r.casualty_fact_id,p+"/casualty_fact_id");
domain_codec::dec(boundary::field(v,"tick",p),r.tick,p+"/tick");
domain_codec::dec(boundary::field(v,"round",p),r.round,p+"/round");
domain_codec::dec(boundary::field(v,"resolution_kind",p),r.resolution_kind,p+"/resolution_kind");
domain_codec::dec(boundary::field(v,"termination_reason",p),r.termination_reason,p+"/termination_reason");
battle_codec::decode(boundary::field(v,"before",p),r.before,p+"/before");
battle_codec::decode(boundary::field(v,"after",p),r.after,p+"/after");
for(const auto& row:boundary::object(boundary::field(v,"damage_sources",p),p+"/damage_sources"))r.damage_sources.emplace_back(row.first,boundary::integer(row.second,p+"/damage_sources"));
for(const auto& row:boundary::array(boundary::field(v,"progress_before",p),p+"/progress_before")){MilitaryRoutProgress q;dec(row,q,p+"/progress_before");r.progress_before.push_back(std::move(q));}
for(const auto& row:boundary::array(boundary::field(v,"progress_after",p),p+"/progress_after")){MilitaryRoutProgress q;dec(row,q,p+"/progress_after");r.progress_after.push_back(std::move(q));}
}
Status walk(CodecVisitor& c,MilitaryRoutRoundFact& r){
c.field("id",r.id);
c.field("battle_id",r.battle_id);
c.field("battle_round_fact_id",r.battle_round_fact_id);
c.field("casualty_fact_id",r.casualty_fact_id);
c.field("tick",r.tick);
c.field("round",r.round);
c.field("resolution_kind",r.resolution_kind);
c.field("termination_reason",r.termination_reason);
c.object("before",[&](auto& child){return battle_codec::visit(child,r.before);});
c.object("after",[&](auto& child){return battle_codec::visit(child,r.after);});
c.sequence("damage_sources",r.damage_sources,[](auto& child,auto& row){child.field("unit_id",row.first);return child.field("factor",row.second);});
c.sequence("progress_before",r.progress_before,[](auto& child,auto& row){return walk(child,row);});
c.sequence("progress_after",r.progress_after,[](auto& child,auto& row){return walk(child,row);});
common_checkpoint::strings(c,"field_order",r.field_order);return c.status();}
}
Status emit_fields(ProjectionWriter& w,const MilitaryRoutEntryFact& f,bool apply_order){MilitaryRoutState s;s.entries={f};auto check=validate_military_rout_shape(s);if(!check.ok)return check;return emit_body(w,f,apply_order);}
BoundaryValue encode_entry(const MilitaryRoutEntryFact& f){return public_json(f);}
Outcome<MilitaryRoutEntryFact> decode_entry(const V& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitaryRoutEntryFact f;dec(v,f,"military/rout/entry");MilitaryRoutState s;s.entries={f};auto check=validate_military_rout_shape(s);return check.ok?Outcome<MilitaryRoutEntryFact>{true,std::move(f),{}}:Outcome<MilitaryRoutEntryFact>{false,{},check.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_fields(ProjectionWriter& w,const MilitaryRoutRoundFact& f,bool apply_order){MilitaryRoutState s;s.rounds={f};auto check=validate_military_rout_shape(s);if(!check.ok)return check;return emit_body(w,f,apply_order);}
BoundaryValue encode_round(const MilitaryRoutRoundFact& f){return public_json(f);}
Outcome<MilitaryRoutRoundFact> decode_round(const V& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitaryRoutRoundFact f;dec(v,f,"military/rout/round");MilitaryRoutState s;s.rounds={f};auto check=validate_military_rout_shape(s);return check.ok?Outcome<MilitaryRoutRoundFact>{true,std::move(f),{}}:Outcome<MilitaryRoutRoundFact>{false,{},check.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_fields(ProjectionWriter& w,const MilitaryRoutState& s){auto checked=validate_military_rout_shape(s);if(!checked.ok)return checked;w.sequence("entries",s.entries,[](auto& c,const auto& row){return c.object("",[&](auto& fields){return emit_body(fields,row,true);});});w.sequence("rounds",s.rounds,[](auto& c,const auto& row){return c.object("",[&](auto& fields){return emit_body(fields,row,true);});});w.order_fields(s.field_order);return w.status();}
BoundaryValue encode(const MilitaryRoutState& s){return public_json(s);}
Outcome<MilitaryRoutState> decode(const V& v){auto unique=military_officer_codec::validate_json_unique_keys(v);if(!unique.ok)return {false,{},unique.error};try{MilitaryRoutState s;boundary::fields(v,{"entries","rounds"},{},"military/rout");s.field_order=boundary::order(v);for(const auto& row:boundary::array(boundary::field(v,"entries","rout"),"rout/entries")){MilitaryRoutEntryFact f;dec(row,f,"rout/entries");s.entries.push_back(std::move(f));}for(const auto& row:boundary::array(boundary::field(v,"rounds","rout"),"rout/rounds")){MilitaryRoutRoundFact f;dec(row,f,"rout/rounds");s.rounds.push_back(std::move(f));}auto check=validate_military_rout_shape(s);return check.ok?Outcome<MilitaryRoutState>{true,std::move(s),{}}:Outcome<MilitaryRoutState>{false,{},check.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,MilitaryRoutState& s){if(!(c.schema_features&military_rout_checkpoint_feature)||!(c.schema_features&military_supply_attrition_checkpoint_feature)||!(c.schema_features&military_recovery_checkpoint_feature))return c.reject("MilitaryRoutPrerequisiteRequired");c.sequence("entries",s.entries,[](auto& child,auto& row){return walk(child,row);});c.sequence("rounds",s.rounds,[](auto& child,auto& row){return walk(child,row);});common_checkpoint::strings(c,"field_order",s.field_order);if(!c.status().ok)return c.status();auto check=validate_military_rout_shape(s);return check.ok?c.status():c.reject(check.error.original.code,check.error.original.path);}
}
