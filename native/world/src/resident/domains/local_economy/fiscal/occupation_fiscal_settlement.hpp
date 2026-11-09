// 接管后财政显式新段，沿原地区税期评税；旧财政1与旧欠款不接纳新政策来源。
#pragma once
#include "fiscal_income_assessment.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_contract.hpp"
#include "resident/domains/politics/takeover/politics_occupation_taxpayer_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* occupation_fiscal_period_kind="occupation_fiscal_period_assessed";
inline constexpr const char* occupation_fiscal_assessment_kind="occupation_fiscal_source_assessed";
inline constexpr const char* occupation_fiscal_suspension_kind="occupation_fiscal_period_suspended";
inline constexpr const char* occupation_fiscal_payment_kind="occupation_fiscal_payment";
struct OccupationFiscalPeriodAssessment {
    StableId id,legacy_period_id,district_id;
    Integer period_start_tick=0,due_tick=0,tax_base_bp=10000;
    FiscalPolicySnapshot policy;
    StableId registration_fact_id,occupation_fact_id;
    PoliticalOccupationRuleSource occupation_rule;
    Integer occupation_tick=0,integration_until_tick=0,income_basis_points=10000;
    std::vector<StableId> source_assessment_ids;
};
// 原账期整数钱粮责任与实际收据结构复用；仅新增原亿分之一以下细余数。
struct OccupationFiscalSourceAssessment {
    FiscalSourceAssessment assessment;
    Integer cash_fine_before=0,grain_fine_before=0,cash_fine_after=0,grain_fine_after=0;
};
// 原财政分数税权仅从最后真实旧期一次认领，旧段保留原字节且不再消费。
struct OccupationFiscalLegacyCarryClaim {
    FiscalIncomeBucketKey key;
    StableId legacy_period_id,legacy_assessment_id,legacy_policy_fact_id,claim_period_id;
    Integer remainder=0;
};
struct OccupationFiscalTaxSourcePause {StableId source_id;Owner original_taxpayer;OccupationFiscalTaxpayerContainer wallet,inventory;};
struct OccupationFiscalPeriodSuspension {OccupationFiscalPeriodAssessment period;std::vector<OccupationFiscalTaxSourcePause> sources;};
struct OccupationFiscalSettlementState {
    Integer version=1,initialized_tick=0,last_step_tick=0;
    std::vector<FiscalIncomeBucket> buckets;
    std::vector<OccupationFiscalLegacyCarryClaim> legacy_carry_claims;
    OrderedTable<OccupationFiscalPeriodAssessment> periods;
    OrderedTable<OccupationFiscalPeriodSuspension> suspended_periods;
    OrderedTable<OccupationFiscalSourceAssessment> assessments;
    OrderedTable<FiscalPaymentReceipt> payments;
};
// 所有输入均借原World候选；政治读独立核合法政策，经济只按唯一原周期实际结算。
struct FiscalTaxpayerContinuityContext;
struct OccupationFiscalSettlementContext {
    const CommonStore& common;const LocalEconomyState& economy;const LocalEconomyCatalog& local;
    const OrganizationRead& organizations;const FiscalPolicyRead& legacy_policies;
    const FiscalSettlementCatalog& catalog;const FiscalSettlementState& legacy;
    const OccupationFiscalPolicyRead& policies;
    const EconomicTakeoverReadContext* takeover=nullptr;const DynamicStepInput* phase=nullptr;
    const OccupationFiscalTaxpayerRead* taxpayer_assets=nullptr;
    const FiscalTaxpayerContinuityContext* legacy_continuity=nullptr;
};
// 新签政策具名预检；仅核当前税源可收，不能借真实旧债历史向新Owner征税。
Status validate_occupation_fiscal_new_sources(const OccupationFiscalSettlementContext&,const StableId& site_id);
Outcome<OccupationFiscalSettlementState> make_initial_occupation_fiscal_settlement(const OccupationFiscalSettlementContext&);
Outcome<LocalEconomyEffects> step_occupation_fiscal_settlement(CommonStore&,OccupationFiscalSettlementState&,const OccupationFiscalSettlementContext&,const DynamicStepInput&);
Status validate_occupation_fiscal_settlement(const OccupationFiscalSettlementContext&,const OccupationFiscalSettlementState&);
std::optional<FiscalAssessmentSummary> occupation_fiscal_assessment_result(const CommonStore&,const OccupationFiscalSettlementState&,const StableId& fact_id);
Outcome<std::optional<FiscalAssessmentSummary>> read_occupation_fiscal_period(const OccupationFiscalSettlementContext&,const OccupationFiscalSettlementState&,const StableId& period_fact_id);
std::optional<DynamicFactView> occupation_fiscal_dynamic_fact(const OccupationFiscalSettlementState&,const FiscalSettlementCatalog&,const StableId& fact_id);
// 新段按显式profile初始化，不能以冷读缺段或未知版本默认为空税账。
class ProjectionWriter;
Status emit_occupation_fiscal_settlement_fields(ProjectionWriter&,const OccupationFiscalSettlementState&);
BoundaryValue encode_occupation_fiscal_settlement(const OccupationFiscalSettlementState&);
Outcome<OccupationFiscalSettlementState> decode_occupation_fiscal_settlement(const BoundaryValue&);
Status visit_occupation_fiscal_settlement_checkpoint(CodecVisitor&,OccupationFiscalSettlementState&);
}
