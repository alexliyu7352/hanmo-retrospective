// 原Trade开工调用顺序与外层回滚门；scope为测试适配器，不冒World保护注册完成。
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/growth/growth_market_capture.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}
struct Capture final:MarketBusinessCaptureServices {
    int opened=0,applied=0,committed=0;bool late_failure=false;
    struct Scope final:MarketBusinessCaptureScope {
        CommonStore& c;Capture& owner;
        Scope(CommonStore& common,Capture& read):c(common),owner(read){}
        Status apply(MarketCaptureStage stage,const ResourceRecipe& recipe,const std::vector<MarketQuantityLeg>& legs)override{
            ++owner.applied;if(stage!=MarketCaptureStage::trade||legs.size()!=2||legs[0].signed_quantity!=-2||legs[1].signed_quantity!=2)return common_failure("TestCaptureLegs");return c.apply_resources(recipe);
        }
        Status commit_trade(const GrowthState& state,const StableId& id)override{
            ++owner.committed;if(!state.life_activities.find(id)||!c.state().facts.find(id)||c.state().facts.order().back()!=id)return common_failure("TestMissingActualTrade");return owner.late_failure?common_failure("TestLateCaptureFailure"):common_success();
        }
        Status commit_economy(const LocalEconomyState&,const DynamicStepInput&,const StableId&)override{return common_failure("TestWrongDomain");}
    };
    Outcome<std::unique_ptr<MarketBusinessCaptureScope>> open_growth_trade(CommonStore& c,const GrowthCatalog&,const ActionCoreView&,const GrowthQuote&)override{++opened;return {true,std::make_unique<Scope>(c,*this),{}};}
    Outcome<std::unique_ptr<MarketBusinessCaptureScope>> open_economic_business(CommonStore&,MarketCauseKind,const StableId&,Integer,const DynamicStepInput&)override{return {false,{},common_failure("TestWrongDomain").error};}
};
}
int main(){try{
    Owner merchant{Owner::Kind::organization,"merchant_org",{},{}},buyer{Owner::Kind::actor,"buyer",{}, {}};auto catalog=std::make_shared<CommonCatalog>();catalog->items.insert("millet",{"millet",1,10});catalog->organization_ids={"merchant_org"};CommonState seed;seed.next_instance_id=10;ActorRecord actor;actor.id="buyer";actor.wallet_id="buyer_wallet";actor.inventory_id="bag";seed.actors.insert(actor.id,actor);actor.id="merchant";seed.actors.insert(actor.id,actor);seed.wallets.insert("buyer_wallet",{"buyer_wallet",buyer,100,{},{}});seed.wallets.insert("shop_wallet",{"shop_wallet",merchant,100,{},{}});seed.inventories.insert("bag",{"bag",buyer,100,0,{},{}});seed.inventories.insert("market",{"market",merchant,100,0,{{"lot:1","millet",{},5,merchant,{}}},{}});
    CommonStore common(seed,catalog);GrowthCatalog growth;growth.regional_life=true;growth.shops.insert("shop",{"shop","place","market","shop_wallet",{}, {}});GrowthState state;ActionCoreView action;action.id="action:3";action.actor_id="buyer";action.kind="trade";GrowthQuote quote;quote.command.kind="trade";InteractionContext context;context.provider_actor_id="merchant";context.place_id="place";quote.context=context;TradeTerms terms;terms.shop_id="shop";terms.item_id="millet";terms.side="buy";terms.quantity=2;terms.total=28;terms.payer_wallet_id="buyer_wallet";terms.owner=buyer;terms.context=context;quote.data=terms;
    Capture capture;auto point=common.begin_savepoint();check(point.ok,"outer NPC savepoint");auto started=growth::start_service_trade(common,state,growth,action,quote,&capture);check(started.ok&&capture.opened==1&&capture.applied==1&&capture.committed==1,"one original recipe then original payload then capture commit");check(common.wallet("buyer_wallet")->balance==72&&common.wallet("shop_wallet")->balance==128&&common.inventory("market")->lots[0].quantity==3,"original cash and goods moved once");check(!growth::start_service_trade(common,state,growth,action,quote,&capture).ok&&capture.opened==1,"duplicate original action rejects before capture");check(common.rollback_savepoint(point.value).ok,"outer point still belongs to caller");state={};
    capture.late_failure=true;point=common.begin_savepoint();started=growth::start_service_trade(common,state,growth,action,quote,&capture);check(!started.ok&&started.error.original.code=="TestLateCaptureFailure","late capture error preserved");check(common.rollback_savepoint(point.value).ok,"late failure Common rollback");state={};check(common.wallet("buyer_wallet")->balance==100&&common.wallet("shop_wallet")->balance==100&&common.inventory("market")->lots[0].quantity==5&&common.inventory("bag")->lots.empty()&&common.state().facts.empty()&&common.state().next_instance_id==10,"late failure restores source assets and instance IDs");
    std::cout<<"GROWTH_MARKET_TRADE_CAPTURE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_MARKET_TRADE_CAPTURE "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
