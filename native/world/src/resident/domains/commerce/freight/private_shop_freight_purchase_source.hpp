// 原买入的同栈钱货初源；只消费真实Growth与Common执行，不提供永久货史或报销资格。
#pragma once
#include "resident/domains/growth/growth_model.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <memory>
namespace hanmo::resident {
struct ResourceExecutionCaptureIdentity;
// 在原Action分配前持有实际私人钱包、市场/背包全值；不允许序列化或跨Common候选使用。
struct PrivateShopFreightPurchaseBoundary {
    const CommonStore* original_common=nullptr;
    WorldIdentity identity;
    StableId actor_id,market_shop_id;
    WalletView payer_before,merchant_before;
    InventoryView market_before,bag_before;
    Integer next_instance_before=0;
    std::size_t recipe_begin=0;
    // 原活动capture唯一身份；结束重开、copy/assignment与move均不能复用这个前界。
    std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
};
// 每项都是原TransferLots循环实际选中的源和新批，不能按同品同量自行配对。
struct PrivateShopFreightPurchaseLot {
    ResourceLotFlowDraft actual_transfer;
    LotView market_lot_before,personal_lot_after;
};
struct PrivateShopFreightPurchaseSource {
    WorldIdentity source_identity;
    LifeActivityRecord original_trade;
    ResourceWalletExecutionDraft actual_debit,actual_credit;
    std::vector<PrivateShopFreightPurchaseLot> purchased_lots;
};
// 原Growth资格/报价与Common临时capture由调用者先准备；本口不执行买入或扩大Task付款权。
// 首片仅本人钱包垫付且原背包属本人；旧TaskOwner或店钱包交易仍走原路径，不套此策略。
Outcome<PrivateShopFreightPurchaseBoundary> capture_private_shop_freight_purchase_boundary(
    const CommonStore&,const GrowthCatalog&,const StableId& actor_id,const StableId& market_shop_id);
// 只在原start_growth成功后立即调用。原life_activity和活动载荷须精确对应这一新Action。
// 只准前界后一份TransferLots+两WalletDelta；允许其后原市场登记头，但不按“最后新头”猜交易。
// 返回值不等于完整Trade/市场冷验，未覆盖跨recipe、直接state写者、货运委托或永久提交结果。
Outcome<PrivateShopFreightPurchaseSource> read_private_shop_freight_purchase(
    const CommonStore&,const GrowthState&,const GrowthCatalog&,
    const PrivateShopFreightPurchaseBoundary&,const StableId& actual_trade_action_id);
}
