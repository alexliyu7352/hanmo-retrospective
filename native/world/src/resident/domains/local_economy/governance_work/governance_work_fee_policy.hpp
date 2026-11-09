// IC2 original author price rules. The catalog is immutable; accepted terms own
// their exact frozen values. No budget amount is interpreted as a price.
#pragma once
#include "governance_work_expense_contract.hpp"
namespace hanmo::resident {
struct GovernanceWorkFeePolicy {
 StableId id,rule_version;WorkKind work_kind=WorkKind::patrol;
 std::vector<WorkFeeLine> fee_lines;std::vector<WorkCostCoverage> cost_coverage;
 WorkAdvanceArrangement advance;
};
struct GovernanceWorkFeeCatalog {
 Integer version=2;CatalogIdentity identity;OrderedTable<GovernanceWorkFeePolicy> policies;
};
SourceRef governance_work_fee_policy_ref(const GovernanceWorkFeePolicy&);
Status validate_governance_work_fee_values(const std::vector<WorkFeeLine>&,const std::vector<WorkCostCoverage>&,const WorkAdvanceArrangement&);
Status validate_governance_work_fee_catalog(const GovernanceWorkFeeCatalog&);
Outcome<const GovernanceWorkFeePolicy*> read_governance_work_fee_policy(const GovernanceWorkFeeCatalog&,const SourceRef&);
Outcome<GovernanceWorkFeeCatalog> decode_governance_work_fee_catalog(const BoundaryValue&,const CatalogIdentity&);
Outcome<BoundaryValue> encode_governance_work_fee_catalog(const GovernanceWorkFeeCatalog&);
Status emit_governance_work_fee_catalog(ProjectionWriter&,const GovernanceWorkFeeCatalog&);
Status visit_governance_work_fee_catalog(CodecVisitor&,GovernanceWorkFeeCatalog&);
// A genuine fresh financial root has no accepted fee sources or reservations.
// Selected /1 and /2 are explicit; restoration never calls this constructor.
Outcome<GovernanceWorkExpenseState> make_initial_governance_work_expenses(const CommonStore&,const GovernanceWorkFeeCatalog&,Integer selected_version);
}
