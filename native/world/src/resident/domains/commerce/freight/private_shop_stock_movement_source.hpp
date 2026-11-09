// 原入库/关店财货写者的同候选纯消费；只证明该次变更，不追认远地货运或报销。
#pragma once
#include "resident/domains/commerce/private_shop.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <memory>
namespace hanmo::resident {
struct ResourceExecutionCaptureIdentity;
enum class PrivateShopStockMovementKind {purchase_stocking,closing_return};
// 调用者已核原Shop/Growth业务准入。这里借真实同候选原账，不重入完整Shop/政治/市场。
struct PrivateShopStockMovementContext {
    const CommonStore& common;const PrivateShopState& shops;const PrivateShopCatalog& catalog;
    const GrowthState* growth=nullptr;
};
// 只在原写者前栈保存。地址只作同候选身份核，不解引用失效地址、不序列化。
struct PrivateShopStockMovementBoundary {
    const CommonStore* original_common=nullptr;const PrivateShopState* original_shops=nullptr;
    const PrivateShopCatalog* original_catalog=nullptr;const GrowthState* original_growth=nullptr;
    WorldIdentity identity;PrivateShopStockMovementKind kind=PrivateShopStockMovementKind::purchase_stocking;
    PrivateShopRecord shop_before;InventoryView bag_before,stock_before;WalletView personal_cash_before,shop_cash_before;
    std::vector<ReadVersion> versions;
    Integer next_instance_before=0;std::size_t recipe_begin=0;
    // 原活动capture唯一身份；结束重开、copy/assignment与move均不能复用这个前界。
    std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
};
// Adjust+Add保原各腿，不伪装成Common Transfer映射；返还现金不是收入或新的运输投入。
struct PrivateShopOriginalStockMovement {
    WorldIdentity source_identity;PrivateShopStockMovementKind kind=PrivateShopStockMovementKind::purchase_stocking;
    StableId original_fact_id,shop_id,proprietor_actor_id,original_trade_fact_id;
    std::variant<PrivateShopPurchaseFact,PrivateShopClosingRecord> original_record;
    ResourceExecutionDraft actual_recipe;
};
// purchase_stocking前界在原Trade已成功后、原stock_received分号前；closing_return在原关店recipe前。
// 不借当前地点证明历史；真实当前处境、责任清偿与执行授权继续由原业务入口负责。
Outcome<PrivateShopStockMovementBoundary> capture_private_shop_stock_movement_boundary(
    const PrivateShopStockMovementContext&,const StableId& shop_id,PrivateShopStockMovementKind);
// 实际原writer成功后立即核精确新头/原载荷/原recipe和两容器当前值；不从当前余额补旧款。
// closing_return明确是退出归还，不是到店、退款或可资本化运费。整函数无ID/资产写。
Outcome<PrivateShopOriginalStockMovement> read_private_shop_original_stock_movement(
    const PrivateShopStockMovementContext&,const PrivateShopStockMovementBoundary&,const StableId& actual_fact_id);
}
