// 原税基与粮率保持旧算式，额外收入系数仅作用本期基数，精确余数按对应受益公库续接。
#include "fiscal_income_assessment.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
using BucketIdentity=std::tuple<StableId,StableId,StableId,StableId,int>;
BucketIdentity identity(const FiscalIncomeBucketKey& key){return {key.district_id,key.source_id,key.organization_id,key.public_account_id,static_cast<int>(key.kind)};}
bool valid_key(const FiscalIncomeBucketKey& key){return !key.district_id.empty()&&!key.source_id.empty()&&!key.organization_id.empty()&&!key.public_account_id.empty()&&(key.kind==FiscalIncomeAccountKind::cash||key.kind==FiscalIncomeAccountKind::grain);}
}
Outcome<FiscalIncomeAssessmentAmounts> calculate_fiscal_income_assessment(const StableId& district,const FiscalTaxSource& source,const FiscalPolicySnapshot& policy,Integer tax_base,Integer produced,Integer income,const std::vector<FiscalIncomeBucket>& existing){
    const auto fail=[&](const StableId& code)->Outcome<FiscalIncomeAssessmentAmounts>{return {false,{},common_failure(code,source.id).error};};
    FiscalIncomeAssessmentAmounts out;
    out.cash_after.key={district,source.id,policy.organization_id,policy.public_wallet_id,FiscalIncomeAccountKind::cash};
    out.grain_after.key={district,source.id,policy.organization_id,policy.public_inventory_id,FiscalIncomeAccountKind::grain};
    if(!valid_key(out.cash_after.key)||!valid_key(out.grain_after.key)||tax_base<5000||tax_base>15000||produced<0||source.base_cash_due<0||source.base_cash_due>2147483647||source.grain_share_bp<0||source.grain_share_bp>10000||source.civilian_grain_reserve<0||source.civilian_grain_reserve>2147483647||income<0||income>10000)return fail("InvalidFiscalIncomeAssessment");
    Integer factor=0;switch(policy.mode){case FiscalTaxMode::lenient:factor=7000;break;case FiscalTaxMode::normal:factor=10000;break;case FiscalTaxMode::surcharge:factor=12500;break;default:return fail("InvalidFiscalTaxMode");}
    // 不仅核当次匹配行；隐藏损坏或重复的历史余数也不能静默留到以后消费。
    std::set<BucketIdentity> seen;const auto cash_key=identity(out.cash_after.key),grain_key=identity(out.grain_after.key);
    for(const auto& row:existing){
        if(!valid_key(row.key)||row.remainder.coarse<0||row.remainder.coarse>=100000000||row.remainder.fine<0||row.remainder.fine>=10000||!seen.insert(identity(row.key)).second)return fail("InvalidFiscalIncomeBucket");
        if(identity(row.key)==cash_key)out.cash_after.remainder=row.remainder;
        if(identity(row.key)==grain_key)out.grain_after.remainder=row.remainder;
    }
    out.cash_before=out.cash_after.remainder;out.grain_before=out.grain_after.remainder;
    Integer cash_factor=0,grain_factor=0;
    if(!local_economy_detail::multiply(factor,tax_base,cash_factor)||!local_economy_detail::multiply(factor,source.grain_share_bp,grain_factor))return fail("FiscalIncomeQuantityOverflow");
    grain_factor=std::min<Integer>(100000000,grain_factor);
    const auto cash=calculate_fiscal_income_due(source.base_cash_due,cash_factor,income,out.cash_after.remainder);if(!cash.ok)return {false,{},cash.error};
    const auto grain=calculate_fiscal_income_due(std::max<Integer>(0,produced-source.civilian_grain_reserve),grain_factor,income,out.grain_after.remainder);if(!grain.ok)return {false,{},grain.error};
    out.cash_due=cash.value.due;out.grain_due=grain.value.due;out.cash_after.remainder=cash.value.remainder;out.grain_after.remainder=grain.value.remainder;return {true,std::move(out),{}};
}
}
