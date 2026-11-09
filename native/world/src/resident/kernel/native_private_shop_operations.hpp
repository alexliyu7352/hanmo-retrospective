// Explicit original Shop2/Shop3 annex selection. No operation presence selects
// a schema.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct WorldBundle;
struct NativePrivateShopOperationVersions {
  Integer shops = 0, sources = 0, capabilities = 0;
  bool exact_operations() const noexcept { return sources != 0; }
};
Outcome<Integer>
selected_native_political_shop_licensing_version(const CatalogIdentity &);
Outcome<NativePrivateShopOperationVersions>
selected_native_private_shop_operation_versions(const CatalogIdentity &);
Status
append_native_private_shop_operation_modules(const BoundaryValue &scenario,
                                             CatalogIdentity &);
Status validate_native_private_shop_operation_catalog(const PreparedCatalog &);
Status validate_native_private_shop_operation_layout(const WorldBundle &);
Status validate_native_private_shop_operation_world(const WorldBundle &);
} // namespace hanmo::resident
