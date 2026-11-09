// Immutable authored vessel identity only. Title, integrity, crew, location and
// current operation rights remain with their actual source-history readers.
#pragma once
#include "naval_assets.hpp"
namespace hanmo::resident {
Status validate_original_naval_vessel_identity(const NavalVesselRecord&,
    const NavalVesselDefinition&);
Outcome<const NavalVesselDefinition*> read_original_naval_vessel_identity(
    const NavalAssetState&,const NavalAssetCatalog&,const StableId& vessel_id);
}
