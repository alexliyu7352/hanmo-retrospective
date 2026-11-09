// Original P24.1 ActualSeparation. This reader owns no position or Care ledger.
#pragma once
#include "care_coverage.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
namespace hanmo::resident {
struct CareActualSeparationView {
 const FactRecord* actual_source=nullptr;Integer source_schema=0;
 WorldMoment occurred_at;
};
// Finite ordinary-place branch. Dependency/transport terms need their actual
// source owner; no current-location fallback or synthetic movement boundary.
class NativeCareActualSeparationRead {
public:
 NativeCareActualSeparationRead(const ActorSpatialHeadSourceReadContext& spatial,
  const MilitarySpatialHandoffState* military=nullptr):spatial_(spatial),military_(military){}
 bool bound_to(const CommonStore&)const noexcept;
 Outcome<std::optional<CareActualSeparationView>> read_actual_separation(
  const CareBinding&,const AcceptedCaregiver&,const MomentOwnerSource&)const;
 // The genuine Completed and end headers bound a retained Common prefix.
 // Subsequent same-M sources do not enter the prior binding's coverage.
 Outcome<std::optional<CareActualSeparationView>> read_first_actual_separation(
  const CareBinding&,const AcceptedCaregiver&,const SourceRef& actual_completed,
  const MomentSpan& original_coverage_span,
  const std::optional<SourceRef>& before_actual_end={})const;
private:
 const ActorSpatialHeadSourceReadContext& spatial_;
 const MilitarySpatialHandoffState* military_;
};
}
