// 当前查核仅在当地柜台；本人已取得证面可随身读取，远方通缉/撤证不进入该不可变投影。
#pragma once
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_political_shop_licensing(const PoliticalShopLicensingContext&,const StableId& place,Integer cursor=0);
}
