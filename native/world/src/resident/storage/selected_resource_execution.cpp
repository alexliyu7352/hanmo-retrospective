// 共享给Market/Freight/原收据的新source版本；旧source matcher不放宽。
#include "selected_resource_execution.hpp"
#include "common_store.hpp"
#include "common_projection.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
bool owner_value(const Owner&a,const Owner&b){return owner_equal(a,b)&&a.extra==b.extra&&a.field_order==b.field_order;}
bool property_ref(const std::optional<LotPropertiesRef>&a,const std::optional<LotPropertiesRef>&b){return a.has_value()==b.has_value()&&(!a||a->properties_id==b->properties_id);}
bool lot_metadata(const LotView&a,const LotView&b){return a.item_id==b.item_id&&a.extra==b.extra&&a.field_order==b.field_order&&a.item_instance_id==b.item_instance_id&&property_ref(a.lot_properties_ref,b.lot_properties_ref);}
bool inventory_value(const InventoryView&a,const InventoryView&b){if(a.id!=b.id||!owner_value(a.owner,b.owner)||a.capacity!=b.capacity||a.revision!=b.revision||a.field_order!=b.field_order||a.lots.size()!=b.lots.size())return false;for(std::size_t i=0;i<a.lots.size();++i)if(!same_resource_lot_value(a.lots[i],b.lots[i]))return false;return true;}
bool wallet_header(const WalletView&a,const WalletView&b){return a.id==b.id&&owner_value(a.owner,b.owner)&&a.field_order==b.field_order;}
bool reservation_value(const Reservation&a,const Reservation&b){return a.action_id==b.action_id&&a.amount==b.amount;}
bool reservation_values(const WalletView&a,const WalletView&b){if(a.reserved.size()!=b.reserved.size())return false;for(std::size_t i=0;i<a.reserved.size();++i)if(!reservation_value(a.reserved[i],b.reserved[i]))return false;return true;}
bool integer_sum(Integer a,Integer b,Integer& result){if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return false;result=a+b;return true;}
bool unchanged_allocator(const ResourceOperationExecutionDraft&e){return e.next_instance_before>=1&&e.next_instance_before==e.next_instance_after;}
bool allocated_lot_id(const StableId& id,Integer allocated){
 char text[32]={'l','o','t',':'};
 const auto digits=std::to_chars(text+4,text+sizeof(text),allocated);
 return allocated>=1&&allocated<INT64_MAX&&digits.ec==std::errc{}&&id.size()==static_cast<std::size_t>(digits.ptr-text)&&std::equal(text,digits.ptr,id.begin());
}
template<class T>Outcome<const T*> frame_failure(const char*code){return {false,nullptr,common_failure(code,{},ErrorKind::internal).error};}
}
bool same_resource_lot_value(const LotView&a,const LotView&b){return a.id==b.id&&a.quantity==b.quantity&&a.reserved_by==b.reserved_by&&owner_value(a.owner,b.owner)&&lot_metadata(a,b);}
bool same_original_wallet_frame(const WalletView&a,const WalletView&b){return wallet_header(a,b)&&a.balance==b.balance&&reservation_values(a,b);}
bool same_original_inventory_frame(const InventoryView&a,const InventoryView&b){return inventory_value(a,b);}
namespace {
bool owner_value_cold(const Owner&a,const Owner&b,const CommonResourceCodecContext&){return owner_equal(a,b)&&a.extra==b.extra&&common_owner_field_order_equivalent(a,b);}
bool lot_metadata_cold(const LotView&a,const LotView&b,const CommonResourceCodecContext&c){return a.item_id==b.item_id&&a.extra==b.extra&&common_lot_field_order_equivalent(a,b,c)&&a.item_instance_id==b.item_instance_id&&property_ref(a.lot_properties_ref,b.lot_properties_ref);}
}
bool same_resource_lot_value_cold(const LotView&a,const LotView&b,const CommonResourceCodecContext&c){return a.id==b.id&&a.quantity==b.quantity&&a.reserved_by==b.reserved_by&&owner_value_cold(a.owner,b.owner,c)&&lot_metadata_cold(a,b,c);}
bool same_original_inventory_frame_cold(const InventoryView&a,const InventoryView&b,const CommonResourceCodecContext&c){if(a.id!=b.id||!owner_value_cold(a.owner,b.owner,c)||a.capacity!=b.capacity||a.revision!=b.revision||!common_inventory_field_order_equivalent(a,b,c)||a.lots.size()!=b.lots.size())return false;for(std::size_t i=0;i<a.lots.size();++i)if(!same_resource_lot_value_cold(a.lots[i],b.lots[i],c))return false;return true;}

Outcome<SelectedUniqueItemTransferRead> read_selected_unique_item_transfer(const TransferSelectedLots&v,const ResourceOperationExecutionDraft&e){
 auto fail=[](const char*code){return Outcome<SelectedUniqueItemTransferRead>{false,{},common_failure(code,{},ErrorKind::internal).error};};
 if(v.source_id.empty()||v.destination_id.empty()||v.source_id==v.destination_id||v.selections.size()!=1||v.owners.empty()||!e.wallets.empty()||e.inventories.size()!=2||e.lot_flows.size()!=1||e.next_instance_before<1||e.next_instance_before!=e.next_instance_after)return fail("SelectedUniqueTransferShapeMismatch");
 const auto&pick=v.selections.front();if(pick.inventory_id!=v.source_id||pick.lot_id.empty()||pick.quantity!=1)return fail("SelectedUniqueTransferSelectionMismatch");
 const ResourceInventoryExecutionDraft*from=nullptr;const ResourceInventoryExecutionDraft*to=nullptr;
 for(const auto&frame:e.inventories){if(frame.inventory_id==v.source_id){if(from)return fail("SelectedUniqueTransferDuplicateFrame");from=&frame;}else if(frame.inventory_id==v.destination_id){if(to)return fail("SelectedUniqueTransferDuplicateFrame");to=&frame;}else return fail("SelectedUniqueTransferForeignFrame");}
 auto header=[](const InventoryView&a,const InventoryView&b){return a.id==b.id&&owner_value(a.owner,b.owner)&&a.capacity==b.capacity&&a.field_order==b.field_order&&a.revision>=0&&a.revision<INT64_MAX&&b.revision==a.revision+1;};
 if(!from||!to||!from->before||!to->before||from->before->id!=v.source_id||to->before->id!=v.destination_id||!header(*from->before,from->after)||!header(*to->before,to->after)||from->before->lots.size()!=from->after.lots.size()+1||to->after.lots.size()!=to->before->lots.size()+1)return fail("SelectedUniqueTransferInventoryMismatch");
 const LotView*original=nullptr;std::size_t next=0;
 for(std::size_t i=0;i<from->before->lots.size();++i){const auto&lot=from->before->lots[i];for(std::size_t j=0;j<i;++j)if(from->before->lots[j].id==lot.id)return fail("SelectedUniqueTransferDuplicateLot");if(lot.id==pick.lot_id){if(original)return fail("SelectedUniqueTransferDuplicateLot");original=&lot;}else{if(next>=from->after.lots.size()||!same_resource_lot_value(lot,from->after.lots[next++]))return fail("SelectedUniqueTransferUnselectedSourceChanged");}}
 if(!original||next!=from->after.lots.size()||original->quantity!=1||!original->item_instance_id||original->item_instance_id->empty()||original->lot_properties_ref||original->extra||original->owner.extra||original->reserved_by!=v.reservation||!std::any_of(v.owners.begin(),v.owners.end(),[&](const Owner&o){return owner_equal(o,original->owner);}))return fail("SelectedUniqueTransferOriginalItemMismatch");
 for(std::size_t i=0;i<to->before->lots.size();++i){const auto&lot=to->before->lots[i];if(lot.id==pick.lot_id||!same_resource_lot_value(lot,to->after.lots[i]))return fail("SelectedUniqueTransferUnselectedDestinationChanged");for(const auto&source:from->before->lots)if(source.id==lot.id)return fail("SelectedUniqueTransferDuplicateLot");for(std::size_t j=0;j<i;++j)if(to->before->lots[j].id==lot.id)return fail("SelectedUniqueTransferDuplicateLot");}
 const auto&delivered=to->after.lots.back();const Owner&after_owner=v.new_owner?*v.new_owner:original->owner;
 if(delivered.id!=original->id||delivered.item_id!=original->item_id||delivered.quantity!=1||!delivered.reserved_by.empty()||!owner_value(delivered.owner,after_owner)||delivered.extra!=original->extra||delivered.field_order!=original->field_order||delivered.item_instance_id!=original->item_instance_id||!property_ref(delivered.lot_properties_ref,original->lot_properties_ref))return fail("SelectedUniqueTransferDeliveredItemMismatch");
 const auto&flow=e.lot_flows.front();if(flow.kind!=ResourceLotFlowDraft::Kind::selected_transfer_whole||flow.source_inventory_id!=v.source_id||flow.destination_inventory_id!=v.destination_id||flow.source_lot_id!=pick.lot_id||flow.destination_lot_id!=pick.lot_id||flow.item_id!=original->item_id||flow.quantity!=1)return fail("SelectedUniqueTransferFlowMismatch");
 return {true,{from,to,&flow,original,&delivered},{}};
}
Status validate_original_fifo_lot_transfer(const TransferLots&v,const OriginalFifoLotTransferEvidenceView&e){
 auto fail=[](const char*code){return common_failure(code,{},ErrorKind::internal);};
 if(v.source_id.empty()||v.destination_id.empty()||v.source_id==v.destination_id||v.item_id.empty()||v.quantity<0||e.next_instance_before<1||e.actual_flows.size()>static_cast<std::size_t>(INT64_MAX))return fail("OriginalFifoTransferShapeMismatch");
 const Integer created=static_cast<Integer>(e.actual_flows.size());
 auto header=[](const InventoryView&a,const InventoryView&b,Integer revisions){
  return a.id==b.id&&owner_value(a.owner,b.owner)&&a.capacity==b.capacity&&a.field_order==b.field_order&&a.revision>=0&&revisions>=0&&a.revision<=INT64_MAX-revisions&&b.revision==a.revision+revisions;
 };
 if(e.source_before.id!=v.source_id||e.destination_before.id!=v.destination_id||!header(e.source_before,e.source_after,1)||!header(e.destination_before,e.destination_after,created)||created>INT64_MAX-e.next_instance_before||e.next_instance_after!=e.next_instance_before+created)return fail("OriginalFifoTransferInventoryMismatch");
 if(e.destination_after.lots.size()<e.destination_before.lots.size()||e.destination_after.lots.size()-e.destination_before.lots.size()!=e.actual_flows.size())return fail("OriginalFifoTransferDestinationSizeMismatch");
 for(std::size_t i=0;i<e.source_before.lots.size();++i){
  const auto&lot=e.source_before.lots[i];
  if(lot.id.empty()||lot.quantity<=0)return fail("OriginalFifoTransferInvalidOriginalLot");
  for(std::size_t j=0;j<i;++j)if(e.source_before.lots[j].id==lot.id)return fail("OriginalFifoTransferDuplicateLot");
 }
 for(std::size_t i=0;i<e.destination_before.lots.size();++i){
  const auto&lot=e.destination_before.lots[i];
  if(lot.id.empty()||lot.quantity<=0||!same_resource_lot_value(lot,e.destination_after.lots[i]))return fail("OriginalFifoTransferUnselectedDestinationChanged");
  for(const auto&source:e.source_before.lots)if(source.id==lot.id)return fail("OriginalFifoTransferDuplicateLot");
  for(std::size_t j=0;j<i;++j)if(e.destination_before.lots[j].id==lot.id)return fail("OriginalFifoTransferDuplicateLot");
 }
 // Consume only the immutable actual flow sequence. There is no copied
 // inventory, writer replay, inferred flow, or temporary ownership ledger.
 auto available=count_inventory_frame_items(e.source_before,v.item_id,v.owners,v.reservation);
 if(!available.ok)return {false,{},available.error};
 if(available.value<v.quantity)return fail("OriginalFifoTransferOriginalQuantityMismatch");
 Integer remaining=v.quantity;std::size_t flow_index=0;
 for(const auto&owner:v.owners){
  if(!remaining)break;
  for(const auto&lot:e.source_before.lots){
   if(lot.item_id!=v.item_id||!owner_equal(lot.owner,owner)||lot.reserved_by!=v.reservation)continue;
   bool already_consumed=false;
   for(std::size_t prior=0;prior<flow_index;++prior)if(e.actual_flows[prior].source_lot_id==lot.id){already_consumed=true;break;}
   if(already_consumed)continue; // A repeated owner cannot consume an exhausted lot twice.
   const Integer used=std::min(remaining,lot.quantity);
   if(lot.item_instance_id||lot.lot_properties_ref)return fail("OriginalFifoTransferSelectedOperationRequired");
   if(flow_index>=e.actual_flows.size())return fail("OriginalFifoTransferFlowMissing");
   const auto&flow=e.actual_flows[flow_index];
   const auto&delivered=e.destination_after.lots[e.destination_before.lots.size()+flow_index];
   const Integer allocated=e.next_instance_before+static_cast<Integer>(flow_index);
   if(!allocated_lot_id(delivered.id,allocated))return fail("OriginalFifoTransferAllocatorMismatch");
   if(flow.kind!=ResourceLotFlowDraft::Kind::transfer||flow.source_inventory_id!=v.source_id||flow.source_lot_id!=lot.id||flow.destination_inventory_id!=v.destination_id||flow.destination_lot_id!=delivered.id||flow.item_id!=v.item_id||flow.quantity!=used)return fail("OriginalFifoTransferFlowMismatch");
   const Owner&delivered_owner=v.new_owner?*v.new_owner:owner;
   if(delivered.item_id!=v.item_id||delivered.quantity!=used||delivered.reserved_by!=ReservationRef{}||!owner_value(delivered.owner,delivered_owner)||delivered.extra||!delivered.field_order.empty()||delivered.item_instance_id||delivered.lot_properties_ref)return fail("OriginalFifoTransferDeliveredLotMismatch");
   for(const auto&old:e.source_before.lots)if(old.id==delivered.id)return fail("OriginalFifoTransferDuplicateLot");
   for(const auto&old:e.destination_before.lots)if(old.id==delivered.id)return fail("OriginalFifoTransferDuplicateLot");
   remaining-=used;++flow_index;if(!remaining)break;
  }
 }
 if(remaining||flow_index!=e.actual_flows.size())return fail("OriginalFifoTransferQuantityMismatch");
 std::size_t after_index=0;
 for(const auto&lot:e.source_before.lots){
  Integer used=0;
  for(const auto&flow:e.actual_flows)if(flow.source_lot_id==lot.id){if(used)return fail("OriginalFifoTransferDuplicateFlow");used=flow.quantity;}
  if(used<0||used>lot.quantity)return fail("OriginalFifoTransferSourceQuantityMismatch");
  if(used==lot.quantity)continue;
  if(after_index>=e.source_after.lots.size())return fail("OriginalFifoTransferSourceLotMissing");
  const auto&after=e.source_after.lots[after_index++];
  if(after.id!=lot.id||after.item_id!=lot.item_id||after.quantity!=lot.quantity-used||after.reserved_by!=lot.reserved_by||!owner_value(after.owner,lot.owner)||after.extra!=lot.extra||after.field_order!=lot.field_order||after.item_instance_id!=lot.item_instance_id||!property_ref(after.lot_properties_ref,lot.lot_properties_ref))return fail("OriginalFifoTransferUnselectedSourceChanged");
 }
 if(after_index!=e.source_after.lots.size())return fail("OriginalFifoTransferForeignSourceLot");
 return common_success();
}
Outcome<OriginalFifoLotTransferRead> read_original_fifo_lot_transfer(const TransferLots&v,const ResourceOperationExecutionDraft&e){
 auto fail=[](const char*code){return Outcome<OriginalFifoLotTransferRead>{false,{},common_failure(code,{},ErrorKind::internal).error};};
 if(v.source_id.empty()||v.destination_id.empty()||v.source_id==v.destination_id||v.item_id.empty()||v.quantity<0||!e.wallets.empty()||e.inventories.size()!=2||e.next_instance_before<1||e.lot_flows.size()>static_cast<std::size_t>(INT64_MAX))return fail("OriginalFifoTransferShapeMismatch");
 const ResourceInventoryExecutionDraft*from=nullptr;const ResourceInventoryExecutionDraft*to=nullptr;
 for(const auto&frame:e.inventories){
  if(frame.inventory_id==v.source_id){if(from)return fail("OriginalFifoTransferDuplicateFrame");from=&frame;}
  else if(frame.inventory_id==v.destination_id){if(to)return fail("OriginalFifoTransferDuplicateFrame");to=&frame;}
  else return fail("OriginalFifoTransferForeignFrame");
 }
 if(!from||!to||!from->before||!to->before)return fail("OriginalFifoTransferInventoryMismatch");
 auto status=validate_original_fifo_lot_transfer(v,{*from->before,from->after,*to->before,to->after,e.lot_flows,e.next_instance_before,e.next_instance_after});
 if(!status.ok)return {false,{},status.error};
 return {true,{from,to,&e.lot_flows},{}};
}
Status validate_original_fifo_lot_reservation(const ReserveLots&v,const OriginalFifoLotReservationEvidenceView&e){
 auto fail=[](const char* code){return common_failure(code,{},ErrorKind::internal);};
 if(v.inventory_id.empty()||v.item_id.empty()||v.action_id.empty()||v.quantity<0||e.next_instance_before<1||e.actual_flows.size()>static_cast<std::size_t>(INT64_MAX))return fail("OriginalFifoReservationShapeMismatch");
 Integer splits=0;
 for(const auto& flow:e.actual_flows){
  if(flow.kind==ResourceLotFlowDraft::Kind::reserve_split)++splits;
  else if(flow.kind!=ResourceLotFlowDraft::Kind::reserve_whole)return fail("OriginalFifoReservationFlowKindMismatch");
 }
 if(splits==INT64_MAX||e.before.id!=v.inventory_id||e.after.id!=v.inventory_id||!owner_value(e.before.owner,e.after.owner)||e.before.capacity!=e.after.capacity||e.before.field_order!=e.after.field_order||e.before.revision<0||e.before.revision>INT64_MAX-1-splits||e.after.revision!=e.before.revision+1+splits||splits>INT64_MAX-e.next_instance_before||e.next_instance_after!=e.next_instance_before+splits)return fail("OriginalFifoReservationInventoryMismatch");
 if(e.after.lots.size()<e.before.lots.size()||e.after.lots.size()-e.before.lots.size()!=static_cast<std::size_t>(splits))return fail("OriginalFifoReservationSizeMismatch");
 for(std::size_t i=0;i<e.before.lots.size();++i){
  const auto& lot=e.before.lots[i];
  if(lot.id.empty()||lot.quantity<=0)return fail("OriginalFifoReservationInvalidOriginalLot");
  for(std::size_t j=0;j<i;++j)if(e.before.lots[j].id==lot.id)return fail("OriginalFifoReservationDuplicateLot");
 }
 // Availability is the sole Common count rule over this actual immutable frame,
 // not a domain-local sum or today's inventory.
 auto available=count_inventory_frame_items(e.before,v.item_id,v.owners,{});
 if(!available.ok)return {false,{},available.error};
 if(available.value<v.quantity)return fail("OriginalFifoReservationOriginalQuantityMismatch");
 Integer remaining=v.quantity,created=0;std::size_t flow_index=0;
 for(const auto& owner:v.owners){
  if(!remaining)break;
  for(const auto& lot:e.before.lots){
   if(lot.item_id!=v.item_id||!owner_equal(lot.owner,owner)||!lot.reserved_by.empty())continue;
   bool already_reserved=false;
   for(std::size_t i=0;i<flow_index;++i)if(e.actual_flows[i].source_lot_id==lot.id){already_reserved=true;break;}
   if(already_reserved)continue;
   const Integer used=std::min(remaining,lot.quantity);
   if(lot.item_instance_id||lot.lot_properties_ref)return fail("OriginalFifoReservationSelectedOperationRequired");
   if(flow_index>=e.actual_flows.size())return fail("OriginalFifoReservationFlowMissing");
   const auto& flow=e.actual_flows[flow_index];
   const bool whole=used==lot.quantity;
   if(flow.kind!=(whole?ResourceLotFlowDraft::Kind::reserve_whole:ResourceLotFlowDraft::Kind::reserve_split)||flow.source_inventory_id!=v.inventory_id||flow.destination_inventory_id!=v.inventory_id||flow.source_lot_id!=lot.id||flow.item_id!=v.item_id||flow.quantity!=used)return fail("OriginalFifoReservationFlowMismatch");
   if(whole){
    if(flow.destination_lot_id!=lot.id)return fail("OriginalFifoReservationWholeIdentityMismatch");
   }else{
    if(created>=splits)return fail("OriginalFifoReservationSplitMissing");
    const auto& added=e.after.lots[e.before.lots.size()+static_cast<std::size_t>(created)];
    if(!allocated_lot_id(added.id,e.next_instance_before+created)||flow.destination_lot_id!=added.id)return fail("OriginalFifoReservationAllocatorMismatch");
    if(added.item_id!=v.item_id||added.quantity!=used||added.reserved_by!=v.action_id||!owner_value(added.owner,owner)||added.extra||!added.field_order.empty()||added.item_instance_id||added.lot_properties_ref)return fail("OriginalFifoReservationSplitMismatch");
    for(const auto& old:e.before.lots)if(old.id==added.id)return fail("OriginalFifoReservationDuplicateLot");
    ++created;
   }
   remaining-=used;++flow_index;if(!remaining)break;
  }
 }
 if(remaining||flow_index!=e.actual_flows.size()||created!=splits)return fail("OriginalFifoReservationQuantityMismatch");
 // Reservation never erases or reorders an original row. Every untouched field
 // is compared through the same original value semantics as the other proofs.
 for(std::size_t i=0;i<e.before.lots.size();++i){
  const auto& before=e.before.lots[i];const auto& after=e.after.lots[i];
  const ResourceLotFlowDraft* selected=nullptr;
  for(const auto& flow:e.actual_flows)if(flow.source_lot_id==before.id){if(selected)return fail("OriginalFifoReservationDuplicateFlow");selected=&flow;}
  if(!selected){if(!same_resource_lot_value(before,after))return fail("OriginalFifoReservationUnselectedChanged");continue;}
  const bool whole=selected->kind==ResourceLotFlowDraft::Kind::reserve_whole;
  if(after.id!=before.id||after.item_id!=before.item_id||after.quantity!=(whole?before.quantity:before.quantity-selected->quantity)||after.reserved_by!=(whole?v.action_id:before.reserved_by)||!owner_value(after.owner,before.owner)||after.extra!=before.extra||after.field_order!=before.field_order||after.item_instance_id!=before.item_instance_id||!property_ref(after.lot_properties_ref,before.lot_properties_ref))return fail("OriginalFifoReservationOriginalLotMismatch");
 }
 return common_success();
}
Outcome<OriginalFifoLotReservationRead> read_original_fifo_lot_reservation(const ReserveLots&v,const ResourceOperationExecutionDraft&e){
 auto fail=[](const char* code){return Outcome<OriginalFifoLotReservationRead>{false,{},common_failure(code,{},ErrorKind::internal).error};};
 if(!e.wallets.empty()||e.inventories.size()!=1)return fail("OriginalFifoReservationShapeMismatch");
 const auto& frame=e.inventories.front();
 if(frame.inventory_id!=v.inventory_id||!frame.before)return fail("OriginalFifoReservationFrameMismatch");
 auto status=validate_original_fifo_lot_reservation(v,{*frame.before,frame.after,e.lot_flows,e.next_instance_before,e.next_instance_after});
 if(!status.ok)return {false,{},status.error};
 return {true,{&frame,&e.lot_flows},{}};
}
Status validate_original_wallet_delta(const WalletDelta&v,const WalletView&before,const WalletView&after){
 if(v.wallet_id.empty())return common_failure("OriginalWalletDeltaShapeMismatch",{},ErrorKind::internal);
 if(before.id!=v.wallet_id||!wallet_header(before,after)||!reservation_values(before,after))return common_failure("OriginalWalletDeltaFrameMismatch",{},ErrorKind::internal);
 Integer amount=0;if(!integer_sum(before.balance,v.delta,amount)||after.balance!=amount)return common_failure("OriginalWalletDeltaAmountMismatch",{},ErrorKind::internal);
 if(v.require_spendable&&v.delta<0){
  auto spendable=read_wallet_spendable(before);
  if(!spendable.ok)return {false,{},spendable.error};
  Integer remaining=0;
  if(!integer_sum(spendable.value,v.delta,remaining)||remaining<0)return common_failure("OriginalWalletDeltaSpendableMismatch",{},ErrorKind::internal);
 }
 return common_success();
}
Status validate_original_wallet_reservation_change(const SetReservation&v,const WalletView&before,const WalletView&after){
 auto fail=[](const char*code){return common_failure(code,{},ErrorKind::internal);};
 if(v.wallet_id.empty()||(!v.erase&&v.amount<0))return fail("OriginalWalletReservationShapeMismatch");
 if(before.id!=v.wallet_id||!wallet_header(before,after)||before.balance!=after.balance)return fail("OriginalWalletReservationFrameMismatch");
 bool found=false;std::size_t next=0;
 for(const auto&hold:before.reserved){
  const bool selected=!found&&hold.action_id==v.action_id;
  if(selected){found=true;if(v.erase)continue;}
  if(next>=after.reserved.size())return fail("OriginalWalletReservationMissing");
  const auto&actual=after.reserved[next++];
  if(selected){if(actual.action_id!=hold.action_id||actual.amount!=v.amount)return fail("OriginalWalletReservationAmountMismatch");}
  else if(!reservation_value(hold,actual))return fail("OriginalWalletReservationUnselectedChanged");
 }
 if(!found&&!v.erase){
  if(next>=after.reserved.size()||after.reserved[next].action_id!=v.action_id||after.reserved[next].amount!=v.amount)return fail("OriginalWalletReservationAppendMismatch");
  ++next;
 }
 if(next!=after.reserved.size())return fail("OriginalWalletReservationForeignHold");
 return common_success();
}
Status validate_original_wallet_creation(const CreateWallet&v,const WalletView&after){
 if(v.wallet.id.empty())return common_failure("OriginalWalletCreationShapeMismatch",{},ErrorKind::internal);
 if(!wallet_header(v.wallet,after)||v.wallet.balance!=after.balance||!reservation_values(v.wallet,after))return common_failure("OriginalWalletCreationFrameMismatch",{},ErrorKind::internal);
 return common_success();
}
Status validate_original_inventory_creation(const CreateInventory&v,const InventoryView&after){
 if(v.inventory.id.empty())return common_failure("OriginalInventoryCreationShapeMismatch",{},ErrorKind::internal);
 if(!inventory_value(v.inventory,after))return common_failure("OriginalInventoryCreationFrameMismatch",{},ErrorKind::internal);
 return common_success();
}
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_delta(const WalletDelta&v,const ResourceOperationExecutionDraft&e){
 if(v.wallet_id.empty()||e.wallets.size()!=1||!e.inventories.empty()||!e.lot_flows.empty()||!unchanged_allocator(e))return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletDeltaShapeMismatch");
 const auto&frame=e.wallets.front();
 if(frame.wallet_id!=v.wallet_id||!frame.before)return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletDeltaFrameMismatch");
 auto status=validate_original_wallet_delta(v,*frame.before,frame.after);if(!status.ok)return {false,nullptr,status.error};return {true,&frame,{}};
}
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_reservation_change(const SetReservation&v,const ResourceOperationExecutionDraft&e){
 if(v.wallet_id.empty()||(!v.erase&&v.amount<0)||e.wallets.size()!=1||!e.inventories.empty()||!e.lot_flows.empty()||!unchanged_allocator(e))return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletReservationShapeMismatch");
 const auto&frame=e.wallets.front();
 if(frame.wallet_id!=v.wallet_id||!frame.before)return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletReservationFrameMismatch");
 auto status=validate_original_wallet_reservation_change(v,*frame.before,frame.after);if(!status.ok)return {false,nullptr,status.error};return {true,&frame,{}};
}
Outcome<const ResourceWalletExecutionDraft*> read_original_wallet_creation(const CreateWallet&v,const ResourceOperationExecutionDraft&e){
 if(v.wallet.id.empty()||e.wallets.size()!=1||!e.inventories.empty()||!e.lot_flows.empty()||!unchanged_allocator(e))return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletCreationShapeMismatch");
 const auto&frame=e.wallets.front();
 if(frame.wallet_id!=v.wallet.id||frame.before)return frame_failure<ResourceWalletExecutionDraft>("OriginalWalletCreationFrameMismatch");
 auto status=validate_original_wallet_creation(v,frame.after);if(!status.ok)return {false,nullptr,status.error};return {true,&frame,{}};
}
Outcome<const ResourceInventoryExecutionDraft*> read_original_inventory_creation(const CreateInventory&v,const ResourceOperationExecutionDraft&e){
 if(v.inventory.id.empty()||e.inventories.size()!=1||!e.wallets.empty()||!e.lot_flows.empty()||!unchanged_allocator(e))return frame_failure<ResourceInventoryExecutionDraft>("OriginalInventoryCreationShapeMismatch");
 const auto&frame=e.inventories.front();
 if(frame.inventory_id!=v.inventory.id||frame.before)return frame_failure<ResourceInventoryExecutionDraft>("OriginalInventoryCreationFrameMismatch");
 auto status=validate_original_inventory_creation(v,frame.after);if(!status.ok)return {false,nullptr,status.error};return {true,&frame,{}};
}
bool is_selected_resource_operation(const ResourceOperation& v){return std::holds_alternative<ReserveSelectedLots>(v)||std::holds_alternative<TransferSelectedLots>(v)||std::holds_alternative<ConsumeSelectedLots>(v);}
namespace {
template<class T,class Evidence>
Status validate_selected_evidence(const T& value,const Evidence& execution){
 if constexpr(!std::is_same_v<T,ReserveSelectedLots>&&!std::is_same_v<T,TransferSelectedLots>&&!std::is_same_v<T,ConsumeSelectedLots>&&!std::is_same_v<T,SelectedTransferParametersBorrow>&&!std::is_same_v<T,SelectedConsumeParametersBorrow>)return common_failure("SelectedResourceOperationRequired");
 else{
  constexpr bool reserve=std::is_same_v<T,ReserveSelectedLots>,transfer=std::is_same_v<T,TransferSelectedLots>||std::is_same_v<T,SelectedTransferParametersBorrow>;
  const CommonResourceCodecContext* cold=nullptr;
  if constexpr(std::is_same_v<Evidence,SelectedResourceEvidenceBorrow>)cold=execution.cold_resources;
  auto same_owner=[&](const Owner&a,const Owner&b){return cold?owner_value_cold(a,b,*cold):owner_value(a,b);};
  auto same_metadata=[&](const LotView&a,const LotView&b){return cold?lot_metadata_cold(a,b,*cold):lot_metadata(a,b);};
  auto same_lot=[&](const LotView&a,const LotView&b){return cold?same_resource_lot_value_cold(a,b,*cold):same_resource_lot_value(a,b);};
  auto fail=[](const char* code){return common_failure(code);};
  const StableId& source_id=[&]()->const StableId&{if constexpr(transfer)return value.source_id;else return value.inventory_id;}();
  if(source_id.empty()||value.selections.empty()||value.owners.empty()||value.selections.size()>static_cast<std::size_t>(INT64_MAX)||execution.lot_flows.size()!=value.selections.size()||!execution.wallets.empty()||execution.inventories.size()!=(transfer?2u:1u)||execution.next_instance_before<1)return fail("SelectedResourceCaptureShape");
  if constexpr(reserve){if(value.reservation.empty())return fail("SelectedResourceCaptureReservation");}
  else if(value.reservation.empty()&&value.reservation!=ReservationRef{})return fail("SelectedResourceCaptureReservation");
  for(std::size_t i=0;i<value.owners.size();++i)for(std::size_t j=0;j<i;++j)if(owner_equal(value.owners[i],value.owners[j]))return fail("SelectedResourceCaptureDuplicateOwner");
  using Frame=typename std::remove_cvref_t<decltype(execution.inventories)>::value_type;
  const Frame* source_frame=nullptr;const Frame* destination_frame=nullptr;
  for(const auto& row:execution.inventories){
   if(row.inventory_id==source_id){if(source_frame)return fail("SelectedResourceCaptureDuplicateFrame");source_frame=&row;}
   if constexpr(transfer)if(row.inventory_id==value.destination_id){if(destination_frame)return fail("SelectedResourceCaptureDuplicateFrame");destination_frame=&row;}
  }
  if(!source_frame||!source_frame->before||source_frame->before->id!=source_id)return fail("SelectedResourceCaptureSourceMissing");
  if constexpr(transfer)if(value.destination_id.empty()||source_id==value.destination_id||!destination_frame||!destination_frame->before||destination_frame->before->id!=value.destination_id)return fail("SelectedResourceCaptureDestinationMissing");
  const auto& source=*source_frame->before;const auto& source_after=source_frame->after;
  auto same_header=[&](const InventoryView& before,const InventoryView& after){return before.id==after.id&&same_owner(before.owner,after.owner)&&before.capacity==after.capacity&&(cold?common_inventory_field_order_equivalent(before,after,*cold):before.field_order==after.field_order);};
  if(!same_header(source,source_after))return fail("SelectedResourceCaptureSourceMismatch");
  for(std::size_t i=0;i<source.lots.size();++i){
   if(source.lots[i].id.empty()||source.lots[i].quantity<=0)return fail("SelectedResourceCaptureInvalidOriginalLot");
   for(std::size_t j=0;j<i;++j)if(source.lots[i].id==source.lots[j].id)return fail("SelectedResourceDuplicateLot");
  }
  if constexpr(transfer){
   const auto& destination=*destination_frame->before;const auto& after=destination_frame->after;
   if(!same_header(destination,after)||after.lots.size()<destination.lots.size()||after.lots.size()-destination.lots.size()!=value.selections.size())return fail("SelectedResourceCaptureDestinationMismatch");
   for(std::size_t i=0;i<destination.lots.size();++i){
    const auto& lot=destination.lots[i];
    if(lot.id.empty()||lot.quantity<=0||!same_lot(lot,after.lots[i]))return fail("SelectedResourceCaptureDestinationMismatch");
    for(const auto& original:source.lots)if(original.id==lot.id)return fail("SelectedResourceDuplicateLot");
    for(std::size_t j=0;j<i;++j)if(destination.lots[j].id==lot.id)return fail("SelectedResourceDuplicateLot");
   }
  }
  // Read only the retained before/after rows and real ordered flow. No shadow
  // inventory, copied Owner/ReservationRef, synthesized flow, or writer replay.
  Integer splits=0;
  for(std::size_t index=0;index<value.selections.size();++index){
   const auto& pick=value.selections[index];
   if(pick.inventory_id!=source_id||pick.lot_id.empty()||pick.quantity<=0)return fail("SelectedResourceCaptureSelection");
   for(std::size_t prior=0;prior<index;++prior)if(value.selections[prior].lot_id==pick.lot_id)return fail("SelectedResourceCaptureSelection");
   const auto at=std::find_if(source.lots.begin(),source.lots.end(),[&](const LotView& lot){return lot.id==pick.lot_id;});
   if(at==source.lots.end()||at->quantity<pick.quantity||at->extra||at->owner.extra||!std::any_of(value.owners.begin(),value.owners.end(),[&](const Owner& owner){return owner_equal(owner,at->owner);})||(at->item_instance_id&&at->lot_properties_ref)||(at->item_instance_id&&(at->item_instance_id->empty()||at->quantity!=1||pick.quantity!=1)))return fail("SelectedResourceCaptureSelection");
   const auto& before=*at;const bool whole=pick.quantity==before.quantity;
   if constexpr(reserve){if(before.reserved_by!=ReservationRef{})return fail("SelectedResourceCaptureReservation");}
   else if(before.reserved_by!=value.reservation)return fail("SelectedResourceCaptureReservation");
   const auto& flow=execution.lot_flows[index];
   if(flow.source_inventory_id!=source_id||flow.source_lot_id!=before.id||flow.item_id!=before.item_id||flow.quantity!=pick.quantity)return fail("SelectedResourceCaptureFlowMismatch");
   if constexpr(reserve||transfer){
    const LotView* appended=nullptr;
    if constexpr(transfer){
     if(flow.destination_inventory_id!=value.destination_id||flow.kind!=(whole?ResourceLotFlowDraft::Kind::selected_transfer_whole:ResourceLotFlowDraft::Kind::selected_transfer_split))return fail("SelectedResourceCaptureFlowMismatch");
     appended=&destination_frame->after.lots[destination_frame->before->lots.size()+index];
    }else{
     if(flow.destination_inventory_id!=source_id||flow.kind!=(whole?ResourceLotFlowDraft::Kind::selected_reserve_whole:ResourceLotFlowDraft::Kind::selected_reserve_split))return fail("SelectedResourceCaptureFlowMismatch");
     if(!whole){
      if(source_after.lots.size()<source.lots.size()||static_cast<std::size_t>(splits)>=source_after.lots.size()-source.lots.size())return fail("SelectedResourceCaptureSourceMismatch");
      appended=&source_after.lots[source.lots.size()+static_cast<std::size_t>(splits)];
     }
    }
    if(whole){if(flow.destination_lot_id!=before.id)return fail("SelectedResourceCaptureFlowMismatch");}
    else{
     if(splits>=INT64_MAX-execution.next_instance_before||!allocated_lot_id(flow.destination_lot_id,execution.next_instance_before+splits))return fail("SelectedResourceCaptureAllocator");
     for(const auto& old:source.lots)if(old.id==flow.destination_lot_id)return fail("SelectedResourceCaptureAllocator");
     if constexpr(transfer)for(const auto& old:destination_frame->before->lots)if(old.id==flow.destination_lot_id)return fail("SelectedResourceCaptureAllocator");
     ++splits;
    }
    if(appended){
     if(appended->id!=flow.destination_lot_id||appended->quantity!=pick.quantity||!same_metadata(before,*appended))return fail("SelectedResourceCaptureFlowMismatch");
     if constexpr(transfer){if(appended->reserved_by!=ReservationRef{}||!same_owner(appended->owner,value.new_owner?*value.new_owner:before.owner))return fail("SelectedResourceCaptureDestinationMismatch");}
     else if(appended->reserved_by!=value.reservation||!same_owner(appended->owner,before.owner))return fail("SelectedResourceCaptureSourceMismatch");
    }
   }else if(flow.kind!=ResourceLotFlowDraft::Kind::selected_consume||!flow.destination_inventory_id.empty()||!flow.destination_lot_id.empty())return fail("SelectedResourceCaptureFlowMismatch");
  }
  // Compare every original row in its actual original order. Whole transfer or
  // consume removes only its selected row; reserve keeps all original rows and
  // appends exactly its proven partial splits in selection order.
  std::size_t after_index=0;
  for(const auto& before:source.lots){
   const CargoSelection* selected=nullptr;
   for(const auto& pick:value.selections)if(pick.lot_id==before.id){selected=&pick;break;}
   const bool whole=selected&&selected->quantity==before.quantity;
   if constexpr(!reserve)if(whole)continue;
   if(after_index>=source_after.lots.size())return fail("SelectedResourceCaptureSourceMismatch");
   const auto& after=source_after.lots[after_index++];
   if(!selected){if(!same_lot(before,after))return fail("SelectedResourceCaptureSourceMismatch");continue;}
   const Integer quantity=(reserve&&whole)?before.quantity:before.quantity-selected->quantity;
   const ReservationRef& reservation=(reserve&&whole)?value.reservation:before.reserved_by;
   if(after.id!=before.id||after.quantity!=quantity||after.reserved_by!=reservation||!same_owner(before.owner,after.owner)||!same_metadata(before,after))return fail("SelectedResourceCaptureSourceMismatch");
  }
  if constexpr(reserve){if(source_after.lots.size()<after_index||source_after.lots.size()-after_index!=static_cast<std::size_t>(splits))return fail("SelectedResourceCaptureSourceMismatch");}
  else if(after_index!=source_after.lots.size())return fail("SelectedResourceCaptureSourceMismatch");
  if(splits==INT64_MAX)return fail("SelectedResourceCaptureRevision");
  const Integer source_revisions=reserve?1+splits:1;
  if(source.revision<0||source.revision>INT64_MAX-source_revisions||source_after.revision!=source.revision+source_revisions)return fail("SelectedResourceCaptureRevision");
  if constexpr(transfer){const auto count=static_cast<Integer>(value.selections.size());if(destination_frame->before->revision<0||destination_frame->before->revision>INT64_MAX-count||destination_frame->after.revision!=destination_frame->before->revision+count)return fail("SelectedResourceCaptureRevision");}
  if(execution.next_instance_after!=execution.next_instance_before+splits)return fail("SelectedResourceCaptureAllocator");
  return common_success();
 }
}
}
// 热执行与冷借用都进入原唯一正文，错误优先级与全未动集核验保持不变。
Status validate_selected_resource_evidence(const SelectedTransferParametersBorrow& value,const SelectedResourceEvidenceBorrow& execution){return validate_selected_evidence(value,execution);}
Status validate_selected_resource_evidence(const SelectedConsumeParametersBorrow& value,const SelectedResourceEvidenceBorrow& execution){return validate_selected_evidence(value,execution);}
Status validate_selected_resource_evidence(const ReserveSelectedLots& value,const SelectedResourceExecutionEvidenceView& execution){return validate_selected_evidence(value,execution);}
Status validate_selected_resource_evidence(const TransferSelectedLots& value,const SelectedResourceExecutionEvidenceView& execution){return validate_selected_evidence(value,execution);}
Status validate_selected_resource_evidence(const ConsumeSelectedLots& value,const SelectedResourceExecutionEvidenceView& execution){return validate_selected_evidence(value,execution);}
Status validate_selected_resource_evidence(const ResourceOperation& operation,const SelectedResourceExecutionEvidenceView& execution){return std::visit([&](const auto& value){return validate_selected_evidence(value,execution);},operation);}
Status validate_selected_resource_execution(const ResourceOperation& operation,const ResourceOperationExecutionDraft& execution){
 return validate_selected_resource_evidence(operation,{execution.wallets,execution.inventories,execution.lot_flows,execution.next_instance_before,execution.next_instance_after});
}
}
