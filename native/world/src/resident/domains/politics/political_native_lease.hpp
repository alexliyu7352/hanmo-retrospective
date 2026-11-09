// 同一 actual-M owning scope 的真实政治子图；外层保活所有跨域原来源。
#pragma once
#include "political_calendar_native_group.hpp"
#include "resident/domains/local_economy/population_training_history.hpp"
#include "resident/kernel/native_domain_read_versions.hpp"

namespace hanmo::resident {
struct NativePoliticalExactDependencies {
  PoliticalCalendarReadInputs calendar;
  const CommonCatalog &common_catalog;
  const LocalEconomyCatalog &economy_catalog;
  const PoliticsCatalog &politics_catalog;
  const OrganizationLifecycleCatalog &lifecycle_catalog;
  const OrganizationLifecycleSourceRead &lifecycle_sources;
  const OrganizationRelationSourceRead &relation_sources;
  // Calendar 仅有任命/辞任三叶，不能冒充移交/继位等全部来源。
  const OfficeTransitionSourceRead &office_transitions;
  const VerifiedDeathRead &deaths;
  const AuthorityIssueSourceRead &authority_sources;
  const AuthorityBudgetRead *budgets;
  const ReadStamp &stamp;
  const ReadStampAuthority &stamp_authority;
  const PublicMarketStaffingContext *staffing;
  const PopulationChangeTimeContext &population;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const SpatialCarrierSources &spatial;
  const ActivityRead &activities;
  const DynamicResultRead &results;
  const OrganizationResourceHistoryRead &resource_history;
  const OrganizationProspectiveGrantRead &prospective_grants;
  const PoliticalTerritoryRead *territories = nullptr;
  const PoliticalTreatyExactReadBinding *treaty_exact = nullptr;
  const MilitaryFormationSourceRead *formations = nullptr;
  const MilitaryReturnSourceRead *returns = nullptr;
  const NativeMilitaryTrainingPeriodRead *periods = nullptr;
};

// 成员次序就是借用寿命。构造不读半成品 World factory，不发布、不持久化。
// 本类及下方 lease 不持跨域 outer owner；仅可放进根的 retaining scope，
// 外层 Complete owner 必须保 core/依赖活到内层 lease 销毁，不能独立发布。
class NativePoliticalExactSources final {
  class WorldClock final : public OrganizationAuthorityClock {
    const WorldBundle &world_;

  public:
    explicit WorldClock(const WorldBundle &world) : world_(world) {}
    Outcome<WorldMoment> read_organization_authority_now() const override;
  };
  const WorldBundle &world_;
  WorldMoment at_;
  NativePoliticalExactDependencies dependencies_;
  WorldClock clock_;
  NativeDomainReadVersionSource versions_;
  NativePoliticalCalendarReadGroup calendar_;
  OrganizationAuthoritySources offices_;
  PopulationChangeTimeContext training_population_;
  OriginalTrainingHistoryContext training_history_;
  NativeOrganizationTrainingCoverageRead training_;
  OrganizationCompleteValidationContext organization_;

public:
  NativePoliticalExactSources(const WorldBundle &, const WorldMoment &,
                              NativePoliticalExactDependencies);
  NativePoliticalExactSources(const NativePoliticalExactSources &) = delete;
  NativePoliticalExactSources &
  operator=(const NativePoliticalExactSources &) = delete;
  NativePoliticalExactSources(NativePoliticalExactSources &&) = delete;
  NativePoliticalExactSources &
  operator=(NativePoliticalExactSources &&) = delete;
  // 只核本次真实装配/读票，不替代 political_exact_phase 的完整原源校验。
  Status validate_bindings() const;
  PoliticalExactReadBinding read_binding() const;
  const NativePoliticalCalendarReadGroup &calendar() const { return calendar_; }
  const NativeOrganizationTrainingCoverageRead &training() const {
    return training_;
  }
  const NativePoliticalExactDependencies &dependencies() const {
    return dependencies_;
  }
};

class NativePoliticalExactReadLease final : public PoliticalExactReadLease {
  NativePoliticalExactSources sources_;

public:
  NativePoliticalExactReadLease(const WorldBundle &world, const WorldMoment &at,
                                NativePoliticalExactDependencies dependencies)
      : sources_(world, at, std::move(dependencies)) {}
  Status validate_bindings() const { return sources_.validate_bindings(); }
  PoliticalExactReadBinding bindings() const override {
    return sources_.read_binding();
  }
  const NativePoliticalExactSources &sources() const { return sources_; }
};

struct NativePoliticalExactWriteDependencies {
  NativePoliticalExactDependencies read;
  CommonStore &common;
  OrganizationState &organization;
  OrganizationRuntime &runtime;
  LocalEconomyState &economy;
  PopulationTransportState &population;
  HouseholdPurchaseState &purchases;
  PoliticsState &politics;
  const MembershipReleaseRead &membership_releases;
  PublicMarketStaffingEffectsWrite *staffing_effects;
  ProductionPopulationChangeContext &production_population;
  PoliticalTreatyExactWriteBinding *treaty_exact = nullptr;
};

class NativePoliticalExactWriteLease final : public PoliticalExactWriteLease {
  WorldBundle &world_;
  NativePoliticalExactWriteDependencies dependencies_;
  NativePoliticalExactSources sources_;
  NativePoliticalCalendarWriteGroup calendar_;

public:
  NativePoliticalExactWriteLease(WorldBundle &, const WorldMoment &,
                                 NativePoliticalExactWriteDependencies);
  NativePoliticalExactWriteLease(const NativePoliticalExactWriteLease &) =
      delete;
  NativePoliticalExactWriteLease &
  operator=(const NativePoliticalExactWriteLease &) = delete;
  NativePoliticalExactWriteLease(NativePoliticalExactWriteLease &&) = delete;
  NativePoliticalExactWriteLease &
  operator=(NativePoliticalExactWriteLease &&) = delete;
  Status validate_bindings() const;
  PoliticalExactWriteBinding bindings() override;
  const NativePoliticalExactSources &sources() const { return sources_; }
};
} // namespace hanmo::resident
