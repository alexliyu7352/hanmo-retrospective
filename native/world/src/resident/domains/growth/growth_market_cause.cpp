// 交易原开工已经同时付款和转货；随后完成、取消或失效均不抹掉原成交数量腿。
#include "growth_market_cause.hpp"
#include "growth_internal.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
Outcome<MarketBusinessCause> read_growth_market_cause(const CommonStore& c,const CommonCatalog& common,const GrowthCatalog& catalog,const GrowthState& state,const StableId& id){
    auto bad=[&]()->Outcome<MarketBusinessCause>{return {false,{},common_failure("GrowthMarketTradeCauseInvalid",id).error};};if(!catalog.regional_life||!catalog.economy)return bad();
    if(id.rfind("action:",0)!=0)return bad();const auto digits=id.substr(7);Integer number=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||number<=0||number>=c.state().next_instance_id||std::to_string(number)!=digits)return bad();
    const auto* head=c.state().facts.find(id);const auto* record=state.life_activities.find(id);if(!head||head->id!=id||head->kind!="life_activity"||head->domain!="growth"||!record||std::find(c.state().facts.order().begin(),c.state().facts.order().end(),id)==c.state().facts.order().end())return bad();
    const auto& v=record->view;const auto* terms=std::get_if<TradeTerms>(&record->terms);const auto* shop=terms?catalog.shops.find(terms->shop_id):nullptr;const auto* stock=shop?common.required_inventories.find(shop->inventory_id):nullptr;const auto* actor=c.actor(v.actor_id);
    if(!terms||!shop||!stock||!actor||v.kind!=LifeActivityKind::trade||v.fact_id!=id||v.action_id!=id||(terms->side!="buy"&&terms->side!="sell")||v.activity!=terms->side||v.item_id!=terms->item_id||!common.items.find(v.item_id)||v.quantity!=terms->quantity||terms->quantity<1||terms->quantity>500||v.amount!=terms->total||terms->total<0||v.start_tick!=terms->valid_tick||v.start_tick<0||v.start_tick>c.tick()||v.settlement_tick!=v.start_tick||record->cost_paid||record->wage_paid)return bad();
    if(v.phase!=LifeActivityPhase::active&&v.phase!=LifeActivityPhase::completed&&v.phase!=LifeActivityPhase::cancelled)return bad();const bool buy=terms->side=="buy";
    if(v.from_inventory_id!=(buy?shop->inventory_id:actor->inventory_id)||v.to_inventory_id!=(buy?actor->inventory_id:shop->inventory_id)||v.payer_wallet_id!=(buy?terms->payer_wallet_id:shop->wallet_id)||v.payee_wallet_id!=(buy?shop->wallet_id:terms->payer_wallet_id)||terms->context.place_id!=shop->place_id)return bad();
    MarketBusinessCause out;out.source_identity=c.state().identity;out.source_fact_id=id;out.kind=MarketCauseKind::growth_trade;out.source_tick=v.start_tick;out.original_business_key=id;out.business_due_tick=v.start_tick;
    out.legs.push_back({v.from_inventory_id,v.item_id,buy?stock->owner:terms->owner,-v.quantity});out.legs.push_back({v.to_inventory_id,v.item_id,buy?terms->owner:stock->owner,v.quantity});return {true,std::move(out),{}};
}
Status validate_growth_market_trade_before(const CommonStore& c,const CommonCatalog& common,const GrowthCatalog& catalog,const GrowthState& state,const StableId& id,const InventoryView& before){
    auto source=read_growth_market_cause(c,common,catalog,state,id);if(!source.ok)return {false,{},source.error};const auto& record=*state.life_activities.find(id);const auto& terms=std::get<TradeTerms>(record.terms);const auto& shop=*catalog.shops.find(terms.shop_id);const auto* initial=common.required_inventories.find(shop.inventory_id);const auto* item=common.items.find(terms.item_id);
    if(!initial||!item||before.id!=shop.inventory_id||before.revision!=terms.market_revision||!owner_equal(before.owner,initial->owner)||before.capacity!=initial->capacity||item->base_price<=0||item->base_price>2147483647)return common_failure("GrowthMarketHistoricalTradeBasisMismatch",id);
    Integer stock=0;for(const auto& lot:before.lots)if(lot.item_id==terms.item_id&&owner_equal(lot.owner,before.owner)&&lot.reserved_by.empty()){if(lot.quantity<0||lot.quantity>INT64_MAX-stock)return common_failure("GrowthMarketQuantityOverflow",id);stock+=lot.quantity;}
    const bool buy=terms.side=="buy";if(buy&&stock<terms.quantity)return common_failure("GrowthMarketHistoricalStockInsufficient",id);const auto discount=terms.discount_bp.value_or(0);if(discount<0||discount>800)return common_failure("GrowthMarketHistoricalDiscountInvalid",id);
    // 高库存的原边际已在半价平台；只把无效高位夹到所有实际买卖腿同价处，避免大乘法溢出。
    const auto bounded=std::min<Integer>(stock,buy?200+terms.quantity:200);const auto total=growth::trade_price(item->base_price,bounded,terms.quantity,buy,discount);return total==terms.total?common_success():common_failure("GrowthMarketHistoricalTradePriceMismatch",id);
}

}
