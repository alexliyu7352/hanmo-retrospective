#include "economic_facility_craft_sources.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/craft/repair_contract.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <limits>

namespace hanmo::resident {
// 首先核借用同根；不能用另一候选的空 claim/context 通过源反读。
bool NativeEconomicCraftFacilityAppliedRead::bound_to(
    const CommonStore &common,
    const FacilityClaimsState &claims) const noexcept {
  const auto &craft = context_.occupancy.craft;
  const auto *repair = context_.occupancy.repair;
  if (repair && (&repair->common != &common ||
                 &repair->items != &craft.materials.instances ||
                 &repair->properties != &craft.materials.properties ||
                 &repair->elapsed != &craft.elapsed))
    return false;
  return &context_.occupancy.craft.materials.common == &common &&
         &context_.physical.common == &common &&
         &context_.occupancy.claims == &claims &&
         &context_.physical.claims == &claims &&
         &context_.occupancy.facility_sources == &context_.physical.state &&
         context_.occupancy.boundaries == &context_;
}

// 统一拒绝跨候选借用，后继业务读取仍由各原来源正文负责。
Status NativeEconomicCraftFacilityAppliedRead::validate_bindings() const {
  if (!bound_to(context_.physical.common, context_.physical.claims))
    return common_failure("EconomicCraftFacilityContextMismatch");
  return common_success();
}

// 原完整 boundary 校验拥有来源/工位/实际资格规则，本层只核所问 slot。
Outcome<CraftFacilityBoundaryView>
NativeEconomicCraftFacilityAppliedRead::read_original_source(
    const FacilitySourceRef &source, const FacilitySlotRef &slot) const {
  const auto binding = validate_bindings();
  if (!binding.ok)
    return {false, {}, binding.error};
  auto result = read_craft_facility_boundary_for_physical_effect(
      context_, {source.source_ref, source.source_schema});
  if (!result.ok)
    return result;
  if (!result.value.original || result.value.owner.slot_ref != slot ||
      !result.value.previous_claim ||
      result.value.previous_claim->slot_ref != slot)
    return {false,
            {},
            common_failure("EconomicCraftFacilitySourceSlotMismatch",
                           source.source_ref.id)
                .error};
  return result;
}

// 时刻只能取已核原 typed 行，不从当前钟或 Common tick 推断。
Outcome<WorldMoment>
NativeEconomicCraftFacilityAppliedRead::read_original_source_moment(
    const FacilitySourceRef &source, const FacilitySlotRef &slot) const {
  const auto result = read_original_source(source, slot);
  if (!result.ok)
    return {false, {}, result.error};
  return {true, result.value.original->occurred_at, {}};
}

// 物理 provider 的 inline 安装序号为零，真实 Common 源使用原 index+1。
Outcome<std::size_t>
NativeEconomicCraftFacilityAppliedRead::read_original_source_order(
    const FacilitySourceRef &source, const FacilitySlotRef &slot) const {
  const auto result = read_original_source(source, slot);
  if (!result.ok)
    return {false, {}, result.error};
  const auto position = read_actual_common_fact_position(
      context_.physical.common, source.source_ref);
  if (!position.ok)
    return {false, {}, position.error};
  if (position.value == std::numeric_limits<std::size_t>::max())
    return {false,
            {},
            common_failure("EconomicCraftFacilitySourceOrderOverflow",
                           source.source_ref.id)
                .error};
  return {true, position.value + 1, {}};
}

// 只列 S9 同一原 episode 的处置，不将空列表冒整槽已处置。
Outcome<std::vector<FacilitySourceRef>>
NativeEconomicCraftFacilityAppliedRead::read_original_dispositions(
    const FacilitySourceRef &source, const FacilitySlotRef &slot) const {
  const auto binding = validate_bindings();
  if (!binding.ok)
    return {false, {}, binding.error};
  return read_craft_facility_original_dispositions(context_, source, slot);
}

// 调用方所给 claim 根也必须是同一原根，再验证实际留存行。
Status
NativeEconomicCraftFacilityAppliedRead::validate_original_retention_entry(
    const FacilityRetentionTransition &transition,
    const FacilityRetentionEntry &entry,
    const FacilityClaimsState &claims) const {
  if (!bound_to(context_.physical.common, claims))
    return common_failure("EconomicCraftFacilityContextMismatch");
  return validate_craft_boundary_retention_entry(context_, transition, entry);
}

// 没有原 boundary 的拒绝沿 S9 传播，不返回空留存冒已完成处置。
Outcome<std::optional<FacilityRetentionEntry>>
NativeEconomicCraftFacilityAppliedRead::read_original_retention_disposition(
    const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt) const {
  const auto binding = validate_bindings();
  if (!binding.ok)
    return {false, {}, binding.error};
  return read_craft_boundary_retention_disposition(context_, claim, receipt);
}

// 物理单元只能来自原边界选位或实际旧占位，不在适配层任意分配。
Outcome<std::vector<FacilityPhysicalUnitRef>>
NativeEconomicCraftFacilityAppliedRead::read_original_admission_units(
    const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt) const {
  const auto binding = validate_bindings();
  if (!binding.ok)
    return {false, {}, binding.error};
  return read_craft_boundary_admission_units(context_, claim, receipt);
}
} // namespace hanmo::resident
