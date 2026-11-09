// Applied 设施来源的真实 S9 部分；不是十四口完成声明，也不代替动态权利或激活。
#pragma once
#include "resident/domains/craft/craft_facility_boundary.hpp"

namespace hanmo::resident {
// 只借同一只读 job 的原 Craft/Repair、物理及 claim 根。返回的原行指针
// 不能保存到持久根，原 context 或任一根发生写入后必须重新读取。
class NativeEconomicCraftFacilityAppliedRead final {
  const CraftFacilityBoundaryReadContext &context_;
  Status validate_bindings() const;

public:
  explicit NativeEconomicCraftFacilityAppliedRead(
      const CraftFacilityBoundaryReadContext &context)
      : context_(context) {}

  bool bound_to(const CommonStore &,
                const FacilityClaimsState &) const noexcept;
  // 唯一已允许来源由 S9 原 reader 决定；不把 boundary 解释成容量变化。
  Outcome<CraftFacilityBoundaryView>
  read_original_source(const FacilitySourceRef &,
                       const FacilitySlotRef &) const;
  Outcome<WorldMoment>
  read_original_source_moment(const FacilitySourceRef &,
                              const FacilitySlotRef &) const;
  Outcome<std::size_t>
  read_original_source_order(const FacilitySourceRef &,
                             const FacilitySlotRef &) const;
  Outcome<std::vector<FacilitySourceRef>>
  read_original_dispositions(const FacilitySourceRef &,
                             const FacilitySlotRef &) const;
  // 三个原业务判断只委托既有 S9 唯一正文，不另抄留存或物理选位规则。
  Status validate_original_retention_entry(const FacilityRetentionTransition &,
                                           const FacilityRetentionEntry &,
                                           const FacilityClaimsState &) const;
  Outcome<std::optional<FacilityRetentionEntry>>
  read_original_retention_disposition(
      const FacilityClaim2 &, const FacilityClaimMutationReceipt &) const;
  Outcome<std::vector<FacilityPhysicalUnitRef>>
  read_original_admission_units(const FacilityClaim2 &,
                                const FacilityClaimMutationReceipt &) const;
};
} // namespace hanmo::resident
