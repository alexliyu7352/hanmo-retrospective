// 私铺有限菜单只读当前本人/当地铺位；按钮每次由真实业务报价产生，未知资格不显示为可执行。
#pragma once
#include "private_shop_boundary.hpp"
#include "private_shop_read.hpp"
namespace hanmo::resident {
struct PrivateShopBusinessQuery {StableId tab="shops",shop_id;Integer cursor=0,fund_amount=100,restock_quantity=1;StableId contract_id;};
Outcome<PrivateShopBusinessQuery> decode_private_shop_business_query(const BoundaryValue&);
Outcome<BoundaryValue> query_private_shop_business(const PrivateShopCommandContext&,const StableId& observer,const PrivateShopBusinessQuery&);
}
