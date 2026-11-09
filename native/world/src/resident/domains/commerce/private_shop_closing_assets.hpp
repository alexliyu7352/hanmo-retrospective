// 私铺关店唯一财货写者的内部接缝；本人关店与具名身后执行分别先验证自己的真实授权。
#pragma once
#include "resident/domains/commerce/private_shop_closing.hpp"
namespace hanmo::resident::private_shop_closing_assets {
// 只报价原店主四容器和已清责任；不授予调用人处分权，业务入口必须先验本人或限店委托。
Outcome<PrivateShopClosingQuote> preview(const PrivateShopContext&,const StableId& shop_id);
// 保留旧本金/成本/工资/租约，唯一recipe转剩余钱货回原本人；调用者拥有Common完整保存点。
Outcome<StableId> commit_in_candidate(CommonStore&,PrivateShopState&,const PrivateShopContext&,const PrivateShopClosingQuote&);
}
