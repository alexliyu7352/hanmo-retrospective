// Shop2 原市场与产权依据；不授予买方控制权，也不代替托管同意。
#pragma once
#include "resident/domains/commerce/private_shop_operation_sources.hpp"
#include "private_inventory_title.hpp"
namespace hanmo::resident {
struct PrivateHouseholdMarketBasisContext {
  const PrivateShopOperationSourceContext &operation;
  const PrivateInventoryTitleContext &inventory_titles;
  const HouseholdPurchaseState &purchases;
};
struct PrivateHouseholdMarketBasis {
  const PrivateShopRecord *shop = nullptr;
  const PrivateShopOperation *operation = nullptr;
  const PrivateShopOperationAcceptance *original_operation_acceptance = nullptr;
  const MarketServiceDefinition *original_market_definition = nullptr;
  PrivateInventoryTitleView proprietor_personal_inventory;
  MomentOwnerSource original_operation_source;
  SourceRef original_usage_right;
};
// 当前真实自行营业的原店铺，不预造作者托管标识。
Outcome<PrivateHouseholdMarketBasis>
read_current_private_household_market_basis(
    const PrivateHouseholdMarketBasisContext &,
    const PrivateHouseholdRetailSeller &);
// 历史切点必须属于真实采购根，不回调完整家庭或零售验证。
Outcome<PrivateHouseholdMarketBasis>
read_private_household_market_basis_before_purchase(
    const PrivateHouseholdMarketBasisContext &, const HouseholdPurchaseState &,
    const HouseholdPurchaseAcceptedTerms &actual_acceptance);
} // namespace hanmo::resident
