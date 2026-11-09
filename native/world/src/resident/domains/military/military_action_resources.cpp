#include "military_action_resources.hpp"
#include "military_action_time_records.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <algorithm>
#include <type_traits>
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<MilitaryReturnResourceOpening1>&&std::is_nothrow_swappable_v<MilitaryReturnResourceOpening1>,"finite opening transfer/swap must preserve one charged payload");
static_assert(std::is_nothrow_move_constructible_v<MilitaryFifoTransferEffect1>,"finite effect transfer must not copy a second payload");
static_assert(std::is_nothrow_move_constructible_v<MilitaryWalletDeltaEffect1>,"finite effect transfer must not copy a second payload");
static_assert(std::is_nothrow_move_constructible_v<MilitaryWalletReservationEffect1>,"finite effect transfer must not copy a second payload");
static_assert(std::is_nothrow_move_constructible_v<MilitaryWalletCreationEffect1>,"finite effect transfer must not copy a second payload");
static_assert(std::is_nothrow_move_constructible_v<MilitaryInventoryCreationEffect1>,"finite effect transfer must not copy a second payload");

CheckpointMemoryCharge reserve(const MilitaryActionResourceEffectBuildView& v){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryActionResourceEffect1& v){return CheckpointMemoryCharge(0);}
MilitaryFifoTransferEffect1 copy(const MilitaryFifoTransferEffectBuildView& v){return {v.request,v.source_before,v.source_after,v.destination_before,v.destination_after,v.actual_flows,v.next_instance_before,v.next_instance_after};}
MilitaryWalletDeltaEffect1 copy(const MilitaryWalletDeltaEffectBuildView& v){return {v.request,v.before,v.after};}
MilitaryWalletReservationEffect1 copy(const MilitaryWalletReservationEffectBuildView& v){return copy_military_wallet_reservation_effect(v);}
MilitaryWalletCreationEffect1 copy(const MilitaryWalletCreationEffectBuildView& v){return {v.request,v.after};}
MilitaryInventoryCreationEffect1 copy(const MilitaryInventoryCreationEffectBuildView& v){return {v.request,v.after};}
Status validate(const MilitaryFifoTransferEffect1& v){return validate_original_fifo_lot_transfer(v.request,{v.source_before,v.source_after,v.destination_before,v.destination_after,v.actual_flows,v.next_instance_before,v.next_instance_after});}
Status validate(const MilitaryWalletDeltaEffect1& v){return validate_original_wallet_delta(v.request,v.before,v.after);}
Status validate(const MilitaryWalletReservationEffect1& v){return validate_original_wallet_reservation_change(v.request,v.before,v.after);}
Status validate(const MilitaryWalletCreationEffect1& v){return validate_original_wallet_creation(v.request,v.after);}
Status validate(const MilitaryInventoryCreationEffect1& v){return validate_original_inventory_creation(v.request,v.after);}
struct InventoryTouch {const InventoryView* before=nullptr;const InventoryView* after=nullptr;};
struct WalletTouch {const WalletView* before=nullptr;const WalletView* after=nullptr;};
struct Touches {InventoryTouch inventories[2];WalletTouch wallet;};
Touches touches(const MilitaryFifoTransferEffect1& v){return {{{&v.source_before,&v.source_after},{&v.destination_before,&v.destination_after}},{}};}
Touches touches(const MilitaryWalletDeltaEffect1& v){return {{},{&v.before,&v.after}};}
Touches touches(const MilitaryWalletReservationEffect1& v){return {{},{&v.before,&v.after}};}
Touches touches(const MilitaryWalletCreationEffect1& v){return {{},{nullptr,&v.after}};}
Touches touches(const MilitaryInventoryCreationEffect1& v){return {{{nullptr,&v.after},{}},{}};}
Touches touches(const MilitaryResourceOperationEffect1& v){return std::visit([](const auto& row){return touches(row);},v);}
void continuity(const MilitaryActionResourceEffect1& v){
 std::optional<Integer> allocator;
 for(std::size_t index=0;index<v.operations.size();++index){
  const auto current=touches(v.operations[index]);
  for(const auto& target:current.inventories)if(target.after){
   const InventoryView* previous=nullptr;
   for(std::size_t prior=index;prior>0&&!previous;--prior)for(const auto& touched:touches(v.operations[prior-1]).inventories)if(touched.after&&touched.after->id==target.after->id){previous=touched.after;break;}
   if(!previous&&v.return_opening&&v.return_opening->source_inventory.id==target.after->id)previous=&v.return_opening->source_inventory;
   if(previous&&(!target.before||!same_original_inventory_frame(*previous,*target.before)))fail("MilitaryResourceInventoryContinuityMismatch",target.after->id);
  }
  if(current.wallet.after){const WalletView* previous=nullptr;
   for(std::size_t prior=index;prior>0&&!previous;--prior){const auto touched=touches(v.operations[prior-1]).wallet;if(touched.after&&touched.after->id==current.wallet.after->id)previous=touched.after;}
   if(previous&&(!current.wallet.before||!same_original_wallet_frame(*previous,*current.wallet.before)))fail("MilitaryResourceWalletContinuityMismatch",current.wallet.after->id);
  }
  if(const auto* transfer=std::get_if<MilitaryFifoTransferEffect1>(&v.operations[index])){
   if(allocator&&*allocator!=transfer->next_instance_before)fail("MilitaryResourceAllocatorContinuityMismatch");allocator=transfer->next_instance_after;
  }
 }
}
}
MilitaryWalletReservationEffect1 copy_military_wallet_reservation_effect(const MilitaryWalletReservationEffectBuildView& v){return {v.request,v.before,v.after};}
Outcome<const ResourceExecutionDraft*> read_original_military_resource_capture(const CommonStore& common,const CommonWrite& original,const std::shared_ptr<const ResourceExecutionCaptureIdentity>& capture,std::size_t first){return guard<const ResourceExecutionDraft*>([&]{
 if(static_cast<const CommonWrite*>(&common)!=&original||!capture||common.resource_execution_capture_identity()!=capture)fail("MilitaryActionResourceCaptureMismatch");
 const auto& drafts=common.resource_execution_drafts();if(drafts.size()<=first||drafts.size()-first!=1)fail("MilitaryActionResourceCaptureCoverage");
 const auto& draft=drafts[first];const auto& actual=common.state().identity;
 if(draft.identity.world_id!=actual.world_id||draft.identity.execution_epoch!=actual.execution_epoch||draft.identity.revision!=actual.revision||draft.identity.tick!=actual.tick||draft.operations.size()!=draft.recipe.operations.size())fail("MilitaryActionResourceCaptureIdentityMismatch");
 return &draft;
});}
MilitaryActionResourceEffect1::MilitaryActionResourceEffect1(const MilitaryActionResourceEffectBuildView& v):copied_fields(reserve(v)){
 operations.reserve(v.operations.size());
 for(const auto& operation:v.operations)std::visit([&](const auto& input){operations.emplace_back(copy(input));},operation);
 if(v.return_opening)return_opening.emplace(*v.return_opening);
}
MilitaryActionResourceEffect1::MilitaryActionResourceEffect1(const MilitaryActionResourceEffect1& v):copied_fields(reserve(v)),operations(v.operations),return_opening(v.return_opening){}
void MilitaryActionResourceEffect1::swap(MilitaryActionResourceEffect1& v)noexcept{copied_fields.swap(v.copied_fields);operations.swap(v.operations);return_opening.swap(v.return_opening);}
Status validate_military_action_resource_effect(const MilitaryActionResourceEffect1& v){return status([&]{
 bool transfer=false;for(const auto& operation:v.operations){take(std::visit([](const auto& value){return validate(value);},operation));transfer=transfer||std::holds_alternative<MilitaryFifoTransferEffect1>(operation);}
 if(v.return_opening&&bool(v.return_opening->destination_inventory)==transfer)fail("MilitaryReturnOpeningTransferShapeMismatch");continuity(v);
});}
MilitaryActionResourceCapture::MilitaryActionResourceCapture(const CommonStore& c):common_(&c),capture_(c.resource_execution_capture_identity()),first_(c.resource_execution_drafts().size()){
 if(!capture_)fail("MilitaryActionResourceCaptureRequired");
}
Status MilitaryActionResourceCapture::capture_return_opening(const CommonWrite& original,const InventoryView& inventory,const InventoryView* destination){return status([&]{
 if(static_cast<const CommonWrite*>(common_)!=&original||common_->resource_execution_capture_identity()!=capture_||common_->resource_execution_drafts().size()!=first_||opening_||effect_||common_->inventory(inventory.id)!=&inventory)fail("MilitaryReturnResourceOpeningCaptureMismatch");
 if(destination&&(common_->inventory(destination->id)!=destination||destination->id==inventory.id))fail("MilitaryReturnResourceOpeningCaptureMismatch");
 const MilitaryReturnResourceOpeningBuildView input{inventory,destination};(opening_memory_).grow(0);opening_.emplace(MilitaryReturnResourceOpening1{inventory,{}});if(destination)opening_->destination_inventory.emplace(*destination);
});}
Status MilitaryActionResourceCapture::capture_original_success(const CommonWrite& original){return status([&]{
 if(effect_)fail("MilitaryActionResourceCaptureMismatch");
 const auto& draft=*take(read_original_military_resource_capture(*common_,original,capture_,first_));
 CheckpointVector<MilitaryResourceOperationEffectBuildView> inputs;inputs.reserve(draft.operations.size());Integer allocator=draft.next_instance_before;
 for(std::size_t index=0;index<draft.operations.size();++index){
  const auto& request=draft.recipe.operations[index];const auto& operation=draft.operations[index];
  if(operation.operation_index!=index||operation.next_instance_before!=allocator)fail("MilitaryActionResourceCaptureOrderMismatch");
  if(const auto* v=std::get_if<TransferLots>(&request)){const auto proof=take(read_original_fifo_lot_transfer(*v,operation));inputs.emplace_back(MilitaryFifoTransferEffectBuildView{*v,*proof.source->before,proof.source->after,*proof.destination->before,proof.destination->after,*proof.actual_flows,operation.next_instance_before,operation.next_instance_after});}
  else if(const auto* v=std::get_if<WalletDelta>(&request)){const auto* proof=take(read_original_wallet_delta(*v,operation));inputs.emplace_back(MilitaryWalletDeltaEffectBuildView{*v,*proof->before,proof->after});}
  else if(const auto* v=std::get_if<SetReservation>(&request)){const auto* proof=take(read_original_wallet_reservation_change(*v,operation));inputs.emplace_back(MilitaryWalletReservationEffectBuildView{*v,*proof->before,proof->after});}
  else if(const auto* v=std::get_if<CreateWallet>(&request)){const auto* proof=take(read_original_wallet_creation(*v,operation));inputs.emplace_back(MilitaryWalletCreationEffectBuildView{*v,proof->after});}
  else if(const auto* v=std::get_if<CreateInventory>(&request)){const auto* proof=take(read_original_inventory_creation(*v,operation));inputs.emplace_back(MilitaryInventoryCreationEffectBuildView{*v,proof->after});}
  else fail("MilitaryActionResourcePrimitiveNotOriginal");
  allocator=operation.next_instance_after;
 }
 if(allocator!=draft.next_instance_after)fail("MilitaryActionResourceCaptureOrderMismatch");
 effect_.emplace(MilitaryActionResourceEffectBuildView{inputs,opening_});take(validate_military_action_resource_effect(*effect_));
});}
Outcome<std::optional<MilitaryActionResourceCapture>> begin_military_action_resource_capture(const CommonStore& c,Integer schema){return guard<std::optional<MilitaryActionResourceCapture>>([&]()->std::optional<MilitaryActionResourceCapture>{
 if(schema==0)return std::nullopt;if(schema!=1)fail("UnsupportedMilitaryActionResourceSchema");return std::optional<MilitaryActionResourceCapture>{std::in_place,c};
});}
namespace {
template<class T>const T& original_row(const std::vector<T>& rows,const StableId& id){const T* found=nullptr;for(const auto& row:rows)if(row.id==id){if(found)fail("MilitaryResourceOriginalOwnerAmbiguous",id);found=&row;}if(!found)fail("MilitaryResourceOriginalOwnerRequired",id);return *found;}
bool original_owner(const Owner& owner,const StableId& organization){return owner.kind==Owner::Kind::organization&&owner.id==organization&&!owner.extra;}
void transfers(const MilitaryActionResourceEffect1& effect,std::size_t& next,const std::vector<std::pair<StableId,Integer>>& goods,const StableId& source,const StableId& destination,const StableId& organization,std::string_view reservation){
 CheckpointVector<const std::pair<StableId,Integer>*> ordered;ordered.reserve(goods.size());for(const auto& row:goods)ordered.push_back(&row);std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return *a<*b;});
 const StableId* previous=nullptr;
 for(const auto* good:ordered){if(good->second<=0||(previous&&*previous==good->first)||next>=effect.operations.size())fail("MilitaryResourceOriginalGoodsMismatch");previous=&good->first;
  const auto* actual=std::get_if<MilitaryFifoTransferEffect1>(&effect.operations[next++]);if(!actual)fail("MilitaryResourceOriginalRecipeMismatch");const auto& request=actual->request;
  if(request.source_id!=source||request.destination_id!=destination||request.item_id!=good->first||request.quantity!=good->second||request.owners.size()!=1||!original_owner(request.owners.front(),organization)||request.reservation.kind!=ReservationRef::Kind::action||request.reservation.id!=reservation||!request.reservation.domain_id.empty()||!request.reservation.activity_kind.empty()||request.new_owner)fail("MilitaryResourceOriginalTransferMismatch",good->first);
 }
}
class ActualMoneyLegs final:public resource_recipe::MoneyTransferLegSink {
 const MilitaryActionResourceEffect1& effect_;std::size_t& next_;
public:
 ActualMoneyLegs(const MilitaryActionResourceEffect1& effect,std::size_t& next):effect_(effect),next_(next){}
 Status wallet_delta(const StableId& wallet,Integer delta,bool spendable)override{
  if(next_>=effect_.operations.size())return common_failure("MilitaryResourceOriginalMoneyCoverage",wallet);
  const auto* actual=std::get_if<MilitaryWalletDeltaEffect1>(&effect_.operations[next_]);
  if(!actual||actual->request.wallet_id!=wallet||actual->request.delta!=delta||actual->request.require_spendable!=spendable)return common_failure("MilitaryResourceOriginalMoneyMismatch",wallet);
  ++next_;return common_success();
 }
};
void money(const MilitaryActionResourceEffect1& effect,std::size_t& next,const StableId& payer,const StableId& payee,Integer amount){
 ActualMoneyLegs actual(effect,next);take(resource_recipe::visit_original_money_transfer(payer,payee,amount,resource_recipe::MoneyTransferPolicy::original_balance,actual));if(next!=effect.operations.size())fail("MilitaryResourceOriginalRecipeCoverage");
}
}
Status validate_original_formation_resource_result(const MilitaryState& military,const MilitaryCatalog& catalog,const DynamicMilitaryActionFact2& result,Integer schema){return status([&]{
 if(schema==0){if(result.actual_resource_effect)fail("UnexpectedMilitaryResourceEffect",result.id);return;}if(schema!=1)fail("MilitaryActionResourceSelectedSchemaRequired");
 if(result.command!="army_form"||result.rule_version!=2)fail("MilitaryFormationResourceOriginalResultRequired",result.id);
 if(result.phase!="completed"||!result.success){if(result.actual_resource_effect)fail("UnexpectedMilitaryResourceEffect",result.id);return;}
 const auto* terms=std::get_if<ArmyFormationTerms>(&result.original_terms);if(!terms||!result.result||!result.result->ok.value_or(false)||!result.actual_resource_effect)fail("MilitaryFormationResourceEffectRequired",result.id);
 const auto& effect=*result.actual_resource_effect;if(effect.return_opening)fail("UnexpectedFormationReturnOpening",result.id);take(validate_military_action_resource_effect(effect));
 const auto& army=original_row(military.armies,result.result->army_id);const auto& camp=original_row(catalog.garrisons,terms->garrison_id);
 if(army.formation_source!=result.id||army.order_id!=terms->order_id||army.organization_id!=terms->organization_id||result.result->order_id!=terms->order_id||terms->source_inventory_id!=camp.inventory_id||terms->source_wallet_id!=camp.wallet_id||effect.operations.size()<2)fail("MilitaryFormationResourceOriginalOwnerMismatch",result.id);
 const auto* inventory=std::get_if<MilitaryInventoryCreationEffect1>(&effect.operations[0]);const auto* wallet=std::get_if<MilitaryWalletCreationEffect1>(&effect.operations[1]);if(!inventory||!wallet)fail("MilitaryFormationOriginalContainerCreationRequired",result.id);
 const auto& created_inventory=inventory->request.inventory;const auto& created_wallet=wallet->request.wallet;
 if(created_inventory.id!=army.inventory_id||!original_owner(created_inventory.owner,terms->organization_id)||created_inventory.capacity!=camp.army_capacity||created_inventory.revision!=0||!created_inventory.lots.empty()||created_wallet.id!=army.wallet_id||!original_owner(created_wallet.owner,terms->organization_id)||created_wallet.balance!=0||!created_wallet.reserved.empty())fail("MilitaryFormationOriginalContainerMismatch",result.id);
 std::size_t next=2;transfers(effect,next,terms->goods,terms->source_inventory_id,army.inventory_id,terms->organization_id,result.id);money(effect,next,terms->source_wallet_id,army.wallet_id,terms->money);
});}
Status validate_original_return_resource_result(const MilitaryState& military,const MilitaryCatalog& catalog,const CommonCatalog* common_catalog,const DynamicMilitaryLogisticsFact2& action,const DynamicMilitaryReturnFact2& result,Integer schema){return status([&]{
 if(schema==0){if(result.actual_resource_effect)fail("UnexpectedMilitaryResourceEffect",result.id);return;}if(schema!=1)fail("MilitaryActionResourceSelectedSchemaRequired");
 const auto& terms=action.original_terms;
 if(action.command!="army_return"||action.rule_version!=2||action.phase!="completed"||!action.success||!action.result||action.result->receipt_id!=result.id||result.logistics_action_ref!=action.id||result.army_id!=terms.army_id||result.garrison_id!=terms.garrison_id||!result.actual_resource_effect)fail("MilitaryReturnResourceOriginalResultRequired",result.id);
 const auto& effect=*result.actual_resource_effect;if(!effect.return_opening)fail("MilitaryReturnResourceOpeningRequired",result.id);if(!common_catalog)fail("MilitaryReturnResourceCatalogRequired",result.id);take(validate_military_action_resource_effect(effect));
 const auto& army=original_row(military.armies,result.army_id);const auto& camp=original_row(catalog.garrisons,result.garrison_id);const auto& opening=*effect.return_opening;
 if(army.order_id!=result.order_id||army.organization_id!=terms.organization_id||opening.source_inventory.id!=army.inventory_id||!original_owner(opening.source_inventory.owner,army.organization_id)||opening.source_inventory.capacity<0||opening.source_inventory.revision<0)fail("MilitaryReturnResourceOpeningMismatch",result.id);
 // Original return_assets rejects any foreign/reserved cargo, and unloads the
 // entire original inventory. No new count/sum engine is needed: the sole FIFO
 // proof plus the actual final empty source establishes complete disposition.
 for(const auto& lot:opening.source_inventory.lots)if(!original_owner(lot.owner,army.organization_id)||!lot.reserved_by.empty())fail("MilitaryForeignOrReservedCargo",result.id);
 std::size_t next=0;transfers(effect,next,result.goods,army.inventory_id,camp.inventory_id,army.organization_id,{});const auto first_money=next;money(effect,next,army.wallet_id,camp.wallet_id,result.money);
 // With cargo the first actual FIFO before frame is the destination opening.
 // Without cargo the genuine same-point opening must retain that missing frame.
 const auto& destination=first_money?std::get<MilitaryFifoTransferEffect1>(effect.operations.front()).destination_before:*opening.destination_inventory;
 if(destination.id!=camp.inventory_id||destination.id==opening.source_inventory.id||destination.capacity<0)fail("MilitaryReturnResourceDestinationMismatch",result.id);
 const auto source_weight=take(read_inventory_weight(opening.source_inventory,*common_catalog));const auto destination_weight=take(read_inventory_weight(destination,*common_catalog));
 // Same original capacity inequality, checked without an overflowing sum.
 if(destination_weight>destination.capacity||source_weight>destination.capacity-destination_weight)fail("GarrisonCapacityExceeded",result.id);
 // The sole original money visitor always requires its debit and credit, even
 // at zero. Its first actual before frame is the original wallet opening.
 const auto& source_wallet=std::get<MilitaryWalletDeltaEffect1>(effect.operations[first_money]).before;
 if(source_wallet.id!=army.wallet_id||!original_owner(source_wallet.owner,army.organization_id)||!source_wallet.reserved.empty()||source_wallet.balance<0||source_wallet.balance!=result.money||source_wallet.balance>2147483647)fail("MilitaryReturnResourceWalletOpeningMismatch",result.id);
 const auto& destination_wallet=std::get<MilitaryWalletDeltaEffect1>(effect.operations[first_money+1]).before;
 if(destination_wallet.id!=camp.wallet_id||destination_wallet.balance>2147483647-source_wallet.balance)fail("MoneyOverflow",result.id);
 const InventoryView* final_inventory=&opening.source_inventory;const WalletView* final_wallet=&source_wallet;
 for(const auto& operation:effect.operations){const auto changed=touches(operation);for(const auto& inventory:changed.inventories)if(inventory.after&&inventory.after->id==army.inventory_id)final_inventory=inventory.after;if(changed.wallet.after&&changed.wallet.after->id==army.wallet_id)final_wallet=changed.wallet.after;}
 if(!final_inventory->lots.empty()||final_wallet->balance!=0||!final_wallet->reserved.empty())fail("MilitaryReturnResourceDispositionIncomplete",result.id);
});}
}
