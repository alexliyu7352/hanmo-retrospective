// 本次候选的原政治日历子组；直接构造真实reader，不回调外层World工厂。
#pragma once
#include "political_exact_phase.hpp"
#include "resident/domains/organization/membership_office_transition.hpp"
#include "resident/domains/organization/organization_clock_adapter.hpp"
#include "resident/domains/organization/organization_population_training_source.hpp"
#include "resident/domains/organization/political_office_transition.hpp"
namespace hanmo::resident {
struct PoliticalCalendarReadInputs {
  PublicMarketOrganizationReadContext market;
  const PoliticsState &politics;
  const WorldTimelineState &timeline;
  const EconomicWorkSourcesState &economic_sources;
  const AuthoredActorFoundationState &foundation;
  const ActorFoundationCatalog &foundation_catalog;
  const std::vector<ActionTimeRegistration> &action_registrations;
  const ActionTimeOriginalSourceRead &action_sources;
  const LifeAgeSources &ages;
};
// 三个原Office来源的有限分派。移交/继位仍由其真实owner另行组合。
class NativePoliticalCalendarOfficeRead final
    : public OfficeTransitionSourceRead {
  const PoliticalOfficeTransitionRead &political_;
  const MembershipOfficeTransitionRead &membership_;

public:
  NativePoliticalCalendarOfficeRead(const PoliticalOfficeTransitionRead &p,
                                    const MembershipOfficeTransitionRead &m)
      : political_(p), membership_(m) {}
  Outcome<VerifiedOfficeTransition>
  read_office_transition(const SourceRef &, const ReadStamp &) const override;
};
// 所有内部引用只指本对象的稳定成员或传入同候选原数据；禁止复制/移动后悬挂。
class NativePoliticalCalendarReadGroup final {
  PoliticalCalendarReadInputs inputs_;
  OrganizationCurrentMomentAdapter current_;
  NativePublicMarketOrganizationRead market_;
  NativeOrganizationOriginalAuthorityRead organization_authority_;
  NativePoliticsAuthorityRead political_authority_;
  NativeOrganizationCalendarSourceMomentRead activity_moments_;
  NativeOrganizationActivityOpeningRead activity_history_;
  EconomicCalendarReadContext opening_;
  NativeOrganizationCalendarAdmissionRead calendar_;
  PoliticalOfficeTransitionRead political_offices_;
  MembershipOfficeTransitionRead membership_offices_;
  NativePoliticalCalendarOfficeRead offices_;
  NativePoliticalActorQualificationRead qualifications_;
  OrganizationTrainingSourceContext training_sources_;

public:
  explicit NativePoliticalCalendarReadGroup(PoliticalCalendarReadInputs);
  NativePoliticalCalendarReadGroup(const NativePoliticalCalendarReadGroup &) =
      delete;
  NativePoliticalCalendarReadGroup &
  operator=(const NativePoliticalCalendarReadGroup &) = delete;
  NativePoliticalCalendarReadGroup(NativePoliticalCalendarReadGroup &&) =
      delete;
  NativePoliticalCalendarReadGroup &
  operator=(NativePoliticalCalendarReadGroup &&) = delete;
  Status validate_same_candidate() const;
  const PoliticalCalendarReadInputs &inputs() const { return inputs_; }
  const CurrentMomentRead &current() const { return current_; }
  const NativePublicMarketOrganizationRead &market() const { return market_; }
  const NativeOrganizationOriginalAuthorityRead &
  organization_authority() const {
    return organization_authority_;
  }
  const NativePoliticsAuthorityRead &political_authority() const {
    return political_authority_;
  }
  const NativeOrganizationActivityOpeningRead &activity_history() const {
    return activity_history_;
  }
  const EconomicCalendarReadContext &opening() const { return opening_; }
  const NativeOrganizationCalendarAdmissionRead &calendar() const {
    return calendar_;
  }
  const NativePoliticalCalendarOfficeRead &offices() const { return offices_; }
  const NativePoliticalActorQualificationRead &qualifications() const {
    return qualifications_;
  }
  const OrganizationTrainingSourceContext &training_sources() const {
    return training_sources_;
  }
};
struct PoliticalCalendarWriteInputs {
  CommonStore &common;
  OrganizationState &organization;
  OrganizationRuntime &runtime;
  LocalEconomyState &economy;
  PopulationTransportState &population;
  HouseholdPurchaseState &purchases;
  const ActivityRead &activities;
  const DynamicResultRead &results;
  const ReadStamp &stamp;
  const MembershipReleaseRead &membership_releases;
  const PublicMarketStaffingContext *staffing;
  PublicMarketStaffingEffectsWrite *staffing_effects;
  ProductionPopulationChangeContext &production_population;
  const OrganizationTrainingCoverageRead &training;
  const HouseholdPurchaseCatalog *household_catalog = nullptr;
  const HouseholdRetailCatalog *retail_catalog = nullptr;
};
// 原写 context 的稳定拥有体；已选公市仍须真 S5
// staffing，未选须原反向门，本类不补空实现。
class NativePoliticalCalendarWriteGroup final {
  const NativePoliticalCalendarReadGroup &reads_;
  PoliticalCalendarWriteInputs inputs_;
  PublicMarketOrganizationWriteContext market_;
  NativePublicMarketStaffingOrganizationWrite market_writer_;
  OrganizationOriginalRuntimeWriteContext original_;
  PoliticalOrganizationRuntimeWriteContext political_;
  OrganizationCalendarWriteContext calendar_;

public:
  NativePoliticalCalendarWriteGroup(const NativePoliticalCalendarReadGroup &,
                                    PoliticalCalendarWriteInputs);
  NativePoliticalCalendarWriteGroup(const NativePoliticalCalendarWriteGroup &) =
      delete;
  NativePoliticalCalendarWriteGroup &
  operator=(const NativePoliticalCalendarWriteGroup &) = delete;
  NativePoliticalCalendarWriteGroup(NativePoliticalCalendarWriteGroup &&) =
      delete;
  NativePoliticalCalendarWriteGroup &
  operator=(NativePoliticalCalendarWriteGroup &&) = delete;
  Status validate_same_candidate() const;
  PublicMarketOrganizationWriteContext &market() { return market_; }
  NativePublicMarketStaffingOrganizationWrite &market_writer() {
    return market_writer_;
  }
  const OrganizationOriginalRuntimeWriteContext &original_runtime() const {
    return original_;
  }
  const PoliticalOrganizationRuntimeWriteContext &political_runtime() const {
    return political_;
  }
  OrganizationCalendarWriteContext &calendar() { return calendar_; }
};
} // namespace hanmo::resident
