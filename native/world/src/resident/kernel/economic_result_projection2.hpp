// P10 reads the original economic receipts. Exact transport debt legs take precedence over calendar publication.
#pragma once
#include "dynamic_result_time2.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp"
#include "resident/domains/local_economy/supply/supply_policy_boundary.hpp"
#include "resident/domains/local_economy/economic_transport_sources.hpp"
namespace hanmo::resident {
struct EconomicResultProjectionAccess {
 const CommonStore& common;const LocalEconomyState& economy;
 const FiscalSettlementCatalog* fiscal_catalog=nullptr;const FiscalSettlementState* fiscal=nullptr;
 const OccupationFiscalSettlementState* occupation=nullptr;
 const EconomicSupplyPolicyContext* supply=nullptr;
 // Null means this capability was not selected, not that a selected table failed to decode.
 const EconomicTransportExpenseState* transport=nullptr;
};
bool native_economic_publication_kind(const FactRecord&);
Outcome<ExactOwnerFactRead> read_economic_publication_v2(const EconomicResultProjectionAccess&,const FactRecord&);
}
