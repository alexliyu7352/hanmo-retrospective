// 名义实船引用只转交原身份读取，不另建船体表或从 Fleet 推测实船。
#include "naval_vessel_ref.hpp"
#include "naval_vessel_identity.hpp"

namespace hanmo::resident {
// 原空键、重复键、缺船及作者身份不符的错误和顺序全部由原读取者保留。
Outcome<const NavalVesselDefinition *>
read_original_naval_vessel_reference(const NavalAssetState &state,
                                     const NavalAssetCatalog &catalog,
                                     const VesselRef &reference) {
  return read_original_naval_vessel_identity(state, catalog,
                                             reference.vessel_id);
}
} // namespace hanmo::resident
