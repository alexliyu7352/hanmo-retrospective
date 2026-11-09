#pragma once
#include "resident/domains/military/named_military_custody.hpp"
namespace hanmo::resident {
class WorldBundle;
struct CommandPayload;
struct CompleteExactWorldReadBindings;
// 只借当前 World 原根；true 表示确有被允许的真实看管，false 才是未看管。
Outcome<bool>
read_native_military_custody_permission(const WorldBundle &, const StableId &,
                                        NamedMilitaryCustodyCommandKind,
                                        const CompleteExactWorldReadBindings *actual = nullptr);
// 公开入口以实际 variant 分类；外层命令名不能把别的动作变成对话或等待。
Outcome<bool> guard_native_military_custody_payload(const WorldBundle &,
                                                    const StableId &,
                                                    const CommandPayload &);
// 原内部公共守卫与自动对白动作继续同门；不替代上面的 typed 预备及采用复核。
Outcome<bool> guard_native_military_custody_common_kind(const WorldBundle &,
                                                        const StableId &,
                                                        const StableId &);
// 完整冷/热世界验证沿同一拥有者核真实 term、release、当前占用及孤头反覆盖。
Status validate_native_named_military_custody_world_sources(const WorldBundle &);
} // namespace hanmo::resident
