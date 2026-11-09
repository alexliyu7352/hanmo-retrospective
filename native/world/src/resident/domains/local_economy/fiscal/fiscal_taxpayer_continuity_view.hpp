// 只显示原税期暂停和仍未收回的旧债；金额来自原财政，公职读权限每页重验。
#pragma once
#include "fiscal_taxpayer_continuity.hpp"
#include "fiscal_settlement_view.hpp"
namespace hanmo::resident {
struct FiscalTaxpayerContinuityViewContext {
 const CommonStore& common;const LocalEconomyState& economy;const LocalEconomyCatalog& local;
 const OrganizationState& organization;const OrganizationRead& organizations;const FiscalPolicyRead& policies;
 const FiscalSettlementCatalog& catalog;const FiscalSettlementState& fiscal;
 const EconomicTakeoverReadContext& takeover;const FiscalTaxpayerContinuityContext& continuity;
};
struct FiscalTaxpayerContinuityRow {
 StableId kind,period_id,assessment_id,district_id,site_id,organization_id,policy_fact_id;
 Integer due_tick=0,cash_outstanding=0,grain_outstanding=0;
 std::vector<StableId> source_ids;
};
struct FiscalTaxpayerContinuityPage {FiscalSettlementPage base;std::vector<FiscalTaxpayerContinuityRow> rows;};
Outcome<FiscalSettlementIndex> build_fiscal_taxpayer_continuity_index(const FiscalTaxpayerContinuityViewContext&);
Outcome<FiscalTaxpayerContinuityPage> query_fiscal_taxpayer_continuity(const FiscalTaxpayerContinuityViewContext&,const FiscalSettlementIndex&,const LocalEconomyQuery&);
BoundaryValue encode_fiscal_taxpayer_continuity_page(const FiscalTaxpayerContinuityPage&);
}
