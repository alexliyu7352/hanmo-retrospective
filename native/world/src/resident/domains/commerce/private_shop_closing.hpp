// 关店先证本店与Task的原责任已清；只沿原共同资产归还，不凭今日余额回填历史。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
struct PrivateShopClosingQuote {
    WorldIdentity source_identity;StableId actor_id,shop_id,to_wallet_id,to_inventory_id;
    Integer returned_cash=0;std::vector<LotView> stock_lots;std::vector<ReadVersion> versions;
};
// before_fact为空只证今日；历史必须是调用方已核载荷的真实关闭头。
Status validate_private_shop_closing_responsibilities(const PrivateShopContext&,const PrivateShopRecord&,Integer tick,const StableId& before_fact={});
Outcome<PrivateShopClosingQuote> preview_private_shop_closing(const PrivateShopContext&,const StableId& actor,const PrivateShopClose&);
// 调用方已有唯一Common保存点；转账/逐批归还/晚事实失败必须整World回滚。
Outcome<StableId> close_private_shop_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const PrivateShopClosingQuote&);
Status validate_private_shop_closings(const PrivateShopContext&);
}
