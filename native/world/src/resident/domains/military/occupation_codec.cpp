#include "resident/domains/military/military_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include <array>
// 冷边界逐字段保全产权清单，运行态不隐藏任意字典或另存资源余额。
#include "occupation_codec.hpp"
#include "occupation_time_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "battle_codec.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::occupation_codec {

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
// 标量和有序集合共用严格decoder，缺失或额外字段由所属记录拒绝。
V encode(const StableId& x){return V{x};}V encode(Integer x){return V{x};}V encode(bool x){return V{x};}
void decode(const V& v,StableId& x,const StableId& p){x=boundary::string(v,p);}void decode(const V& v,Integer& x,const StableId& p){x=boundary::integer(v,p);}void decode(const V& v,bool& x,const StableId& p){x=boundary::boolean(v,p);}
Status visit(CodecVisitor& c,StableId& x){return c.field("value",x);}Status visit(CodecVisitor& c,Integer& x){return c.field("value",x);}Status visit(CodecVisitor& c,bool& x){return c.field("value",x);}
// 公产来源快照复用共同预留codec，旧action字段形状保持不变。
V encode(const ReservationRef& x){return boundary::encode_reservation_ref(x);}
void decode(const V& v,ReservationRef& x,const StableId& p){x=boundary::decode_reservation_ref(v,p);}
Status visit(CodecVisitor& c,ReservationRef& x){return common_checkpoint::reservation(c,x,"value");}
V encode(const Owner& x){return boundary::encode_owner(x);}void decode(const V& v,Owner& x,const StableId& p){x=boundary::decode_owner(v,p);}Status visit(CodecVisitor& c,Owner& x){return common_checkpoint::owner(c,x);}
V encode(const MilitaryActorChange& x){return battle_codec::encode(x);}void decode(const V& v,MilitaryActorChange& x,const StableId& p){battle_codec::decode(v,x,p);}Status visit(CodecVisitor& c,MilitaryActorChange& x){return battle_codec::visit(c,x);}
Status emit_fields(ProjectionWriter& w,const SiegeAssetEffect& x){w.field("kind",x.kind);w.field("army_id",x.army_id);w.field("item_id",x.item_id);w.field("quantity",x.quantity);return w.field("source_id",x.source_id);}
V encode(const SiegeAssetEffect& x){return encode_projected(x);}
void decode(const V& v,SiegeAssetEffect& x,const StableId& p){boundary::fields(v,{"kind","army_id","item_id","quantity","source_id"},{},p);x.kind=boundary::get_string(v,"kind",p);x.army_id=boundary::get_string(v,"army_id",p);x.item_id=boundary::get_string(v,"item_id",p);x.quantity=boundary::get_int(v,"quantity",p);x.source_id=boundary::get_string(v,"source_id",p);}
Status visit(CodecVisitor& c,SiegeAssetEffect& x){c.field("kind",x.kind);c.field("army_id",x.army_id);c.field("item_id",x.item_id);c.field("quantity",x.quantity);return c.field("source_id",x.source_id);}
template<class T>V encode(const std::vector<T>& rows){A a;for(auto& r:rows)a.push_back(encode(r));return V{std::move(a)};}
template<class T>void decode(const V& v,std::vector<T>& out,const StableId& p){out.clear();for(auto& r:boundary::array(v,p)){T x;decode(r,x,p+"/"+std::to_string(out.size()));out.push_back(std::move(x));}}
template<class T>Status visit(CodecVisitor& c,std::vector<T>& rows){return c.sequence("rows",rows,[](auto& c,auto& x){return visit(c,x);});}
template<class T>V encode(const std::vector<std::pair<StableId,T>>& rows){D d;for(auto& r:rows)d.emplace_back(r.first,encode(r.second));return V{std::move(d)};}
template<class T>void decode(const V& v,std::vector<std::pair<StableId,T>>& out,const StableId& p){out.clear();for(auto& r:boundary::object(v,p)){T x;decode(r.second,x,p+"/"+r.first);out.emplace_back(r.first,std::move(x));}}
template<class T>Status visit(CodecVisitor& c,std::vector<std::pair<StableId,T>>& rows){return c.sequence("pairs",rows,[](auto& c,auto& r){c.field("key",r.first);return visit(c,r.second);});}
// OccupationPublicLot仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,OccupationPublicLot& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"id","item_id","quantity"},{},p);
decode(boundary::field(v,"id",p),r.id,p+"/id");
decode(boundary::field(v,"item_id",p),r.item_id,p+"/item_id");
decode(boundary::field(v,"quantity",p),r.quantity,p+"/quantity");
}
Status emit_fields(ProjectionWriter& w,const OccupationPublicLot& r){
emit_value(w,"id",r.id);
emit_value(w,"item_id",r.item_id);
emit_value(w,"quantity",r.quantity);
w.order_fields(r.field_order);return w.status();}
V encode(const OccupationPublicLot& r){return encode_projected(r);}
Status visit(CodecVisitor& c,OccupationPublicLot& r){
c.object("id",[&](auto& c){return visit(c,r.id);});
c.object("item_id",[&](auto& c){return visit(c,r.item_id);});
c.object("quantity",[&](auto& c){return visit(c,r.quantity);});
visit_order(c,r.field_order);return c.status();}
// LotView仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,LotView& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"id","item_id","quantity","owner","reserved_by"},{},p);
decode(boundary::field(v,"id",p),r.id,p+"/id");
decode(boundary::field(v,"item_id",p),r.item_id,p+"/item_id");
decode(boundary::field(v,"quantity",p),r.quantity,p+"/quantity");
decode(boundary::field(v,"owner",p),r.owner,p+"/owner");
decode(boundary::field(v,"reserved_by",p),r.reserved_by,p+"/reserved_by");
}
Status emit_fields(ProjectionWriter& w,const LotView& r){
emit_value(w,"id",r.id);
emit_value(w,"item_id",r.item_id);
emit_value(w,"quantity",r.quantity);
emit_value(w,"owner",r.owner);
emit_value(w,"reserved_by",r.reserved_by);
w.order_fields(r.field_order);return w.status();}
V encode(const LotView& r){return encode_projected(r);}
Status visit(CodecVisitor& c,LotView& r){
c.object("id",[&](auto& c){return visit(c,r.id);});
c.object("item_id",[&](auto& c){return visit(c,r.item_id);});
c.object("quantity",[&](auto& c){return visit(c,r.quantity);});
c.object("owner",[&](auto& c){return visit(c,r.owner);});
c.object("reserved_by",[&](auto& c){return visit(c,r.reserved_by);});
visit_order(c,r.field_order);return c.status();}
// OccupationWalletManifest仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,OccupationWalletManifest& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"from_owner","to_owner","balance"},{},p);
decode(boundary::field(v,"from_owner",p),r.from_owner,p+"/from_owner");
decode(boundary::field(v,"to_owner",p),r.to_owner,p+"/to_owner");
decode(boundary::field(v,"balance",p),r.balance,p+"/balance");
}
Status emit_fields(ProjectionWriter& w,const OccupationWalletManifest& r){
emit_value(w,"from_owner",r.from_owner);
emit_value(w,"to_owner",r.to_owner);
emit_value(w,"balance",r.balance);
w.order_fields(r.field_order);return w.status();}
V encode(const OccupationWalletManifest& r){return encode_projected(r);}
Status visit(CodecVisitor& c,OccupationWalletManifest& r){
c.object("from_owner",[&](auto& c){return visit(c,r.from_owner);});
c.object("to_owner",[&](auto& c){return visit(c,r.to_owner);});
c.object("balance",[&](auto& c){return visit(c,r.balance);});
visit_order(c,r.field_order);return c.status();}
// OccupationInventoryManifest仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,OccupationInventoryManifest& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"from_owner","to_owner","public_lots","protected_lots"},{},p);
decode(boundary::field(v,"from_owner",p),r.from_owner,p+"/from_owner");
decode(boundary::field(v,"to_owner",p),r.to_owner,p+"/to_owner");
decode(boundary::field(v,"public_lots",p),r.public_lots,p+"/public_lots");
decode(boundary::field(v,"protected_lots",p),r.protected_lots,p+"/protected_lots");
}
Status emit_fields(ProjectionWriter& w,const OccupationInventoryManifest& r){
emit_value(w,"from_owner",r.from_owner);
emit_value(w,"to_owner",r.to_owner);
emit_value(w,"public_lots",r.public_lots);
emit_value(w,"protected_lots",r.protected_lots);
w.order_fields(r.field_order);return w.status();}
V encode(const OccupationInventoryManifest& r){return encode_projected(r);}
Status visit(CodecVisitor& c,OccupationInventoryManifest& r){
c.object("from_owner",[&](auto& c){return visit(c,r.from_owner);});
c.object("to_owner",[&](auto& c){return visit(c,r.to_owner);});
c.object("public_lots",[&](auto& c){return visit(c,r.public_lots);});
c.object("protected_lots",[&](auto& c){return visit(c,r.protected_lots);});
visit_order(c,r.field_order);return c.status();}
// OccupationAssetManifest仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,OccupationAssetManifest& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"wallets","inventories"},{},p);
decode(boundary::field(v,"wallets",p),r.wallets,p+"/wallets");
decode(boundary::field(v,"inventories",p),r.inventories,p+"/inventories");
}
Status emit_fields(ProjectionWriter& w,const OccupationAssetManifest& r){
emit_value(w,"wallets",r.wallets);
emit_value(w,"inventories",r.inventories);
w.order_fields(r.field_order);return w.status();}
V encode(const OccupationAssetManifest& r){return encode_projected(r);}
Status visit(CodecVisitor& c,OccupationAssetManifest& r){
c.object("wallets",[&](auto& c){return visit(c,r.wallets);});
c.object("inventories",[&](auto& c){return visit(c,r.inventories);});
visit_order(c,r.field_order);return c.status();}
// OccupationCustodyTransfer仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,OccupationCustodyTransfer& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"person_id","from_army_id","to_army_id"},{},p);
decode(boundary::field(v,"person_id",p),r.person_id,p+"/person_id");
decode(boundary::field(v,"from_army_id",p),r.from_army_id,p+"/from_army_id");
decode(boundary::field(v,"to_army_id",p),r.to_army_id,p+"/to_army_id");
}
Status emit_fields(ProjectionWriter& w,const OccupationCustodyTransfer& r){
emit_value(w,"person_id",r.person_id);
emit_value(w,"from_army_id",r.from_army_id);
emit_value(w,"to_army_id",r.to_army_id);
w.order_fields(r.field_order);return w.status();}
V encode(const OccupationCustodyTransfer& r){return encode_projected(r);}
Status visit(CodecVisitor& c,OccupationCustodyTransfer& r){
c.object("person_id",[&](auto& c){return visit(c,r.person_id);});
c.object("from_army_id",[&](auto& c){return visit(c,r.from_army_id);});
c.object("to_army_id",[&](auto& c){return visit(c,r.to_army_id);});
visit_order(c,r.field_order);return c.status();}
// MilitaryOccupationFact仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,MilitaryOccupationFact& r,const StableId& p,Integer selected){if(selected!=1&&selected!=2)boundary::fail("UnsupportedMilitaryOccupationSchema",p);r={};r.field_order=order(v);if(selected==1)boundary::fields(v,{"kind","phase","tick","siege_id","campaign_id","army_id","stronghold_id","garrison_id","from_organization_id","to_organization_id","previous_occupation_id","public_inventory_ids","public_wallet_ids"},{"assets_applied","asset_manifest","custody_transfers","disabled_office_ids","actor_changes"},p);else boundary::fields(v,{"kind","phase","tick","siege_id","campaign_id","army_id","stronghold_id","garrison_id","from_organization_id","to_organization_id","previous_occupation_id","public_inventory_ids","public_wallet_ids","actual_occupation_at","producer_kind","trigger_source"},{"assets_applied","asset_manifest","custody_transfers","disabled_office_ids","actor_changes"},p);
decode(boundary::field(v,"phase",p),r.phase,p+"/phase");
decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
decode(boundary::field(v,"siege_id",p),r.siege_id,p+"/siege_id");
decode(boundary::field(v,"campaign_id",p),r.campaign_id,p+"/campaign_id");
decode(boundary::field(v,"army_id",p),r.army_id,p+"/army_id");
decode(boundary::field(v,"stronghold_id",p),r.stronghold_id,p+"/stronghold_id");
decode(boundary::field(v,"garrison_id",p),r.garrison_id,p+"/garrison_id");
decode(boundary::field(v,"from_organization_id",p),r.from_organization_id,p+"/from_organization_id");
decode(boundary::field(v,"to_organization_id",p),r.to_organization_id,p+"/to_organization_id");
decode(boundary::field(v,"previous_occupation_id",p),r.previous_occupation_id,p+"/previous_occupation_id");
decode(boundary::field(v,"public_inventory_ids",p),r.public_inventory_ids,p+"/public_inventory_ids");
decode(boundary::field(v,"public_wallet_ids",p),r.public_wallet_ids,p+"/public_wallet_ids");
if(auto x=v.find("assets_applied")){r.assets_applied.emplace();decode(*x,*r.assets_applied,p+"/assets_applied");}
if(auto x=v.find("asset_manifest")){r.asset_manifest.emplace();decode(*x,*r.asset_manifest,p+"/asset_manifest");}
if(auto x=v.find("custody_transfers")){r.custody_transfers.emplace();decode(*x,*r.custody_transfers,p+"/custody_transfers");}
if(auto x=v.find("disabled_office_ids")){r.disabled_office_ids.emplace();decode(*x,*r.disabled_office_ids,p+"/disabled_office_ids");}
if(auto x=v.find("actor_changes")){r.actor_changes.emplace();decode(*x,*r.actor_changes,p+"/actor_changes");}
if(selected==2){try{ActorFoundationJsonVisitor reader(v);auto checked=occupation_time_codec::tail(reader,r);if(!checked.ok)boundary::fail(checked.error.original.code,p);}catch(const exact_fields::CodecFailure& e){throw boundary::DecodeFailure{e.error};}}
}
Status emit_fields(ProjectionWriter& w,const MilitaryOccupationFact& r,Integer selected){
if(selected!=1&&selected!=2)return w.reject("UnsupportedMilitaryOccupationSchema",r.id);if(selected==1&&r.exact_time)return w.reject("UnexpectedMilitaryOccupationExactSource",r.id);
w.field("kind",StableId("military_occupation"));
emit_value(w,"phase",r.phase);
emit_value(w,"tick",r.tick);
emit_value(w,"siege_id",r.siege_id);
emit_value(w,"campaign_id",r.campaign_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"stronghold_id",r.stronghold_id);
emit_value(w,"garrison_id",r.garrison_id);
emit_value(w,"from_organization_id",r.from_organization_id);
emit_value(w,"to_organization_id",r.to_organization_id);
emit_value(w,"previous_occupation_id",r.previous_occupation_id);
emit_value(w,"public_inventory_ids",r.public_inventory_ids);
emit_value(w,"public_wallet_ids",r.public_wallet_ids);
if(r.assets_applied)emit_value(w,"assets_applied",*r.assets_applied);
if(r.asset_manifest)emit_value(w,"asset_manifest",*r.asset_manifest);
if(r.custody_transfers)emit_value(w,"custody_transfers",*r.custody_transfers);
if(r.disabled_office_ids)emit_value(w,"disabled_office_ids",*r.disabled_office_ids);
if(r.actor_changes)emit_value(w,"actor_changes",*r.actor_changes);
if(selected==2){try{ActorFoundationProjectionArchive writer(w);auto checked=occupation_time_codec::tail(writer,const_cast<MilitaryOccupationFact&>(r));if(!checked.ok)return checked;}catch(const exact_fields::CodecFailure& e){return w.reject(e.error.original.code,e.error.original.path);}}
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryOccupationFact& r,Integer selected){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,r,selected);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Status visit(CodecVisitor& c,MilitaryOccupationFact& r,Integer selected){
if(selected!=1&&selected!=2)return c.reject("UnsupportedMilitaryOccupationSchema",r.id);if(selected==1&&r.exact_time)return c.reject("UnexpectedMilitaryOccupationExactSource",r.id);
c.field("id",r.id);
c.object("phase",[&](auto& c){return visit(c,r.phase);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("siege_id",[&](auto& c){return visit(c,r.siege_id);});
c.object("campaign_id",[&](auto& c){return visit(c,r.campaign_id);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("stronghold_id",[&](auto& c){return visit(c,r.stronghold_id);});
c.object("garrison_id",[&](auto& c){return visit(c,r.garrison_id);});
c.object("from_organization_id",[&](auto& c){return visit(c,r.from_organization_id);});
c.object("to_organization_id",[&](auto& c){return visit(c,r.to_organization_id);});
c.object("previous_occupation_id",[&](auto& c){return visit(c,r.previous_occupation_id);});
c.object("public_inventory_ids",[&](auto& c){return visit(c,r.public_inventory_ids);});
c.object("public_wallet_ids",[&](auto& c){return visit(c,r.public_wallet_ids);});
c.optional("assets_applied",r.assets_applied,[](auto& c,auto& x){return visit(c,x);});
c.optional("asset_manifest",r.asset_manifest,[](auto& c,auto& x){return visit(c,x);});
c.optional("custody_transfers",r.custody_transfers,[](auto& c,auto& x){return visit(c,x);});
c.optional("disabled_office_ids",r.disabled_office_ids,[](auto& c,auto& x){return visit(c,x);});
c.optional("actor_changes",r.actor_changes,[](auto& c,auto& x){return visit(c,x);});
visit_order(c,r.field_order);if(selected==2){try{return occupation_time_codec::tail(c,r);}catch(const exact_fields::CodecFailure& e){return c.reject(e.error.original.code,e.error.original.path);}}return c.status();}
// MilitaryReliefFact仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,MilitaryReliefFact& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"kind","phase","tick","siege_id","campaign_id","army_id","stronghold_id"},{},p);
decode(boundary::field(v,"phase",p),r.phase,p+"/phase");
decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
decode(boundary::field(v,"siege_id",p),r.siege_id,p+"/siege_id");
decode(boundary::field(v,"campaign_id",p),r.campaign_id,p+"/campaign_id");
decode(boundary::field(v,"army_id",p),r.army_id,p+"/army_id");
decode(boundary::field(v,"stronghold_id",p),r.stronghold_id,p+"/stronghold_id");
}
Status emit_fields(ProjectionWriter& w,const MilitaryReliefFact& r){
w.field("kind",StableId("military_relief"));
emit_value(w,"phase",r.phase);
emit_value(w,"tick",r.tick);
emit_value(w,"siege_id",r.siege_id);
emit_value(w,"campaign_id",r.campaign_id);
emit_value(w,"army_id",r.army_id);
emit_value(w,"stronghold_id",r.stronghold_id);
w.order_fields(r.field_order);return w.status();}
V encode(const MilitaryReliefFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,MilitaryReliefFact& r){
c.field("id",r.id);
c.object("phase",[&](auto& c){return visit(c,r.phase);});
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("siege_id",[&](auto& c){return visit(c,r.siege_id);});
c.object("campaign_id",[&](auto& c){return visit(c,r.campaign_id);});
c.object("army_id",[&](auto& c){return visit(c,r.army_id);});
c.object("stronghold_id",[&](auto& c){return visit(c,r.stronghold_id);});
visit_order(c,r.field_order);return c.status();}
// SiegeMaterialFact仅承载原协议字段，失败由外层丢弃候选。
void decode(const V& v,SiegeMaterialFact& r,const StableId& p){r={};r.field_order=order(v);boundary::fields(v,{"kind","tick","source_id","effect"},{},p);
decode(boundary::field(v,"tick",p),r.tick,p+"/tick");
decode(boundary::field(v,"source_id",p),r.source_id,p+"/source_id");
decode(boundary::field(v,"effect",p),r.effect,p+"/effect");
r.effect_field_order=order(boundary::field(v,"effect",p));
}
Status emit_fields(ProjectionWriter& w,const SiegeMaterialFact& r){
w.field("kind",StableId("siege_material"));
emit_value(w,"tick",r.tick);
emit_value(w,"source_id",r.source_id);
w.object("effect",r.effect_field_order,[&](auto& child){return emit_fields(child,r.effect);});
w.order_fields(r.field_order);return w.status();}
V encode(const SiegeMaterialFact& r){return encode_projected(r);}
Status visit(CodecVisitor& c,SiegeMaterialFact& r){
c.field("id",r.id);
c.object("tick",[&](auto& c){return visit(c,r.tick);});
c.object("source_id",[&](auto& c){return visit(c,r.source_id);});
c.object("effect",[&](auto& c){return visit(c,r.effect);});
c.object("effect_order",[&](auto& c){return visit_order(c,r.effect_field_order);});
visit_order(c,r.field_order);return c.status();}
// 全局事实注册仍由中央排序，领域只输出自己唯一持有的typed载荷。
bool decode_fact(const StableId& kind,const V& v,const StableId& id,MilitaryState& s,Integer selected){
if(kind=="military_occupation"){MilitaryOccupationFact x;decode(v,x,id,selected);x.id=id;s.occupation_facts.push_back(std::move(x));return true;}
if(kind=="military_relief"){MilitaryReliefFact x;decode(v,x,id);x.id=id;s.relief_facts.push_back(std::move(x));return true;}
if(kind=="siege_material"){SiegeMaterialFact x;decode(v,x,id);x.id=id;s.siege_material_facts.push_back(std::move(x));return true;}
return false;}
Status emit_fact_fields(ProjectionWriter& w,const MilitaryState& s,Integer selected){
if(selected!=1&&selected!=2)return w.reject("UnsupportedMilitaryOccupationSchema");
for(const auto& f:s.occupation_facts){auto checked=w.object(f.id,[&](auto& child){return emit_fields(child,f,selected);});if(!checked.ok)return checked;}
#define APPEND(table) for(const auto& f:s.table){auto checked=emit_value(w,f.id,f);if(!checked.ok)return checked;}
APPEND(relief_facts);APPEND(siege_material_facts);
#undef APPEND
return w.status();}
void append_state(D& facts,const MilitaryState& s,Integer selected){append_projected_fields(facts,[&](auto& w){return emit_fact_fields(w,s,selected);});}
Status visit_state(CodecVisitor& c,MilitaryState& s,Integer selected){if(selected!=1&&selected!=2)return c.reject("UnsupportedMilitaryOccupationSchema");c.sequence("occupation_facts",s.occupation_facts,[&](auto& c,auto& r){return visit(c,r,selected);});c.sequence("relief_facts",s.relief_facts,[](auto& c,auto& r){return visit(c,r);});c.sequence("siege_material_facts",s.siege_material_facts,[](auto& c,auto& r){return visit(c,r);});return c.status();}
}
