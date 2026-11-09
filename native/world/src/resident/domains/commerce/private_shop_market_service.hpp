#pragma once
#include "private_shop_action_time.hpp"
#include "resident/domains/local_economy/market_service_original.hpp"
namespace hanmo::resident {
// Original S7 concrete, borrowing its actual Operation/acceptance/P29/S0/S4/S1
// owners. No private capacity table, integer attendance adapter, or fake
// source.
class NativePrivateMarketServiceOriginalRead final
    : public PrivateMarketServiceOriginalRead {
  const PrivateShopOperationSourceContext &x_;
  const ProductionFacilityConditionsState *conditions_;
  const CatalogIdentity &identity_;

public:
  NativePrivateMarketServiceOriginalRead(
      const PrivateShopOperationSourceContext &x,
      const ProductionFacilityConditionsState *conditions,
      const CatalogIdentity &identity)
      : x_(x), conditions_(conditions), identity_(identity) {}
  Status
  validate_capacity_source_coverage(const HouseholdPurchaseState &) const;
  Outcome<MarketServiceChangeCause>
  read_private_market_service_change(const MarketServiceDefinition &,
                                     const FacilitySourceRef &) const override;
  Outcome<PrivateMarketServiceBasis>
  read_private_market_basis(const MarketServiceDefinition &,
                            const MarketServiceChangeCause &) const override;
  Outcome<WorldMoment>
  read_private_market_change_moment(const SourceRef &) const override;
  Outcome<FacilitySourceRef>
  read_private_market_change_source(const SourceRef &) const override;
  Outcome<std::vector<MarketServiceWorkerSegment>>
  read_private_market_workers(const PrivateMarketOperationSource &,
                              const MomentSpan &) const override;
};
} // namespace hanmo::resident
