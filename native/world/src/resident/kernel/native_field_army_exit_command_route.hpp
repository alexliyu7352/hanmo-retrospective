// 唯一玩家选口共享接线：借原 Complete owner，仍由原候选提交与 Common 封口发布。
#pragma once
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
bool native_field_army_exit_command_kind(const StableId &) noexcept;
// 这里只核本人/生命/选型；真实主将、战场、出口资格由领域原读口核定。
Status guard_native_field_army_exit_command(const WorldBundle &, const StableId &actor);
Status preview_native_field_army_exit_command(
    const WorldBundle &, const StableId &actor, const CommandPayload &,
    const CompleteExactWorldReadBindings * = nullptr);
Outcome<ReceiptDetails> apply_native_field_army_exit_command(
    WorldBundle &, const StableId &actor, const PreparedDomainOp &,
    const CompleteExactWorldReadBindings * = nullptr);
// 只在本次真实 Common 成功结果/history 已存在后调用，不建立第二份结果账。
Status bind_native_field_army_exit_command_result(
    WorldBundle &, const StableId &actor, Integer sequence, const CommitResult &);
} // namespace hanmo::resident
