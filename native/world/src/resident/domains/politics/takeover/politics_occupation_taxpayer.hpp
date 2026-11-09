// 被占领组织税源的具名产权读；只证明原纳税人资产是否还可用，不授予换纳税人权。
#pragma once
#include "politics_takeover_rules.hpp"
#include "politics_occupation_taxpayer_contract.hpp"
namespace hanmo::resident {
class PoliticalOccupationFiscalTaxpayerAccess final:public OccupationFiscalTaxpayerRead {
public:
    explicit PoliticalOccupationFiscalTaxpayerAccess(PoliticalTakeoverContext context):context_(context){}
    Outcome<OccupationFiscalTaxpayerSource> occupation_fiscal_taxpayer_source(const Owner&,const StableId&,const StableId&,Integer,const std::optional<StableId>&)const override;
private:
    PoliticalTakeoverContext context_;
};
}
