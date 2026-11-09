// Explicit P11 original root. The original LE base wire remains unchanged.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;
struct NativeEconomicObligationVersions {Integer local_economy=0,sources=0,named_birth_population=0;};
Outcome<NativeEconomicObligationVersions> selected_native_economic_obligation_versions(const CatalogIdentity&);
Outcome<Integer> read_native_economic_obligation_selection(const BoundaryValue&,Integer original_base_version);
Status append_native_economic_obligation_module(const BoundaryValue&,CatalogIdentity&);
Status validate_native_economic_obligation_catalog(const PreparedCatalog&);
Status initialize_native_economic_obligation_root(const PreparedCatalog&,InitialWorldSeed&);
Status validate_native_economic_obligation_layout(const WorldBundle&);
}
