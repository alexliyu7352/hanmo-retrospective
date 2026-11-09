// Explicit Named birth population source over the original LE physical bases.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct WorldBundle;struct EconomicExactReadBinding;struct EconomicExactWriteBinding;
Outcome<Integer> read_native_born_population_selection(const BoundaryValue&,Integer original_version);
Status append_native_born_population_module(const BoundaryValue&,CatalogIdentity&);
Status validate_native_born_population_catalog(const PreparedCatalog&);
Status validate_native_born_population_layout(const WorldBundle&);
Status validate_native_born_population_read_binding(const WorldBundle&,const EconomicExactReadBinding&);
Status validate_native_born_population_write_binding(const WorldBundle&,const EconomicExactWriteBinding&);
}
