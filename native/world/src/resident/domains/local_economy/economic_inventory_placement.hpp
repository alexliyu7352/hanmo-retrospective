// Original fixed inventory locations from locked S5/S7/Growth definitions.
// This reads physical placement only; it grants no owner, custody or use right.
#pragma once
#include "local_economy_model.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/domains/travel/inventory_spatial_history.hpp"
namespace hanmo::resident {
struct PrivateShopContext;
struct PrivateShopOpeningReceipt;
struct EconomicFixedInventoryPlacementContext {
  const CommonStore &common;
  const CommonCatalog &shared;
  const LocalEconomyCatalog &economy;
  const EstateCatalog &estates;
  const GrowthCatalog &growth;
  const TravelCatalog &travel;
  // 实际开店后创建的容器只借原 S7 开店来源，不填入初始 required 容器表。
  const PrivateShopContext *private_shops = nullptr;
};
class NativeEconomicFixedInventoryPlacementRead final
    : public FixedInventoryPlacementRead {
  const EconomicFixedInventoryPlacementContext &x_;

public:
  explicit NativeEconomicFixedInventoryPlacementRead(
      const EconomicFixedInventoryPlacementContext &x)
      : x_(x) {}
  Outcome<std::vector<StableId>>
  enumerate_original_fixed_inventory_ids() const override;
  Outcome<FixedInventoryPlacement>
  read_original_fixed_inventory_placement(const StableId &) const override;
  Status
  validate_original_fixed_inventory_creation(const CommonStore &,
                                             const CommonCatalog &,
                                             const StableId &) const override;
};
// Finite static source resolver. Only the exact locked row can produce
// evidence; no Common fact, current cohort location, or new physical state is
// installed here.
Outcome<FixedInventoryPlacement> read_economic_fixed_inventory_placement_source(
    const EconomicFixedInventoryPlacementContext &, const MomentOwnerSource &);
// 仅原开店同候选尾调用；旧档缺绑定不得由这里补写。
Status bind_opened_private_shop_inventory_in_candidate(
    SpatialCarrierState &, const EconomicFixedInventoryPlacementContext &,
    const PrivateShopOpeningReceipt &actual_opening, const CurrentMomentRead &);
} // namespace hanmo::resident
