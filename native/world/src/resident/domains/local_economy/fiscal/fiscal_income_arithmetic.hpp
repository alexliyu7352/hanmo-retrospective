// 占领收入的纯整数算式；调用者必须另验政治权源，此叶不授权评税或移动钱粮。
#pragma once
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// coarse沿原财政1的亿分之一余数，fine保存其下万分之一；没有第二份余额。
struct FiscalIncomeRemainder { Integer coarse=0,fine=0; };
struct FiscalIncomeAmount { Integer due=0;FiscalIncomeRemainder remainder; };
// baseline_numerator仍是原政策倍率乘税基/粮率，分母1亿；income_bp只缩放本期新税基。
// 旧余数不再次乘减收比例，满额时与原财政整数算式一致，不对旧应付或生产量重算。
Outcome<FiscalIncomeAmount> calculate_fiscal_income_due(Integer basis,Integer baseline_numerator,
    Integer income_basis_points,const FiscalIncomeRemainder& before);
}
