// 原Common/LE半日与原日租日历顺序门；人工资格只隔离实际私债付款，不冒自然世界。
#define main original_private_shop_transaction_fixture_main
#include "private_shop_transaction_test.cpp"
#undef main
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_business.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
namespace {
struct ClosedPublicAuthority final:OrganizationRead {
 bool organization_exists(const StableId&)const override{return false;}bool organization_active(const StableId&)const override{return false;}
 bool institutional_obligation_allowed(const StableId&,const StableId&,const StableId&,const StableId&,Integer,Integer)const override{return false;}
 bool actor_accepts_duty(const StableId&,const StableId&)const override{return false;}std::optional<Integer> authority_epoch(const StableId&)const override{return {};}
 Outcome<AuthorityProof> authorize(const ActingAuthority&,const StableId&,const ScopeRef&,Integer)const override{return {false,{},common_failure("Denied").error};}
 std::optional<OrganizationMemberView> member_view(const StableId&,const StableId&)const override{return {};}
};
}
int main(){try{
 Fixture f;PrivateShopDailyRentCatalog profile;PrivateShopDailyRentState rent;auto x=f.context();x.daily_rent_catalog=&profile;x.daily_rent=&rent;auto q=preview_private_shop_opening(x,f.meeting(),"owner",{"stall"});check(q.ok,"real daily lease quote");auto opened=open_private_shop(f.common,f.economy,f.state,x,f.meeting(),q.value,&rent);check(opened.ok,"actual opening before phase");ClosedPublicAuthority organizations;
 auto step=[&](Integer tick){DynamicStepInput phase;phase.from_tick=tick-1;phase.to_tick=tick;PrivateShopResponsibilitySourceContext source{f.common,f.economy,f.economic_catalog,f.state,f.catalog,&phase,&profile,&rent};auto advance=step_local_economy_in_candidate(f.common,f.economy,f.economic_catalog,organizations,f.activities,phase,nullptr,nullptr,nullptr,nullptr,nullptr,&source);check(advance.ok,"original macro step uses exact same-candidate private sources");f.common.state().identity.tick=tick;f.state.last_step_tick=tick;check(reconcile_private_shop_daily_rent_in_candidate(f.common,f.economy,f.state,rent,profile,phase).ok,"calendar only after actual clock commit");};
 for(Integer tick=1;tick<=301;++tick)step(tick);
 auto ledger=read_private_shop_rent_ledger(f.common,f.economy,f.state,profile,rent,opened.value.shop_id);check(ledger.ok&&ledger.value.suspended&&!ledger.value.pending_arrear_id.empty(),"one precise unpaid period pauses lease");
 auto add=preview_private_shop_management(x,"owner",PrivateShopFund{opened.value.shop_id,10});check(add.ok,"actual owner can fund paused original shop");check(manage_private_shop(f.common,f.state,x,add.value).ok,"same original capital transfer");auto payment_count=f.economy.payments.size();step(302);check(f.common.wallet(opened.value.wallet_id)->balance==10&&f.economy.payments.size()==payment_count&&f.economy.arrears.find(ledger.value.pending_arrear_id)->paid_amount==0,"macro step cannot silently spend new funds on paused private debt");
 DynamicStepInput next;next.from_tick=302;next.to_tick=303;LocalEconomyState foreign=f.economy;PrivateShopResponsibilitySourceContext wrong{f.common,foreign,f.economic_catalog,f.state,f.catalog,&next,&profile,&rent};check(!step_local_economy_in_candidate(f.common,f.economy,f.economic_catalog,organizations,f.activities,next,nullptr,nullptr,nullptr,nullptr,nullptr,&wrong).ok,"foreign LE reference cannot exempt original obligations");std::cout<<checks<<"/0 exact private-debt phase checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
