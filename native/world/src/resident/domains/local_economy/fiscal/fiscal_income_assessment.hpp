// 新税期只计算原税源应付；余数按真实受益机构与单个公共账户分桶，不持余额或授予权源。
#pragma once
#include "fiscal_income_arithmetic.hpp"
#include "fiscal_settlement.hpp"
namespace hanmo::resident {
enum class FiscalIncomeAccountKind { cash,grain };
struct FiscalIncomeBucketKey {
    StableId district_id,source_id,organization_id,public_account_id;
    FiscalIncomeAccountKind kind=FiscalIncomeAccountKind::cash;
};
struct FiscalIncomeBucket { FiscalIncomeBucketKey key;FiscalIncomeRemainder remainder; };
struct FiscalIncomeAssessmentAmounts {
    Integer cash_due=0,grain_due=0;
    FiscalIncomeRemainder cash_before,grain_before;
    FiscalIncomeBucket cash_after,grain_after;
};
// 调用者须先以具名政治读核原政策及due；本函数不认定政策合法、不导入旧财政1余数。
// 同一受益者改档续桶；新受益者或对应公库从零起，另一种财货的原桶不受影响。
Outcome<FiscalIncomeAssessmentAmounts> calculate_fiscal_income_assessment(
    const StableId& district_id,const FiscalTaxSource&,const FiscalPolicySnapshot&,
    Integer tax_base_bp,Integer produced_grain,Integer income_basis_points,
    const std::vector<FiscalIncomeBucket>& existing);
}
