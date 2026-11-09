// 显式日租后继的期次纯算术；不授租约、查余额、生成应付或修改旧 Shop1。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// 从真实开设时刻起每两个半日一期，[from_tick,through_tick)；即使停业也不能改写旧期次。
struct PrivateShopRentPeriod {
    Integer index=0,from_tick=0,through_tick=0;
};
Outcome<PrivateShopRentPeriod> quote_private_shop_rent_period(Integer actual_opened_tick,Integer period_index);
Outcome<PrivateShopRentPeriod> locate_private_shop_rent_period(Integer actual_opened_tick,Integer observed_tick);
// 仅核同一原租约的数值承诺。暂停只保一笔原未付款，不以经过天数乘出无限债务。
struct PrivateShopRentFunding {
    Integer period_rent=0,available_cash=0,payable_cash=0,unpaid_cash=0;
    bool fully_funded=false;
};
Outcome<PrivateShopRentFunding> quote_private_shop_rent_funding(Integer frozen_daily_rent,Integer actual_spendable_cash);
}
