// 本人委托证面、真实当前会面和已收文书办理分开；远方死亡/撤约不由账本全局状态泄露。
#pragma once
#include "private_shop_liquidation_boundary.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_estate_boundary.hpp"
namespace hanmo::resident {
struct PrivateShopLiquidationViewQuery {StableId tab="documents",notice_binding_fact_id;Integer cursor=0;};
using PrivateShopLiquidationPreview=std::function<Outcome<PrivateShopLiquidationQuote>(const StableId&,const PrivateShopLiquidationCommand&)>;
using PrivateShopEstateOrderPreview=std::function<Outcome<TaskShopOrderEstateCancellationQuote>(const StableId&,const TaskShopOrderEstateCancel&)>;
Outcome<PrivateShopLiquidationViewQuery> decode_private_shop_liquidation_query(const BoundaryValue&);
Outcome<BoundaryValue> query_private_shop_liquidations(const PrivateShopLiquidationContext&,const PrivateShopMeetingContext&,const PrivateShopLiquidationPreview&,const PrivateShopLiquidationViewQuery&,const PrivateShopEstateOrderPreview& orders={});
}
