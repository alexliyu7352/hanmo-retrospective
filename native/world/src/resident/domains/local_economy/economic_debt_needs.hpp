// S5's original fiscal/wage -> Affairs adapters at the real Calendar closing boundary.
#pragma once
#include "fiscal/fiscal_need_adapter.hpp"
#include "wages/cohort_wage_need_adapter.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct FiscalDebtNeedSources {
 const FiscalSettlementState& fiscal;const FiscalSettlementCatalog& taxes;const FiscalNeedCatalog& policy;
 // Present only for the explicitly selected occupation-debt Need profile.
 const OccupationFiscalSettlementState* occupation=nullptr;
};
struct CohortDebtNeedSources {
 const CohortWageNeedCatalog& policy;
 const EconomicTakeoverReadContext* takeover=nullptr;const EconomicTakeoverExecutionState* execution=nullptr;
};
struct EconomicDebtNeedClosingContext {
 const LocalEconomyState& economy;const LocalEconomyCatalog& catalog;const CurrentMomentRead& clock;
 std::optional<FiscalDebtNeedSources> fiscal;std::optional<CohortDebtNeedSources> cohort_wages;
};
// This is the one producer called before the ordinary supply/obligation reconciliation.
// Selection comes from the compiled catalog lease, never from a command-supplied flag.
Status reconcile_economic_debt_needs_at_calendar_boundary_in_candidate(CommonStore&,AffairsState&,const EconomicDebtNeedClosingContext&,const ExactPeriodContext&);
}
