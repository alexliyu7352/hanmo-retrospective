#pragma once
#include "political_budget_commands.hpp"
#include "political_budget_policy.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
struct PoliticalTakeoverContext;
// Concrete selected military/public-asset lease implements this exact-source
// protocol; no empty/default provider and no caller-invented historical M.
class PublicBudgetTreasuryHistoryRead {public:
 virtual ~PublicBudgetTreasuryHistoryRead()=default;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual Outcome<Owner> read_public_budget_asset_owner_before_approval(
  const PoliticalBudgetState&,const BudgetAuthorizationSource&,const ResourceKey&)const=0;
};
// 借同候选公库/辖区及锁定作者政策；原SingleGrant仍是独立实际权源门。
class NativePublicBudgetPolicyRead final : public PublicBudgetPolicyRead {
public:
  NativePublicBudgetPolicyRead(const WorldBundle &, const PoliticsState &,
                               const PublicBudgetPolicyCatalog &,
                               const CatalogIdentity &,
                               const PoliticalTakeoverContext *,
                               const PoliticalBudgetState &,
                               const PublicBudgetTreasuryHistoryRead &);
  Status validate_approval_terms(const ActingAuthority &,
                                 PublicBudgetDecisionKind,
                                 const PublicBudgetTerms &) const override;
  Status validate_approval_transition_terms(const ActingAuthority&,PublicBudgetDecisionKind,
                                 const PublicBudgetTerms&,const BudgetAuthorizationSource*) const override;
  Status
  validate_historical_terms(const BudgetAuthorizationSource &) const override;
  bool bound_to(const WorldBundle &) const override;
  bool bound_to_budget(const PoliticalBudgetState &s) const override {
    return &s == &budgets_;
  }

private:
  Status validate_terms(const PublicBudgetTerms &, const StableId &principal,
                        const WorldMoment &,const BudgetAuthorizationSource *,const SourceRef *actual_boundary=nullptr) const;
  const WorldBundle &world_;
  const PoliticsState &politics_;
  const PublicBudgetPolicyCatalog &policies_;
  const CatalogIdentity &identity_;
  // Borrow the already built original context, never the later P1/full scope.
  const PoliticalTakeoverContext *takeover_;
  const PoliticalBudgetState &budgets_;
  const PublicBudgetTreasuryHistoryRead &treasury_;
  Outcome<StableId> jurisdiction_owner(const StableId &,
                                       const BudgetAuthorizationSource *) const;
};
} // namespace hanmo::resident
