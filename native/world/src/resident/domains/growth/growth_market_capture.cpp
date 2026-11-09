// Common已完成真实staging而尚未发布；读取原before与实际after，不另执行资源或分配事实。
#include "growth_market_capture.hpp"
namespace hanmo::resident {
Outcome<std::optional<MarketCaptureStep>> capture_protected_market_recipe(
    const CommonStore& common,const ProtectedMarketAccounts& accounts,const ResourceRecipe& recipe,
    const MarketRecipeAuthorization* scope,MarketCaptureStage stage,
    const OrderedTable<InventoryView>& staged,Integer next){
    auto inspected=inspect_protected_market_recipe(common,accounts,recipe,scope);
    if(!inspected.ok)return {false,{},inspected.error};
    if(inspected.value.inventories.empty())return {true,{}, {}};
    if(!scope||(scope->kind==MarketCauseKind::growth_trade&&stage!=MarketCaptureStage::trade)||
       (scope->kind==MarketCauseKind::local_production&&stage!=MarketCaptureStage::production_inputs&&stage!=MarketCaptureStage::production_outputs)||
       (scope->kind==MarketCauseKind::local_ration&&stage!=MarketCaptureStage::ration))
        return {false,{},common_failure("ProtectedMarketCaptureStageInvalid").error};
    for(const auto& touch:inspected.value.inventories)for(const auto& leg:touch.legs)
        if((stage==MarketCaptureStage::production_inputs&&leg.signed_quantity>=0)||(stage==MarketCaptureStage::production_outputs&&leg.signed_quantity<=0))
            return {false,{},common_failure("ProtectedMarketCaptureStageInvalid").error};
    MarketCaptureStep result;result.stage=stage;result.next_instance_before=common.state().next_instance_id;result.next_instance_after=next;
    for(const auto& touch:inspected.value.inventories){
        const auto* before=common.inventory(touch.inventory_id);const auto* after=staged.find(touch.inventory_id);
        if(!before||!after)return {false,{},common_failure("ProtectedMarketCaptureInventoryMissing",touch.inventory_id).error};
        auto captured=capture_protected_market_inventory(accounts,touch,*before,*after,result.next_instance_before,next);
        if(!captured.ok)return {false,{},captured.error};result.inventories.push_back(std::move(captured.value));
    }
    return {true,std::move(result),{}};
}
}
