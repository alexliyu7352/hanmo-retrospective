#include "military_formation_reservations.hpp"
#include "military_action_resources.hpp"
#include "military_action_time_records.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {using namespace movement_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<MilitaryInventoryReservationEffect1>,"one fresh reservation payload must transfer without a second copy");
static_assert(std::is_nothrow_swappable_v<std::optional<InventoryView>>,"inventory opening swap must preserve its owning charge");
static_assert(std::is_nothrow_swappable_v<MilitaryWalletReservationEffect1>,"original wallet payload swap must preserve its real owner");
CheckpointMemoryCharge reserve(const MilitaryFormationReservationsBuildView& v){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryFormationReservations1& v){return CheckpointMemoryCharge(0);}
MilitaryInventoryReservationEffect1 copy(const MilitaryInventoryReservationEffectBuildView& v){return {v.request,v.before,v.after,v.actual_flows,v.next_instance_before,v.next_instance_after};}
bool original_owner(const Owner& v,const StableId& organization){return v.kind==Owner::Kind::organization&&v.id==organization&&!v.extra;}
bool reservation(const ReservationRef& v,const StableId& action){return v.kind==ReservationRef::Kind::action&&v.id==action&&v.domain_id.empty()&&v.activity_kind.empty();}
}
MilitaryFormationReservations1::MilitaryFormationReservations1(const MilitaryFormationReservationsBuildView& v)
 :copied_fields(reserve(v)),wallet_reservation(copy_military_wallet_reservation_effect(v.wallet_reservation)){
 if(v.empty_inventory_opening)empty_inventory_opening.emplace(*v.empty_inventory_opening);
 inventory_reservations.reserve(v.inventory_reservations.size());
 for(const auto& row:v.inventory_reservations)inventory_reservations.emplace_back(copy(row));
}
MilitaryFormationReservations1::MilitaryFormationReservations1(const MilitaryFormationReservations1& v)
 :copied_fields(reserve(v)),inventory_reservations(v.inventory_reservations),wallet_reservation(v.wallet_reservation),empty_inventory_opening(v.empty_inventory_opening){}
void MilitaryFormationReservations1::swap(MilitaryFormationReservations1& v)noexcept{copied_fields.swap(v.copied_fields);inventory_reservations.swap(v.inventory_reservations);using std::swap;swap(wallet_reservation,v.wallet_reservation);swap(empty_inventory_opening,v.empty_inventory_opening);}
Status validate_military_formation_reservations(const MilitaryFormationReservations1& v){return status([&]{
 if(v.empty_inventory_opening&&!v.inventory_reservations.empty())fail("UnexpectedMilitaryFormationInventoryOpening");
 const MilitaryInventoryReservationEffect1* prior=nullptr;
 for(const auto& row:v.inventory_reservations){
  take(validate_original_fifo_lot_reservation(row.request,{row.before,row.after,row.actual_flows,row.next_instance_before,row.next_instance_after}));
  if(prior&&(prior->next_instance_after!=row.next_instance_before||!same_original_inventory_frame(prior->after,row.before)))fail("MilitaryFormationReservationContinuityMismatch");
  prior=&row;
 }
 const auto& wallet=v.wallet_reservation;take(validate_original_wallet_reservation_change(wallet.request,wallet.before,wallet.after));
});}
MilitaryFormationReservationCapture::MilitaryFormationReservationCapture(const CommonStore& c,Integer selected):common_(&c),capture_(c.resource_execution_capture_identity()),first_(c.resource_execution_drafts().size()),selected_schema_(selected){
 if(selected!=1&&selected!=2)fail("UnsupportedMilitaryFormationReservationSchema");
 if(!capture_)fail("MilitaryFormationReservationCaptureRequired");
}
Status MilitaryFormationReservationCapture::capture_original_opening(const CommonWrite& original,const ArmyFormationTerms& terms){return status([&]{
 if(selected_schema_==1)return;
 if(selected_schema_!=2||opening_captured_||effect_)fail("MilitaryFormationInventoryOpeningCaptureInvalid");
 if(static_cast<const CommonWrite*>(common_)!=&original||common_->resource_execution_capture_identity()!=capture_||common_->resource_execution_drafts().size()!=first_)fail("MilitaryFormationInventoryOpeningCandidateMismatch");
 if(terms.goods.empty()){
  const auto* opening=common_->inventory(terms.source_inventory_id);
  if(!opening||opening->id!=terms.source_inventory_id||!original_owner(opening->owner,terms.organization_id))fail("MilitaryPublicAssetAuthority",terms.source_inventory_id);
  opening_memory_=CheckpointMemoryCharge(0);
  empty_inventory_opening_.emplace(*opening);
 }
 opening_captured_=true;
});}
Status MilitaryFormationReservationCapture::capture_original_reservation(const CommonWrite& original){return status([&]{
 if(effect_)fail("MilitaryFormationReservationCaptureDuplicate");
 if(selected_schema_==2&&!opening_captured_)fail("MilitaryFormationInventoryOpeningCaptureRequired");
 const auto& draft=*take(read_original_military_resource_capture(*common_,original,capture_,first_));
 if(draft.operations.empty())fail("MilitaryFormationReservationRecipeRequired");
 CheckpointVector<MilitaryInventoryReservationEffectBuildView> inventory;inventory.reserve(draft.operations.size()-1);
 Integer allocator=draft.next_instance_before;
 for(std::size_t index=0;index+1<draft.operations.size();++index){
  const auto* request=std::get_if<ReserveLots>(&draft.recipe.operations[index]);const auto& operation=draft.operations[index];
  if(!request||operation.operation_index!=index||operation.next_instance_before!=allocator)fail("MilitaryFormationReservationCaptureOrderMismatch");
  const auto proof=take(read_original_fifo_lot_reservation(*request,operation));
  inventory.emplace_back(MilitaryInventoryReservationEffectBuildView{*request,*proof.inventory->before,proof.inventory->after,*proof.actual_flows,operation.next_instance_before,operation.next_instance_after});allocator=operation.next_instance_after;
 }
 const auto index=draft.operations.size()-1;const auto& operation=draft.operations[index];const auto* request=std::get_if<SetReservation>(&draft.recipe.operations[index]);
 if(!request||operation.operation_index!=index||operation.next_instance_before!=allocator)fail("MilitaryFormationReservationCaptureOrderMismatch");
 const auto* wallet=take(read_original_wallet_reservation_change(*request,operation));
 if(operation.next_instance_after!=draft.next_instance_after)fail("MilitaryFormationReservationCaptureOrderMismatch");
 if(selected_schema_==2){
  if(inventory.empty()!=bool(empty_inventory_opening_))fail("MilitaryFormationInventoryOpeningCoverageMismatch");
  if(empty_inventory_opening_){
   const auto* unchanged=common_->inventory(empty_inventory_opening_->id);
   if(!unchanged||!same_original_inventory_frame(*empty_inventory_opening_,*unchanged))fail("MilitaryFormationInventoryOpeningChanged",empty_inventory_opening_->id);
  }
 }
 effect_.emplace(MilitaryFormationReservationsBuildView{inventory,{*request,*wallet->before,wallet->after},empty_inventory_opening_?&*empty_inventory_opening_:nullptr});take(validate_military_formation_reservations(*effect_));
});}
Outcome<std::optional<MilitaryFormationReservationCapture>> begin_military_formation_reservation_capture(const CommonStore& c,Integer selected){return guard<std::optional<MilitaryFormationReservationCapture>>([&]()->std::optional<MilitaryFormationReservationCapture>{
 if(selected==0)return std::nullopt;if(selected!=1&&selected!=2)fail("UnsupportedMilitaryFormationReservationSchema");return std::optional<MilitaryFormationReservationCapture>{std::in_place,c,selected};
});}
Status validate_original_formation_acceptance_reservations(const MilitaryCatalog& catalog,const DynamicMilitaryActionFact2& f,Integer selected){return status([&]{
 if(selected==0){if(f.acceptance_resource_reservations)fail("UnexpectedMilitaryFormationReservations",f.id);return;}
 if(selected!=1&&selected!=2)fail("MilitaryFormationReservationSelectedSchemaRequired");
 const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);
 if(f.command!="army_form"||f.rule_version!=2||!terms||f.actor_id!=terms->actor_id||!f.acceptance_resource_reservations)fail("MilitaryFormationAcceptanceReservationsRequired",f.id);
 const auto& effect=*f.acceptance_resource_reservations;take(validate_military_formation_reservations(effect));
 if(selected==1&&effect.empty_inventory_opening)fail("UnexpectedMilitaryFormationInventoryOpening",f.id);
 if(selected==2&&bool(effect.empty_inventory_opening)!=terms->goods.empty())fail("MilitaryFormationInventoryOpeningCoverageMismatch",f.id);
 const GarrisonDefinition* camp=nullptr;for(const auto& row:catalog.garrisons)if(row.id==terms->garrison_id){if(camp)fail("MilitaryFormationGarrisonAmbiguous",row.id);camp=&row;}
 if(!camp||terms->source_inventory_id!=camp->inventory_id||terms->source_wallet_id!=camp->wallet_id||terms->money<0||terms->money>1000000||effect.inventory_reservations.size()!=terms->goods.size())fail("MilitaryFormationReservationTermsMismatch",f.id);
 CheckpointVector<const std::pair<StableId,Integer>*> goods;goods.reserve(terms->goods.size());for(const auto& row:terms->goods)goods.push_back(&row);std::sort(goods.begin(),goods.end(),[](const auto* a,const auto* b){return *a<*b;});
 const StableId* previous=nullptr;
 for(std::size_t index=0;index<goods.size();++index){const auto& good=*goods[index];const auto& row=effect.inventory_reservations[index];const auto& request=row.request;
  if(good.first.empty()||good.second<0||(previous&&*previous==good.first)||request.inventory_id!=terms->source_inventory_id||request.item_id!=good.first||request.quantity!=good.second||request.owners.size()!=1||!original_owner(request.owners.front(),terms->organization_id)||!reservation(request.action_id,f.id)||!original_owner(row.before.owner,terms->organization_id))fail("MilitaryFormationReservationOriginalRecipeMismatch",f.id);previous=&good.first;
 }
 if(selected==2){
  const auto* opening=effect.empty_inventory_opening?&*effect.empty_inventory_opening:&effect.inventory_reservations.front().before;
  if(opening->id!=terms->source_inventory_id||!original_owner(opening->owner,terms->organization_id))fail("MilitaryPublicAssetAuthority",f.id);
  const Owner own{Owner::Kind::organization,terms->organization_id,{}};const StableId no_reservation;
  // The original preview counts food even when the selected amount is zero.
  take(count_inventory_frame_items(*opening,camp->food_item_id,{own},ReservationRef{no_reservation}));
 }
 const auto& wallet=effect.wallet_reservation;
 if(wallet.request.wallet_id!=terms->source_wallet_id||!reservation(wallet.request.action_id,f.id)||wallet.request.amount!=terms->money||wallet.request.erase||!original_owner(wallet.before.owner,terms->organization_id))fail("MilitaryFormationWalletReservationMismatch",f.id);
 if(take(read_wallet_spendable(wallet.before))<terms->money)fail("InsufficientMilitarySupply",f.id);
 // /2 closes the actual zero-goods opening only. These original frames do not
 // replace the independent frozen-goods weight, roster, authority or service gates.
});}
}
