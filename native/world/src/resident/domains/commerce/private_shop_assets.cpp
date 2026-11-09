// 开设费和本金同一recipe对等转移；只创建0余额空容器，不将本金计作经营收入。
#include "private_shop_assets.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
}
bool private_shop_opening_terms_equal(const PrivateShopOpeningTerms& a,const PrivateShopOpeningTerms& b){return a.premises_id==b.premises_id&&a.place_id==b.place_id&&a.proprietor_actor_id==b.proprietor_actor_id&&a.lessor_actor_id==b.lessor_actor_id&&a.payer_wallet_id==b.payer_wallet_id&&a.lessor_wallet_id==b.lessor_wallet_id&&a.market_shop_id==b.market_shop_id&&a.opening_fee==b.opening_fee&&a.initial_capital==b.initial_capital&&a.inventory_capacity==b.inventory_capacity&&a.daily_rent==b.daily_rent&&a.rent_rate_profile==b.rent_rate_profile;}
Outcome<PrivateShopOpeningAssetQuote> quote_private_shop_opening_assets(const CommonStore& c,const PrivateShopPremises& p,const StableId& actor){
 const auto* proprietor=c.actor(actor);const auto* lessor=c.actor(p.lessor_actor_id);
 if(p.id.empty()||p.place_id.empty()||p.market_shop_id.empty()||p.opening_fee<0||p.initial_capital<=0||p.inventory_capacity<=0||p.daily_rent<0)return fail<PrivateShopOpeningAssetQuote>("InvalidPrivateShopPremises");
 if(!proprietor||!lessor||actor==lessor->id)return fail<PrivateShopOpeningAssetQuote>("PrivateShopPartiesRequired");
 const auto* payer=c.wallet(proprietor->wallet_id);const auto* payee=c.wallet(p.lessor_wallet_id);
 if(!payer||!payee||payer->id==payee->id||!owner_equal(payer->owner,{Owner::Kind::actor,actor,{}})||!owner_equal(payee->owner,{Owner::Kind::actor,lessor->id,{}}))return fail<PrivateShopOpeningAssetQuote>("PrivateShopPrivateWalletRequired");
 Integer total=0,credited=0;if(__builtin_add_overflow(p.opening_fee,p.initial_capital,&total)||__builtin_add_overflow(payee->balance,p.opening_fee,&credited))return fail<PrivateShopOpeningAssetQuote>("PrivateShopOpeningOverflow");
 auto available=c.spendable(payer->id);if(!available.ok)return {false,{},available.error};if(available.value<total)return fail<PrivateShopOpeningAssetQuote>("PrivateShopInsufficientOpeningFunds");
 PrivateShopOpeningAssetQuote out;out.terms={p.id,p.place_id,actor,p.lessor_actor_id,payer->id,payee->id,p.market_shop_id,p.opening_fee,p.initial_capital,p.inventory_capacity,p.daily_rent,p.rent_rate_profile};out.total_private_cash=total;
 out.versions={c.read_version({ResourceKey::Kind::wallet,payer->id}),c.read_version({ResourceKey::Kind::wallet,payee->id})};return {true,std::move(out),{}};
}
Outcome<PrivateShopOpeningAssetPlan> prepare_private_shop_opening_assets(const CommonStore& c,const PrivateShopOpeningAssetQuote& quote,const StableId& fact_id){
 const auto* fact=c.state().facts.find(fact_id);const auto sequence=organization_instance_sequence(fact_id,private_shop_open_prefix);
 if(!fact||fact->domain!=private_shop_module||fact->kind!=private_shop_open_kind||!sequence||*sequence>=c.state().next_instance_id)return fail<PrivateShopOpeningAssetPlan>("PrivateShopOpeningFactRequired");
 const auto& t=quote.terms;PrivateShopPremises p;p.id=t.premises_id;p.place_id=t.place_id;p.lessor_actor_id=t.lessor_actor_id;p.lessor_wallet_id=t.lessor_wallet_id;p.market_shop_id=t.market_shop_id;p.opening_fee=t.opening_fee;p.initial_capital=t.initial_capital;p.inventory_capacity=t.inventory_capacity;p.daily_rent=t.daily_rent;p.rent_rate_profile=t.rent_rate_profile;
 // 本叶只复核财货字段；本人会面与资格必须由完整合同入口独立核实。
 auto current=quote_private_shop_opening_assets(c,p,t.proprietor_actor_id);if(!current.ok)return {false,{},current.error};if(!private_shop_opening_terms_equal(current.value.terms,t)||current.value.total_private_cash!=quote.total_private_cash)return fail<PrivateShopOpeningAssetPlan>("StalePrivateShopOpeningAssets");
 if(quote.versions.size()!=current.value.versions.size())return fail<PrivateShopOpeningAssetPlan>("StalePrivateShopOpeningAssets");for(std::size_t i=0;i<quote.versions.size();++i){const auto& a=quote.versions[i];const auto& b=current.value.versions[i];if(a.resource.kind!=b.resource.kind||a.resource.id!=b.resource.id||a.version!=b.version)return fail<PrivateShopOpeningAssetPlan>("StalePrivateShopOpeningAssets");}
 PrivateShopOpeningAssetPlan out;out.wallet_id="private_shop_wallet:"+std::to_string(*sequence);out.inventory_id="private_shop_inventory:"+std::to_string(*sequence);
 if(c.wallet(out.wallet_id)||c.inventory(out.inventory_id))return fail<PrivateShopOpeningAssetPlan>("PrivateShopAssetsAlreadyExist");
 const Owner owner{Owner::Kind::actor,t.proprietor_actor_id,{}};out.resources.expected_versions=quote.versions;
 out.resources.operations={CreateWallet{{out.wallet_id,owner,0,{},{}}},CreateInventory{{out.inventory_id,owner,t.inventory_capacity,0,{},{}}},WalletDelta{t.payer_wallet_id,-quote.total_private_cash,true},WalletDelta{t.lessor_wallet_id,t.opening_fee,false},WalletDelta{out.wallet_id,t.initial_capital,false}};
 return {true,std::move(out),{}};
}
}
