// 本人店单页仅读同栈原订单与Task；报价直接复用实际接受、当面交付和取消门。
#pragma once
#include "task_shop_order_boundary.hpp"
namespace hanmo::resident {
struct TaskShopOrderViewAccess {
    const CommonCatalog& common_catalog;
    const TaskCatalog& task_catalog;
    const PrivateShopOrderRead& orders;
    const TaskShopOrderAcceptanceMeeting& acceptance_meeting;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
};
// order_id仅来自当前本人店页选中的原待接订单；不可见来源只给统一准入失败，不回显原条款。
Outcome<BoundaryValue> query_task_shop_orders(const CommonStore&,const TaskState&,const TaskShopOrderState&,
    const TaskShopOrderViewAccess&,Integer cursor=0,const StableId& order_id={});
}
