// The original Control owner is selected once and persists as its own typed
// section. No current-state inference upgrades Control2 bytes to Control3.
#pragma once
#include "resident/domains/family/control_continuation.hpp"
namespace hanmo::resident {
struct PreparedCatalog;struct WorldBundle;
Outcome<Integer> selected_native_control_continuation_version(const CatalogIdentity&);
Status append_native_control_continuation_module(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_control_continuation_catalog(const BoundaryValue&,PreparedCatalog&);
Status validate_native_control_continuation_catalog(const PreparedCatalog&);
Status validate_native_control_continuation_layout(const WorldBundle&);
Status validate_native_control_continuation_world(const WorldBundle&);
}
