// 中央Common可借的选定市场纯检查/捕获合同；本头不改变CommonStore或授予公开资源写入口。
#pragma once
#include "growth_market_history_contract.hpp"
namespace hanmo::resident {
// 只由正常Installed初态构造；新版profile不得运行中选择曾被裁剪的旧库存。
struct ProtectedMarketAccounts { OrderedTable<RequiredContainer> inventories; };
// 中央临时scope持有此值，原业务先核准入后逐recipe提供；它自身不是可提交命令/序列化许可。
struct MarketRecipeAuthorization {
    WorldIdentity source_identity;
    MarketCauseKind kind=MarketCauseKind::growth_trade;
    StableId original_business_key;
    std::vector<MarketQuantityLeg> expected_market_legs;
};
struct MarketRecipeInventoryTouch {
    StableId inventory_id;
    Integer consume_operations=0,add_operations=0;
    bool fragmented_adds=false;
    std::vector<MarketQuantityLeg> legs;
};
struct MarketRecipeInspection { std::vector<MarketRecipeInventoryTouch> inventories; };
// 整recipe发布前只读执行；无scope只准不触市场的原操作，间接预约释放也必须扫描。
Outcome<MarketRecipeInspection> inspect_protected_market_recipe(const CommonStore&,const ProtectedMarketAccounts&,const ResourceRecipe&,const MarketRecipeAuthorization* active_scope);
struct MarketLotChange { StableId lot_id;std::optional<LotView> before,after;FieldOrder field_order; };
struct MarketInventoryChange { StableId inventory_id;Integer revision_before=0,revision_after=0;std::vector<MarketLotChange> lots;FieldOrder field_order; };
// before/after来自原Common真实staging的同一容器；只产差量草稿，不写市场账/事实/ID，不递归recipe。
Outcome<MarketInventoryChange> capture_protected_market_inventory(const ProtectedMarketAccounts&,const MarketRecipeInventoryTouch&,const InventoryView& before,const InventoryView& after,Integer next_instance_before,Integer next_instance_after);
}
