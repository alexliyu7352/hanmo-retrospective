// 日租费用接入原 Shop 唯一财产回放；只返回具名事件差量，不保存第二余额。
#pragma once
#include "private_shop_daily_rent_business.hpp"
namespace hanmo::resident {
struct PrivateShopRentCashEffect {
    Integer tick=0,shop_cash_delta=0;
    std::vector<std::pair<StableId,StableId>> allocated_sources;
    std::optional<StableId> status;StableId last_local_payment_id;
};
Status validate_private_shop_daily_rent_selection(const PrivateShopContext&);
// 原全账 replay 给出该事实前的现金；历史员工剩余预约从原 Action/服务/终点重建，不拿今天余额倒填。
Outcome<std::optional<PrivateShopRentCashEffect>> private_shop_rent_cash_effect(const PrivateShopContext&,const PrivateShopRecord&,const StableId& fact_id,Integer actual_cash_before);
Outcome<bool> private_shop_rent_use_current(const PrivateShopContext&,const StableId& shop_id);
}
