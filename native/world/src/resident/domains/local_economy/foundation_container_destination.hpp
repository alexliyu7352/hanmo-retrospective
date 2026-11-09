// B1 实转只受领到本次原初始安装的真实容器；这层只绑定来源，不授权支出。
#pragma once
#include "foundation_container_opening.hpp"
namespace hanmo::resident {
struct FoundationAssetFulfillment;
// 调用者必须先验同一实际 Installation 的 opening 原证据与原章程。
// 写腿在钱货操作前调用，空地址只表示这次原贡献没有对应资源腿；不选默认仓。
Status validate_foundation_container_destination_binding(
    const FoundationContainerOpeningReceipt &, const StableId &organization,
    const StableId &wallet, const StableId &inventory);
// 热/冷注册都先反读原完整履行，再用此口核其目标与同一次安装。不能拿它
// 代替实际资源前后帧、原本人同意、成立资格或原 Grant 权限。
Status validate_foundation_asset_fulfillment_container_binding(
    const FoundationContainerOpeningReceipt &,
    const FoundationAssetFulfillment &);
} // namespace hanmo::resident
