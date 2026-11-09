// 原税政仅保历史来源；公库转属由真实军事清单和政治登记证成，不授收税权。
#pragma once
#include "politics_takeover_rules.hpp"
#include "resident/domains/politics/politics_fiscal_contract.hpp"
namespace hanmo::resident {
class PoliticalTakeoverFiscalAccess final:public FiscalPolicyOccupationRead {
public:
    explicit PoliticalTakeoverFiscalAccess(PoliticalTakeoverContext context):context_(context){}
    Outcome<std::optional<FiscalPolicyOccupationBoundary>> fiscal_policy_occupation(const FiscalPolicySnapshot&)const override;
private:
    PoliticalTakeoverContext context_;
};
}
