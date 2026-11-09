// Retained S5/S7 reader graph. Original world roots remain uniquely owned by
// WorldBundle; this lease owns the concrete adapters and their borrowed
// contexts.
#pragma once
#include "household_native_environment.hpp"
#include "household_service_runtime.hpp"
#include "market_service_original.hpp"
#include "resident/domains/commerce/private_shop_market_service.hpp"
namespace hanmo::resident {
// 家户市场自己拥有真实容量 reader；构造输入不借被覆盖的全设施 provider。
// 这里只保原世界引用，完整 Named context 在 market_capacity_ 构成后一次绑定。
struct NativeHouseholdNamedSources {
  const CommonStore &common;
  const CommonCatalog &shared;
  const HouseholdPurchaseCatalog &catalog;
  const HouseholdPurchaseState &state;
  const ActionElapsedStore &elapsed;
  const CharacterConditionSources &body;
  const FacilityClaimsState &claims;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const TransportRegistrationState &registrations;
  const SpatialCarrierSources &spatial;
  const CurrentMomentRead &clock;
  const MilitarySpatialHandoffState *military_handoffs;

  NativeHouseholdNamedSources(
      const CommonStore &common, const CommonCatalog &shared,
      const HouseholdPurchaseCatalog &catalog,
      const HouseholdPurchaseState &state, const ActionElapsedStore &elapsed,
      const CharacterConditionSources &body, const FacilityClaimsState &claims,
      const SpatialCarrierState &carriers, const MovementExactState &movement,
      const TransportRegistrationState &registrations,
      const SpatialCarrierSources &spatial, const CurrentMomentRead &clock,
      const MilitarySpatialHandoffState *military_handoffs = nullptr)
      : common(common), shared(shared), catalog(catalog), state(state),
        elapsed(elapsed), body(body), claims(claims), carriers(carriers),
        movement(movement), registrations(registrations), spatial(spatial),
        clock(clock), military_handoffs(military_handoffs) {}
  // 保持原完整 Named context 调用兼容，但不把其容量来源留在依赖图。
  NativeHouseholdNamedSources(const HouseholdNamedEnvironmentContext &x)
      : NativeHouseholdNamedSources(x.common, x.shared, x.catalog, x.state,
                                    x.elapsed, x.body, x.claims, x.carriers,
                                    x.movement, x.registrations, x.spatial,
                                    x.clock, x.military_handoffs) {}
};
struct NativeHouseholdRetailDependencies {
  const PrivateShopState &shops;
  const PrivateShopCatalog *shops_catalog;
  const GrowthCatalog &markets;
  const HouseholdRetailCatalog &catalog;
  const HouseholdRetailState &state;
  const PrivateShopMarketHistoryRead *market_history = nullptr;
  HouseholdRetailMarketCapture *market_capture = nullptr;
};
struct NativeMarketServiceOriginalDependencies {
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
  const PublicMarketAuthorityRead *public_authority;
  const PrivateShopOperationSourceContext *private_operations;
  WorldMoment current;
  ExactArithmeticBudget arithmetic{};
  const WorldBundle *unselected_shops_world = nullptr;
};
struct NativeHouseholdServiceDependencies {
  NativeHouseholdNamedSources named;
  const HouseholdCohortEnvironmentContext &cohort;
  NativeMarketServiceOriginalDependencies market_original;
  // 暂定：家庭控制是真实必借源，不从未选公市的人员适配器取得。
  const HouseholdProvisionControlRead &controls;
  const PublicMarketStaffingContext *staffing;
  const FacilityHolderSourceRead &all_holders;
  NativeHouseholdRetailDependencies retail;
  const FacilityClaimsCatalog &claim_catalog;
  const TimeScaleRef &scale;
  const EconomicTransportExpenseState &transport_expenses;
  const CargoHandlingState &handling;
  const TravelCatalog &travel;
  const GroupMovementExecutionState &group_executions;
  const InventorySpatialHistoryContext &inventory_history;
  const WorldMoment &now;
  Integer selected_household_version = 2;
  const HouseholdMobileCustodyContext *mobile_custody = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
// The surrounding same-world owner must outlive this graph. No callbacks or
// caller-supplied HouseholdPurchaseEnvironmentRead can replace these readers.
// Cross-domain authority and the complete holder router remain central-owned.
class NativeHouseholdServiceLease final {
  // The retail context must borrow a fully constructed base subobject. This
  // private link is bound once after the actual contract graph exists; it never
  // implements an alternative acceptance or settlement rule.
  class RetailAcceptanceLink final : public HouseholdRetailAcceptanceRead {
    friend class NativeHouseholdServiceLease;
    const HouseholdPurchaseContracts *contracts_ = nullptr;
    RetailAcceptanceLink() = default;
    void bind(const HouseholdPurchaseContracts &value) noexcept {
      contracts_ = &value;
    }
    bool bound_to(const HouseholdPurchaseContracts &value) const noexcept {
      return contracts_ == &value;
    }

  public:
    Outcome<HouseholdRetailAcceptance>
    read_household_retail_acceptance(const StableId &) const override;
    Status validate_household_retail_settlement(
        const HouseholdRetailSettlement &) const override;
  };
  NativeHouseholdServiceDependencies dependencies_;
  std::optional<NativePrivateMarketServiceOriginalRead> private_originals_;
  MarketServiceOriginalContext market_original_context_;
  NativeMarketServiceOriginalRead market_originals_;
  MarketServiceCapacityContext market_;
  NativeMarketServiceCapacityRead market_capacity_;
  HouseholdNamedEnvironmentContext named_;
  RetailAcceptanceLink retail_acceptance_;
  HouseholdRetailReadContext retail_;
  NativeHouseholdEnvironmentContext environment_context_;
  NativeHouseholdPurchaseEnvironmentRead environment_;
  HouseholdPurchaseContext household_;
  HouseholdPurchaseContracts contracts_;
  HouseholdFacilitySourceRead holders_;

public:
  explicit NativeHouseholdServiceLease(NativeHouseholdServiceDependencies);
  NativeHouseholdServiceLease(const NativeHouseholdServiceLease &) = delete;
  NativeHouseholdServiceLease &
  operator=(const NativeHouseholdServiceLease &) = delete;
  NativeHouseholdServiceLease(NativeHouseholdServiceLease &&) = delete;
  NativeHouseholdServiceLease &
  operator=(NativeHouseholdServiceLease &&) = delete;
  Status validate_bindings() const;
  Status validate_sources() const;
  HouseholdServiceReadBinding read_binding() const {
    return {household_, market_};
  }
  Outcome<HouseholdServiceWriteBinding>
  write_binding(CommonStore &, HouseholdPurchaseState &,
                ActionElapsedStore &) const;
  const HouseholdPurchaseContext &household() const { return household_; }
  const HouseholdRetailReadContext &retail() const { return retail_; }
  const MarketServiceCapacityContext &market() const { return market_; }
  const HouseholdPurchaseContracts &contracts() const { return contracts_; }
  const HouseholdFacilitySourceRead &holders() const { return holders_; }
  const NativeMarketServiceCapacityRead &capacity() const {
    return market_capacity_;
  }
  const NativePrivateMarketServiceOriginalRead *
  private_market_originals() const {
    return private_originals_ ? &*private_originals_ : nullptr;
  }
  const MarketServiceOriginalContext &market_original_context() const {
    return market_original_context_;
  }
};
} // namespace hanmo::resident
