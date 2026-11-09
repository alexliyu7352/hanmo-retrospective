// 分段除法保持精确十二位整数余数，避免为新倍率引入浮点或无必要的中间乘积溢出。
#include "fiscal_income_arithmetic.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
namespace hanmo::resident {
Outcome<FiscalIncomeAmount> calculate_fiscal_income_due(Integer basis,Integer numerator,Integer income,const FiscalIncomeRemainder& before){
    constexpr Integer coarse_scale=100000000,basis_points=10000,fine_scale=1000000000000;
    const auto fail=[](const StableId& code)->Outcome<FiscalIncomeAmount>{return {false,{},common_failure(code).error};};
    if(basis<0||numerator<0||income<0||income>basis_points||before.coarse<0||before.coarse>=coarse_scale||before.fine<0||before.fine>=basis_points)return fail("InvalidFiscalIncomeArithmetic");
    Integer raw=0;if(!local_economy_detail::multiply(basis,numerator,raw))return fail("FiscalIncomeQuantityOverflow");
    const Integer whole=raw/coarse_scale,fraction=raw%coarse_scale;
    Integer scaled_whole=0,scaled_fraction=0,carry=0;
    if(!local_economy_detail::multiply(whole,income,scaled_whole)||!local_economy_detail::multiply(fraction,income,scaled_fraction)||!local_economy_detail::multiply(before.coarse,basis_points,carry)||!local_economy_detail::add(carry,before.fine,carry))return fail("FiscalIncomeQuantityOverflow");
    Integer remainder=(scaled_whole%basis_points)*coarse_scale;
    if(!local_economy_detail::add(remainder,scaled_fraction,remainder)||!local_economy_detail::add(remainder,carry,remainder))return fail("FiscalIncomeQuantityOverflow");
    FiscalIncomeAmount out;out.due=scaled_whole/basis_points;
    if(!local_economy_detail::add(out.due,remainder/fine_scale,out.due))return fail("FiscalIncomeQuantityOverflow");
    remainder%=fine_scale;out.remainder={remainder/basis_points,remainder%basis_points};return {true,out,{}};
}
}
