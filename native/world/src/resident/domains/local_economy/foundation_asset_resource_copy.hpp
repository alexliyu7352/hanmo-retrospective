// 本次成功 Common 原语的有限效果复制；原实际帧先核、后按同 job fresh-copy
// 预留，再构造冷保留值。不是 hot draft 序列化或另一套资源执行器。
#pragma once
#include "foundation_asset_resource_effects.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class CommonStore;
Outcome<FoundationAssetPreparationEffects>
copy_foundation_asset_preparation_capture(
    const CommonStore &, std::size_t actual_recipe_index,
    CheckpointMemoryCharge &actual_candidate_fields);
Outcome<FoundationAssetFulfillmentEffects>
copy_foundation_asset_fulfillment_capture(
    const CommonStore &, std::size_t actual_recipe_index,
    CheckpointMemoryCharge &actual_candidate_fields);
} // namespace hanmo::resident
