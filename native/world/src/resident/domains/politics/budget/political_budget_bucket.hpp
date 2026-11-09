// PC14 2026-10-27: derived original-source membership, never a second usage ledger.
#pragma once
#include "political_budget_contract.hpp"
namespace hanmo::resident {
struct PublicBudgetBucketMembers {
 StableId principal_org_id;
 BudgetAuthorizationRef root;
 BudgetPeriod period;
 BudgetExecutionPurpose purpose;
 BudgetUnit unit;
 std::vector<BudgetExecutionPurpose> root_accounted_purposes;
 Integer authorized_root_total_cap=0,authorized_root_purpose_cap=0;
 // Each ID occurs exactly once across both lists. Revisions are deliberately
 // not members: S5 visits each original binding/leg exactly once, across revisions.
 std::vector<StableId> current_root_member_budget_ids;
 std::vector<StableId> inherited_root_member_budget_ids;
};
// Closed roots being replaced at an actual new-root boundary. Historical
// calls exclude the boundary and all later facts; hot calls use the real prefix.
Outcome<std::vector<BudgetAuthorizationRef>> read_public_budget_reopening_roots(
 const CommonStore&,const PoliticalBudgetState&,const StableId& principal,
 const BudgetPeriod&,const std::vector<BudgetExecutionPurpose>&,
 const SourceRef* actual_boundary=nullptr);
Status validate_public_budget_bucket_usage(const CommonStore&,const PoliticalBudgetState&,const BudgetUsageRead&);
// Structural and actual-header cold scan. Called by the original S6 validator;
// does not ask S5 for usage and therefore cannot recurse into BudgetUsageRead.
Status validate_public_budget_bucket_history(const CommonStore&,const PoliticalBudgetState&);
Outcome<PublicBudgetBucketMembers> read_public_budget_bucket_members(
 const CommonStore&,const PoliticalBudgetState&,const StableId& budget_id,
 Integer revision,const BudgetPeriod&,const BudgetUnit&,BudgetExecutionPurpose);
}
