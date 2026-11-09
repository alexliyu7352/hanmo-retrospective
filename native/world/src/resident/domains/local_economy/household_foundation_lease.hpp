// 家户买方与私铺卖方的同世界借用图；只持有原读口，不复制钱粮或业务状态。
#pragma once
#include "economic_inventory_placement.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "household_service_lease.hpp"
#include "private_shop_household_custody.hpp"
#include "household_transport_registration.hpp"

namespace hanmo::resident {
// 中央先持有原 PrivateShopWorldAccess 和跨域根，再按实际时刻构造此依赖包。
// 这些 context 按值保留其借用关系，调用者的临时聚合对象不必继续存活。
struct NativeHouseholdFoundationDependencies {
  const CatalogIdentity &identity;
  NativeHouseholdNamedSources named;
  HouseholdCohortEnvironmentContext cohort;
  std::optional<PrivateShopContext> shops;
  const PrivateShopState &shops_state;
  const PrivateShopOperationSourcesState *operations;
  const PrivateShopOperationCapabilitiesState *operation_capabilities;
  ActorSpatialHeadSourceReadContext operation_spatial;
  const FamilyLifeHistoryRead4 *life_history;
  EconomicFixedInventoryPlacementContext fixed_inventory;
  MobileInventoryAdoptionContext mobile_inventory;
  const GroupMovementExecutionState &group_executions;
  const HouseholdProvisionControlRead &controls;
  const PublicMarketStaffingContext *staffing;
  const PublicMarketAuthorityRead *public_authority;
  const FacilityHolderSourceRead &all_holders;
  // production_management 与成对的 09 根各按锁目录选择，未选必须为空。
  const ProductionManagementCatalog *assets;
  const EconomicFacilitySourcesState *installed;
  const ProductionFacilityConditionsCatalog *condition_catalog;
  const ProductionFacilityConditionsState *conditions;
  const HouseholdRetailCatalog &retail_catalog;
  const HouseholdRetailState &retail_state;
  const PrivateShopMarketHistoryRead *market_history;
  HouseholdRetailMarketCapture *market_capture;
  const FacilityClaimsCatalog &claim_catalog;
  const TimeScaleRef &scale;
  const EconomicTransportExpenseState &transport_expenses;
  const CargoHandlingState &handling;
  WorldMoment now;
  Integer selected_household_version = 2;
  std::optional<HouseholdMobileCustodyContext> mobile_custody;
  const WorldBundle *unselected_shops_world = nullptr;
  const PrivateInventoryTitleContext *private_inventory_titles = nullptr;
  const PrivateShopHouseholdCustodyCatalog *private_custody_catalog = nullptr;
  const PrivateShopHouseholdCustodyState *private_custody_state = nullptr;
  const TravelCatalog *private_custody_travel = nullptr;
};

// 每个实际时刻重新构造，禁止搬动；外层 owner 最后销毁其所有借用来源。
// 构造只做私有一次绑定，验证来源必须等中央的 holder/router 全部接好。
class NativeHouseholdFoundationLease final : public CheckpointAllocatedObject {
  NativeHouseholdFoundationDependencies dependencies_;
  bool original_shop_links_unbound_;
  NativeEconomicFixedInventoryPlacementRead fixed_inventory_;
  InventorySpatialHistoryContext inventory_history_;
  std::optional<HouseholdMobileCustodyContext> mobile_custody_;
  std::optional<PrivateShopOperationSourceContext> operations_;
  std::optional<PrivateHouseholdMarketBasisContext> market_basis_;
  std::optional<PrivateShopHouseholdCustodyContext> private_custody_;
  NativeHouseholdServiceLease service_;
  NativeHouseholdTransportRegistrationBusinessRead transport_;

  NativeHouseholdServiceDependencies service_dependencies();

public:
  explicit NativeHouseholdFoundationLease(
      NativeHouseholdFoundationDependencies);
  NativeHouseholdFoundationLease(const NativeHouseholdFoundationLease &) =
      delete;
  NativeHouseholdFoundationLease &
  operator=(const NativeHouseholdFoundationLease &) = delete;
  NativeHouseholdFoundationLease(NativeHouseholdFoundationLease &&) = delete;
  NativeHouseholdFoundationLease &
  operator=(NativeHouseholdFoundationLease &&) = delete;

  // 只校验选择、地址及实际时刻；不调用尚未绑定的中央 holder/router。
  Status validate_bindings() const;
  // 中央完整绑定后才调用，保留原实际库存、私铺与家户的来源验证。
  Status validate_sources() const;
  bool bound_to(const CommonStore &, const LocalEconomyState &,
                const HouseholdPurchaseState &, const PrivateShopState &,
                const HouseholdRetailState &,
                const ActionElapsedStore &) const noexcept;
  const NativeHouseholdServiceLease &service() const { return service_; }
  // 只借本 owner 已持有的人口原图；调用者不得另造历史或延长借用生命期。
  const HouseholdCohortEnvironmentContext &cohort() const {
    return dependencies_.cohort;
  }
  // 与原营业/人物位置读相同的完整来源图，不返回临时聚合上下文。
  const ActorSpatialHeadSourceReadContext &operation_spatial() const {
    return dependencies_.operation_spatial;
  }
  const PrivateShopContext *shops() const {
    return dependencies_.shops ? &*dependencies_.shops : nullptr;
  }
  const PrivateShopOperationSourceContext *operations() const {
    return operations_ ? &*operations_ : nullptr;
  }
  const PrivateShopHouseholdCustodyContext *private_custody() const {
    return private_custody_ ? &*private_custody_ : nullptr;
  }
  const InventorySpatialHistoryContext &inventory_history() const {
    return inventory_history_;
  }
  const NativeEconomicFixedInventoryPlacementRead &fixed_inventory() const {
    return fixed_inventory_;
  }
  const NativeHouseholdTransportRegistrationBusinessRead &transport() const {
    return transport_;
  }
};

// 工厂只在构造与绑定核验成功后交出 owner；源校验留给完整图的发布门。
Outcome<std::unique_ptr<NativeHouseholdFoundationLease>>
    make_native_household_foundation_lease(
        NativeHouseholdFoundationDependencies);
} // namespace hanmo::resident
