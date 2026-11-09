// 仅非负整数费用进入分摊；全宽中间值避免先溢出后截断，原资产和事实头由上层同候选核验。
#include "private_shop_accounting.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
using Wide=__int128;
constexpr Integer limit=std::numeric_limits<Integer>::max();
template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
bool fits(Wide value){return value>=0&&value<=limit;}
}
Outcome<PrivateShopCostSlice> quote_private_shop_cost_slice(const PrivateShopCostBasis& b,Integer quantity){
 if(b.acquisition_fact_id.empty()||b.received_quantity<=0||b.purchase_cash<0||b.allocated_transport_cash<0||b.sold_quantity<0||b.sold_quantity>b.received_quantity||b.recognized_cost<0||quantity<=0||quantity>b.received_quantity-b.sold_quantity)return fail<PrivateShopCostSlice>("InvalidPrivateShopCostBasis");
 const Wide total=Wide(b.purchase_cash)+b.allocated_transport_cash;if(!fits(total))return fail<PrivateShopCostSlice>("PrivateShopCostOverflow");
 const Wide before=total*b.sold_quantity/b.received_quantity;if(before!=b.recognized_cost)return fail<PrivateShopCostSlice>("PrivateShopCostHistoryMismatch");
 const auto sold=b.sold_quantity+quantity;const Wide after=total*sold/b.received_quantity;
 return {true,{quantity,static_cast<Integer>(after-before),sold,static_cast<Integer>(after)},{}};
}
Outcome<PrivateShopRetailRate> quote_private_shop_retail_rate(Integer middle,Integer markup){
 if(middle<=0||(markup!=0&&markup!=500&&markup!=1000))return fail<PrivateShopRetailRate>("InvalidPrivateShopRetailRate");
 const Wide price=Wide(middle)*(10000+markup)/10000;if(!fits(price))return fail<PrivateShopRetailRate>("PrivateShopRetailOverflow");
 return {true,{static_cast<Integer>(price),markup==0?3:markup==500?2:1},{}};
}
Outcome<PrivateShopOperatingResult> quote_private_shop_operating_result(const PrivateShopRetailRate& rate,const PrivateShopCostSlice& slice,Integer rent,Integer wage,Integer transport){
 if(rate.unit_price<=0||rate.daily_quantity_limit<1||rate.daily_quantity_limit>3||slice.quantity<1||slice.quantity>rate.daily_quantity_limit||slice.cost<0||rent<0||wage<0||transport<0)return fail<PrivateShopOperatingResult>("InvalidPrivateShopOperatingCosts");
 const Wide income=Wide(rate.unit_price)*slice.quantity;const Wide expenses=Wide(slice.cost)+rent+wage+transport;
 if(!fits(income)||!fits(expenses))return fail<PrivateShopOperatingResult>("PrivateShopOperatingOverflow");
 return {true,{slice.quantity,static_cast<Integer>(income),slice.cost,rent,wage,transport,static_cast<Integer>(income-expenses)},{}};
}
}
