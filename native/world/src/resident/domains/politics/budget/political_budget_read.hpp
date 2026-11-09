#pragma once
#include "political_budget_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PublicBudgetPolicyRead;
// 窄适配只读同一候选真实批准、Common事实和组织历史；没有默认批准分支。
class PoliticalBudgetRead final : public BudgetAuthorizationRead,
                                  public AuthorityBudgetRead,
                                  public AuthorityIssueSourceRead {
public:
  PoliticalBudgetRead(const WorldBundle &, const PoliticalBudgetState &,
                      const AuthorityBasisRegistry &, const OrganizationState &,
                      const OrganizationCatalog &, const CatalogIdentity &,
                      const PublicBudgetPolicyRead *policies = nullptr);
  Outcome<BudgetAuthorizationSnapshot>
  read_budget_authorization(const StableId &, Integer, const BudgetPeriod &,
                            const BudgetUnit &) const override;
  Outcome<std::optional<Integer>>
  read_budget_purpose_cap(const StableId &, Integer, const BudgetPeriod &,
                          const BudgetUnit &,
                          BudgetExecutionPurpose) const override;
  Outcome<BudgetBucketAuthorization>
  read_budget_bucket_authorization(const StableId&,Integer,const BudgetPeriod&,
                                   const BudgetUnit&,BudgetExecutionPurpose) const override;
  Status
  validate_budget_execution_adoption(const BudgetExecutionBinding &,
                                     BudgetExecutionCheck) const override;
  Status validate_budget_authorization(const BudgetAuthorizationRef &,
                                       const AuthorityBasis &) const override;
  Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &) const override;
  Outcome<WorldMoment> read_organization_authority_now() const override;
  Status validate_sources() const;
  // 只核同一个真实候选地址，不代替预算来源验证。
  bool bound_to(const WorldBundle &world,const PoliticalBudgetState &state)const noexcept;
  bool bound_to(const CommonStore &common,
                const PoliticalBudgetState &state) const noexcept {
    return &common_ == &common && &state_ == &state;
  }

private:
  const WorldBundle &world_;
  const CommonStore &common_;
  const PoliticalBudgetState &state_;
  const AuthorityBasisRegistry &authority_bases_;
  const OrganizationState &organization_;
  const OrganizationCatalog &organization_catalog_;
  const CatalogIdentity &catalog_identity_;
  const PublicBudgetPolicyRead *policies_;
};
} // namespace hanmo::resident
