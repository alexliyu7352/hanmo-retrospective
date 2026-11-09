// 日租热门仅由完整 Shop 候选调用；明确区分原店金、自愿本人出资和原身后执行权。
#pragma once
#include "private_shop_daily_rent.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_contract.hpp"
#include "resident/domains/family/family_death.hpp"
namespace hanmo::resident {
// 执行权只覆盖精确原欠款与本次自愿来源，不授复业、本人私产代扣或新增经营。
struct PrivateShopRentVoluntaryRequest {
    StableId shop_id,assessment_fact_id,arrear_id,payer_actor_id,payer_wallet_id,payee_wallet_id;
    StableId mandate_fact_id,death_notice_binding_fact_id;
    Integer amount=0;
};
struct PrivateShopRentVoluntaryAuthority {
    WorldIdentity source_identity;PrivateShopRentVoluntaryRequest request;
    StableId proprietor_actor_id,acceptance_fact_id,death_fact_id,death_cause_fact_id,notice_handoff_fact_id;
    Integer observed_tick=0;
};
class PrivateShopRentVoluntaryAuthorityRead {
public:
    virtual ~PrivateShopRentVoluntaryAuthorityRead()=default;
    virtual Outcome<PrivateShopRentVoluntaryAuthority> private_shop_rent_voluntary_authority(const PrivateShopRentVoluntaryRequest&)const=0;
    virtual Outcome<PrivateShopRentVoluntaryAuthority> private_shop_rent_voluntary_authority_before_payment(const PrivateShopRentVoluntaryRequest&,const StableId& actual_payment_fact,Integer payment_tick)const=0;
};
struct PrivateShopLiquidationContext;
// 自持于 World 的同栈读，仅复用既有生前委托/死因/文书来源；不反调完整 Shop。
class PrivateShopRentExecutorAccess final:public PrivateShopRentVoluntaryAuthorityRead {
public:
    PrivateShopRentExecutorAccess(const PrivateShopLiquidationContext& authority,const LocalEconomyState& economy,const PrivateShopDailyRentState& rent):authority_(authority),economy_(economy),rent_(rent){}
    Outcome<PrivateShopRentVoluntaryAuthority> private_shop_rent_voluntary_authority(const PrivateShopRentVoluntaryRequest&)const override;
    Outcome<PrivateShopRentVoluntaryAuthority> private_shop_rent_voluntary_authority_before_payment(const PrivateShopRentVoluntaryRequest&,const StableId&,Integer)const override;
private:
    const PrivateShopLiquidationContext& authority_;const LocalEconomyState& economy_;const PrivateShopDailyRentState& rent_;
};
struct PrivateShopPayRentArrear {
    StableId shop_id,assessment_fact_id,payer_wallet_id,mandate_fact_id,death_notice_binding_fact_id;
};
struct PrivateShopResumeRent {StableId shop_id;};
using PrivateShopDailyRentCommand=std::variant<PrivateShopPayRentArrear,PrivateShopResumeRent>;
struct PrivateShopDailyRentQuote {
    WorldIdentity source_identity;StableId actor_id;PrivateShopDailyRentCommand command;
    StableId shop_id,assessment_fact_id,arrear_id,from_wallet_id,to_wallet_id;
    Integer amount=0,period_index=0,from_tick=0,through_tick=0;
    std::vector<ReadVersion> versions;
};
// 本 context 仅借完整外层同候选；权限回调不允许回调完整 Shop 验证。
struct PrivateShopDailyRentContext {
    const PrivateShopContext& shops;
    const PrivateShopDailyRentCatalog& catalog;const PrivateShopDailyRentState& state;
    const PrivateShopRentVoluntaryAuthorityRead* executors=nullptr;
    FamilyDeathOriginRead death_origin;
};
// 具名付款/复业的实际来源已核后，再核本人仍在世或精确执行人授权；不授任何新增经营。
Status validate_private_shop_daily_rent_authority(const PrivateShopDailyRentContext&);
Outcome<PrivateShopDailyRentQuote> preview_private_shop_daily_rent(const PrivateShopDailyRentContext&,const StableId& actor,const PrivateShopDailyRentCommand&);
// 成功后返回本次唯一玩家业务源，外层实际缓存/result 已产生后才永久绑定。
Outcome<StableId> apply_private_shop_daily_rent_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopDailyRentState&,const PrivateShopDailyRentContext&,const PrivateShopDailyRentQuote&);
// 实际开设原资产已写后紧接首期，正常半日只在 clock 提交后按原到期逐店应计。
Status initialize_private_shop_rent_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopDailyRentState&,const StableId& actual_new_shop);
Status reconcile_private_shop_daily_rent_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopDailyRentState&,const PrivateShopDailyRentCatalog&,const DynamicStepInput& actual_step);
}
