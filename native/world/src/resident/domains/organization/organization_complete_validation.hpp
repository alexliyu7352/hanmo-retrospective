// 完整6/7 body与唯一runtime一组反验。只接同次真实owning
// lease，不接外部full开关。
#pragma once
#include "organization_calendar_admission.hpp"
#include "organization_runtime.hpp"
#include "organization_validation.hpp"
#include "public_market_organization.hpp"
namespace hanmo::resident {
struct OrganizationCompleteValidationContext {
  const OrganizationRuntime &runtime;
  const OrganizationLifecycleCatalog &lifecycle_catalog;
  const OrganizationLifecycleSourceRead &lifecycle_sources;
  const OrganizationRelationSourceRead &relation_sources;
  const OrganizationAuthoritySources &office_sources;
  const AuthorityIssueSourceRead &authority_sources;
  const AuthorityBudgetRead *budgets;
  const OrganizationAuthorityClock &clock;
  const ReadStampAuthority &stamp_authority;
  const DomainReadVersionSource &versions;
  const PublicMarketOrganizationReadContext &market;
  const PublicMarketStaffingContext *staffing;
  const OrganizationCalendarAdmissionRead &calendar;
  const OrganizationTrainingCoverageRead *training = nullptr;
  // 暂定内部接缝：null 只借同 World 原目录证明公市未选，不代表公市能力。
  const HouseholdPurchaseCatalog *household_catalog = nullptr;
  const HouseholdRetailCatalog *retail_catalog = nullptr;
};
Status validate_complete_organization(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const DynamicResultRead *, const ActivityRead *,
    const OrganizationResourceHistoryRead &,
    const OrganizationProspectiveGrantRead &,
    const OrganizationCompleteValidationContext &);
} // namespace hanmo::resident
