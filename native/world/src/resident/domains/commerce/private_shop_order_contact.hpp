// 新Task接触profile的成本前纯消费口；原订单/执行源及完整Task接触史须由调用者先验。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
class TaskShopOrderContactRead;
// 同候选严格phase读允许本次真实完成尚未分配result；开工的前次actual-result必须已永久绑定。
// 新profile在capture及完整成本冷验两处强制调用；旧profile保持原接口与存档字节。
// 当前目录要求的接触服务必须存在；纯订单结构读不调用此门，避免Task来源回环。
Status validate_private_shop_order_contact_access(const PrivateShopContext&);
// 实际成功交货按显式profile消费原接触史；取消没有成功接触，不调用来源读。
Status validate_private_shop_order_delivery_contact(const PrivateShopContext&,const PrivateShopOrderRecord&,
    const PrivateShopOrderExecution&);
Status validate_private_shop_order_delivery_contact(const PrivateShopContext&,const PrivateShopOrderRecord&,
    const PrivateShopOrderExecution&,const TaskShopOrderContactRead&);
}
