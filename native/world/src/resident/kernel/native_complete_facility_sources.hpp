// 同一World已选P05和经济09的有限分派；只转交真正原owner，不安装空能力。
#pragma once
#include "resident/facility_claims/facility_claims.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeHouseholdServiceLease;
class NativeEconomicFacilitySourceRead;
class NativeCompleteFacilitySources final : public FacilityClaimSourceRead {
  const WorldBundle &world_;
  const NativeHouseholdServiceLease *household_;
  const NativeEconomicFacilitySourceRead *economic_;
  Outcome<const FacilityClaimSourceRead *> owner(const FacilitySlotRef &) const;

public:
  NativeCompleteFacilitySources(const WorldBundle &,
                                const NativeHouseholdServiceLease *,
                                const NativeEconomicFacilitySourceRead *);
  Status validate_bindings() const;
  Outcome<WorldMoment>
  initial_source_at(const FacilitySlotRef &s) const override;
  Outcome<std::vector<VerifiedFacilityCapacitySegment>>
  read_verified_capacity(const FacilitySourceRef &r, const FacilitySlotRef &s,
                         const MomentSpan &w) const override;
  Outcome<FacilityCapacityChange>
  read_capacity_history_base(const FacilitySlotRef &s,
                             const FacilityHistoryBase &b) const override;
  Outcome<FacilitySourceRef> read_capacity_reconciliation_source(
      const FacilitySlotRef &s, const FacilityCapacityChange &c) const override;
  Outcome<FacilityMutationCapacityOrder> compare_mutation_to_capacity_change(
      const FacilitySlotRef &s, const FacilityClaimMutationReceipt &r,
      const FacilityCapacityChange &c) const override;
  Outcome<FacilitySourceRef>
  capacity_source_at(const FacilitySlotRef &s,
                     const WorldMoment &at) const override;
  Outcome<std::vector<FacilityCapacityChange>>
  enumerate_capacity_changes(const FacilitySlotRef &s, const WorldMoment &a,
                             const WorldMoment &b) const override;
  Outcome<std::vector<FacilityClaimDependency>>
  enumerate_claim_dependencies(const FacilitySlotRef &s) const override;
  Status validate_claim_admission(
      const FacilityClaim2 &c,
      const FacilityClaimMutationReceipt &r) const override;
  Status
  validate_claim_close(const FacilityClaim2 &c, FacilityClaimCloseReason why,
                       const FacilityClaimMutationReceipt &r) const override;
  Status validate_mutation_source(
      const FacilitySlotRef &s,
      const FacilityClaimMutationReceipt &r) const override;
  Status
  validate_pruned_prefix(const FacilitySlotRef &s,
                         const VacantPrunedFacilityPrefix &p) const override;
  Status validate_retention_and_physical_occupancy(
      const FacilitySlotRef &s, const MomentSpan &w,
      const std::vector<const FacilityClaim2 *> &c,
      const FacilityCapacitySource2 &r) const override;
  Outcome<Integer> read_additional_normal_capacity(
      const FacilitySlotRef &s, const MomentSpan &w,
      const std::vector<const FacilityClaim2 *> &c,
      const FacilityCapacitySource2 &r) const override;
  Outcome<bool>
  is_verified_retained_claim(const FacilityClaim2 &c, const MomentSpan &w,
                             const FacilityCapacitySource2 &r) const override;
  Outcome<bool>
  conflicting_source_visible(const ReservationRef &r,
                             const FacilityClaim2 &c) const override;
  Outcome<bool> source_change_affects_claims(
      const FacilityCapacityChange &,
      const std::vector<const FacilityClaim2 *> &) const override;
};
} // namespace hanmo::resident
