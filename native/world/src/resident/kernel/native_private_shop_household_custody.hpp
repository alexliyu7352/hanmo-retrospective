// 显式私铺托管选型及同 World 根约束；不从开店行为推导额外同意。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct BoundaryValue;
Outcome<Integer>
selected_native_private_shop_household_custody_version(const CatalogIdentity &);
Status append_native_private_shop_household_custody_module(
    const BoundaryValue &scenario, CatalogIdentity &);
Status prepare_native_private_shop_household_custody_catalog(
    const BoundaryValue &scenario, PreparedCatalog &);
Status
validate_native_private_shop_household_custody_catalog(const PreparedCatalog &);
Status
validate_native_private_shop_household_custody_layout(const WorldBundle &);
Status
validate_native_private_shop_household_custody_world(const WorldBundle &);
} // namespace hanmo::resident
