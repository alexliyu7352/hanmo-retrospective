// 原Common原子recipe仍唯一转货扣钱；此叶只复核同一交易双方并摘录实际批次。
#include "verified_trade_recipe.hpp"
#include "resident/domains/local_economy/service_finance_execution.hpp"
#include <set>
#include "resident/storage/selected_resource_execution.hpp"
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> trade_failure(const StableId& code){return {false,{},common_failure(code).error};}
}
Outcome<ResourceRecipe> prepare_verified_trade_recipe(const CommonStore& c,const VerifiedTradeTerms& t){
 const auto* from=c.inventory(t.from_inventory_id);const auto* to=c.inventory(t.to_inventory_id);const auto* payer=c.wallet(t.from_wallet_id);const auto* payee=c.wallet(t.to_wallet_id);const auto* item=c.item_definition(t.item_id);
 if(!from||!to||!payer||!payee||!item||t.quantity<=0||t.total<0||from->id==to->id||payer->id==payee->id)return trade_failure<ResourceRecipe>("InvalidVerifiedTrade");
 if(!owner_equal(from->owner,t.seller)||!owner_equal(payer->owner,t.buyer)||!owner_equal(payee->owner,t.seller))return trade_failure<ResourceRecipe>("VerifiedTradeOwnershipMismatch");
 Integer selected_quantity=0;std::set<StableId> selected_ids;if(t.selected_cargo.empty())return trade_failure<ResourceRecipe>("EmptyCargoSelection");
 for(const auto& pick:t.selected_cargo){if(pick.inventory_id!=from->id||pick.quantity<=0||!selected_ids.insert(pick.lot_id).second)return trade_failure<ResourceRecipe>("InvalidCargoSelection");auto lot=std::find_if(from->lots.begin(),from->lots.end(),[&](const LotView& row){return row.id==pick.lot_id;});if(lot==from->lots.end()||lot->item_id!=t.item_id||!owner_equal(lot->owner,t.seller)||!lot->reserved_by.empty()||lot->quantity<pick.quantity||__builtin_add_overflow(selected_quantity,pick.quantity,&selected_quantity))return trade_failure<ResourceRecipe>("StaleSelectedLot");}
 if(selected_quantity!=t.quantity)return trade_failure<ResourceRecipe>("SelectedTradeQuantityMismatch");
 auto funds=c.spendable(payer->id);if(!funds.ok)return {false,{},funds.error};if(funds.value<t.total)return trade_failure<ResourceRecipe>("InsufficientFunds");
 auto weight=c.inventory_weight(to->id);if(!weight.ok)return {false,{},weight.error};Integer extra=0,result=0;
 if(item->weight<0||__builtin_mul_overflow(item->weight,t.quantity,&extra)||__builtin_add_overflow(weight.value,extra,&result)||result>to->capacity)return trade_failure<ResourceRecipe>("OverCapacity");
 if(__builtin_add_overflow(payee->balance,t.total,&result))return trade_failure<ResourceRecipe>("MoneyOverflow");
 ResourceRecipe out;out.operations={TransferSelectedLots{from->id,to->id,t.selected_cargo,{t.seller},{},t.buyer}};
 out.expected_versions={c.read_version({ResourceKey::Kind::inventory,from->id}),c.read_version({ResourceKey::Kind::inventory,to->id})};auto money=service_finance::append_money_transfer_recipe(c,out,payer->id,payee->id,t.total);if(!money.ok)return {false,{},money.error};return {true,std::move(out),{}};
}
Outcome<VerifiedTradeExecution> execute_verified_trade_recipe_in_candidate(CommonStore& c,const VerifiedTradeTerms& t){
 auto recipe=prepare_verified_trade_recipe(c,t);if(!recipe.ok)return {false,{},recipe.error};
 if(!c.resource_execution_capture_identity())return trade_failure<VerifiedTradeExecution>("VerifiedTradeCaptureRequired");
 const auto before=c.resource_execution_drafts().size();auto applied=c.apply_resources(recipe.value);if(!applied.ok)return {false,{},applied.error};const auto& capture=c.resource_execution_drafts();if(capture.size()!=before+1||capture.back().operations.size()!=3)return trade_failure<VerifiedTradeExecution>("VerifiedTradeActualLotsMismatch");
 const auto& operation=capture.back().operations[0];auto checked=validate_selected_resource_execution(recipe.value.operations[0],operation);if(!checked.ok)return {false,{},checked.error};VerifiedTradeExecution result;for(const auto& flow:operation.lot_flows)result.lots.push_back({flow.source_lot_id,flow.destination_lot_id,flow.quantity});return {true,std::move(result),{}};
}
}
