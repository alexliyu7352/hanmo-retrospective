// 实际候选的 B1 钱货捕获绑定。只借原 Common 同栈执行，不制造 hot draft。
#pragma once
#include "foundation_asset_sources.hpp"
namespace hanmo::resident {
class CommonStore;
// 冷证据必须逐字段来自同一个实际候选的原成功预约/交割捕获；同一原
// kind3 ID 唯一匹配。此门不替代原双方同意、S6 登记或跨域最终准入。
Status validate_foundation_asset_actual_capture(
    const CommonStore &, const FoundationAssetOffer &,
    const FoundationAssetPreparation &, const FoundationAssetFulfillment &);
} // namespace hanmo::resident
