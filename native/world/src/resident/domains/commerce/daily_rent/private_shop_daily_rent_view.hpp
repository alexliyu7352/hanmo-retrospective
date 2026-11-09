// 有限日租页只在当地给当前债务；远方店主仅保本人原凭据，执行人仍限已收文书的当地原欠期。
#pragma once
#include "private_shop_daily_rent_boundary.hpp"
namespace hanmo::resident {
struct PrivateShopDailyRentQuery {StableId shop_id,mandate_fact_id,death_notice_binding_fact_id;Integer cursor=0;};
Outcome<PrivateShopDailyRentQuery> decode_private_shop_daily_rent_query(const BoundaryValue&);
Outcome<BoundaryValue> query_private_shop_daily_rent(const PrivateShopDailyRentContext&,const StableId& observer,const PrivateShopDailyRentQuery&);
}
