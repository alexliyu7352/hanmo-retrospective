// 选定市场永久原因链；只有历史事实，不存在第二份可花库存。Shared尚未注册，不单独激活。
#pragma once
#include "growth_market_resource_contract.hpp"
#include "growth_market_read.hpp"
#include "growth_market_fifo_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <functional>
namespace hanmo::resident {
struct GrowthCatalog;
class MarketOriginalFifoBusinessRead;
// 此初态只由正规Installed预备过程给出，禁止由当前Common或旧档推导。
struct GrowthMarketHistoryCatalog {
    std::vector<StableId> shop_ids;
    OrderedTable<InventoryView> initial_inventories;
    Integer initial_next_instance_id=1;
    // 由实际Installed模块选中，不能按新字段存在或当前库存自动升级。
    Integer selected_version=1;
    CommonResourceCodecContext resource_codec;
};
// 原业务中各recipe的唯一位置，生产先投入后产出；空的市场腿没有空回执。
struct MarketCaptureStep {
    MarketCaptureStage stage=MarketCaptureStage::trade;
    Integer next_instance_before=1,next_instance_after=1;
    std::vector<MarketInventoryChange> inventories;
    FieldOrder field_order;
    MarketCaptureSourceBinding source_binding;
};
struct MarketMovementRecord {
    StableId kind="growth_market_movement",business_fact_id;
    MarketCauseKind business_kind=MarketCauseKind::growth_trade;
    Integer source_tick=0,observed_tick=0;
    std::vector<MarketCaptureStep> steps;
    FieldOrder field_order;
    Integer business_source_schema=1;
    std::optional<OriginalBusinessFifoExecution> original_business_execution;
};
struct GrowthMarketHistoryState {
    OrderedTable<MarketMovementRecord> movements;FieldOrder field_order;
    Integer version=1;
};
// 正向原因与反向枚举来自原Growth/LE，不能拿本账自己的ID清单充作完整覆盖证明。
struct GrowthMarketHistoryAccess {
    const MarketBusinessCauseRead& causes;
    std::function<Status(const StableId& exact_trade_fact_id,const InventoryView& actual_market_before)> trade_before;
    std::function<Outcome<std::vector<StableId>>(const std::vector<StableId>& protected_inventory_ids)> source_ids;
    // 仅物理2反读真正原业务；物理1不获得此能力，缺口不以空reader降级。
    const MarketOriginalFifoBusinessRead* original_fifo=nullptr;
};
Status validate_growth_market_history(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthMarketHistoryCatalog&,const GrowthMarketHistoryState&,const GrowthMarketHistoryAccess&);
// 原资源与原业务已在同一未发布候选内成功，调用者必须持Common/Growth/LE/市场总保存点。
// 本函数只追加原事实绑定，不重放转钱/转货；不自建/提交/回滚Common保存点。
// 返回失败可能已追加本域事实，必须回滚外层整候选；LE逐个原业务即时调用，不能批后逐项补账。
Outcome<StableId> record_growth_market_movement_in_candidate(CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthMarketHistoryCatalog&,GrowthMarketHistoryState&,const GrowthMarketHistoryAccess&,MarketMovementRecord);
// 两口都先核完整永久链到当前实物。历史边界仅原private_shop_day源，S5先核其payload.tick。
Outcome<GrowthMarketMidpoint> read_growth_market_history_current(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthMarketHistoryCatalog&,const GrowthMarketHistoryState&,const GrowthMarketHistoryAccess&,const StableId& shop_id,const StableId& item_id);
Outcome<GrowthMarketMidpoint> read_growth_market_history_before_shop_result(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthMarketHistoryCatalog&,const GrowthMarketHistoryState&,const GrowthMarketHistoryAccess&,const StableId& shop_id,const StableId& item_id,Integer observed_tick,const StableId& exact_day_fact_id);
}
