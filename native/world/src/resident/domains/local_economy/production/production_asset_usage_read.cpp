// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 唯一原安装/own_initial_asset_use 校验仍由原 owner
// 执行；这里仅追加实际终止头。
#include "production_asset_usage_read.hpp"
#include "../economic_facility_initial_usage.hpp"

namespace hanmo::resident {
Outcome<ProductionAssetInitialUsageRead>
read_production_asset_initial_usage_history(
    const ProductionAssetLifecycleReadContext &x, const FacilitySourceRef &ref,
    const Owner &original_owner) {
  if (ref.source_schema != 1)
    return {false,
            {},
            common_failure("ProductionAssetInitialUsageSchemaRequired",
                           ref.source_ref.id)
                .error};
  auto installed = read_economic_facility_initial_owned_usage(
      x.installation, ref.source_ref, original_owner);
  if (!installed.ok)
    return {false, {}, installed.error};
  auto title = read_current_production_asset_title(
      x, installed.value->original_asset_ref);
  if (!title.ok)
    return {false, {}, title.error};
  if (title.value.original_facility != installed.value)
    return {false,
            {},
            common_failure("ProductionAssetOriginalUsageBindingMismatch",
                           ref.source_ref.id)
                .error};
  const auto &basis = std::get<InstalledPhysicalFacility>(
      installed.value->initialization_basis);
  ProductionAssetInitialUsageRead out{installed.value,
                                      &basis.installation_evidence.installed_at,
                                      nullptr, nullptr};
  // 当前 title 的完整只读核已经逐源验证全部交割。这里按实际 Common 顺序
  // 取该资产第一次真实易主，不把 vector 行序或今日 Owner 相等当历史延续。
  for (const auto &entry : x.installation.common.state().facts.rows()) {
    if (entry.second.domain != "local_economy" ||
        entry.second.kind != "production_asset_title_transferred")
      continue;
    for (const auto &transfer : x.state.transfers) {
      if (transfer.transfer_id != entry.first)
        continue;
      for (const auto &offer : x.state.offers)
        if (offer.offer_id == transfer.offer_id &&
            offer.terms.asset_ref == installed.value->original_asset_ref) {
          out.ended_at = &transfer.applied_at;
          out.actual_termination = &transfer;
          return {true, out, {}};
        }
    }
  }
  return {true, out, {}};
}
} // namespace hanmo::resident
