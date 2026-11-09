#pragma once
#include "resident/domains/local_economy/private_shop_household_custody_pins.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 从同一实际 World 借原托管 owner；目录与初始化来源仍以原 typed sink 传递。
// 此入口不注册共享保留表，也不把初始化结果伪装成 Common 事实。
Status visit_native_private_shop_custody_retention(
    const WorldBundle &, const MomentOwnerSource &actual_source,
    PrivateShopHouseholdCustodyPinSink &);
// 完整验证实际根后逐真实接受行枚举；未选择且确无根/孤儿事实时才允许空枚举。
// 原 sink 不携 consumer_id；共享 registry 必须逐实际接受 ID
// 调用上面的单源重载， 不得用本聚合重载把不同接受行的同名 slot
// 合并为一个消费者。
Status visit_native_private_shop_custody_retention(
    const WorldBundle &, PrivateShopHouseholdCustodyPinSink &);
} // namespace hanmo::resident
