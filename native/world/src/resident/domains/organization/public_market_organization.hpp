// 公共市场批准和终局只沿原OrgActivity/OrganizationFact与唯一AuthorityBasisRegistry。
#pragma once
#include "organization_activity_history.hpp"
#include "organization_runtime.hpp"
#include "resident/domains/local_economy/household_purchase_contract.hpp"
#include "resident/domains/local_economy/public_market_staffing.hpp"
namespace hanmo::resident {
struct PublicMarketOrganizationReadContext {
  const CommonStore &common;
  const OrganizationState &organization;
  const OrganizationCatalog &catalog;
  const OrganizationRuntime &runtime;
  const CatalogIdentity &catalog_identity;
  const HouseholdPurchaseState &purchases;
  const LocalEconomyState &economy;
  const PopulationTransportState &population;
  const OrganizationAuthorityClock &clock;
};
// 完整冷读与原真实签发时间读；返回来源值不会改变状态或重签旧权。
class NativePublicMarketOrganizationRead final
    : public AuthorityIssueSourceRead,
      public OrganizationActivitySourceMomentRead {
public:
  explicit NativePublicMarketOrganizationRead(
      PublicMarketOrganizationReadContext x)
      : context_(x) {}
  Outcome<WorldMoment> read_organization_authority_now() const override;
  Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &) const override;
  Outcome<WorldMoment> read_organization_activity_source_moment(
      const OrganizationFact &) const override;
  Outcome<OrganizationFact>
  read_verified_public_market_staffing_approval(const SourceRef &) const;
  Outcome<OrganizationFact>
  read_verified_public_market_staffing_boundary(const SourceRef &) const;
  Status validate_offer(const PublicMarketStaffingOffer &) const;
  const PublicMarketOrganizationReadContext &context() const {
    return context_;
  }

private:
  PublicMarketOrganizationReadContext context_;
};
// 市场批准借同一候选的原日历；缺少精确准入来源时由原活动实现拒绝。
struct PublicMarketOrganizationWriteContext {
  CommonStore &common;
  OrganizationState &organization;
  OrganizationRuntime &runtime;
  const OrganizationCatalog &catalog;
  const CatalogIdentity &identity;
  LocalEconomyState &economy;
  PopulationTransportState &population;
  HouseholdPurchaseState &purchases;
  const ActivityRead &activity;
  const DynamicResultRead &results;
  const OrganizationAuthorityClock &clock;
  const OrganizationCalendarAdmissionRead *calendar = nullptr;
};
// 只由同一S5接受候选的真实组合构造；没有外部system/allow_inflight参数。
class NativePublicMarketStaffingOrganizationWrite final
    : public PublicMarketStaffingOrganizationWrite {
public:
  explicit NativePublicMarketStaffingOrganizationWrite(
      PublicMarketOrganizationWriteContext x)
      : context_(x) {}
  Outcome<SourceRef> record_public_market_staffing_offer_authority_in_candidate(
      const SourceRef &, const PublicMarketStaffingTerms &,
      const ActingAuthority &) override;
  Outcome<OrganizationEffects> approve_public_market_staffing_in_candidate(
      const SourceRef &, const SourceRef &, const StableId &, const StableId &,
      const ActingAuthority &) override;

private:
  PublicMarketOrganizationWriteContext context_;
};
// 到期只消费原已接受责任；取消另核当前专项Grant和原真实ReleaseConsent。
Outcome<OrganizationEffects> end_public_market_staffing_in_candidate(
    PublicMarketOrganizationWriteContext &, const PublicMarketStaffingContext &,
    PublicMarketStaffingEffectsWrite &, const StableId &activity);
Outcome<OrganizationEffects> cancel_public_market_staffing_in_candidate(
    PublicMarketOrganizationWriteContext &, const PublicMarketStaffingContext &,
    PublicMarketStaffingEffectsWrite &, const StableId &binding,
    const StableId &release_consent, const ActingAuthority &);
Status
validate_public_market_organization(const PublicMarketOrganizationReadContext &,
                                    const PublicMarketStaffingContext &);
// 公市确实未选时不伪造来源；须同时通过原 S5 选择门和 S6 反向覆盖。
Status validate_unselected_public_market_organization(
    const CommonStore &, const OrganizationState &,
    const HouseholdPurchaseCatalog &, const HouseholdPurchaseState &,
    const PopulationTransportState &, const HouseholdRetailCatalog &);
} // namespace hanmo::resident
