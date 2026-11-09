// 原财政阶段仅调用具名税源连续性门；此处不持政治决定或第二本税款。
#pragma once
#include "fiscal_taxpayer_continuity.hpp"
namespace hanmo::resident::fiscal_taxpayer_continuity_detail {
Status context_clock(const CommonStore&,const FiscalSettlementState&,const FiscalTaxpayerContinuityContext&);
Outcome<bool> available(const CommonStore&,const FiscalTaxSource&,const Owner& original,const FiscalTaxpayerContinuityContext&,Integer tick,const std::optional<StableId>& before={});
Outcome<std::vector<StableId>> blocked_sources(const CommonStore&,const LocalEconomyState&,const FiscalDistrictProfile&,const FiscalTaxpayerContinuityContext&,Integer tick,const StableId& before);
Status remember_pause(FiscalTaxpayerContinuityExecutionContext&,const FiscalPeriodAssessment&,const FiscalPolicySnapshot&,const std::vector<StableId>&);
Status validate_pause(const CommonStore&,const LocalEconomyState&,const FiscalDistrictProfile&,const FiscalPeriodAssessment&,const FiscalPolicySnapshot&,const FiscalTaxpayerContinuityContext&);
Status validate_registry(const CommonStore&,const FiscalSettlementState&,const FiscalTaxpayerContinuityContext&);
}
