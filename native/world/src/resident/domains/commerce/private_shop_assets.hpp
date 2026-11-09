// 本叶只核真实私钱的资源腿，不代替本人同意、客户、商术、铺位排他和司法准入。
#pragma once
#include "private_shop_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 报价和冷账复核同一冻结条款，不能只比较铺位ID后按被改费用成交。
bool private_shop_opening_terms_equal(const PrivateShopOpeningTerms&,const PrivateShopOpeningTerms&);
struct PrivateShopOpeningAssetQuote {
    PrivateShopOpeningTerms terms;
    std::vector<ReadVersion> versions;
    Integer total_private_cash=0;
};
// 仅支持已声明的私人房东；公共铺位须另接具名Office/Grant，不能借本口接受组织钱包。
Outcome<PrivateShopOpeningAssetQuote> quote_private_shop_opening_assets(const CommonStore&,const PrivateShopPremises&,const StableId& proprietor);
struct PrivateShopOpeningAssetPlan {
    StableId wallet_id,inventory_id;
    ResourceRecipe resources;
};
// 已分配且实际登记的开设头是容器身份锚；此口只构造recipe，外层完整业务候选才可应用。
Outcome<PrivateShopOpeningAssetPlan> prepare_private_shop_opening_assets(const CommonStore&,const PrivateShopOpeningAssetQuote&,const StableId& actual_opening_fact_id);
}
