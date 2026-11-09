// 独立收费段严格枚举字段，不把JSON自述Owner当作经营权；历史另由原军令/占领链二验。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "naval_repair_charges_codec.hpp"
#include "resident/storage/common_projection.hpp"
#include <array>
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_codec.hpp"
#include "naval_json_guard.hpp"
#include <charconv>
#include <set>
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident::naval_repair_charges_codec {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
Status shape(const FieldOrder& order,std::initializer_list<StableId> fields){if(order.empty())return common_success();std::set<StableId> wanted(fields);if(order.size()!=wanted.size())return common_failure("InvalidNavalRepairChargeFieldOrder");for(const auto& key:order)if(wanted.erase(key)!=1)return common_failure("InvalidNavalRepairChargeFieldOrder",key);return common_success();}
// The original JSON field writer is also the borrowed no-tree writer.
Status emit_value(ProjectionWriter& w,const StableId& key,const StableId& x){return w.field(key,x);}
Status emit_value(ProjectionWriter& w,const StableId& key,Integer x){return w.field(key,x);}
Status emit_value(ProjectionWriter& w,const StableId& key,bool x){return w.field(key,x);}
Status emit_value(ProjectionWriter& w,const StableId& key,const Owner& x){return emit_common_owner(w,key,x);}
Status emit_record_fields(ProjectionWriter&,const NavalRepairChargeTerms&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalRepairChargeTerms& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
Status emit_record_fields(ProjectionWriter&,const NavalRepairChargeFact&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalRepairChargeFact& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
Status emit_record_fields(ProjectionWriter&,const NavalRepairChargeState&);
Status emit_value(ProjectionWriter& w,const StableId& key,const NavalRepairChargeState& x){return w.object(key,[&](auto& c){return emit_record_fields(c,x);});}
template<class T>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T>& rows){return w.sequence(key,rows,[](auto& c,const auto& row){return emit_value(c,"",row);});}
template<class T>V projected_value(const T& value){V out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}

V enc(const StableId& x){return V{x};}V enc(Integer x){return V{x};}V enc(const Owner& x){return boundary::encode_owner(x);}
void dec(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void dec(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}void dec(const V& v,Owner& x,const StableId& p){x=boundary::decode_owner(v,p);}
Status walk(CodecVisitor& c,StableId& x){return c.field("value",x);}Status walk(CodecVisitor& c,Integer& x){return c.field("value",x);}Status walk(CodecVisitor& c,Owner& x){return common_checkpoint::owner(c,x);}
V enc(const NavalRepairChargeTerms&);void dec(const V&,NavalRepairChargeTerms&,const StableId&);Status walk(CodecVisitor&,NavalRepairChargeTerms&);
V enc(const NavalRepairChargeFact&);void dec(const V&,NavalRepairChargeFact&,const StableId&);Status walk(CodecVisitor&,NavalRepairChargeFact&);
V enc(const NavalRepairChargeState&);void dec(const V&,NavalRepairChargeState&,const StableId&);Status walk(CodecVisitor&,NavalRepairChargeState&);
template<class T>V enc(const std::vector<T>& x){A a;for(const auto& item:x)a.push_back(enc(item));return V{std::move(a)};}
template<class T>void dec(const V& v,std::vector<T>& rows,const StableId& p){domain_codec::dec_list(v,rows,p,[&](const V& input,T& output,const StableId&){dec(input,output,p);});}
template<class T>Status walk(CodecVisitor& c,std::vector<T>& x){return c.sequence("rows",x,[&](auto& child,auto& item){return walk(child,item);});}
// NavalRepairChargeTerms按具名格式保存原序，缺失/重名/附加字段均拒绝。
Status emit_record_fields(ProjectionWriter& w,const NavalRepairChargeTerms& r){auto check=shape(r.field_order,{"service_id","service_version","rule_id","berth_place_id","garrison_id","occupation_source_id","army_id","payer_wallet_id","payee_wallet_id","payer_owner","payee_owner","amount"});if(!check.ok)return check;emit_value(w,"service_id",r.service_id);emit_value(w,"service_version",r.service_version);emit_value(w,"rule_id",r.rule_id);emit_value(w,"berth_place_id",r.berth_place_id);emit_value(w,"garrison_id",r.garrison_id);emit_value(w,"occupation_source_id",r.occupation_source_id);emit_value(w,"army_id",r.army_id);emit_value(w,"payer_wallet_id",r.payer_wallet_id);emit_value(w,"payee_wallet_id",r.payee_wallet_id);emit_value(w,"payer_owner",r.payer_owner);emit_value(w,"payee_owner",r.payee_owner);emit_value(w,"amount",r.amount);w.order_fields(r.field_order);return w.status();}
V enc(const NavalRepairChargeTerms& r){return projected_value(r);}
void dec(const V& v,NavalRepairChargeTerms& r,const StableId& p){boundary::fields(v,{"service_id","service_version","rule_id","berth_place_id","garrison_id","occupation_source_id","army_id","payer_wallet_id","payee_wallet_id","payer_owner","payee_owner","amount"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"service_id",p),r.service_id,checkpoint_decode_boundary_join({p,"/service_id"}));dec(boundary::field(v,"service_version",p),r.service_version,checkpoint_decode_boundary_join({p,"/service_version"}));dec(boundary::field(v,"rule_id",p),r.rule_id,checkpoint_decode_boundary_join({p,"/rule_id"}));dec(boundary::field(v,"berth_place_id",p),r.berth_place_id,checkpoint_decode_boundary_join({p,"/berth_place_id"}));dec(boundary::field(v,"garrison_id",p),r.garrison_id,checkpoint_decode_boundary_join({p,"/garrison_id"}));dec(boundary::field(v,"occupation_source_id",p),r.occupation_source_id,checkpoint_decode_boundary_join({p,"/occupation_source_id"}));dec(boundary::field(v,"army_id",p),r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));dec(boundary::field(v,"payer_wallet_id",p),r.payer_wallet_id,checkpoint_decode_boundary_join({p,"/payer_wallet_id"}));dec(boundary::field(v,"payee_wallet_id",p),r.payee_wallet_id,checkpoint_decode_boundary_join({p,"/payee_wallet_id"}));dec(boundary::field(v,"payer_owner",p),r.payer_owner,checkpoint_decode_boundary_join({p,"/payer_owner"}));dec(boundary::field(v,"payee_owner",p),r.payee_owner,checkpoint_decode_boundary_join({p,"/payee_owner"}));dec(boundary::field(v,"amount",p),r.amount,checkpoint_decode_boundary_join({p,"/amount"}));}
Status walk(CodecVisitor& c,NavalRepairChargeTerms& r){c.object("service_id",[&](auto& v){return walk(v,r.service_id);});c.object("service_version",[&](auto& v){return walk(v,r.service_version);});c.object("rule_id",[&](auto& v){return walk(v,r.rule_id);});c.object("berth_place_id",[&](auto& v){return walk(v,r.berth_place_id);});c.object("garrison_id",[&](auto& v){return walk(v,r.garrison_id);});c.object("occupation_source_id",[&](auto& v){return walk(v,r.occupation_source_id);});c.object("army_id",[&](auto& v){return walk(v,r.army_id);});c.object("payer_wallet_id",[&](auto& v){return walk(v,r.payer_wallet_id);});c.object("payee_wallet_id",[&](auto& v){return walk(v,r.payee_wallet_id);});c.object("payer_owner",[&](auto& v){return walk(v,r.payer_owner);});c.object("payee_owner",[&](auto& v){return walk(v,r.payee_owner);});c.object("amount",[&](auto& v){return walk(v,r.amount);});common_checkpoint::strings(c,"field_order",r.field_order);if(!c.status().ok)return c.status();auto check=shape(r.field_order,{"service_id","service_version","rule_id","berth_place_id","garrison_id","occupation_source_id","army_id","payer_wallet_id","payee_wallet_id","payer_owner","payee_owner","amount"});return check.ok?c.status():c.reject(check.error.original.code);}
// NavalRepairChargeFact按具名格式保存原序，缺失/重名/附加字段均拒绝。
Status emit_record_fields(ProjectionWriter& w,const NavalRepairChargeFact& r){auto check=shape(r.field_order,{"action_id","terms"});if(!check.ok)return check;emit_value(w,"action_id",r.action_id);emit_value(w,"terms",r.terms);w.order_fields(r.field_order);return w.status();}
V enc(const NavalRepairChargeFact& r){return projected_value(r);}
void dec(const V& v,NavalRepairChargeFact& r,const StableId& p){boundary::fields(v,{"action_id","terms"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"action_id",p),r.action_id,checkpoint_decode_boundary_join({p,"/action_id"}));dec(boundary::field(v,"terms",p),r.terms,checkpoint_decode_boundary_join({p,"/terms"}));}
Status walk(CodecVisitor& c,NavalRepairChargeFact& r){c.object("action_id",[&](auto& v){return walk(v,r.action_id);});c.object("terms",[&](auto& v){return walk(v,r.terms);});common_checkpoint::strings(c,"field_order",r.field_order);if(!c.status().ok)return c.status();auto check=shape(r.field_order,{"action_id","terms"});return check.ok?c.status():c.reject(check.error.original.code);}
// NavalRepairChargeState按具名格式保存原序，缺失/重名/附加字段均拒绝。
Status emit_record_fields(ProjectionWriter& w,const NavalRepairChargeState& r){auto check=shape(r.field_order,{"charges"});if(!check.ok)return check;emit_value(w,"charges",r.charges);w.order_fields(r.field_order);return w.status();}
V enc(const NavalRepairChargeState& r){return projected_value(r);}
void dec(const V& v,NavalRepairChargeState& r,const StableId& p){boundary::fields(v,{"charges"},{},p);r.field_order=boundary::order(v);dec(boundary::field(v,"charges",p),r.charges,checkpoint_decode_boundary_join({p,"/charges"}));}
Status walk(CodecVisitor& c,NavalRepairChargeState& r){c.object("charges",[&](auto& v){return walk(v,r.charges);});common_checkpoint::strings(c,"field_order",r.field_order);if(!c.status().ok)return c.status();auto check=shape(r.field_order,{"charges"});return check.ok?c.status():c.reject(check.error.original.code);}
bool allocated(const StableId& id){if(id.rfind("action:",0)!=0)return false;Integer value=0;auto begin=id.data()+7;auto read=std::from_chars(begin,id.data()+id.size(),value);return read.ec==std::errc{}&&read.ptr==id.data()+id.size()&&value>0&&id=="action:"+std::to_string(value);}
}
// Exact C21 keeps this same original charge row at its unique start source.
Status emit_fact_fields(ProjectionWriter& w,const NavalRepairChargeFact& v){return emit_record_fields(w,v);}
Outcome<NavalRepairChargeFact> decode_fact(const BoundaryValue& raw){
 auto path=checkpoint_decode_boundary_text("naval_repair/charge");auto unique=validate_naval_json_unique_keys(raw,path);if(!unique.ok)return {false,{},unique.error};
 try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<NavalRepairChargeFact>();
  NavalRepairChargeFact out;dec(raw,out,path);return {true,std::move(out),{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Outcome<NavalRepairChargeFact> decode_fact(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){
 static_assert(std::is_nothrow_move_constructible_v<NavalRepairChargeFact>,"ordinary original payload must transfer without a hidden copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","naval_repair/charge");
 return decode_fact(raw);
}
Status visit_fact(CodecVisitor& c,NavalRepairChargeFact& v){return walk(c,v);}
// 金额只描述一次原预约；历史必须再核作者工法与同一action开工边界。
Status validate(const NavalRepairChargeState& state){std::set<StableId> actions;for(const auto& row:state.charges){const auto& t=row.terms;if(!allocated(row.action_id)||!actions.insert(row.action_id).second||t.service_id.empty()||t.service_version.empty()||t.rule_id.empty()||t.berth_place_id.empty()||t.garrison_id.empty()||t.army_id.empty()||t.payer_wallet_id.empty()||t.payee_wallet_id.empty()||t.payer_wallet_id==t.payee_wallet_id||t.payer_owner.kind!=Owner::Kind::organization||t.payer_owner.id.empty()||t.payer_owner.extra||t.payee_owner.kind!=Owner::Kind::organization||t.payee_owner.id.empty()||t.payee_owner.extra||t.amount<1||t.amount>1000000)return common_failure("InvalidNavalRepairChargeFact",row.action_id);}return common_success();}
Status emit_fields(ProjectionWriter& w,const NavalRepairChargeState& state){auto checked=validate(state);if(!checked.ok)return checked;return emit_record_fields(w,state);}
BoundaryValue encode(const NavalRepairChargeState& state){V out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,state);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
BoundaryValue encode_terms(const NavalRepairChargeTerms& terms){return enc(terms);}
Outcome<NavalRepairChargeState> decode(const BoundaryValue& value){auto unique=validate_naval_json_unique_keys(value,"naval/repair_charges");if(!unique.ok)return {false,{},unique.error};try{NavalRepairChargeState state;dec(value,state,"naval/repair_charges");auto valid=validate(state);return valid.ok?Outcome<NavalRepairChargeState>{true,std::move(state),{}}:Outcome<NavalRepairChargeState>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit(CodecVisitor& c,NavalRepairChargeState& state){if(c.naval_codec_version!=4&&c.naval_codec_version!=5&&c.naval_codec_version!=6)return c.reject("NavalRepairChargeProfileRequired");auto walked=walk(c,state);if(!walked.ok)return walked;auto valid=validate(state);return valid.ok?c.status():c.reject(valid.error.original.code,valid.error.original.path);}
}
