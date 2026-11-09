#pragma once
#include "political_budget_read.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct PublicBudgetTerms {
  StableId policy_id;
  Integer policy_revision = 0;
  BudgetPeriod period;
  ScopeRef scope;
  std::vector<StableId> container_refs;
  std::vector<BudgetExecutionPurpose> purposes;
  std::vector<PublicBudgetLimit> total_caps;
  std::vector<PublicBudgetCategoryLimit> category_caps;
};
struct AuthorizePublicBudget {
  ActingAuthority acting;
  PublicBudgetTerms terms;
};
struct DelegatePublicBudget {
  ActingAuthority acting;
  PublicBudgetTerms terms;
  BudgetAuthorizationRef parent;
};
struct AmendPublicBudget {
  ActingAuthority acting;
  StableId budget_id;
  Integer expected_revision = 0;
  PublicBudgetTerms terms;
};
struct ClosePublicBudget {
  ActingAuthority acting;
  StableId budget_id;
  Integer expected_revision = 0;
};
using PublicBudgetCommand =
    std::variant<AuthorizePublicBudget, DelegatePublicBudget, AmendPublicBudget,
                 ClosePublicBudget>;
class PublicBudgetPolicyRead {
public:
  virtual ~PublicBudgetPolicyRead() = default;
  virtual Status
  validate_historical_terms(const BudgetAuthorizationSource &) const = 0;
  virtual bool bound_to(const WorldBundle &) const = 0;
  virtual bool bound_to_budget(const PoliticalBudgetState &) const = 0;
  // 正式目录/当前辖区控制/公有资源与原政策版本。仅Grant存在不替这些业务条件。
  virtual Status validate_approval_terms(const ActingAuthority &,
                                         PublicBudgetDecisionKind,
                                         const PublicBudgetTerms &) const = 0;
  virtual Status validate_approval_transition_terms(const ActingAuthority&,PublicBudgetDecisionKind,
                                         const PublicBudgetTerms&,const BudgetAuthorizationSource*) const {
    return common_failure("PublicBudgetPolicyTransitionSourceRequired",{},ErrorKind::unsupported_capability);
  }
};
struct PublicBudgetWriteContext {
  const WorldBundle &world;
  AuthorityBasisRegistry &authority_bases;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const CatalogIdentity &catalog_identity;
  const BudgetExecutionState &executions;
  const BudgetResponsibilityRead &responsibilities;
  const PublicBudgetPolicyRead &policies;
};
// 只由完整未发布候选调用；失败须连同Common IDs/事实一起丢弃，不单独发布本域。
Outcome<BudgetAuthorizationRef> apply_public_budget_in_candidate(
    CommonStore &, PoliticalBudgetState &, const PublicBudgetWriteContext &,
    const StableId &command_actor, const PublicBudgetCommand &);
} // namespace hanmo::resident
