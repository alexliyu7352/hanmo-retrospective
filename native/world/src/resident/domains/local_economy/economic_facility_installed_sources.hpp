// 真实初装的有限应用来源；不把未实现的动态权利、激活或处置伪装为空成功。
#pragma once
#include "economic_facility_provider.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// 本轮仅接原 production_asset_facility 的 Installed 自用分支。其他原资产
// 类型仍由其真实产权/租赁 owner 接入，不用初始 title 证明今日使用权。
class NativeEconomicInstalledFacilityAppliedRead final
    : public EconomicFacilityAppliedBusinessRead,
      public CheckpointAllocatedObject {
  EconomicFacilityInstallationReadContext installation_;
  const ProductionFacilityConditionsCatalog &condition_catalog_;
  const ProductionFacilityConditionsState &conditions_;
  const FacilityClaimsState &claims_;

public:
  NativeEconomicInstalledFacilityAppliedRead(
      EconomicFacilityInstallationReadContext,
      const ProductionFacilityConditionsCatalog &,
      const ProductionFacilityConditionsState &, const FacilityClaimsState &);
  NativeEconomicInstalledFacilityAppliedRead(
      const NativeEconomicInstalledFacilityAppliedRead &) = delete;
  NativeEconomicInstalledFacilityAppliedRead &
  operator=(const NativeEconomicInstalledFacilityAppliedRead &) = delete;
  const EconomicFacilityInstallationReadContext &installation() const noexcept {
    return installation_;
  }
  bool bound_to_conditions(
      const ProductionFacilityConditionsCatalog &catalog,
      const ProductionFacilityConditionsState &state) const noexcept {
    return &condition_catalog_ == &catalog && &conditions_ == &state;
  }
  Status validate_bindings() const;
  Outcome<EconomicFacilityActivationRead> read_applied_facility_activation(
      const FacilityActivationReceiptRef &) const override;
  Outcome<std::vector<MomentSpan>> read_complete_applied_facility_opening(
      const SourceRef &, const FacilitySlotRef &) const override;
  Outcome<EconomicFacilityUsageRightBasis>
  read_applied_facility_right_basis(const FacilitySourceRef &,
                                    const FacilitySlotRef &) const override;
  Outcome<std::vector<MomentSpan>>
  read_applied_facility_opening(const SourceRef &, const FacilitySlotRef &,
                                const MomentSpan &) const override;
  Outcome<std::vector<MomentSpan>>
  read_applied_facility_usage_right(const EconomicFacilityUsageRightBasis &,
                                    const PhysicalServiceFacilityExtension &,
                                    const MomentSpan &) const override;
  Outcome<WorldMoment>
  read_applied_facility_change(const FacilitySourceRef &,
                               const FacilitySlotRef &) const override;
  Outcome<std::size_t>
  read_applied_facility_source_order(const FacilitySourceRef &,
                                     const FacilitySlotRef &) const override;
  Status
  validate_original_retention_entry(const FacilityRetentionTransition &,
                                    const FacilityRetentionEntry &,
                                    const FacilityClaimsState &) const override;
  Outcome<EconomicFacilityChangeCause>
  read_applied_capacity_change_basis(const FacilitySourceRef &,
                                     const FacilitySlotRef &) const override;
  Outcome<std::optional<FacilityRetentionEntry>>
  read_original_facility_retention_disposition(
      const FacilityClaim2 &,
      const FacilityClaimMutationReceipt &) const override;
  Outcome<std::vector<FacilityPhysicalUnitRef>>
  read_original_facility_admission_units(
      const FacilityClaim2 &,
      const FacilityClaimMutationReceipt &) const override;
  Outcome<std::vector<FacilitySourceRef>>
  read_original_facility_dispositions(const FacilitySourceRef &,
                                      const FacilitySlotRef &) const override;
  Outcome<std::optional<FacilitySourceRef>>
  read_applied_whole_slot_restriction(const FacilitySourceRef &,
                                      const FacilitySlotRef &) const override;
  Outcome<bool>
  facility_conflict_visible(const ReservationRef &,
                            const FacilityClaim2 &) const override;
};
} // namespace hanmo::resident
