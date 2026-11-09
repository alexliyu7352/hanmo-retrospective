#pragma once
#include "governance_work_expense_contract.hpp"
namespace hanmo::resident {
struct EconomicWorkChargeReadContext {const CommonStore& common;const LocalEconomyState& economy;const EconomicWorkCatalog& catalog;const EconomicWorkSourcesState& state;const ProductionInputSourceRead& inputs;};
// Concrete original S3/S5 branch used by the composed original-source provider.
// nullopt means exactly OutcomeApplied or ReportReceived: their true owner must
// supply its corresponding typed branch. Bad/missing Work sources are errors.
Outcome<std::optional<WorkChargeMeasurement>> read_original_s3_work_charge_evidence(const WorkExecutionRead&,const EconomicWorkChargeReadContext&,const GovernanceWorkExpenseTerms&,const WorkFeeLine&,const WorkChargeEvidence&);
}
