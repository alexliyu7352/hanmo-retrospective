// One finite physical sequence; Common frames/owner/reservation keep their sole codecs.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_action_resource_model.hpp"
#include "military_formation_reservation_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
namespace hanmo::resident::military_action_resource_fields {
struct Layout {bool typed;const CommonResourceCodecContext& resources;};
template<class C>Status value(C&, ReserveLots&,const Layout&);
template<class C>Status value(C&, MilitaryInventoryReservationEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryFormationReservations1&,const Layout&,Integer selected=1);
template<class C>Status value(C&, TransferLots&,const Layout&);
template<class C>Status value(C&, WalletDelta&,const Layout&);
template<class C>Status value(C&, SetReservation&,const Layout&);
template<class C>Status value(C&, CreateWallet&,const Layout&);
template<class C>Status value(C&, CreateInventory&,const Layout&);
template<class C>Status value(C&, ResourceLotFlowDraft&,const Layout&);
template<class C>Status value(C&, MilitaryFifoTransferEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryWalletDeltaEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryWalletReservationEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryWalletCreationEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryInventoryCreationEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryResourceOperationEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryActionResourceEffect1&,const Layout&);
template<class C>Status value(C&, MilitaryReturnResourceOpening1&,const Layout&);
template<class C>Status value(C& c,Owner& v,const Layout&){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return common_checkpoint::owner(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget)->Outcome<Owner>{try{
  if(ScopedCheckpointJsonDecode::current()!=&budget)return {false,{},common_failure("CheckpointJsonContextMismatch").error};
  budget.default_values<Owner>();return {true,boundary::decode_owner(raw,"military_resource_effect/owner"),{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}},emit_common_owner_fields);
}
template<class C>Status value(C& c,WalletView& v,const Layout& layout){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return common_checkpoint::wallet(c,v);
 else return c.foreign_record(v,[&](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_common_wallet_frame(raw,"military_resource_effect/wallet",layout.typed,layout.resources,budget);},[&](ProjectionWriter& w,const WalletView& row){return emit_common_wallet_frame_fields(w,row,layout.typed,layout.resources);});
}
template<class C>Status value(C& c,InventoryView& v,const Layout& layout){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return common_checkpoint::inventory(c,v);
 else return c.foreign_record(v,[&](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return decode_common_inventory_frame(raw,"military_resource_effect/inventory",layout.resources,budget);},[&](ProjectionWriter& w,const InventoryView& row){return emit_common_inventory_frame_fields(w,row,layout.typed,layout.resources);});
}
template<class C>Status reservation(C& c,std::string_view name,ReservationRef& v,const Layout& layout){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>){
  return c.object(name,[&](auto& child){static const StableId original_name="reservation";return common_checkpoint::reservation(child,v,original_name);});
 }else if(!layout.typed){
  if(!c.reading()&&(v.kind!=ReservationRef::Kind::action||!v.domain_id.empty()||!v.activity_kind.empty()))return c.reject("UnsupportedReservationKind",name);
  if(c.reading()){v.kind=ReservationRef::Kind::action;v.domain_id.clear();v.activity_kind.clear();}return c.field(name,v.id);
 }else return c.object(name,[&](auto& child){return child.foreign_record(v,[&](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget)->Outcome<ReservationRef>{try{
  if(ScopedCheckpointJsonDecode::current()!=&budget)return {false,{},common_failure("CheckpointJsonContextMismatch").error};
  if(raw.as<StableId>())return {false,{},common_failure("TypedReservationRequired").error};budget.default_values<ReservationRef>();return {true,boundary::decode_reservation_ref(raw,"military_resource_effect/reservation",layout.resources.domain_activity_reservation_refs_enabled),{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}},[&](ProjectionWriter& w,const ReservationRef& row){return emit_common_reservation_fields(w,row,true,layout.resources.domain_activity_reservation_refs_enabled);});});
}
template<class C,class T>Status object(C& c,std::string_view name,T& v,const Layout& l){return c.object(name,[&](auto& child){return value(child,v,l);});}
template<class C>Status value(C& c,ReserveLots& v,const Layout& l){
 c.field("inventory_id",v.inventory_id);c.field("item_id",v.item_id);c.field("quantity",v.quantity);
 c.sequence("owners",v.owners,[&](auto& child,auto& row){return value(child,row,l);});reservation(c,"action_id",v.action_id,l);return c.status();
}
template<class C>Status value(C& c,TransferLots& v,const Layout& l){
 c.field("source_id",v.source_id);c.field("destination_id",v.destination_id);c.field("item_id",v.item_id);c.field("quantity",v.quantity);
 c.sequence("owners",v.owners,[&](auto& child,auto& row){return value(child,row,l);});reservation(c,"reservation",v.reservation,l);
 c.optional("new_owner",v.new_owner,[&](auto& child,auto& row){return value(child,row,l);});return c.status();
}
template<class C>Status value(C& c,WalletDelta& v,const Layout&){c.field("wallet_id",v.wallet_id);c.field("delta",v.delta);c.field("require_spendable",v.require_spendable);return c.status();}
template<class C>Status value(C& c,SetReservation& v,const Layout& l){c.field("wallet_id",v.wallet_id);reservation(c,"action_id",v.action_id,l);c.field("amount",v.amount);c.field("erase",v.erase);return c.status();}
template<class C>Status value(C& c,CreateWallet& v,const Layout& l){return object(c,"wallet",v.wallet,l);}
template<class C>Status value(C& c,CreateInventory& v,const Layout& l){return object(c,"inventory",v.inventory,l);}
enum class FlowSelection {transfer,reservation};
template<class C>Status flow(C& c,ResourceLotFlowDraft& v,FlowSelection selected){
 Integer kind=Integer(v.kind);c.field("kind",kind);if(!c.status().ok)return c.status();
 if(selected==FlowSelection::transfer&&kind!=Integer(ResourceLotFlowDraft::Kind::transfer))return c.reject("MilitaryActionResourceFlowNotTransfer");
 if(selected==FlowSelection::reservation&&kind!=Integer(ResourceLotFlowDraft::Kind::reserve_whole)&&kind!=Integer(ResourceLotFlowDraft::Kind::reserve_split))return c.reject("MilitaryFormationResourceFlowNotReservation");
 if(c.reading())v.kind=static_cast<ResourceLotFlowDraft::Kind>(kind);
 c.field("source_inventory_id",v.source_inventory_id);c.field("source_lot_id",v.source_lot_id);c.field("destination_inventory_id",v.destination_inventory_id);c.field("destination_lot_id",v.destination_lot_id);c.field("item_id",v.item_id);c.field("quantity",v.quantity);return c.status();
}
template<class C>Status value(C& c,ResourceLotFlowDraft& v,const Layout&){return flow(c,v,FlowSelection::transfer);}
template<class C>Status value(C& c,MilitaryInventoryReservationEffect1& v,const Layout& l){
 object(c,"request",v.request,l);object(c,"before",v.before,l);object(c,"after",v.after,l);
 c.sequence("actual_flows",v.actual_flows,[&](auto& child,auto& row){return flow(child,row,FlowSelection::reservation);});c.field("next_instance_before",v.next_instance_before);c.field("next_instance_after",v.next_instance_after);return c.status();
}
template<class C>Status value(C& c,MilitaryFormationReservations1& v,const Layout& l,Integer selected){
 if(selected!=1&&selected!=2)return c.reject("MilitaryFormationReservationSelectedSchemaRequired");
 if(selected==1&&v.empty_inventory_opening)return c.reject("UnexpectedMilitaryFormationInventoryOpening");
 c.sequence("inventory_reservations",v.inventory_reservations,[&](auto& child,auto& row){return value(child,row,l);});object(c,"wallet_reservation",v.wallet_reservation,l);
 if(selected==2)c.optional("empty_inventory_opening",v.empty_inventory_opening,[&](auto& child,auto& row){return value(child,row,l);});
 if(!c.status().ok)return c.status();
 if(selected==2&&bool(v.empty_inventory_opening)!=v.inventory_reservations.empty())return c.reject("MilitaryFormationInventoryOpeningCoverageMismatch");
 return validate_military_formation_reservations(v);
}
template<class C>Status value(C& c,MilitaryFifoTransferEffect1& v,const Layout& l){
 object(c,"request",v.request,l);object(c,"source_before",v.source_before,l);object(c,"source_after",v.source_after,l);object(c,"destination_before",v.destination_before,l);object(c,"destination_after",v.destination_after,l);
 c.sequence("actual_flows",v.actual_flows,[&](auto& child,auto& row){return value(child,row,l);});c.field("next_instance_before",v.next_instance_before);c.field("next_instance_after",v.next_instance_after);return c.status();
}
template<class C>Status value(C& c,MilitaryWalletDeltaEffect1& v,const Layout& l){object(c,"request",v.request,l);object(c,"before",v.before,l);object(c,"after",v.after,l);return c.status();}
template<class C>Status value(C& c,MilitaryWalletReservationEffect1& v,const Layout& l){object(c,"request",v.request,l);object(c,"before",v.before,l);object(c,"after",v.after,l);return c.status();}
template<class C>Status value(C& c,MilitaryWalletCreationEffect1& v,const Layout& l){object(c,"request",v.request,l);object(c,"after",v.after,l);return c.status();}
template<class C>Status value(C& c,MilitaryInventoryCreationEffect1& v,const Layout& l){object(c,"request",v.request,l);object(c,"after",v.after,l);return c.status();}
template<std::size_t I=0,class C>bool construct(C& c,MilitaryResourceOperationEffect1& v,Integer tag){
 if constexpr(I<std::variant_size_v<MilitaryResourceOperationEffect1>){if(tag==Integer(I)){if(prepare_checkpoint_default_values<std::variant_alternative_t<I,MilitaryResourceOperationEffect1>>(c).ok)v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}return false;
}
template<class C>Status value(C& c,MilitaryResourceOperationEffect1& v,const Layout& l){
 if(v.valueless_by_exception())return c.reject("InvalidMilitaryResourceEffectVariant");Integer tag=Integer(v.index());c.field("tag",tag);if(!c.status().ok)return c.status();
 if(tag<0||tag>4)return c.reject("InvalidMilitaryResourceEffectVariant");if(c.reading())construct(c,v,tag);if(!c.status().ok)return c.status();
 return c.object("value",[&](auto& child){return std::visit([&](auto& row){return value(child,row,l);},v);});
}
template<class C>Status value(C& c,MilitaryReturnResourceOpening1& v,const Layout& l){object(c,"source_inventory",v.source_inventory,l);c.optional("destination_inventory",v.destination_inventory,[&](auto& child,auto& row){return value(child,row,l);});return c.status();}
template<class C>Status value(C& c,MilitaryActionResourceEffect1& v,const Layout& l){
 c.sequence("operations",v.operations,[&](auto& child,auto& row){return value(child,row,l);});c.optional("return_opening",v.return_opening,[&](auto& child,auto& row){return value(child,row,l);});if(!c.status().ok)return c.status();return validate_military_action_resource_effect(v);
}
}
