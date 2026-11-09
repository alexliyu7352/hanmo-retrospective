#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;
Outcome<Integer> selected_native_military_battle_lifecycle(const CatalogIdentity&);
Status append_native_military_battle_lifecycle_module(const BoundaryValue& scenario,CatalogIdentity&);
Status validate_native_military_battle_lifecycle_catalog(const PreparedCatalog&);
Status validate_native_military_battle_lifecycle_layout(const WorldBundle&);
Status validate_native_military_battle_lifecycle_sources(const WorldBundle&);
}
