// Explicit original Military body B+32; the occupation row remains its only owner.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;
Status append_native_military_occupation_module(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_military_occupation_selection(const BoundaryValue& scenario,PreparedCatalog&);
Outcome<Integer> selected_military_occupation_schema(const CatalogIdentity&);
Status validate_native_military_occupation_catalog(const PreparedCatalog&);
Status validate_native_military_occupation_selection(const WorldBundle&);
Outcome<std::uint32_t> military_checkpoint_base_version(std::uint32_t physical,Integer selected_schema);
}
