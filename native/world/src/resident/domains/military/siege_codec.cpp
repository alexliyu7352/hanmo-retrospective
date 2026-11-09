#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include <array>
// 唯一围城冷codec保留原字段序，非法类型/未知状态字段拒绝；不参与热轮次求值。
#include "siege_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident::siege_codec {

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
 else return w.object(key,[&](auto& child){return emit_fields(child,value);});
}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<T,Alloc>& rows){return w.sequence(key,rows,[](auto& child,const auto& row){return emit_value(child,"",row);});}
template<class T,class Alloc>Status emit_value(ProjectionWriter& w,const StableId& key,const std::vector<std::pair<StableId,T>,Alloc>& rows){return w.object(key,[&](auto& child){for(const auto& row:rows){auto result=emit_value(child,row.first,row.second);if(!result.ok)return result;}return child.status();});}
template<class T>BoundaryValue encode_projected(const T& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=emit_value(sink,"",value);if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
}
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
using domain_codec::ordered;using domain_codec::order;using domain_codec::visit_order;
// 标量codec逐型拷贝，不经浮点或动态热字段转换。
V encode(const StableId& x){return V{x};}
V encode(Integer x){return V{x};}
V encode(bool x){return V{x};}
void decode(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}
void decode(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}
void decode(const V& v,bool& x,const StableId& p){x=boundary::boolean(v,p);}
Status visit(CodecVisitor& c,StableId& x){return c.field("value",x);}
Status visit(CodecVisitor& c,Integer& x){return c.field("value",x);}
Status visit(CodecVisitor& c,bool& x){return c.field("value",x);}
// 有序数组与关联表只在冷读写边界转换，不另存同一字段的第二份值。
template<class T> V encode(const std::vector<T>& rows){A a;for(const auto& r:rows)a.push_back(encode(r));return V{std::move(a)};}
template<class T> void decode(const V& v,std::vector<T>& out,const StableId& p){out.clear();for(const auto& r:boundary::array(v,p)){T x;decode(r,x,p+"/"+std::to_string(out.size()));out.push_back(std::move(x));}}
template<class T> Status visit(CodecVisitor& c,std::vector<T>& rows){return c.sequence("rows",rows,[](auto& c,auto& x){return visit(c,x);});}
template<class T> V encode(const std::vector<std::pair<StableId,T>>& rows){D d;for(const auto& r:rows)d.emplace_back(r.first,encode(r.second));return V{std::move(d)};}
template<class T> void decode(const V& v,std::vector<std::pair<StableId,T>>& out,const StableId& p){out.clear();for(const auto& r:boundary::object(v,p)){T x;decode(r.second,x,p+"/"+r.first);out.emplace_back(r.first,std::move(x));}}
template<class T> Status visit(CodecVisitor& c,std::vector<std::pair<StableId,T>>& rows){return c.sequence("pairs",rows,[](auto& c,auto& r){c.field("key",r.first);return visit(c,r.second);});}
// SiegeMaterial固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeMaterial& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"item_id","quantity"},{},p);
    decode(boundary::field(v,"item_id",p),r.item_id,p+"/item_id");
    decode(boundary::field(v,"quantity",p),r.quantity,p+"/quantity");
}
// SiegeMaterial导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeMaterial& r){
    emit_value(w,"item_id",r.item_id);
    emit_value(w,"quantity",r.quantity);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeMaterial& r){return encode_projected(r);}
// SiegeMaterial检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeMaterial& r){
    c.object("item_id",[&](auto& c){return visit(c,r.item_id);});
    c.object("quantity",[&](auto& c){return visit(c,r.quantity);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeEngineUse固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeEngineUse& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"inventory_id","item_id","quantity"},{},p);
    decode(boundary::field(v,"inventory_id",p),r.inventory_id,p+"/inventory_id");
    decode(boundary::field(v,"item_id",p),r.item_id,p+"/item_id");
    decode(boundary::field(v,"quantity",p),r.quantity,p+"/quantity");
}
// SiegeEngineUse导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeEngineUse& r){
    emit_value(w,"inventory_id",r.inventory_id);
    emit_value(w,"item_id",r.item_id);
    emit_value(w,"quantity",r.quantity);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeEngineUse& r){return encode_projected(r);}
// SiegeEngineUse检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeEngineUse& r){
    c.object("inventory_id",[&](auto& c){return visit(c,r.inventory_id);});
    c.object("item_id",[&](auto& c){return visit(c,r.item_id);});
    c.object("quantity",[&](auto& c){return visit(c,r.quantity);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeOrder固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeOrder& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"action"},{"node_id","engine_use"},p);
    decode(boundary::field(v,"action",p),r.action,p+"/action");
    if(const auto* x=v.find("node_id")){r.node_id.emplace();decode(*x,*r.node_id,p+"/node_id");}
    if(const auto* x=v.find("engine_use")){r.engine_use.emplace();decode(*x,*r.engine_use,p+"/engine_use");}
}
// SiegeOrder导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeOrder& r){
    emit_value(w,"action",r.action);
    if(r.node_id)emit_value(w,"node_id",*r.node_id);
    if(r.engine_use)emit_value(w,"engine_use",*r.engine_use);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeOrder& r){return encode_projected(r);}
// SiegeOrder检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeOrder& r){
    c.object("action",[&](auto& c){return visit(c,r.action);});
    c.optional("node_id",r.node_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("engine_use",r.engine_use,[](auto& c,auto& x){return visit(c,x);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeEngineWork固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeEngineWork& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"node_id","remaining"},{},p);
    decode(boundary::field(v,"node_id",p),r.node_id,p+"/node_id");
    decode(boundary::field(v,"remaining",p),r.remaining,p+"/remaining");
}
// SiegeEngineWork导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeEngineWork& r){
    emit_value(w,"node_id",r.node_id);
    emit_value(w,"remaining",r.remaining);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeEngineWork& r){return encode_projected(r);}
// SiegeEngineWork检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeEngineWork& r){
    c.object("node_id",[&](auto& c){return visit(c,r.node_id);});
    c.object("remaining",[&](auto& c){return visit(c,r.remaining);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeBattleRequest固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeBattleRequest& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"source_id","node_id","battlefield_id","unit_ids","origin_nodes"},{},p);
    decode(boundary::field(v,"source_id",p),r.source_id,p+"/source_id");
    decode(boundary::field(v,"node_id",p),r.node_id,p+"/node_id");
    decode(boundary::field(v,"battlefield_id",p),r.battlefield_id,p+"/battlefield_id");
    decode(boundary::field(v,"unit_ids",p),r.unit_ids,p+"/unit_ids");
    decode(boundary::field(v,"origin_nodes",p),r.origin_nodes,p+"/origin_nodes");
}
// SiegeBattleRequest导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeBattleRequest& r){
    emit_value(w,"source_id",r.source_id);
    emit_value(w,"node_id",r.node_id);
    emit_value(w,"battlefield_id",r.battlefield_id);
    emit_value(w,"unit_ids",r.unit_ids);
    emit_value(w,"origin_nodes",r.origin_nodes);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeBattleRequest& r){return encode_projected(r);}
// SiegeBattleRequest检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeBattleRequest& r){
    c.object("source_id",[&](auto& c){return visit(c,r.source_id);});
    c.object("node_id",[&](auto& c){return visit(c,r.node_id);});
    c.object("battlefield_id",[&](auto& c){return visit(c,r.battlefield_id);});
    c.object("unit_ids",[&](auto& c){return visit(c,r.unit_ids);});
    c.object("origin_nodes",[&](auto& c){return visit(c,r.origin_nodes);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeResult固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeResult& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"kind","winner_side","tick"},{"terms","reason"},p);
    decode(boundary::field(v,"kind",p),r.kind,p+"/kind");
    decode(boundary::field(v,"winner_side",p),r.winner_side,p+"/winner_side");
    decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
    if(const auto* x=v.find("terms")){r.terms.emplace();decode(*x,*r.terms,p+"/terms");}
    if(const auto* x=v.find("reason")){r.reason.emplace();decode(*x,*r.reason,p+"/reason");}
}
// SiegeResult导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeResult& r){
    emit_value(w,"kind",r.kind);
    if(r.terms)emit_value(w,"terms",*r.terms);
    emit_value(w,"winner_side",r.winner_side);
    if(r.reason)emit_value(w,"reason",*r.reason);
    emit_value(w,"tick",r.tick);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeResult& r){return encode_projected(r);}
// SiegeResult检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeResult& r){
    c.object("kind",[&](auto& c){return visit(c,r.kind);});
    c.object("winner_side",[&](auto& c){return visit(c,r.winner_side);});
    c.object("tick",[&](auto& c){return visit(c,r.tick);});
    c.optional("terms",r.terms,[](auto& c,auto& x){return visit(c,x);});
    c.optional("reason",r.reason,[](auto& c,auto& x){return visit(c,x);});
    visit_order(c,r.field_order);return c.status();
}
// SiegeEvent固定字段按原序解码；可选项保留未出现与空值的差别。
void decode(const V& v,SiegeEvent& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"kind"},{"unit_id","node_id","stronghold_id","campaign_id","terms","defender_actor_id","reason","damage","remaining_hp","army_ids","battle_source_ids"},p);
    decode(boundary::field(v,"kind",p),r.kind,p+"/kind");
    if(const auto* x=v.find("unit_id")){r.unit_id.emplace();decode(*x,*r.unit_id,p+"/unit_id");}
    if(const auto* x=v.find("node_id")){r.node_id.emplace();decode(*x,*r.node_id,p+"/node_id");}
    if(const auto* x=v.find("stronghold_id")){r.stronghold_id.emplace();decode(*x,*r.stronghold_id,p+"/stronghold_id");}
    if(const auto* x=v.find("campaign_id")){r.campaign_id.emplace();decode(*x,*r.campaign_id,p+"/campaign_id");}
    if(const auto* x=v.find("terms")){r.terms.emplace();decode(*x,*r.terms,p+"/terms");}
    if(const auto* x=v.find("defender_actor_id")){r.defender_actor_id.emplace();decode(*x,*r.defender_actor_id,p+"/defender_actor_id");}
    if(const auto* x=v.find("reason")){r.reason.emplace();decode(*x,*r.reason,p+"/reason");}
    if(const auto* x=v.find("damage")){r.damage.emplace();decode(*x,*r.damage,p+"/damage");}
    if(const auto* x=v.find("remaining_hp")){r.remaining_hp.emplace();decode(*x,*r.remaining_hp,p+"/remaining_hp");}
    if(const auto* x=v.find("army_ids")){r.army_ids.emplace();decode(*x,*r.army_ids,p+"/army_ids");}
    if(const auto* x=v.find("battle_source_ids")){r.battle_source_ids.emplace();decode(*x,*r.battle_source_ids,p+"/battle_source_ids");}
}
// SiegeEvent导出仅重建已声明字段，原键序校验失败直接终止冷边界。
Status emit_fields(ProjectionWriter& w,const SiegeEvent& r){
    emit_value(w,"kind",r.kind);
    if(r.unit_id)emit_value(w,"unit_id",*r.unit_id);
    if(r.node_id)emit_value(w,"node_id",*r.node_id);
    if(r.damage)emit_value(w,"damage",*r.damage);
    if(r.remaining_hp)emit_value(w,"remaining_hp",*r.remaining_hp);
    if(r.stronghold_id)emit_value(w,"stronghold_id",*r.stronghold_id);
    if(r.campaign_id)emit_value(w,"campaign_id",*r.campaign_id);
    if(r.terms)emit_value(w,"terms",*r.terms);
    if(r.defender_actor_id)emit_value(w,"defender_actor_id",*r.defender_actor_id);
    if(r.battle_source_ids)emit_value(w,"battle_source_ids",*r.battle_source_ids);
    if(r.army_ids)emit_value(w,"army_ids",*r.army_ids);
    if(r.reason)emit_value(w,"reason",*r.reason);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeEvent& r){return encode_projected(r);}
// SiegeEvent检查点逐字段访问，不保存动态对象或隐藏兼容副本。
Status visit(CodecVisitor& c,SiegeEvent& r){
    c.object("kind",[&](auto& c){return visit(c,r.kind);});
    c.optional("unit_id",r.unit_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("node_id",r.node_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("stronghold_id",r.stronghold_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("campaign_id",r.campaign_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("terms",r.terms,[](auto& c,auto& x){return visit(c,x);});
    c.optional("defender_actor_id",r.defender_actor_id,[](auto& c,auto& x){return visit(c,x);});
    c.optional("reason",r.reason,[](auto& c,auto& x){return visit(c,x);});
    c.optional("damage",r.damage,[](auto& c,auto& x){return visit(c,x);});
    c.optional("remaining_hp",r.remaining_hp,[](auto& c,auto& x){return visit(c,x);});
    c.optional("army_ids",r.army_ids,[](auto& c,auto& x){return visit(c,x);});
    c.optional("battle_source_ids",r.battle_source_ids,[](auto& c,auto& x){return visit(c,x);});
    visit_order(c,r.field_order);return c.status();
}
// 完整会话的空result仍输出空字典；根层提交字段可省略，已有存在位不得丢失。
void decode(const V& v,SiegeRecord& r,const StableId& p){
    r={};r.field_order=order(v);boundary::fields(v,{"id","campaign_id","stronghold_id","topology_id","phase","started_tick","last_tick","round","armies","unit_nodes","node_hp","inner_hold_ticks","unsupplied_ticks","blockaded","engine_work","pending_nodes","ai_plans","seen_receipt_ids","withdrawn_army_ids","result","last_events"},{"plan_submitted","pending_orders"},p);
    decode(boundary::field(v,"id",p),r.id,p+"/id");
    decode(boundary::field(v,"campaign_id",p),r.campaign_id,p+"/campaign_id");
    decode(boundary::field(v,"stronghold_id",p),r.stronghold_id,p+"/stronghold_id");
    decode(boundary::field(v,"topology_id",p),r.topology_id,p+"/topology_id");
    decode(boundary::field(v,"phase",p),r.phase,p+"/phase");
    decode(boundary::field(v,"started_tick",p),r.started_tick,p+"/started_tick");
    decode(boundary::field(v,"last_tick",p),r.last_tick,p+"/last_tick");
    decode(boundary::field(v,"round",p),r.round,p+"/round");
    decode(boundary::field(v,"armies",p),r.armies,p+"/armies");
    decode(boundary::field(v,"unit_nodes",p),r.unit_nodes,p+"/unit_nodes");
    decode(boundary::field(v,"node_hp",p),r.node_hp,p+"/node_hp");
    decode(boundary::field(v,"inner_hold_ticks",p),r.inner_hold_ticks,p+"/inner_hold_ticks");
    decode(boundary::field(v,"unsupplied_ticks",p),r.unsupplied_ticks,p+"/unsupplied_ticks");
    decode(boundary::field(v,"blockaded",p),r.blockaded,p+"/blockaded");
    decode(boundary::field(v,"engine_work",p),r.engine_work,p+"/engine_work");
    decode(boundary::field(v,"pending_nodes",p),r.pending_nodes,p+"/pending_nodes");
    decode(boundary::field(v,"ai_plans",p),r.ai_plans,p+"/ai_plans");
    decode(boundary::field(v,"seen_receipt_ids",p),r.seen_receipt_ids,p+"/seen_receipt_ids");
    decode(boundary::field(v,"withdrawn_army_ids",p),r.withdrawn_army_ids,p+"/withdrawn_army_ids");
    const auto& result=boundary::field(v,"result",p);if(!boundary::object(result,p+"/result").empty()){r.result.emplace();decode(result,*r.result,p+"/result");}
    decode(boundary::field(v,"last_events",p),r.last_events,p+"/last_events");
    if(const auto* x=v.find("plan_submitted"))decode(*x,r.plan_submitted,p+"/plan_submitted");
    if(const auto* x=v.find("pending_orders"))decode(*x,r.pending_orders,p+"/pending_orders");
}
// 根层新建会话补提交字段，纯规则旧快照若原本省略则继续省略。
Status emit_fields(ProjectionWriter& w,const SiegeRecord& r){
    emit_value(w,"id",r.id);
    emit_value(w,"campaign_id",r.campaign_id);
    emit_value(w,"stronghold_id",r.stronghold_id);
    emit_value(w,"topology_id",r.topology_id);
    emit_value(w,"phase",r.phase);
    emit_value(w,"started_tick",r.started_tick);
    emit_value(w,"last_tick",r.last_tick);
    emit_value(w,"round",r.round);
    emit_value(w,"armies",r.armies);
    emit_value(w,"unit_nodes",r.unit_nodes);
    emit_value(w,"node_hp",r.node_hp);
    emit_value(w,"inner_hold_ticks",r.inner_hold_ticks);
    emit_value(w,"unsupplied_ticks",r.unsupplied_ticks);
    emit_value(w,"blockaded",r.blockaded);
    emit_value(w,"engine_work",r.engine_work);
    emit_value(w,"pending_nodes",r.pending_nodes);
    emit_value(w,"ai_plans",r.ai_plans);
    emit_value(w,"seen_receipt_ids",r.seen_receipt_ids);
    emit_value(w,"withdrawn_army_ids",r.withdrawn_army_ids);
    if(r.result)emit_value(w,"result",*r.result);else w.object("result",[](auto& child){return child.status();});
    emit_value(w,"last_events",r.last_events);
    if(r.field_order.empty()||domain_codec::had(r.field_order,"plan_submitted"))emit_value(w,"plan_submitted",r.plan_submitted);
    if(r.field_order.empty()||domain_codec::had(r.field_order,"pending_orders"))emit_value(w,"pending_orders",r.pending_orders);
    w.order_fields(r.field_order);return w.status();}
V encode(const SiegeRecord& r){return encode_projected(r);}
// 检查点包括根层提交位与冷键序，不运行围城业务或补默认兵额。
Status visit(CodecVisitor& c,SiegeRecord& r){
    c.object("id",[&](auto& c){return visit(c,r.id);});
    c.object("campaign_id",[&](auto& c){return visit(c,r.campaign_id);});
    c.object("stronghold_id",[&](auto& c){return visit(c,r.stronghold_id);});
    c.object("topology_id",[&](auto& c){return visit(c,r.topology_id);});
    c.object("phase",[&](auto& c){return visit(c,r.phase);});
    c.object("started_tick",[&](auto& c){return visit(c,r.started_tick);});
    c.object("last_tick",[&](auto& c){return visit(c,r.last_tick);});
    c.object("round",[&](auto& c){return visit(c,r.round);});
    c.object("armies",[&](auto& c){return visit(c,r.armies);});
    c.object("unit_nodes",[&](auto& c){return visit(c,r.unit_nodes);});
    c.object("node_hp",[&](auto& c){return visit(c,r.node_hp);});
    c.object("inner_hold_ticks",[&](auto& c){return visit(c,r.inner_hold_ticks);});
    c.object("unsupplied_ticks",[&](auto& c){return visit(c,r.unsupplied_ticks);});
    c.object("blockaded",[&](auto& c){return visit(c,r.blockaded);});
    c.object("engine_work",[&](auto& c){return visit(c,r.engine_work);});
    c.object("pending_nodes",[&](auto& c){return visit(c,r.pending_nodes);});
    c.object("ai_plans",[&](auto& c){return visit(c,r.ai_plans);});
    c.object("seen_receipt_ids",[&](auto& c){return visit(c,r.seen_receipt_ids);});
    c.object("withdrawn_army_ids",[&](auto& c){return visit(c,r.withdrawn_army_ids);});
    c.optional("result",r.result,[](auto& c,auto& x){return visit(c,x);});
    c.object("last_events",[&](auto& c){return visit(c,r.last_events);});
    c.object("plan_submitted",[&](auto& c){return visit(c,r.plan_submitted);});
    c.object("pending_orders",[&](auto& c){return visit(c,r.pending_orders);});
    visit_order(c,r.field_order);return c.status();
}
// 拓扑作者字段只投影现有规则所需结构；外层目录仍保存统一作者与表现元数据。
void decode(const V& v,SiegeTopology& t,const StableId& p){
    t={};t.id=boundary::get_string(v,"id",p);if(const auto* n=v.find("name"))t.name=boundary::string(*n,p+"/name");
    t.attacker_entry_id=boundary::get_string(v,"attacker_entry_id",p);t.defender_entry_id=boundary::get_string(v,"defender_entry_id",p);t.inner_id=boundary::get_string(v,"inner_id",p);
    decode(boundary::field(v,"supply_node_ids",p),t.supply_node_ids,p+"/supply_node_ids");
    for(const auto& raw:boundary::array(boundary::field(v,"nodes",p),p+"/nodes")){
        SiegeNode node;auto path=p+"/nodes/"+std::to_string(t.nodes.size());node.id=boundary::get_string(raw,"id",path);node.kind=boundary::get_string(raw,"kind",path);node.battlefield_id=boundary::get_string(raw,"battlefield_id",path);node.max_hp=boundary::get_int(raw,"max_hp",path);if(const auto* n=raw.find("name"))node.name=boundary::string(*n,path+"/name");t.nodes.push_back(std::move(node));
    }
    for(const auto& raw:boundary::array(boundary::field(v,"edges",p),p+"/edges")){
        SiegeEdge edge;auto path=p+"/edges/"+std::to_string(t.edges.size());edge.from=boundary::get_string(raw,"from",path);edge.to=boundary::get_string(raw,"to",path);if(const auto* b=raw.find("blocker_id"))edge.blocker_id=boundary::string(*b,path+"/blocker_id");t.edges.push_back(std::move(edge));
    }
    const auto& rules=boundary::field(v,"rules",p);const auto path=p+"/rules";
    t.rules.hold_ticks=boundary::get_int(rules,"hold_ticks",path);
    t.rules.blockade_units=boundary::get_int(rules,"blockade_units",path);
    t.rules.starvation_ticks=boundary::get_int(rules,"starvation_ticks",path);
    t.rules.max_ticks=boundary::get_int(rules,"max_ticks",path);
    t.rules.gate_damage=boundary::get_int(rules,"gate_damage",path);
    t.rules.repair_amount=boundary::get_int(rules,"repair_amount",path);
    t.rules.engine_bonus=boundary::get_int(rules,"engine_bonus",path);
    t.rules.engine_ticks=boundary::get_int(rules,"engine_ticks",path);
    t.rules.surrender_morale=boundary::get_int(rules,"surrender_morale",path);
    t.rules.wall_defense_bp=boundary::get_int(rules,"wall_defense_bp",path);
    t.rules.engine_item_id=boundary::get_string(rules,"engine_item_id",path);
    decode(boundary::field(rules,"engine_materials",path),t.rules.engine_materials,path+"/engine_materials");
    decode(boundary::field(rules,"repair_materials",path),t.rules.repair_materials,path+"/repair_materials");
}
// 冷目录装载先验证全部引用，坏拓扑整批失败，不把未知机制降级为成功。
Outcome<std::vector<SiegeTopology>> decode_catalog(const V& root,const MilitaryBattleCatalog& battles){
    try{
        std::vector<SiegeTopology> result;const auto* rows=root.find("siege_topologies");if(!rows)return {true,{},{}};
        std::vector<StableId> items;for(const auto& item:boundary::object(boundary::field(root,"items","data"),"items"))items.push_back(item.first);
        for(const auto& row:boundary::object(*rows,"siege_topologies")){
            SiegeTopology topology;decode(row.second,topology,"siege_topologies/"+row.first);
            if(topology.id!=row.first)boundary::fail("InvalidSiegeTopology","siege_topologies/"+row.first);
            const auto checked=validate_siege_topology(topology,battles,items);if(!checked.ok)return {false,{},checked.error};result.push_back(std::move(topology));
        }
        return {true,std::move(result),{}};
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
}
