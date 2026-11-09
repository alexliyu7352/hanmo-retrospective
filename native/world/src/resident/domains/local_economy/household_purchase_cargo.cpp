#include "household_purchase_cargo.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident { namespace {
template<class T>Outcome<T> fail(const char* code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
ReservationRef holder(const StableId& id){return {ReservationRef::Kind::domain_activity,id,"local_economy","household_purchase"};}
bool same_time(const WorldMoment&a,const WorldMoment&b){auto v=compare_world_moments(a,b);return v.ok&&!v.value;}
bool add(Integer a,Integer b,Integer& out){return !__builtin_add_overflow(a,b,&out);}
}
namespace {
Outcome<HouseholdPurchaseCargoView> read_cargo(const HouseholdPurchaseContext& x,const StableId& id,const WorldMoment* adopted_at,const SourceRef* adopted_source){
 const auto* p=x.state.purchases.find(id);const auto* accepted=p?x.state.accepted_terms.find(p->accepted_terms_ref.id):nullptr;
 const auto* retail=p?x.retail.state.settlements.find(p->retail_receipt_ref):nullptr;
 if(!p||!accepted||!retail||retail->terms.purchase_id!=id||p->holding_inventory_ref!=retail->terms.custody_inventory_id)
  return fail<HouseholdPurchaseCargoView>("HouseholdCargoRetailSourceMissing",id);
 std::optional<std::size_t> cut;std::size_t acceptance_order=0,index=0;bool accepted_header=false;
 if(adopted_source){
  for(const auto&fact:x.common.state().facts.rows()){if(fact.first==adopted_source->id){if(fact.second.domain!=adopted_source->domain||fact.second.kind!=adopted_source->kind)return fail<HouseholdPurchaseCargoView>("HouseholdCargoAdoptionHeaderMismatch",adopted_source->id);cut=index;}if(fact.first==accepted->source.id){accepted_header=fact.second.domain==accepted->source.domain&&fact.second.kind==accepted->source.kind;acceptance_order=index;}++index;}
  auto now=compare_world_moments(*adopted_at,x.now),after=compare_world_moments(accepted->accepted_at,*adopted_at);
  if(!cut||!accepted_header||acceptance_order>=*cut||!now.ok||!after.ok||now.value>0||after.value>0)return fail<HouseholdPurchaseCargoView>("HouseholdCargoAdoptionSourceRequired",id);
 }
 HouseholdPurchaseCargoView out;out.purchase_id=id;
 for(const auto& lot:retail->lots){
  if(lot.quantity<=0||lot.destination_lot_id.empty()||std::any_of(out.unreceived_cargo.begin(),out.unreceived_cargo.end(),[&](const auto& v){return v.lot_id==lot.destination_lot_id;})||!add(out.purchased_quantity,lot.quantity,out.purchased_quantity))return fail<HouseholdPurchaseCargoView>("InvalidHouseholdRetailCargo",id);
  out.unreceived_cargo.push_back({retail->terms.custody_inventory_id,lot.destination_lot_id,lot.quantity});
 }
 if(out.purchased_quantity!=accepted->quantity)return fail<HouseholdPurchaseCargoView>("HouseholdCargoRetailQuantityMismatch",id);
 WorldMoment previous=accepted->accepted_at;std::size_t covered=0;
 index=0;
 for(const auto& fact:x.common.state().facts.rows()){
  const auto fact_order=index++;
  const auto* h=x.state.handovers.find(fact.first);if(!h||h->purchase_id!=id)continue;
  if(cut){auto relation=compare_world_moments(h->occurred_at,*adopted_at);if(!relation.ok)return {false,{},relation.error};if((fact_order<*cut&&relation.value>0)||(fact_order>=*cut&&relation.value<0))return fail<HouseholdPurchaseCargoView>("HouseholdCargoAdoptionCausalityMismatch",h->id);if(fact_order>=*cut)continue;}
  const bool received=h->to_inventory_id==accepted->plan.destination_inventory;
  if(h->id!=fact.first||fact.second.domain!="local_economy"||fact.second.kind!=(received?"household_purchase_delivered":"household_purchase_cargo_handover")||h->from_inventory_id.empty()||h->to_inventory_id.empty()||h->from_inventory_id==h->to_inventory_id||h->actual_lots.empty()||!valid_source_ref(h->actual_delivery_source))return fail<HouseholdPurchaseCargoView>("InvalidHouseholdCargoHandover",fact.first);
  auto order=compare_world_moments(previous,h->occurred_at);auto future=compare_world_moments(h->occurred_at,x.now);
  if(!order.ok)return {false,{},order.error};if(!future.ok)return {false,{},future.error};
  if(order.value>0||future.value>0)return fail<HouseholdPurchaseCargoView>("HouseholdCargoHandoverTimeMismatch",h->id);
  for(const auto& used:out.actual_handover_sources)if(same_source_ref(used,h->actual_delivery_source))return fail<HouseholdPurchaseCargoView>("DuplicateHouseholdCargoActualSource",h->id);
  auto actual=x.environment.read_purchase_delivery(*p,h->actual_delivery_source);if(!actual.ok)return {false,{},actual.error};
  if(actual.value.actual_loss||actual.value.purchase_id!=id||!same_source_ref(actual.value.actual_source,h->actual_delivery_source)||!same_time(actual.value.occurred_at,h->occurred_at)||actual.value.from_inventory_id!=h->from_inventory_id||actual.value.to_inventory_id!=h->to_inventory_id||actual.value.cargo.size()!=h->actual_lots.size())return fail<HouseholdPurchaseCargoView>("HouseholdCargoActualSourceMismatch",h->id);
  for(std::size_t n=0;n<h->actual_lots.size();++n){const auto& flow=h->actual_lots[n];
   if(flow.quantity<=0||flow.original_lot_id.empty()||flow.destination_lot_id.empty())return fail<HouseholdPurchaseCargoView>("InvalidHouseholdCargoFlow",h->id);
   for(std::size_t j=0;j<n;++j)if(h->actual_lots[j].original_lot_id==flow.original_lot_id||h->actual_lots[j].destination_lot_id==flow.destination_lot_id)return fail<HouseholdPurchaseCargoView>("DuplicateHouseholdCargoFlow",h->id);
   if(std::count_if(actual.value.cargo.begin(),actual.value.cargo.end(),[&](const auto& c){return c.inventory_id==h->from_inventory_id&&c.lot_id==flow.original_lot_id&&c.quantity==flow.quantity;})!=1)return fail<HouseholdPurchaseCargoView>("HouseholdCargoSelectionMismatch",h->id);
   auto from=std::find_if(out.unreceived_cargo.begin(),out.unreceived_cargo.end(),[&](const auto& c){return c.inventory_id==h->from_inventory_id&&c.lot_id==flow.original_lot_id;});
   if(from==out.unreceived_cargo.end()||flow.quantity>from->quantity)return fail<HouseholdPurchaseCargoView>("HouseholdCargoFlowOutsidePurchase",h->id);
   const bool whole=flow.quantity==from->quantity;
   if((whole&&flow.destination_lot_id!=flow.original_lot_id)||(!whole&&flow.destination_lot_id==flow.original_lot_id))return fail<HouseholdPurchaseCargoView>("HouseholdCargoSplitIdentityMismatch",h->id);
   if(whole)out.unreceived_cargo.erase(from);else from->quantity-=flow.quantity;
   if(received){if(!add(out.received_quantity,flow.quantity,out.received_quantity))return fail<HouseholdPurchaseCargoView>("HouseholdCargoQuantityOverflow",id);}
   else{if(std::any_of(out.unreceived_cargo.begin(),out.unreceived_cargo.end(),[&](const auto& c){return c.lot_id==flow.destination_lot_id;}))return fail<HouseholdPurchaseCargoView>("HouseholdCargoDestinationIdentityConflict",h->id);out.unreceived_cargo.push_back({h->to_inventory_id,flow.destination_lot_id,flow.quantity});}
  }
  previous=h->occurred_at;out.actual_handover_sources.push_back(h->actual_delivery_source);++covered;
 }
 std::size_t expected=0;for(const auto& h:x.state.handovers.rows())if(h.second.purchase_id==id){bool found=false;std::size_t n=0;for(const auto&f:x.common.state().facts.rows()){if(f.first==h.first){found=true;if(!cut||n<*cut)++expected;break;}++n;}if(!found)return fail<HouseholdPurchaseCargoView>("HouseholdCargoOriginalHeaderMissing",h.first);}
 Integer conserved=out.received_quantity;for(const auto& lot:out.unreceived_cargo)if(!add(conserved,lot.quantity,conserved))return fail<HouseholdPurchaseCargoView>("HouseholdCargoQuantityOverflow",id);
 if(covered!=expected||conserved!=out.purchased_quantity)return fail<HouseholdPurchaseCargoView>("HouseholdCargoSourceCoverageMismatch",id);
 return {true,std::move(out),{}};
}
}
Outcome<HouseholdPurchaseCargoView> read_household_purchase_cargo(const HouseholdPurchaseContext&x,const StableId&id){return read_cargo(x,id,nullptr,nullptr);}
Outcome<HouseholdPurchaseCargoView> read_household_purchase_cargo_at_adoption(const HouseholdPurchaseContext&x,const StableId&id,const WorldMoment&at,const SourceRef&source){
 auto complete=read_cargo(x,id,nullptr,nullptr);if(!complete.ok)return complete;
 return read_cargo(x,id,&at,&source);
}
Status validate_household_purchase_cargo(const HouseholdPurchaseContext& x,const HouseholdPurchase& p){
 auto cargo=read_household_purchase_cargo(x,p.purchase_id);if(!cargo.ok)return {false,{},cargo.error};
 const auto* retail=x.retail.state.settlements.find(p.retail_receipt_ref);if(!retail)return common_failure("HouseholdCargoRetailSourceMissing",p.purchase_id);
 const auto reservation=holder(p.purchase_id);
 if(p.state==HouseholdPurchasePhase::received&&!cargo.value.unreceived_cargo.empty())return common_failure("HouseholdReceivedCargoIncomplete",p.purchase_id);
 for(const auto& expected:cargo.value.unreceived_cargo){
  const auto* inventory=x.common.inventory(expected.inventory_id);if(!inventory)return common_failure("HouseholdCargoInventoryMissing",expected.inventory_id);
  auto lot=std::find_if(inventory->lots.begin(),inventory->lots.end(),[&](const auto& v){return v.id==expected.lot_id;});
  if(p.state!=HouseholdPurchasePhase::lost&&(lot==inventory->lots.end()||lot->quantity!=expected.quantity||lot->item_id!=retail->terms.item_id||lot->reserved_by!=reservation||!owner_equal(lot->owner,retail->terms.buyer)))return common_failure("HouseholdPurchaseHeldCargoMismatch",expected.lot_id);
 }
 for(const auto& inventory:x.common.state().inventories.rows())for(const auto& lot:inventory.second.lots)if(lot.reserved_by==reservation){
  if(p.state==HouseholdPurchasePhase::lost||std::count_if(cargo.value.unreceived_cargo.begin(),cargo.value.unreceived_cargo.end(),[&](const auto& expected){return expected.inventory_id==inventory.first&&expected.lot_id==lot.id&&expected.quantity==lot.quantity;})!=1)return common_failure("OrphanHouseholdPurchaseCargoReservation",lot.id);
 }
 return common_success();
}
}
