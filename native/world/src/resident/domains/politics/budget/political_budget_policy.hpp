// 已批准的有限作者政策；不保存余额、支用或第二权源。
#pragma once
#include "political_budget_contract.hpp"
namespace hanmo::resident {
enum class PublicBudgetPolicyPurpose { aid, work };
struct PublicBudgetPolicyScope {
  enum class Kind { organization, jurisdiction };
  Kind kind = Kind::organization;
  StableId id;
};
struct PublicBudgetPolicyLimit {
  BudgetUnit unit;
  Integer cap = 0;
};
struct PublicBudgetPolicyPurposeLimit {
  PublicBudgetPolicyPurpose purpose = PublicBudgetPolicyPurpose::aid;
  BudgetUnit unit;
  Integer cap = 0;
};
struct PublicBudgetPolicy {
  StableId policy_id;
  Integer revision = 0;
  std::vector<PublicBudgetPolicyPurpose> purposes;
  PublicBudgetPolicyScope scope;
  MomentSpan period;
  std::vector<ResourceKey> containers;
  std::vector<PublicBudgetPolicyLimit> total_caps;
  std::vector<PublicBudgetPolicyPurposeLimit> purpose_caps;
};
struct PublicBudgetPolicyCatalog {
  CatalogIdentity identity;
  std::vector<PublicBudgetPolicy> policies;
};
const char *public_budget_policy_purpose_name(PublicBudgetPolicyPurpose);
Status validate_public_budget_policy_shape(const PublicBudgetPolicy &);
Status
validate_public_budget_policy_catalog_shape(const PublicBudgetPolicyCatalog &);
const PublicBudgetPolicy *
find_public_budget_policy(const PublicBudgetPolicyCatalog &, const StableId &,
                          Integer);
} // namespace hanmo::resident
