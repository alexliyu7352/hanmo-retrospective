// 生产进入原商店库存后沿原交易公式报价和转移；接触回调由夹具明确提供，不代替Main准入验收。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/growth/growth_internal.hpp"

// 两份粮的既有配方在家庭口粮扣除后增加真实存量；旧报价必须失效，购买只花原钱包。
int main(){
    Fixture f;
    f.common_catalog->items.find("grain")->base_price=100;
    f.catalog.recipes.find("grain.recipe")->outputs[0].numerator=2;
    assert(f.common.apply_resources({{WalletDelta{"student.wallet",1000,true}},{}}).ok);
    GrowthCatalog shops;shops.economy=true;
    shops.tradeable_items={"grain"};
    shops.shops.insert("market",{"market","town","city.inventory","city.wallet",{"teacher.role"},{}});
    GrowthAccess access;
    access.contact=[](const StableId& actor,const InteractionContext& context,const StableId& kind,const StableId&)->Outcome<InteractionContext>{
        assert(actor=="student"&&context.place_id=="town"&&context.provider_actor_id=="teacher"&&context.role_id=="teacher.role"&&kind=="trade");
        return {true,context,{}};
    };
    GrowthCommand command;command.kind="trade";command.shop_id="market";command.item_id="grain";command.side="buy";command.payer_wallet_id="student.wallet";command.quantity=1;command.context=InteractionContext{"town","teacher","teacher.role",{},{},{},{}};
    const auto before=encode_common_state(f.common.state());
    auto first=growth::preview_service_trade(f.common,shops,*f.common_catalog,access,"student",command,{});
    assert(first.ok&&compare_boundary(before,encode_common_state(f.common.state())).exact());
    const auto old=std::get<TradeTerms>(first.value.data);
    f.step(1);
    const auto& receipt=f.economy.production_receipts.rows().back().second;
    assert(receipt.outputs[0].quantity==20);
    auto stock=f.common.count("city.inventory","grain",{{Owner::Kind::organization,"city",{}}});
    assert(stock.ok&&stock.value==30);
    command.quoted_total=old.total;command.market_revision=old.market_revision;
    auto stale=growth::preview_service_trade(f.common,shops,*f.common_catalog,access,"student",command,{});
    assert(!stale.ok&&stale.error.original.code=="StaleQuote"&&stale.error.new_total&&*stale.error.new_total<old.total);
    command.quoted_total.reset();command.market_revision.reset();
    auto current=growth::preview_service_trade(f.common,shops,*f.common_catalog,access,"student",command,{});
    assert(current.ok);
    const auto terms=std::get<TradeTerms>(current.value.data);
    assert(terms.total==growth::trade_price(100,30,1,true)&&terms.market_revision==f.common.inventory("city.inventory")->revision);
    const auto buyer_before=f.common.wallet("student.wallet")->balance,shop_before=f.common.wallet("city.wallet")->balance;
    GrowthState growth_state;
    ActionCoreView action;action.id="market.purchase";action.kind="trade";action.actor_id="student";action.start_tick=1;action.remaining_ticks=1;
    assert(growth::start_service_trade(f.common,growth_state,shops,action,current.value).ok);
    stock=f.common.count("city.inventory","grain",{{Owner::Kind::organization,"city",{}}});
    auto bought=f.common.count("student.bag","grain",{{Owner::Kind::actor,"student",{}}});
    assert(stock.ok&&stock.value==29&&bought.ok&&bought.value==1);
    assert(f.common.wallet("student.wallet")->balance==buyer_before-terms.total&&f.common.wallet("city.wallet")->balance==shop_before+terms.total);
    assert(receipt.outputs[0].quantity==20&&growth_state.actions.find(action.id));
    std::cout<<"production_market: PASS (real production stock 20->30, original price falls, old revision rejected, exact 1 lot and conserved money transfer)\n";
}
