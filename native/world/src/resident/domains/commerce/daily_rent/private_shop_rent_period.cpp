// 原期次使用全宽中间值；资金不足不暗中部分扣款，最终两腿仍须由原 Common recipe 执行。
#include "private_shop_rent_period.hpp"
#include <limits>
namespace hanmo::resident {namespace {
template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
}
Outcome<PrivateShopRentPeriod> quote_private_shop_rent_period(Integer opened,Integer index){
    if(opened<0||index<0)return fail<PrivateShopRentPeriod>("InvalidPrivateShopRentPeriod");
    const __int128 from=__int128(opened)+__int128(index)*2,through=from+2;
    if(through>std::numeric_limits<Integer>::max())return fail<PrivateShopRentPeriod>("PrivateShopRentTickOverflow");
    return {true,{index,static_cast<Integer>(from),static_cast<Integer>(through)},{}};
}
Outcome<PrivateShopRentPeriod> locate_private_shop_rent_period(Integer opened,Integer observed){
    if(opened<0||observed<opened)return fail<PrivateShopRentPeriod>("PrivateShopRentBoundaryMismatch");
    return quote_private_shop_rent_period(opened,(observed-opened)/2);
}
Outcome<PrivateShopRentFunding> quote_private_shop_rent_funding(Integer rent,Integer spendable){
    if(rent<=0||spendable<0)return fail<PrivateShopRentFunding>("InvalidPrivateShopRentFunding");
    const bool paid=spendable>=rent;
    return {true,{rent,spendable,paid?rent:0,paid?0:rent,paid},{}};
}
}
