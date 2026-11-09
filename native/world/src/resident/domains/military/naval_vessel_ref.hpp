// 原 Naval 实船键的名义强型；不代表舰队、局部运动身体或运输权限。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
struct NavalAssetState;
struct NavalAssetCatalog;
struct NavalVesselDefinition;

// 首个 Care 根的工程承载，只包原唯一实船键；没有新增船世代或复制船体。
struct VesselRef {
  StableId vessel_id;
};

// 只复用原实船与作者身份的同源读取；不授权载荷、操船、产权或患者同意。
Outcome<const NavalVesselDefinition *> read_original_naval_vessel_reference(
    const NavalAssetState &, const NavalAssetCatalog &, const VesselRef &);
} // namespace hanmo::resident
