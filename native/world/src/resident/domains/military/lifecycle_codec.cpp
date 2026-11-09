#include "resident/domains/military/military_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include <array>
// 逐字段保留生命周期事实及request存在位，未实现字段不藏进通用extras。
#include "lifecycle_codec.hpp"
#include "battle_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::lifecycle_codec {

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
 else if constexpr(std::is_same_v<T,MilitaryActorChange>)return w.object(key,[&](auto& child){return battle_codec::emit_fields(child,value);});
 else return w.object(key,[&](auto& child){return emit_fields(child,value);});
}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& child,const auto& row){return emit_value(child,"",row);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<std::pair<StableId,T>,Alloc>& rows){return w.object(key,[&](auto& child){for(const auto& row:rows){auto result=emit_value(child,row.first,row.second);if(!result.ok)return result;}return child.status();});}
template<class Fn>void append_projected_fields(BoundaryValue::Dict& target,Fn emit){BoundaryValue result;BoundaryProjectionWriter sink(result);auto checked=sink.object("",emit);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);auto* rows=result.as<BoundaryValue::Dict>();for(auto& row:*rows)target.push_back(std::move(row));}
template<class T>BoundaryValue encode_projected(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
}
using V=BoundaryValue;using D=V::Dict;using A=V::Array;using domain_codec::order;using domain_codec::ordered;using domain_codec::visit_order;
V encode(const StableId& x){return V{x};}V encode(Integer x){return V{x};}V encode(bool x){return V{x};}
void decode(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void decode(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}void decode(const V& v,bool& x,const StableId& p){x=boundary::boolean(v,p);}
Status visit(CodecVisitor& c,StableId& x){return c.field("value",x);}Status visit(CodecVisitor& c,Integer& x){return c.field("value",x);}Status visit(CodecVisitor& c,bool& x){return c.field("value",x);}
V encode(const Owner& x){return boundary::encode_owner(x);}void decode(const V& v,Owner& x,const StableId& p){x=boundary::decode_owner(v,p);}Status visit(CodecVisitor& c,Owner& x){return common_checkpoint::owner(c,x);}
V encode(const InteractionContext& x){return interaction_codec::encode_context(x);}void decode(const V& v,InteractionContext& x,const StableId& p){x=interaction_codec::decode_context(v,p);}Status visit(CodecVisitor& c,InteractionContext& x){return interaction_codec::visit_context(c,x);}
V encode(const MilitaryActorChange& x){return battle_codec::encode(x);}void decode(const V& v,MilitaryActorChange& x,const StableId& p){battle_codec::decode(v,x,p);}Status visit(CodecVisitor& c,MilitaryActorChange& x){return battle_codec::visit(c,x);}
template<class T>V encode(const std::vector<T>& rows){A a;for(auto& r:rows)a.push_back(encode(r));return V{std::move(a)};}
template<class T>void decode(const V& v,std::vector<T>& out,const StableId& p){domain_codec::dec_list(v,out,p,[](const V& input,T& row,const StableId& path){decode(input,row,path);});}
template<class T>Status visit(CodecVisitor& c,std::vector<T>& rows){return c.sequence("rows",rows,[](auto& c,auto& x){return visit(c,x);});}
template<class T>V encode(const std::vector<std::pair<StableId,T>>& rows){D d;for(auto& r:rows)d.emplace_back(r.first,encode(r.second));return V{std::move(d)};}
template<class T>void decode(const V& v,std::vector<std::pair<StableId,T>>& out,const StableId& p){domain_codec::dec_pairs(v,out,p,[](const V& input,T& row,const StableId& path){decode(input,row,path);});}
template<class T>Status visit(CodecVisitor& c,std::vector<std::pair<StableId,T>>& rows){return c.sequence("pairs",rows,[](auto& c,auto& r){c.field("key",r.first);return visit(c,r.second);});}
Outcome<std::vector<std::pair<StableId,Integer>>> decode_return_goods(const BoundaryValue& raw){try{std::vector<std::pair<StableId,Integer>> rows;decode(raw,rows,checkpoint_decode_boundary_text("exact_military_return/goods"));return {true,std::move(rows),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_return_goods(CodecVisitor& c,std::vector<std::pair<StableId,Integer>>& rows){return visit(c,rows);}
Status emit_return_goods_fields(ProjectionWriter& w,const std::vector<std::pair<StableId,Integer>>& rows){for(const auto& row:rows){auto status=emit_value(w,row.first,row.second);if(!status.ok)return status;}return w.status();}
// MilitaryReturnAuthority只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryReturnAuthority& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","id"},{},p);
decode(boundary::field(v,"kind",p),r.kind,checkpoint_decode_boundary_join({p,"/kind"}));
decode(boundary::field(v,"id",p),r.id,checkpoint_decode_boundary_join({p,"/id"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryReturnAuthority& r){
emit_value(w,"kind",r.kind);
emit_value(w,"id",r.id);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryReturnAuthority& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryReturnAuthority& r){
c.object("kind",[&](auto& c){return visit(c,r.kind);});
c.object("id",[&](auto& c){return visit(c,r.id);});
visit_order(c,r.field_order);return c.status();}
// CampaignObjectiveResult只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,CampaignObjectiveResult& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","tick"},{"source_id"},p);
decode(boundary::field(v,"kind",p),r.kind,checkpoint_decode_boundary_join({p,"/kind"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
if(auto x=v.find("source_id")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.source_id)>::value_type>();r.source_id.emplace();decode(*x,*r.source_id,checkpoint_decode_boundary_join({p,"/source_id"}));}
}
Status emit_fields(ProjectionWriter& w,const CampaignObjectiveResult& r){
emit_value(w,"kind",r.kind);
emit_value(w,"tick",r.tick);
if(r.source_id)emit_value(w,"source_id",*r.source_id);
w.order_fields(r.field_order);return w.status();}
V encode(const CampaignObjectiveResult& r){return encode_projected(r);}
Status visit(CodecVisitor& c,CampaignObjectiveResult& r){
c.object("kind",[&](auto& c){return visit(c,r.kind);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.optional("source_id",r.source_id,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryLogisticsTerms只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryLogisticsTerms& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"army_id","actor_id","garrison_id","source_garrison_id","ownership_id","place_id","organization_id"},{"authority","goods","money","source_inventory_id","source_wallet_id"},p);
decode(boundary::field(v,"army_id",p),r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"garrison_id",p),r.garrison_id,checkpoint_decode_boundary_join({p,"/garrison_id"}));
decode(boundary::field(v,"source_garrison_id",p),r.source_garrison_id,checkpoint_decode_boundary_join({p,"/source_garrison_id"}));
decode(boundary::field(v,"ownership_id",p),r.ownership_id,checkpoint_decode_boundary_join({p,"/ownership_id"}));
decode(boundary::field(v,"place_id",p),r.place_id,checkpoint_decode_boundary_join({p,"/place_id"}));
decode(boundary::field(v,"organization_id",p),r.organization_id,checkpoint_decode_boundary_join({p,"/organization_id"}));
if(auto x=v.find("authority")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.authority)>::value_type>();r.authority.emplace();decode(*x,*r.authority,checkpoint_decode_boundary_join({p,"/authority"}));}
if(auto x=v.find("goods")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.goods)>::value_type>();r.goods.emplace();decode(*x,*r.goods,checkpoint_decode_boundary_join({p,"/goods"}));}
if(auto x=v.find("money")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.money)>::value_type>();r.money.emplace();decode(*x,*r.money,checkpoint_decode_boundary_join({p,"/money"}));}
if(auto x=v.find("source_inventory_id")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.source_inventory_id)>::value_type>();r.source_inventory_id.emplace();decode(*x,*r.source_inventory_id,checkpoint_decode_boundary_join({p,"/source_inventory_id"}));}
if(auto x=v.find("source_wallet_id")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.source_wallet_id)>::value_type>();r.source_wallet_id.emplace();decode(*x,*r.source_wallet_id,checkpoint_decode_boundary_join({p,"/source_wallet_id"}));}
}
Status emit_fields(ProjectionWriter& w,const MilitaryLogisticsTerms& r){
emit_value(w,"army_id",r.army_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"source_garrison_id",r.source_garrison_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"place_id",r.place_id);
emit_value(w,"organization_id",r.organization_id);
if(r.authority)emit_value(w,"authority",*r.authority);
if(r.goods)emit_value(w,"goods",*r.goods);
if(r.money)emit_value(w,"money",*r.money);
if(r.source_inventory_id)emit_value(w,"source_inventory_id",*r.source_inventory_id);
if(r.source_wallet_id)emit_value(w,"source_wallet_id",*r.source_wallet_id);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryLogisticsTerms& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryLogisticsTerms& r){
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("garrison_id",[&](auto& c){return visit(c,r.garrison_id);});
c.object("source_garrison_id",[&](auto& c){return visit(c,r.source_garrison_id);});
c.object("ownership_id",[&](auto& c){return visit(c,r.ownership_id);});
c.object("place_id",[&](auto& c){return visit(c,r.place_id);});
c.object("organization_id",[&](auto& c){return visit(c,r.organization_id);});
c.optional("authority",r.authority,[](auto& c,auto& x){return visit(c,x);});
c.optional("goods",r.goods,[](auto& c,auto& x){return visit(c,x);});
c.optional("money",r.money,[](auto& c,auto& x){return visit(c,x);});
c.optional("source_inventory_id",r.source_inventory_id,[](auto& c,auto& x){return visit(c,x);});
c.optional("source_wallet_id",r.source_wallet_id,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryLogisticsResult只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryLogisticsResult& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{},{"ok","army_id","order_id","receipt_id","actor_changes"},p);
if(auto x=v.find("ok")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.ok)>::value_type>();r.ok.emplace();decode(*x,*r.ok,checkpoint_decode_boundary_join({p,"/ok"}));}
if(auto x=v.find("army_id")){decode(*x,r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));}
if(auto x=v.find("order_id")){decode(*x,r.order_id,checkpoint_decode_boundary_join({p,"/order_id"}));}
if(auto x=v.find("receipt_id")){decode(*x,r.receipt_id,checkpoint_decode_boundary_join({p,"/receipt_id"}));}
if(auto x=v.find("actor_changes")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.actor_changes)>::value_type>();r.actor_changes.emplace();decode(*x,*r.actor_changes,checkpoint_decode_boundary_join({p,"/actor_changes"}));}
}
// 新回营结果沿原literal先写receipt_id；原载入结果仍按其已保存键序恢复。
Status emit_fields(ProjectionWriter& w,const MilitaryLogisticsResult& r){
if(r.ok)emit_value(w,"ok",*r.ok);
if((!r.receipt_id.empty()||domain_codec::had(r.field_order,"receipt_id")))emit_value(w,"receipt_id",r.receipt_id);
if((!r.army_id.empty()||domain_codec::had(r.field_order,"army_id")))emit_value(w,"army_id",r.army_id);
if((!r.order_id.empty()||domain_codec::had(r.field_order,"order_id")))emit_value(w,"order_id",r.order_id);
if(r.actor_changes)emit_value(w,"actor_changes",*r.actor_changes);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryLogisticsResult& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryLogisticsResult& r){
c.optional("ok",r.ok,[](auto& c,auto& x){return visit(c,x);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("order_id",[&](auto& c){return visit(c,r.order_id);});
c.object("receipt_id",[&](auto& c){return visit(c,r.receipt_id);});
c.optional("actor_changes",r.actor_changes,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// MilitaryReturnFact只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryReturnFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","army_id","order_id","garrison_id","source_garrison_id","ownership_id","actor_id","authority","tick","goods","money","unit_ids","returned_retainers","lost_retainers","prisoner_ids","actor_changes"},{},p);
if(boundary::get_string(v,"kind",p)!="military_return")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"army_id",p),r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));
decode(boundary::field(v,"order_id",p),r.order_id,checkpoint_decode_boundary_join({p,"/order_id"}));
decode(boundary::field(v,"garrison_id",p),r.garrison_id,checkpoint_decode_boundary_join({p,"/garrison_id"}));
decode(boundary::field(v,"source_garrison_id",p),r.source_garrison_id,checkpoint_decode_boundary_join({p,"/source_garrison_id"}));
decode(boundary::field(v,"ownership_id",p),r.ownership_id,checkpoint_decode_boundary_join({p,"/ownership_id"}));
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"authority",p),r.authority,checkpoint_decode_boundary_join({p,"/authority"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
decode(boundary::field(v,"goods",p),r.goods,checkpoint_decode_boundary_join({p,"/goods"}));
decode(boundary::field(v,"money",p),r.money,checkpoint_decode_boundary_join({p,"/money"}));
decode(boundary::field(v,"unit_ids",p),r.unit_ids,checkpoint_decode_boundary_join({p,"/unit_ids"}));
decode(boundary::field(v,"returned_retainers",p),r.returned_retainers,checkpoint_decode_boundary_join({p,"/returned_retainers"}));
decode(boundary::field(v,"lost_retainers",p),r.lost_retainers,checkpoint_decode_boundary_join({p,"/lost_retainers"}));
decode(boundary::field(v,"prisoner_ids",p),r.prisoner_ids,checkpoint_decode_boundary_join({p,"/prisoner_ids"}));
decode(boundary::field(v,"actor_changes",p),r.actor_changes,checkpoint_decode_boundary_join({p,"/actor_changes"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryReturnFact& r){
w.field("kind",StableId("military_return"));
emit_value(w,"army_id",r.army_id);
emit_value(w,"order_id",r.order_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"source_garrison_id",r.source_garrison_id);
emit_value(w,"ownership_id",r.ownership_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"authority",r.authority);
emit_value(w,"tick",r.tick);
emit_value(w,"goods",r.goods);
emit_value(w,"money",r.money);
emit_value(w,"unit_ids",r.unit_ids);
emit_value(w,"returned_retainers",r.returned_retainers);
emit_value(w,"lost_retainers",r.lost_retainers);
emit_value(w,"prisoner_ids",r.prisoner_ids);
emit_value(w,"actor_changes",r.actor_changes);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryReturnFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryReturnFact& r){
c.field("id",r.id);
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("order_id",[&](auto& c){return visit(c,r.order_id);});
c.object("garrison_id",[&](auto& c){return visit(c,r.garrison_id);});
c.object("source_garrison_id",[&](auto& c){return visit(c,r.source_garrison_id);});
c.object("ownership_id",[&](auto& c){return visit(c,r.ownership_id);});
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("authority",[&](auto& c){return visit(c,r.authority);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("goods",[&](auto& c){return visit(c,r.goods);});
c.object("money",[&](auto& c){return visit(c,r.money);});
c.object("unit_ids",[&](auto& c){return visit(c,r.unit_ids);});
c.object("returned_retainers",[&](auto& c){return visit(c,r.returned_retainers);});
c.object("lost_retainers",[&](auto& c){return visit(c,r.lost_retainers);});
c.object("prisoner_ids",[&](auto& c){return visit(c,r.prisoner_ids);});
c.object("actor_changes",[&](auto& c){return visit(c,r.actor_changes);});
visit_order(c,r.field_order);return c.status();}
// MilitaryMusterFact只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryMusterFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","actor_id","estate_id","order_id","actor_ids","tick"},{},p);
if(boundary::get_string(v,"kind",p)!="military_muster")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"estate_id",p),r.estate_id,checkpoint_decode_boundary_join({p,"/estate_id"}));
decode(boundary::field(v,"order_id",p),r.order_id,checkpoint_decode_boundary_join({p,"/order_id"}));
decode(boundary::field(v,"actor_ids",p),r.actor_ids,checkpoint_decode_boundary_join({p,"/actor_ids"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryMusterFact& r){
w.field("kind",StableId("military_muster"));
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"estate_id",r.estate_id);
emit_value(w,"order_id",r.order_id);
emit_value(w,"actor_ids",r.actor_ids);
emit_value(w,"tick",r.tick);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryMusterFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryMusterFact& r){
c.field("id",r.id);
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("estate_id",[&](auto& c){return visit(c,r.estate_id);});
c.object("order_id",[&](auto& c){return visit(c,r.order_id);});
c.object("actor_ids",[&](auto& c){return visit(c,r.actor_ids);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
visit_order(c,r.field_order);return c.status();}
// MilitaryResignationFact只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryResignationFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","actor_id","office_id","tick"},{},p);
if(boundary::get_string(v,"kind",p)!="office_resignation")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"office_id",p),r.office_id,checkpoint_decode_boundary_join({p,"/office_id"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryResignationFact& r){
w.field("kind",StableId("office_resignation"));
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"office_id",r.office_id);
emit_value(w,"tick",r.tick);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryResignationFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryResignationFact& r){
c.field("id",r.id);
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("office_id",[&](auto& c){return visit(c,r.office_id);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
visit_order(c,r.field_order);return c.status();}
// MilitaryDeliveryFact只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,MilitaryDeliveryFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","campaign_id","army_id","actor_id","tick","stronghold_id","recipient_army_id","item_id","quantity","from_inventory_id","to_inventory_id","source_owner","recipient_owner"},{},p);
if(boundary::get_string(v,"kind",p)!="military_delivery")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"campaign_id",p),r.campaign_id,checkpoint_decode_boundary_join({p,"/campaign_id"}));
decode(boundary::field(v,"army_id",p),r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
decode(boundary::field(v,"stronghold_id",p),r.stronghold_id,checkpoint_decode_boundary_join({p,"/stronghold_id"}));
decode(boundary::field(v,"recipient_army_id",p),r.recipient_army_id,checkpoint_decode_boundary_join({p,"/recipient_army_id"}));
decode(boundary::field(v,"item_id",p),r.item_id,checkpoint_decode_boundary_join({p,"/item_id"}));
decode(boundary::field(v,"quantity",p),r.quantity,checkpoint_decode_boundary_join({p,"/quantity"}));
decode(boundary::field(v,"from_inventory_id",p),r.from_inventory_id,checkpoint_decode_boundary_join({p,"/from_inventory_id"}));
decode(boundary::field(v,"to_inventory_id",p),r.to_inventory_id,checkpoint_decode_boundary_join({p,"/to_inventory_id"}));
decode(boundary::field(v,"source_owner",p),r.source_owner,checkpoint_decode_boundary_join({p,"/source_owner"}));
decode(boundary::field(v,"recipient_owner",p),r.recipient_owner,checkpoint_decode_boundary_join({p,"/recipient_owner"}));
}
Status emit_fields(ProjectionWriter& w,const MilitaryDeliveryFact& r){
w.field("kind",StableId("military_delivery"));
emit_value(w,"campaign_id",r.campaign_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"tick",r.tick);
emit_value(w,"stronghold_id",r.stronghold_id);
emit_value(w,"recipient_army_id",r.recipient_army_id);
emit_value(w,"item_id",r.item_id);
emit_value(w,"quantity",r.quantity);
emit_value(w,"from_inventory_id",r.from_inventory_id);
emit_value(w,"to_inventory_id",r.to_inventory_id);
emit_value(w,"source_owner",r.source_owner);
emit_value(w,"recipient_owner",r.recipient_owner);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryDeliveryFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryDeliveryFact& r){
c.field("id",r.id);
c.object("campaign_id",[&](auto& c){return visit(c,r.campaign_id);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("stronghold_id",[&](auto& c){return visit(c,r.stronghold_id);});
c.object("recipient_army_id",[&](auto& c){return visit(c,r.recipient_army_id);});
c.object("item_id",[&](auto& c){return visit(c,r.item_id);});
c.object("quantity",[&](auto& c){return visit(c,r.quantity);});
c.object("from_inventory_id",[&](auto& c){return visit(c,r.from_inventory_id);});
c.object("to_inventory_id",[&](auto& c){return visit(c,r.to_inventory_id);});
c.object("source_owner",[&](auto& c){return visit(c,r.source_owner);});
c.object("recipient_owner",[&](auto& c){return visit(c,r.recipient_owner);});
visit_order(c,r.field_order);return c.status();}
// CampaignSettlementFact只保原定义字段，失败由中央丢弃未发布候选。
void decode(const V& v,CampaignSettlementFact& r,const StableId& p){r.field_order=order(v);boundary::fields(v,{"kind","campaign_id","army_id","actor_id","tick","reason","objective_result","return_receipt_id"},{"report_context"},p);
if(boundary::get_string(v,"kind",p)!="campaign_settlement")boundary::fail("InvalidMilitaryFact",p);
decode(boundary::field(v,"campaign_id",p),r.campaign_id,checkpoint_decode_boundary_join({p,"/campaign_id"}));
decode(boundary::field(v,"army_id",p),r.army_id,checkpoint_decode_boundary_join({p,"/army_id"}));
decode(boundary::field(v,"actor_id",p),r.actor_id,checkpoint_decode_boundary_join({p,"/actor_id"}));
decode(boundary::field(v,"tick",p),r.tick,checkpoint_decode_boundary_join({p,"/tick"}));
decode(boundary::field(v,"reason",p),r.reason,checkpoint_decode_boundary_join({p,"/reason"}));
if(!boundary::object(boundary::field(v,"objective_result",p),checkpoint_decode_boundary_join({p,"/objective_result"})).empty()){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.objective_result)>::value_type>();r.objective_result.emplace();decode(boundary::field(v,"objective_result",p),*r.objective_result,checkpoint_decode_boundary_join({p,"/objective_result"}));}
decode(boundary::field(v,"return_receipt_id",p),r.return_receipt_id,checkpoint_decode_boundary_join({p,"/return_receipt_id"}));
if(auto x=v.find("report_context")){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.report_context)>::value_type>();r.report_context.emplace();decode(*x,*r.report_context,checkpoint_decode_boundary_join({p,"/report_context"}));}
}
Status emit_fields(ProjectionWriter& w,const CampaignSettlementFact& r){
w.field("kind",StableId("campaign_settlement"));
emit_value(w,"campaign_id",r.campaign_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"actor_id",r.actor_id);
emit_value(w,"tick",r.tick);
emit_value(w,"reason",r.reason);
if(r.objective_result)emit_value(w,"objective_result",*r.objective_result);else w.object("objective_result",[](auto& child){return child.status();});
emit_value(w,"return_receipt_id",r.return_receipt_id);
if(r.report_context)emit_value(w,"report_context",*r.report_context);
w.order_fields(r.field_order);return w.status();}
V encode(const CampaignSettlementFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,CampaignSettlementFact& r){
c.field("id",r.id);
c.object("campaign_id",[&](auto& c){return visit(c,r.campaign_id);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("actor_id",[&](auto& c){return visit(c,r.actor_id);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("reason",[&](auto& c){return visit(c,r.reason);});
c.optional("objective_result",r.objective_result,[](auto& c,auto& x){return visit(c,x);});
c.object("return_receipt_id",[&](auto& c){return visit(c,r.return_receipt_id);});
c.optional("report_context",r.report_context,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);return c.status();}
// request保留省略与坏类型标记；正式预览按旧业务首错拒绝，不预先猜默认目标。
Status emit_logistics_request_fields(ProjectionWriter& w,const ArmyLogistics& r){w.field("army_id",r.army_id);if(r.garrison_id)w.field("garrison_id",*r.garrison_id);if(r.food_quantity)w.field("food_quantity",*r.food_quantity);if(r.money)w.field("money",*r.money);w.order_fields(r.field_order);return w.status();}
V encode_request(const ArmyLogistics& r){V result;BoundaryProjectionWriter sink(result);auto checked=sink.object("",[&](auto& w){return emit_logistics_request_fields(w,r);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return result;}
bool command_supported(const StableId& kind){return kind=="army_return"||kind=="army_resupply"||kind=="army_pledge"||kind=="office_resign"||kind=="campaign_return"||kind=="campaign_cancel"||kind=="campaign_deliver"||kind=="campaign_report";}
Outcome<MilitaryCommand> decode_command(const StableId& kind,const V& p){try{boundary::object(p,"params");auto text=[&](const char* key){auto x=p.find(key);return x&&x->as<StableId>()?*x->as<StableId>():StableId{};};auto context=[&](){InteractionContext result;auto x=p.find("context");if(!x||!x->as<D>()){result.input_error="MissingInteractionContext";return result;}try{return interaction_codec::decode_context(*x,"params/context");}catch(const boundary::DecodeFailure&){result.input_error="InvalidInteractionContext";return result;}};
if(kind=="army_return"||kind=="army_resupply"){boundary::fields(p,{}, {"army_id","garrison_id","food_quantity","money"},"params");ArmyLogistics r;r.kind=kind;r.army_id=text("army_id");r.field_order=order(p);if(auto x=p.find("garrison_id")){if(x->as<StableId>())r.garrison_id=*x->as<StableId>();else r.garrison_valid=false;}if(auto x=p.find("food_quantity")){if(x->as<Integer>())r.food_quantity=*x->as<Integer>();else r.food_valid=false;}if(auto x=p.find("money")){if(x->as<Integer>())r.money=*x->as<Integer>();else r.money_valid=false;}return {true,r,{}};}
if(kind=="office_resign"){boundary::fields(p,{}, {"office_id","context"},"params");return {true,OfficeResign{text("office_id"),context(),order(p)},{}};}
if(kind=="army_pledge"){boundary::fields(p,{}, {"order_id","estate_id","actor_ids"},"params");ArmyPledge r;r.order_id=text("order_id");r.estate_id=text("estate_id");r.field_order=order(p);auto x=p.find("actor_ids");if(!x||!x->as<A>())r.actors_valid=false;else for(auto& v:*x->as<A>()){if(!v.as<StableId>()){r.actors_valid=false;break;}r.actor_ids.push_back(*v.as<StableId>());}return {true,r,{}};}
if(command_supported(kind)){boundary::fields(p,{}, {"campaign_id","garrison_id","context"},"params");CampaignManage r;r.kind=kind;r.campaign_id=text("campaign_id");r.garrison_id=text("garrison_id");r.field_order=order(p);if(auto x=p.find("garrison_id"))r.garrison_valid=x->as<StableId>()!=nullptr;if(kind=="campaign_report")r.context=context();return {true,r,{}};}return {false,{},common_failure("UnknownMilitaryCommand").error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_quote(const MilitaryQuote& q,const MilitaryCommand& command){D d{{"ok",V{true}}};if(std::holds_alternative<CampaignManage>(command)){d.emplace_back("campaign_id",V{q.campaign_id});if(q.delivery){auto& x=*q.delivery;d.emplace_back("source_inventory_id",V{x.source_inventory_id});d.emplace_back("destination_inventory_id",V{x.destination_inventory_id});d.emplace_back("source_owner",encode(x.source_owner));d.emplace_back("recipient_owner",encode(x.recipient_owner));d.emplace_back("item_id",V{x.item_id});d.emplace_back("quantity",V{x.quantity});}}else if(auto request=std::get_if<ArmyLogistics>(&command)){d.emplace_back("duration",V{q.duration});d.emplace_back("params",encode_request(*request));d.emplace_back("participants",encode(q.participants));if(q.logistics_terms)d.emplace_back("terms",encode(*q.logistics_terms));}else if(auto r=std::get_if<OfficeResign>(&command)){d.emplace_back("duration",V{Integer(0)});d.emplace_back("params",ordered(V{D{{"office_id",V{r->office_id}},{"context",encode(r->context)}}},r->field_order));d.emplace_back("context",encode(*q.context));d.emplace_back("participants",encode(q.participants));auto& t=*q.resignation;d.emplace_back("terms",V{D{{"office_id",V{t.office_id}},{"actor_id",V{t.actor_id}},{"organization_id",V{t.organization_id}},{"place_id",V{t.place_id}},{"provider_actor_id",V{t.provider_actor_id}}}});}else if(auto r=std::get_if<ArmyPledge>(&command)){d.emplace_back("duration",V{Integer(0)});d.emplace_back("params",ordered(V{D{{"order_id",V{r->order_id}},{"estate_id",V{r->estate_id}},{"actor_ids",encode(r->actor_ids)}}},r->field_order));auto& t=*q.pledge;d.emplace_back("terms",V{D{{"estate_id",V{t.estate_id}},{"order_id",V{t.order_id}},{"actor_ids",encode(t.actor_ids)}}});}return {true,V{std::move(d)},{}};}
bool decode_action(const StableId& kind,const V& v,const StableId& id,MilitaryState& s){if(kind!="army_return"&&kind!="army_resupply")return false;auto& p=boundary::field(v,"params","actions/"+id);boundary::fields(p,{"terms","request"},{},"actions/"+id);MilitaryLogisticsAction r;r.action_id=id;r.field_order=order(p);decode(boundary::field(p,"terms",id),r.terms,id);auto command=decode_command(kind,boundary::field(p,"request",id));if(!command.ok)boundary::fail(command.error.original.code,command.error.original.path);r.request=std::get<ArmyLogistics>(command.value);s.logistics_actions.push_back(r);return true;}
bool decode_fact(const StableId& kind,const V& v,const StableId& id,MilitaryState& s){if(kind=="military_action"){auto command=boundary::get_string(v,"command",id);if(command!="army_return"&&command!="army_resupply")return false;boundary::fields(v,{"kind","command","actor_id","start_tick","completed_tick","phase","success","terms","context","result"},{},id);if(!boundary::object(boundary::field(v,"context",id),id).empty())boundary::fail("MilitaryActionFactMismatch",id);MilitaryLogisticsFact r;r.id=id;r.command=command;r.field_order=order(v);r.actor_id=boundary::get_string(v,"actor_id",id);r.start_tick=boundary::get_int(v,"start_tick",id);r.completed_tick=boundary::get_int(v,"completed_tick",id);r.phase=boundary::get_string(v,"phase",id);r.success=boundary::get_bool(v,"success",id);decode(boundary::field(v,"terms",id),r.terms,id);auto& result=boundary::field(v,"result",id);if(!boundary::object(result,id).empty()||r.phase=="completed"){if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<typename std::decay_t<decltype(r.result)>::value_type>();r.result.emplace();decode(result,*r.result,id);}s.logistics_facts.push_back(r);return true;}
#define FACT(k,Type,table) if(kind==k){Type r;decode(v,r,id);r.id=id;s.table.push_back(std::move(r));return true;}
FACT("military_return",MilitaryReturnFact,return_facts);FACT("military_muster",MilitaryMusterFact,muster_facts);FACT("office_resignation",MilitaryResignationFact,resignation_facts);FACT("military_delivery",MilitaryDeliveryFact,delivery_facts);FACT("campaign_settlement",CampaignSettlementFact,settlement_facts);
#undef FACT
return false;}
Status emit_fields(ProjectionWriter& child,const MilitaryLogisticsAction& p){emit_value(child,"terms",p.terms);return child.object("request",[&](auto& request){return emit_logistics_request_fields(request,p.request);});}
Status emit_fields(ProjectionWriter& child,const MilitaryLogisticsFact& f){child.field("kind",StableId("military_action"));child.field("command",f.command);child.field("actor_id",f.actor_id);child.field("start_tick",f.start_tick);child.field("completed_tick",f.completed_tick);child.field("phase",f.phase);child.field("success",f.success);emit_value(child,"terms",f.terms);child.object("context",[](auto& empty){return empty.status();});if(f.result)return emit_value(child,"result",*f.result);return child.object("result",[](auto& empty){return empty.status();});}
Status emit_action_payload_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& p:s.logistics_actions){auto checked=w.object(p.action_id,p.field_order,[&](auto& child){return emit_fields(child,p);});if(!checked.ok)return checked;}return w.status();}
Status emit_fact_fields(ProjectionWriter& w,const MilitaryState& s){for(const auto& f:s.logistics_facts){auto checked=w.object(f.id,f.field_order,[&](auto& child){return emit_fields(child,f);});if(!checked.ok)return checked;}
#define APPEND(table) for(const auto& f:s.table){auto checked=emit_value(w,f.id,f);if(!checked.ok)return checked;}
APPEND(return_facts);APPEND(muster_facts);APPEND(resignation_facts);APPEND(delivery_facts);APPEND(settlement_facts);
#undef APPEND
return w.status();}
void append_state(D& actions,D& facts,const MilitaryState& s){append_projected_fields(actions,[&](auto& w){return emit_action_payload_fields(w,s);});append_projected_fields(facts,[&](auto& w){return emit_fact_fields(w,s);});}

Status visit_state(CodecVisitor& c,MilitaryState& s){
#define ROWS(n) c.sequence(#n,s.n,[](auto& c,auto& r){return visit(c,r);})
ROWS(return_facts);ROWS(muster_facts);ROWS(resignation_facts);ROWS(delivery_facts);ROWS(settlement_facts);
#undef ROWS
c.sequence("logistics_actions",s.logistics_actions,[](auto& c,MilitaryLogisticsAction& p){c.field("action_id",p.action_id);c.object("terms",[&](auto& c){return visit(c,p.terms);});auto& r=p.request;c.field("kind",r.kind);c.field("army_id",r.army_id);c.optional("garrison_id",r.garrison_id,[](auto& c,auto& x){return visit(c,x);});c.optional("food_quantity",r.food_quantity,[](auto& c,auto& x){return visit(c,x);});c.optional("money",r.money,[](auto& c,auto& x){return visit(c,x);});visit_order(c,r.field_order);visit_order(c,p.field_order);return c.status();});c.sequence("logistics_facts",s.logistics_facts,[](auto& c,MilitaryLogisticsFact& r){c.field("id",r.id);c.field("command",r.command);c.field("actor_id",r.actor_id);c.field("start_tick",r.start_tick);c.field("completed_tick",r.completed_tick);c.field("phase",r.phase);c.field("success",r.success);c.object("terms",[&](auto& c){return visit(c,r.terms);});c.optional("result",r.result,[](auto& c,auto& x){return visit(c,x);});visit_order(c,r.field_order);return c.status();});return c.status();}
Outcome<std::vector<std::pair<StableId,Integer>>> decode_return_goods(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","exact_military_return/goods");
 return decode_return_goods(raw);
}

namespace {
template<class T>Outcome<T> decode_checkpoint_original(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget,std::string_view path){
 static_assert(std::is_same_v<T,MilitaryLogisticsTerms>||std::is_same_v<T,MilitaryLogisticsResult>||std::is_same_v<T,MilitaryReturnAuthority>);
 static_assert(std::is_nothrow_move_constructible_v<T>,"ordinary original payload must transfer without a hidden copy");
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch",path);
 try{budget.default_values<T>();T row;decode(raw,row,checkpoint_decode_boundary_text(path));return {true,std::move(row),{}};}
 catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
Outcome<MilitaryLogisticsTerms> decode_logistics_terms(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_checkpoint_original<MilitaryLogisticsTerms>(raw,budget,"exact_military_action/original");}
Outcome<MilitaryLogisticsResult> decode_logistics_result(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_checkpoint_original<MilitaryLogisticsResult>(raw,budget,"exact_military_action/original");}
Outcome<MilitaryReturnAuthority> decode_return_authority(const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_checkpoint_original<MilitaryReturnAuthority>(raw,budget,"exact_military_action/original");}

}
