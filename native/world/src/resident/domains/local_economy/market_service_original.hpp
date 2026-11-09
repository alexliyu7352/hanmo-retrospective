#pragma once
#include "economic_facility_provider.hpp"
#include "household_purchase_contract.hpp"
#include "market_service_capacity.hpp"
#include "public_market_authority_read.hpp"
namespace hanmo::resident {
struct WorldBundle;
// S7只提供自己的真实开店/经营/员工参与来源，S5不复制店铺或经营进度。
struct PrivateMarketServiceBasis {
  PrivateMarketAssetBasis asset;
  MarketServiceOpeningBasis opening;
  std::vector<PrivateMarketOperationSource> staffing;
  MarketServiceUsageRightBasis usage;
};
class PrivateMarketServiceOriginalRead {
public:
  virtual ~PrivateMarketServiceOriginalRead() = default;
  virtual Outcome<MarketServiceChangeCause>
  read_private_market_service_change(const MarketServiceDefinition &,
                                     const FacilitySourceRef &) const = 0;
  virtual Outcome<PrivateMarketServiceBasis>
  read_private_market_basis(const MarketServiceDefinition &,
                            const MarketServiceChangeCause &) const = 0;
  virtual Outcome<WorldMoment>
  read_private_market_change_moment(const SourceRef &) const = 0;
  virtual Outcome<FacilitySourceRef>
  read_private_market_change_source(const SourceRef &) const = 0;
  virtual Outcome<std::vector<MarketServiceWorkerSegment>>
  read_private_market_workers(const PrivateMarketOperationSource &,
                              const MomentSpan &) const = 0;
};
struct MarketServiceOriginalContext {
  const CatalogIdentity &identity;
  const CommonStore &common;
  const HouseholdPurchaseCatalog &catalog;
  const HouseholdPurchaseState &purchases;
  // production_management 与成对的 09 根各按锁目录选择，未选必须为空。
  const ProductionManagementCatalog *assets;
  const PrivateShopCatalog *shops_catalog;
  const EconomicFacilitySourcesState *installed;
  const ProductionFacilityConditionsCatalog *condition_catalog;
  const ProductionFacilityConditionsState *conditions;
  // 暂定：未选公市时可不借公市权源；真实公分派必须显式拒缺源，不提供空成功。
  const PublicMarketAuthorityRead *public_authority;
  const PrivateMarketServiceOriginalRead *private_business;
  WorldMoment current;
  ExactArithmeticBudget arithmetic{};
  const WorldBundle *unselected_shops_world = nullptr;
  MarketServiceOriginalContext(
      const CatalogIdentity &i, const CommonStore &c,
      const HouseholdPurchaseCatalog &k, const HouseholdPurchaseState &p,
      const ProductionManagementCatalog *a, const PrivateShopCatalog *shops,
      const EconomicFacilitySourcesState *f,
      const ProductionFacilityConditionsCatalog *cc,
      const ProductionFacilityConditionsState *cs,
      const PublicMarketAuthorityRead *authority,
      const PrivateMarketServiceOriginalRead *business, WorldMoment now,
      ExactArithmeticBudget budget = {}, const WorldBundle *absent = nullptr)
      : identity(i), common(c), catalog(k), purchases(p), assets(a),
        shops_catalog(shops), installed(f), condition_catalog(cc),
        conditions(cs), public_authority(authority), private_business(business),
        current(std::move(now)), arithmetic(budget),
        unselected_shops_world(absent) {}
  MarketServiceOriginalContext(const CatalogIdentity &i, const CommonStore &c,
                               const HouseholdPurchaseCatalog &k,
                               const HouseholdPurchaseState &p,
                               const ProductionManagementCatalog *a,
                               const PrivateShopCatalog &shops,
                               const EconomicFacilitySourcesState *f,
                               const ProductionFacilityConditionsCatalog *cc,
                               const ProductionFacilityConditionsState *cs,
                               const PublicMarketAuthorityRead *authority,
                               const PrivateMarketServiceOriginalRead &business,
                               WorldMoment now,
                               ExactArithmeticBudget budget = {})
      : MarketServiceOriginalContext(i, c, k, p, a, &shops, f, cc, cs,
                                     authority, &business, std::move(now),
                                     budget) {}
};
// 只按锁目录显式选择借原根；未选时同时反查作者、已存市场来源及 Common 孤头。
Status validate_market_service_condition_selection(
    const CatalogIdentity &, const CommonStore &,
    const ProductionFacilityConditionsState *);
Status validate_market_service_original_physical_sources(
    const MarketServiceOriginalContext &);
class NativeMarketServiceOriginalRead final : public MarketServiceOriginalRead {
  const MarketServiceOriginalContext &x_;

public:
  explicit NativeMarketServiceOriginalRead(
      const MarketServiceOriginalContext &x)
      : x_(x) {}
  Outcome<MarketServiceChangeCause> resolve_original_market_service_change(
      const MarketServiceDefinition &,
      const FacilitySourceRef &) const override;
  Outcome<MarketServiceOriginalInputs> read_original_market_service_inputs(
      const MarketServiceDefinition &,
      const MarketServiceChangeCause &) const override;
  Status validate_original_market_service_inputs(
      const MarketServiceDefinition &,
      const MarketServiceCapacitySource &) const override;
  Outcome<FacilitySourceRef> read_original_market_change_source(
      const FacilitySlotRef &, const MarketServiceChangeCause &) const override;
  Outcome<WorldMoment> read_original_market_change_at(
      const FacilitySlotRef &, const MarketServiceChangeCause &) const override;
  Outcome<std::vector<MarketServiceWorkerSegment>>
  read_private_market_service_workers(const PrivateMarketOperationSource &,
                                      const MomentSpan &) const override;
  Outcome<std::vector<MarketServiceConditionSegment>>
  read_market_service_condition(const MarketServiceDefinition &,
                                const MarketServiceConditionBasis &,
                                const MomentSpan &) const override;
  Outcome<bool>
  market_claim_conflict_visible(const ReservationRef &,
                                const FacilityClaim2 &) const override;
};
Status adopt_locked_market_service_definitions_in_candidate(
    HouseholdPurchaseState &, const HouseholdPurchaseCatalog &,
    const CommonStore &);
} // namespace hanmo::resident
