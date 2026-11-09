// 身后清店是生前双方明确接受的限店执行权；不因遗赠受益身份而获得经营、遗产或公权。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline constexpr const char* private_shop_liquidation_module="private_shop_liquidations";
inline constexpr const char* private_shop_liquidation_mandate_prefix="private_shop_executor";
inline constexpr const char* private_shop_liquidation_mandate_kind="private_shop_executor_appointed";
inline constexpr const char* private_shop_liquidation_acceptance_prefix="private_shop_executor_acceptance";
inline constexpr const char* private_shop_liquidation_acceptance_kind="private_shop_executor_accepted";
inline constexpr const char* private_shop_liquidation_revocation_prefix="private_shop_executor_revoked";
inline constexpr const char* private_shop_liquidation_revocation_kind="private_shop_executor_revoked";
inline constexpr const char* private_shop_liquidation_receipt_prefix="private_shop_estate_closed";
inline constexpr const char* private_shop_liquidation_receipt_kind="private_shop_estate_closed";

// 客户、预约、旧Task和两资产腿不重签；只有此原店的原未交订单可引用该请求核取消权。
struct PrivateShopEstateCancellationRequest {
    StableId shop_id,order_id,original_proprietor_actor_id,executor_actor_id;
    StableId mandate_fact_id,death_notice_binding_fact_id;
};
// 带精确事实前界的许可凭证不等于执行回执；消费者另验原订单属于指定店且仍未交付。
struct PrivateShopEstateCancellationSource {
    WorldIdentity source_identity;
    StableId scenario_id;
    PrivateShopEstateCancellationRequest request;
    StableId mandate_fact_id,acceptance_fact_id,death_fact_id,death_cause_fact_id;
    StableId death_notice_binding_fact_id,death_notice_handoff_fact_id;
    StableId shop_wallet_id,shop_inventory_id,return_wallet_id,return_inventory_id;
    Integer appointed_tick=0,accepted_tick=0,death_tick=0,death_recorded_tick=0,observed_tick=0;
    StableId before_settlement_fact_id;
};
class PrivateShopEstateCancellationRead {
public:
    virtual ~PrivateShopEstateCancellationRead()=default;
    // 当前只读严格已提交钟；需生前授权、执行人本人接受和真实死亡来源，不授权任何新营业。
    virtual Outcome<PrivateShopEstateCancellationSource> private_shop_estate_cancellation(const PrivateShopEstateCancellationRequest&)const=0;
    // 只收原Task具名取消事实，Task先核其actor/order/tick/termination和零货款，再反查当时许可。
    virtual Outcome<PrivateShopEstateCancellationSource> private_shop_estate_cancellation_before_settlement(const PrivateShopEstateCancellationRequest&,const StableId& actual_task_settlement_fact_id,Integer settlement_tick)const=0;
};
}
