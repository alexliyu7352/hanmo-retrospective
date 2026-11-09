// 接管后财政明细仍按真实inspect公职分页；金额只读新账，原受益组织不会改派。
#pragma once
#include "occupation_fiscal_settlement.hpp"
#include "fiscal_settlement_view.hpp"
namespace hanmo::resident {
struct OccupationFiscalAssessmentDetail {
    StableId assessment_id,public_wallet_id,public_inventory_id,occupation_fact_id,registration_fact_id,collection_pause_reason;
    Integer income_basis_points=10000;
    FiscalIncomeRemainder cash_bucket,grain_bucket;
    std::vector<StableId> payment_fact_ids;
};
struct OccupationFiscalSuspensionRow {StableId fact_id,district_id,site_id,organization_id,policy_fact_id,public_wallet_id,public_inventory_id;Integer due_tick=0;std::vector<StableId> source_ids;};
struct OccupationFiscalSettlementPage {FiscalSettlementPage base;std::vector<OccupationFiscalAssessmentDetail> details;std::vector<OccupationFiscalSuspensionRow> suspensions;};
Outcome<FiscalSettlementIndex> build_occupation_fiscal_settlement_index(const OccupationFiscalSettlementContext&,const OccupationFiscalSettlementState&,const OrganizationState&);
Outcome<OccupationFiscalSettlementPage> query_occupation_fiscal_settlement(const OccupationFiscalSettlementContext&,const OccupationFiscalSettlementState&,const FiscalSettlementIndex&,const LocalEconomyQuery&);
BoundaryValue encode_occupation_fiscal_settlement_page(const OccupationFiscalSettlementPage&);
}
