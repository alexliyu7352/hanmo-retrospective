// 完整 Care owning 根的原来源装配；只借同候选根，不另存表或安装空 provider。
#pragma once
#include "care_handover_time.hpp"
#include "care_period_original_action.hpp"
#include "care_requirements_state.hpp"
#include "care_separation.hpp"
#include "resident/domains/travel/care_dependent_placement.hpp"

namespace hanmo::resident {
// S1 原 placement 的全部原输入，唯一 handovers 由下面的本域装配绑定。
// 不接受一份预先绑定其它 Care 根的 Context 再偷偷覆盖其指针。
struct CareRequirementsPlacementInputs {
  const CommonCatalog &catalog;
  const MovementExactState &movement;
  const TravelDependentState &dependents;
  const SpatialCarrierState &carriers;
  const TransportRegistrationState &registrations;
  const AuthoredActorFoundationState &foundation;
  const ActorFoundationCatalog &actors;
  const SpatialCarrierSources &spatial;
  const ActorSpatialHeadSourceReadContext &separation_spatial;
  const FamilyCatalog *family_catalog = nullptr;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
  const BirthPlacementRead *births = nullptr;
  const MilitaryActionPlacementRead *military_actions = nullptr;
  Integer selected_dependents_version = 2;
  ExactArithmeticBudget budget{};
};

// 每个必需原业务 reader 都是非空引用；没有“无行所以成功”的缺源替身。
// 它们仍由自己的真实 owner 构造，Lifetime 必须覆盖本同步装配。
struct CareRequirementsOriginalInputs {
  const CommonStore &common;
  const PreparedCatalog &catalog;
  const BornCharacterSourceRead &births;
  const CareRequirementRuleRead &rules;
  const FamilyDeathState &deaths;
  const FamilyLifeCauseSources4 &life;
  const CurrentMomentRead &clock;
  const FamilyContinuationPeopleRead4 &people;
  const ProfileControlEligibilitySources &controls;
  const CareArrangementOriginalRead &arrangements;
  const CharacterConditionSources &body;
  const ActionElapsedStore &elapsed;
  const TimeScaleRef &time_scale;
  const CareHandoverPeriodOriginalRead &handover_periods;
  const CareCoverageOriginalRead &coverage;
  const CareHandoverBusinessRead &handover_business;
  const EconomicCalendarReadContext &calendar;
  const CarePeriodOriginalRead &periods;
  const CareOriginalActionPeriodSources &original_actions;
  const FacilityClaimsState &claims;
  const FacilityClaimsCatalog &claim_catalog;
  const CareFacilityUseOriginalRead &facilities;
};

// 所有回指只在构造初始化表中建立。禁止复制/移动，否则原 Context 的引用会
// 指向另一个候选；也没有公开 setter 或完成后回补 provider 的入口。
class NativeCareRequirementsOriginalSources final {
  const CareRequirementsState &state_;
  const PreparedCatalog &catalog_;
  const CommonCatalog &placement_catalog_;
  const CareHandoverBusinessRead &business_;
  CareGoalSourceContext goals_;
  CareAcceptanceContext acceptance_;
  CareHandoverSourceContext handovers_;
  NativeCareDependentPlacementRead placements_;
  NativeCareActualSeparationRead separations_;
  CareCoverageSources coverage_;
  CarePeriodSources periods_;
  CareFacilityUseSources facilities_;

public:
  NativeCareRequirementsOriginalSources(
      const CareRequirementsState &, const CareRequirementsOriginalInputs &,
      const CareRequirementsPlacementInputs &);
  NativeCareRequirementsOriginalSources(
      const NativeCareRequirementsOriginalSources &) = delete;
  NativeCareRequirementsOriginalSources &
  operator=(const NativeCareRequirementsOriginalSources &) = delete;
  NativeCareRequirementsOriginalSources(
      NativeCareRequirementsOriginalSources &&) = delete;
  NativeCareRequirementsOriginalSources &
  operator=(NativeCareRequirementsOriginalSources &&) = delete;

  // 只暴露原只读 Context，writer 仍需同 owning 根的实际写绑定。
  const CareGoalSourceContext &goals() const noexcept {
    return goals_;
  }
  const CareAcceptanceContext &acceptance() const noexcept {
    return acceptance_;
  }
  const CareHandoverSourceContext &handovers() const noexcept {
    return handovers_;
  }
  const CareCoverageSources &coverage() const noexcept {
    return coverage_;
  }
  const CarePeriodSources &periods() const noexcept {
    return periods_;
  }
  const CareFacilityUseSources &facilities() const noexcept {
    return facilities_;
  }

  // 核实际根/版本/候选身份并调用这些已有 owner 的完整来源验证。
  // 本口不包括尚未接完的 Education/Maturity/occupancy/disposition/clearance/
  // transport 来源与 PIN，不能用它宣称十九表完整发布或 Complete ready。
  Status validate_bound_original_sources() const;
};
} // namespace hanmo::resident
