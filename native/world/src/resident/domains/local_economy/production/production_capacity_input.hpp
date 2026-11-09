// Capacity and asset-use inputs of the existing production cycle.
#pragma once
#include "production_adoption_source.hpp"
#include "resident/domains/politics/takeover/politics_takeover_history2.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Shared interpretation of the original S6 responsibility decision. An absent
// result is an actual pending/suspended responsibility, not a technical
// failure.
Outcome<std::optional<Owner>> read_production_responsibility_execution_owner(
    const PoliticalResponsibilityMomentRead &, const Owner &original,
    const PoliticalResponsibilityRef &,
    const std::vector<StableId> &original_assets);
struct OriginalProductionCapacityInput {
  StableId unit_id;
  MomentOwnerSource adoption_source;
  WorldMoment adopted_at;
  Integer batch_capacity = 0;
  std::vector<SourceRef> condition_source_refs, asset_use_source_refs;
  std::optional<MomentOwnerSource> actual_blocking_source;
};
struct OriginalProductionCapacityContext {
  const EconomicProductionAdoptionContext &original;
  const EconomicProductionAdoptionRead &adoptions;
  const NativePoliticalProductionTakeoverRead *takeovers = nullptr;
  const WorldBundle *unselected_takeover_world = nullptr;
  OriginalProductionCapacityContext(
      const EconomicProductionAdoptionContext &o,
      const EconomicProductionAdoptionRead &a,
      const NativePoliticalProductionTakeoverRead &t)
      : original(o), adoptions(a), takeovers(&t) {}
  OriginalProductionCapacityContext(
      const EconomicProductionAdoptionContext &o,
      const EconomicProductionAdoptionRead &a,
      const NativePoliticalProductionTakeoverRead *t, const WorldBundle *absent)
      : original(o), adoptions(a), takeovers(t),
        unselected_takeover_world(absent) {}
};
// 每次仅读前重核真实未选根及生产来源反覆盖，不缓存“已验证”布尔值。
Status validate_unselected_economic_production_takeover_sources(
    const OriginalProductionCapacityContext &);
Outcome<OriginalProductionCapacityInput>
read_original_production_capacity_input(
    const OriginalProductionCapacityContext &, const StableId &unit,
    const MomentOwnerSource &actual_adoption);
// Validate a frozen source through real half-open elapsed time. This reads old
// condition/rights; it neither installs inputs nor awards work or products.
Status validate_original_production_capacity_window(
    const OriginalProductionCapacityContext &,
    const OriginalProductionCapacityInput &, const MomentSpan &);
} // namespace hanmo::resident
