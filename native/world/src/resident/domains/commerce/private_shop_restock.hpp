// 补货沿原Growth贸易开工与时间；S5只捕获同候选刚购批次，不能另算买价或省略交易Action。
#pragma once
#include "private_shop.hpp"
#include "resident/domains/growth/growth_model.hpp"
namespace hanmo::resident {
struct PrivateShopRestock {StableId shop_id,item_id;Integer quantity=0;InteractionContext context;};
// 仅原命令栈临时持有，不持久化、不从客户端解码；开工前取得，原Growth写后立即消费。
struct PrivateShopRestockAdmission {
    WorldIdentity source_identity;StableId actor_id,shop_id,bag_id,market_inventory_id,merchant_wallet_id;
    GrowthQuote trade;
    Integer shop_cash_before=0,merchant_cash_before=0,market_stock_before=0,next_instance_before=0;
    std::vector<LotView> bag_lots_before;ReadVersion shop_inventory_version;
};
Outcome<PrivateShopRestockAdmission> prepare_private_shop_restock(const PrivateShopContext&,const GrowthState&,const SocialState&,const GrowthAccess&,const StableId& actor,const PrivateShopRestock&);
// 外层必须同备Common、Growth和PrivateShop。失败不能单独保留原Trade实付、life_activity或Action。
Outcome<StableId> capture_private_shop_restock_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const PrivateShopRestockAdmission&,const StableId& actual_trade_action_id);
}
