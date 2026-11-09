#include "native_field_army_exit_command_route.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/military/field_battle/field_army_exit_selection.hpp"
#include "resident/domains/military/field_battle/field_result_codec.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
// 析构顺序反向：借图先销毁，再销毁 Complete owner 与原实际 M 读阶段。
struct OriginalFieldExitRead {
  std::unique_ptr<PhaseCurrentReadScope> phase;
  std::unique_ptr<CompleteExactWorldBindingOwner> owner;
  std::unique_ptr<NativeFieldGroupWorld> group;
  const NativeCompleteWorldCoreSources *core = nullptr;
};
Status selected(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->field_battle || !w.field_battle ||
      !w.catalog->exact_time || !exact_complete_selected(*w.catalog->exact_time))
    return common_failure("FieldArmyExitOriginalCompleteWorldRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Status request_kind(const CommandPayload &command) {
  if (!native_field_army_exit_command_kind(command.kind) ||
      !std::holds_alternative<FieldArmyExitSelect>(command.data))
    return common_failure("InvalidFieldArmyExitSelect");
  return common_success();
}
// 当前报价与候选写入都使用同 World 的原组移动/身体/历史 owner，不造空 provider。
Outcome<OriginalFieldExitRead> original(
    const WorldBundle &w, const CompleteExactWorldReadBindings *actual,
    WorldBundle *candidate = nullptr) {
  auto valid = selected(w);
  if (!valid.ok) return {false, {}, std::move(valid.error)};
  OriginalFieldExitRead read;
  if (!actual) {
    auto now = w.actual_phase_moment();
    if (!now.ok) return {false, {}, std::move(now.error)};
    if (!w.has_active_read_phase()) {
      auto phase = w.enter_phase_read_context("field_army_exit_command", "field_battle",
                                             {now.value, now.value}, now.value);
      if (!phase.ok) return {false, {}, std::move(phase.error)};
      read.phase = std::move(phase.value);
    }
    auto owner = w.complete_exact_time_bindings();
    if (!owner.ok) return {false, {}, std::move(owner.error)};
    read.owner = std::move(owner.value);
    if (!read.owner)
      return {false, {}, common_failure("FieldArmyExitOwningLeaseRequired", {},
                                       ErrorKind::internal).error};
    actual = &read.owner->reads();
  }
  if (!w.catalog->complete_exact_time_runtime ||
      &actual->runtime != w.catalog->complete_exact_time_runtime.get() ||
      !actual->original_core || &actual->original_core->characters.early.world != &w ||
      (candidate && candidate != &w) ||
      !actual->original_core->characters.early.field_application_clock ||
      !actual->original_core->characters.early.original_read_boundary)
    return {false, {}, common_failure("FieldArmyExitSameOriginalOwnerRequired", {},
                                     ErrorKind::internal).error};
  read.core = actual->original_core;
  auto group = NativeFieldGroupWorld::create(w, *read.core, candidate);
  if (!group.ok) return {false, {}, std::move(group.error)};
  read.group = std::move(group.value);
  if (!read.group || !read.group->bound_to(w) || !read.group->field().exact_contacts())
    return {false, {}, common_failure("FieldArmyExitOriginalFieldSourcesRequired", {},
                                     ErrorKind::internal).error};
  valid = read.group->validate_bindings();
  if (!valid.ok) return {false, {}, std::move(valid.error)};
  return {true, std::move(read), {}};
}
} // namespace
bool native_field_army_exit_command_kind(const StableId &kind) noexcept {
  return kind == "FieldArmyExitSelect";
}
Status guard_native_field_army_exit_command(const WorldBundle &w, const StableId &actor) {
  auto valid = selected(w); if (!valid.ok) return valid;
  const auto *person = w.common.actor(actor);
  if (!person) return common_failure("UnknownActor");
  if (!person->alive || actor != w.common.state().metadata.player_actor_id)
    return common_failure("FieldArmyExitPlayerCommanderRequired", actor);
  return common_success();
}
Status preview_native_field_army_exit_command(
    const WorldBundle &w, const StableId &actor, const CommandPayload &command,
    const CompleteExactWorldReadBindings *actual) {
  auto valid = request_kind(command); if (!valid.ok) return valid;
  valid = guard_native_field_army_exit_command(w, actor); if (!valid.ok) return valid;
  auto read = original(w, actual);
  if (!read.ok) return {false, {}, std::move(read.error)};
  return preview_field_army_exit_select(
      *read.value.group->field().exact_contacts(), *w.catalog->field_battle,
      read.value.group->field().abilities(), read.value.core->characters.early.military_access,
      actor, std::get<FieldArmyExitSelect>(command.data));
}
Outcome<ReceiptDetails> apply_native_field_army_exit_command(
    WorldBundle &w, const StableId &actor, const PreparedDomainOp &op,
    const CompleteExactWorldReadBindings *actual) {
  auto valid = request_kind(op.command);
  if (!valid.ok) return {false, {}, std::move(valid.error)};
  const auto *duration = std::get_if<DurationQuote>(&op.quote.data);
  if (op.quote.kind != op.command.kind || !duration || !duration->duration ||
      *duration->duration != 0)
    return {false, {}, common_failure("InvalidFieldArmyExitSelectQuote").error};
  valid = guard_native_field_army_exit_command(w, actor);
  if (!valid.ok) return {false, {}, std::move(valid.error)};
  if (!w.active_exact_write_context() || !w.common.actor_condition_capture_identity())
    return {false, {}, common_failure("FieldArmyExitActualCandidateCaptureRequired", {},
                                     ErrorKind::internal).error};
  auto read = original(w, actual, &w);
  if (!read.ok) return {false, {}, std::move(read.error)};
  auto applied = apply_field_army_exit_select_in_candidate(
      w.common, *w.field_battle, *w.catalog->field_battle,
      read.value.group->field().abilities(), read.value.core->characters.early.military_access,
      actor, std::get<FieldArmyExitSelect>(op.command.data), w.checkpoint_copy_memory);
  if (!applied.ok) return {false, {}, std::move(applied.error)};
  FieldBattleReceipt receipt{std::move(applied.value)};
  valid = validate_field_battle_receipt(op.command.kind, receipt);
  if (!valid.ok) return {false, {}, std::move(valid.error)};
  return {true, ReceiptDetails(std::move(receipt)), {}};
}
Status bind_native_field_army_exit_command_result(
    WorldBundle &w, const StableId &actor, Integer sequence, const CommitResult &receipt) {
  if (!native_field_army_exit_command_kind(receipt.kind)) return common_success();
  auto valid = selected(w); if (!valid.ok) return valid;
  // 不重新取得一个 owner：未绑定结果的候选只能交给已封口 Common 的原领域 binder。
  return bind_field_army_exit_command_result(w.common, *w.field_battle, actor,
                                            sequence, receipt, w.checkpoint_copy_memory);
}
} // namespace hanmo::resident
