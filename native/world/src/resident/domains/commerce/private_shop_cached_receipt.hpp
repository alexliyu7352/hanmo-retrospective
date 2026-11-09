// 完整私铺账已先验后，核原请求回执的唯一业务事实与本次全局分配区间。
#pragma once
#include "resident/domains/commerce/private_shop.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct PrivateShopLiquidationState;
Status validate_private_shop_cached_results(const CommonStore&,const PrivateShopState*,const PrivateShopLiquidationState* liquidations=nullptr,ProjectionWriter* projection=nullptr);
}
