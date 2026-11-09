// SOURCE_ONLY：显式日租期次与有限欠额纯边界；此测试不创建运行期付款或清算权。
#include "resident/domains/commerce/daily_rent/private_shop_rent_period.hpp"
#include <iostream>
#include <limits>
using namespace hanmo::resident;
namespace {Integer checks=0,failed=0;void check(bool ok,const char* name){++checks;if(!ok){++failed;std::cerr<<name<<'\n';}}}
int main(){
    for(Integer opened=0;opened<8;++opened)for(Integer index=0;index<8;++index){
        auto p=quote_private_shop_rent_period(opened,index);check(p.ok,"valid original period");if(!p.ok)continue;
        check(p.value.index==index&&p.value.from_tick==opened+2*index&&p.value.through_tick==opened+2*index+2,"exact two-halfday interval");
        for(Integer delta=0;delta<2;++delta){auto at=locate_private_shop_rent_period(opened,p.value.from_tick+delta);check(at.ok&&at.value.index==index,"same original period contains both halfdays");}
        auto next=locate_private_shop_rent_period(opened,p.value.through_tick);check(next.ok&&next.value.index==index+1,"exclusive end is next actual period");
    }
    check(!quote_private_shop_rent_period(-1,0).ok,"negative opening rejected");check(!quote_private_shop_rent_period(0,-1).ok,"negative period rejected");check(!locate_private_shop_rent_period(4,3).ok,"preopening observation rejected");
    const auto max=std::numeric_limits<Integer>::max();auto last=quote_private_shop_rent_period(max-2,0);check(last.ok&&last.value.through_tick==max,"last representable complete period");
    check(!quote_private_shop_rent_period(max-1,0).ok,"unrepresentable end rejected");check(!quote_private_shop_rent_period(1,max).ok,"multiplication overflow rejected");check(!locate_private_shop_rent_period(0,max).ok,"terminal halfday cannot create incomplete interval");
    auto empty=quote_private_shop_rent_funding(2,0);check(empty.ok&&!empty.value.fully_funded&&empty.value.payable_cash==0&&empty.value.unpaid_cash==2,"one actual unpaid period only");
    auto shortfall=quote_private_shop_rent_funding(2,1);check(shortfall.ok&&shortfall.value.payable_cash==0&&shortfall.value.unpaid_cash==2,"no silent partial debit");
    auto paid=quote_private_shop_rent_funding(2,2);check(paid.ok&&paid.value.fully_funded&&paid.value.payable_cash==2&&paid.value.unpaid_cash==0,"original exact charge");
    auto rich=quote_private_shop_rent_funding(2,max);check(rich.ok&&rich.value.payable_cash==2,"balance is not income or extra rent");
    check(!quote_private_shop_rent_funding(0,3).ok&&!quote_private_shop_rent_funding(2,-1).ok,"bad fee or cash rejected");
    std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
