#pragma once
#include "../economic_facility_provider.hpp"
#include "facility_condition_contract.hpp"
namespace hanmo::resident {
class NativeEconomicInstalledFacilityAppliedRead;
// Pure projections of the one original installation/condition chain. The
// adopted Common cut is supplied only by an already verified original owner.
struct ProductionFacilityHistoryContext {
  const EconomicFacilityInstallationReadContext &facilities;
  const ProductionFacilityConditionsCatalog &condition_catalog;
  const ProductionFacilityConditionsState &conditions;
  const ProductionFacilityEffectRead *effects;
  const CurrentMomentRead &clock;
  const NativeEconomicInstalledFacilityAppliedRead *installed_sources = nullptr;
  // 原动态effect引用入口保持；仅具名Installed证明可启用初态无effect分支。
  ProductionFacilityHistoryContext(
      const EconomicFacilityInstallationReadContext &f,
      const ProductionFacilityConditionsCatalog &c,
      const ProductionFacilityConditionsState &s,
      const ProductionFacilityEffectRead &e, const CurrentMomentRead &now)
      : facilities(f), condition_catalog(c), conditions(s), effects(&e),
        clock(now) {}
  ProductionFacilityHistoryContext(
      const EconomicFacilityInstallationReadContext &f,
      const ProductionFacilityConditionsCatalog &c,
      const ProductionFacilityConditionsState &s,
      const ProductionFacilityEffectRead *e, const CurrentMomentRead &now,
      const NativeEconomicInstalledFacilityAppliedRead &installed)
      : facilities(f), condition_catalog(c), conditions(s), effects(e),
        clock(now), installed_sources(&installed) {}
};
Status validate_production_facility_history_sources(
    const ProductionFacilityHistoryContext &);
struct ConditionBoundAssetCapacitySegment {
  MomentSpan window;
  ConditionBoundAssetCapacity capacity;
};
// A null cut is restricted to the trusted current Moment. With a cut, changes
// at that same Moment are adopted only if their actual Common head precedes it.
Outcome<ConditionBoundAssetCapacity>
read_production_facility_capacity_at_adoption(
    const ProductionFacilityHistoryContext &, const StableId &original_asset,
    const WorldMoment &, const SourceRef *actual_adoption_cut);
// The original input-change writer adopts after its actual effect source.
// The caller has already verified that real owner cut; this includes its own
// condition change while excluding later same-M changes.
Outcome<ConditionBoundAssetCapacity>
read_production_facility_capacity_after_adoption(
    const ProductionFacilityHistoryContext &, const StableId &original_asset,
    const WorldMoment &, const SourceRef &actual_adoption_cut);
// The whole half-open window is replayed from original effects. Damage and
// repair inside the window remain separate even if the endpoint values agree.
Outcome<std::vector<ConditionBoundAssetCapacitySegment>>
read_production_facility_capacity_window(
    const ProductionFacilityHistoryContext &, const StableId &original_asset,
    const MomentSpan &);
} // namespace hanmo::resident
