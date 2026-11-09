// 首店成本必要组件；不声称原生商铺/雇员/历史客户与World接线已完成。
#include "resident/domains/commerce/private_shop_accounting.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("private shop accounting gate "+std::to_string(checks));}
// 分批消费必须保留同一原来源；没有实际资源操作或模拟成交。
void apply(PrivateShopCostBasis& b,const PrivateShopCostSlice& s){b.sold_quantity=s.sold_quantity_after;b.recognized_cost=s.recognized_cost_after;}
}
int main(){
 PrivateShopCostBasis b{"real.purchase",3,8,2,0,0};auto all=quote_private_shop_cost_slice(b,3);check(all.ok&&all.value.cost==10);
 auto first=quote_private_shop_cost_slice(b,1);check(first.ok&&first.value.cost==3);apply(b,first.value);auto second=quote_private_shop_cost_slice(b,1);check(second.ok&&second.value.cost==3);apply(b,second.value);auto last=quote_private_shop_cost_slice(b,1);check(last.ok&&last.value.cost==4&&last.value.recognized_cost_after==all.value.cost);apply(b,last.value);check(!quote_private_shop_cost_slice(b,1).ok);
 b={"real.purchase",3,8,2,1,2};check(!quote_private_shop_cost_slice(b,1).ok);b.recognized_cost=3;check(quote_private_shop_cost_slice(b,1).ok);b.acquisition_fact_id.clear();check(!quote_private_shop_cost_slice(b,1).ok);
 const auto maximum=std::numeric_limits<Integer>::max();b={"real.purchase",maximum,maximum,0,maximum-1,maximum-1};auto bound=quote_private_shop_cost_slice(b,1);check(bound.ok&&bound.value.cost==1&&bound.value.recognized_cost_after==maximum);b.allocated_transport_cash=1;check(!quote_private_shop_cost_slice(b,1).ok);
 for(auto markup:{0,500,1000}){auto rate=quote_private_shop_retail_rate(41,markup);check(rate.ok&&rate.value.unit_price==(markup==0?41:markup==500?43:45)&&rate.value.daily_quantity_limit==(markup==0?3:markup==500?2:1));}
 check(!quote_private_shop_retail_rate(0,0).ok);check(!quote_private_shop_retail_rate(40,501).ok);check(!quote_private_shop_retail_rate(maximum,1000).ok);check(quote_private_shop_retail_rate(maximum,0).ok);
 auto rate=quote_private_shop_retail_rate(40,500);b={"real.purchase",5,100,5,0,0};auto cost=quote_private_shop_cost_slice(b,2);check(cost.ok&&cost.value.cost==42);auto day=quote_private_shop_operating_result(rate.value,cost.value,2,3);check(day.ok&&day.value.retail_income==84&&day.value.net_income==37);
 auto zero=cost.value;zero.quantity=0;check(!quote_private_shop_operating_result(rate.value,zero,2,3).ok);zero.quantity=3;check(!quote_private_shop_operating_result(rate.value,zero,2,3).ok);check(!quote_private_shop_operating_result(rate.value,cost.value,-1,3).ok);
 auto loss=quote_private_shop_operating_result({1,1},{1,8,1,8},2,3);check(loss.ok&&loss.value.net_income==-12);auto overflow=quote_private_shop_operating_result({maximum,3},{2,0,2,0},0,0);check(!overflow.ok);overflow=quote_private_shop_operating_result({1,1},{1,maximum,1,maximum},1,0);check(!overflow.ok);
 std::cout<<"private shop integer cost and retail "<<checks<<"/0; full business runtime not registered\n";
}
