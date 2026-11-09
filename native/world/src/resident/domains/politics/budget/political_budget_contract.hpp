// S6只拥有批准容量及其历史；S5唯一投影原责任的毛支出与未付承诺。
#pragma once
#include "resident/domains/local_economy/fiscal/budget_execution_contract.hpp"
#include "resident/domains/organization/authority_registry.hpp"
namespace hanmo::resident {
inline constexpr std::size_t political_budget_history_limit = 1000000;
struct PublicBudgetLimit {
  BudgetUnit unit;
  Integer cap = 0, civil_floor = 0, garrison_floor = 0;
  std::vector<StableId> container_refs;
};
struct PublicBudgetCategoryLimit {
  BudgetExecutionPurpose purpose;
  BudgetUnit unit;
  Integer cap = 0;
};
enum class PublicBudgetDecisionKind { authorize, delegate, amend, close };
// 每次真实批准保存不可变revision，父引用固定签发源；当前分授按子最新保留容量读取。
struct BudgetAuthorizationSource {
  WorldIdentity identity;
  StableId budget_id, policy_id, principal_org_id, issuer_actor_id;
  Integer budget_revision = 0, policy_revision = 0;
  BudgetPeriod period;
  ScopeRef scope;
  std::vector<StableId> container_refs;
  std::vector<BudgetExecutionPurpose> purposes;
  std::vector<PublicBudgetLimit> total_caps;
  std::vector<PublicBudgetCategoryLimit> category_caps;
  std::optional<BudgetAuthorizationRef> parent_budget_ref;
  AuthorityBasisRef authority_basis_ref;
  SourceRef approval_source;
  WorldMoment approved_at;
  PublicBudgetDecisionKind decision = PublicBudgetDecisionKind::authorize;
  bool closed = false;
};
struct PoliticalBudgetState {
  std::vector<BudgetAuthorizationSource> authorization_history;
  Integer schema_version = 1;
};
struct BudgetDelegationChild {
  BudgetAuthorizationRef child_budget_ref;
  Integer allocated_cap = 0, reclaimable_cap = 0;
};
struct BudgetDelegationView {
  WorldIdentity identity;
  BudgetAuthorizationRef parent_budget_ref;
  BudgetUnit unit;
  Integer period_from_tick = 0, period_until_tick = 0, authorized_cap = 0,
          delegated_cap = 0, retained_cap = 0;
  std::vector<BudgetDelegationChild> children;
};
// 不调用S5，供S5 AuthorizationRead使用，避免BudgetUsage递归调用自己。
struct BudgetAllocationView {
  Integer authorized_cap = 0, delegated_cap = 0, retained_cap = 0;
};
const BudgetAuthorizationSource *
find_budget_authorization(const PoliticalBudgetState &, const StableId &,
                          Integer revision);
const BudgetAuthorizationSource *
current_budget_authorization(const PoliticalBudgetState &, const StableId &);
Status validate_budget_authorization_shape(const BudgetAuthorizationSource &);
Status validate_budget_authorization_shape(const BudgetAuthorizationSource &,
                                           Integer selected_version);
Outcome<BudgetAllocationView>
read_budget_allocation(const PoliticalBudgetState &, const StableId &,
                       Integer expected_revision, const BudgetUnit &);
Outcome<BudgetAllocationView>
read_budget_category_allocation(const PoliticalBudgetState &, const StableId &,
                                Integer, const BudgetUnit &,
                                BudgetExecutionPurpose);
// 有限显式查询才调用S5原支用投影；子自己的分授保留也不可回收。
Outcome<BudgetDelegationView>
read_budget_delegation(const PoliticalBudgetState &, const BudgetUsageRead &,
                       const WorldIdentity &, const StableId &,
                       Integer expected_revision, const BudgetUnit &);
} // namespace hanmo::resident
