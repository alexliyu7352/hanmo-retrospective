#include "native_military_custody_command.hpp"
#include "command_types.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "resident/domains/military/native_named_military_custody_world.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include <new>
#include <stdexcept>
#include "native_field_application_read.hpp"
#include "native_military_state.hpp"
#include "original_world_read_boundary.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/formation_original_sources.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 只在完整来源图已可构造的原读取入口调用；给定 lease 时不重入 Complete。
struct CustodySourceFailure { RuleError error; };
template<class T> T take_source(Outcome<T> result) {
  if (!result.ok) throw CustodySourceFailure{std::move(result.error)};
  return std::move(result.value);
}
void take_source(Status result) {
  if (!result.ok) throw CustodySourceFailure{std::move(result.error)};
}
void require_source(bool value, const char *code) {
  if (!value) throw CustodySourceFailure{common_failure(code).error};
}
template<class T, class F> Outcome<T> source_guard(F &&fn) {
  try { return {true, fn(), {}}; }
  catch (CustodySourceFailure &failure) { return {false, {}, std::move(failure.error)}; }
  catch (const std::bad_alloc &) { return {false, {}, common_failure("NamedMilitaryCustodyResourceExhausted").error}; }
  catch (const std::length_error &) { return {false, {}, common_failure("NamedMilitaryCustodyResourceExhausted").error}; }
}
template<class T, class F> T with_original_custody(
    const WorldBundle &world, const CompleteExactWorldReadBindings *actual, F &&read) {
  const auto at = take_source(world.actual_phase_moment());
  require_source(bool(world.read_stamps), "NamedMilitaryCustodyOriginalReadStampRequired");
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase())
    phase = take_source(world.enter_phase_read_context(
        "named_military_custody_original_sources", "military", {at, at}, at));
  std::unique_ptr<CompleteExactWorldBindingOwner> owner;
  if (!actual) {
    owner = take_source(world.complete_exact_time_bindings());
    require_source(bool(owner), "NamedMilitaryCustodyCompleteOwnerRequired");
    actual = &owner->reads();
  }
  require_source(actual->original_core &&
      &actual->original_core->characters.early.world == &world,
      "NamedMilitaryCustodyOriginalCoreMismatch");
  const auto &core = *actual->original_core;
  const auto *custody = core.named_custody;
  require_source(custody && custody->bound_to(world.common, world.military),
      "NamedMilitaryCustodyOriginalReleaseOwnerRequired");
  require_source(take_source(compare_world_moments(
      take_source(core.characters.early.clock.read_current_moment()), at)) == 0,
      "NamedMilitaryCustodyOriginalMomentMismatch");
  take_source(validate_growth_fixed_spatial_source_moment(core.characters.spatial, at));
  return read(*custody);
}
// 有限白名单仅含原同处对话及其会话流；新增变体默认仍为世界变更。
struct CustodyDialogueKind {
  const char *operator()(const EncounterTalk &) const {
    return "encounter_talk";
  }
  const char *operator()(const Talk &) const { return "talk"; }
  const char *operator()(const TalkActor &) const { return "talk_actor"; }
  const char *operator()(const AskNews &) const { return "ask_news"; }
  const char *operator()(const DialogueNext &) const { return "dialogue_next"; }
  const char *operator()(const DialogueChoose &) const {
    return "dialogue_choose";
  }
  const char *operator()(const DialogueConfirm &) const {
    return "dialogue_confirm";
  }
  const char *operator()(const DialogueCancel &) const {
    return "dialogue_cancel";
  }
  const char *operator()(const EndInteraction &) const {
    return "end_interaction";
  }
  template <class T> const char *operator()(const T &) const { return nullptr; }
};
bool dialogue_name(const StableId &kind) {
  return kind == "encounter_talk" || kind == "talk" || kind == "talk_actor" ||
         kind == "ask_news" || kind == "dialogue_next" ||
         kind == "dialogue_choose" || kind == "dialogue_confirm" ||
         kind == "dialogue_cancel" || kind == "end_interaction";
}
// 等待不安装个人 Action；请求只可沿原下一半日边界，真实 scheduler 仍可先停在
// due。
Status half_day_request(const WorldBundle &world, const StableId &actor,
                        const AdvanceCommand *advance) {
  const auto *person = world.common.actor(actor);
  if (!person || !person->alive || !person->action_id.empty())
    return common_failure("ActorInCustody", actor);
  if (!advance || !advance->requested_to)
    return common_success();
  if (!world.catalog->exact_time)
    return common_failure("ActorInCustody", actor);
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  if (now.value.half_day_tick == std::numeric_limits<Integer>::max())
    return common_failure("WorldTickOverflow");
  const WorldMoment boundary{now.value.half_day_tick + 1, {}};
  auto same = compare_world_moments(*advance->requested_to, boundary);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  return same.value == 0 ? common_success()
                         : common_failure("ActorInCustody", actor);
}
} // namespace
Outcome<bool>
read_native_military_custody_permission(const WorldBundle &world,
                                        const StableId &actor,
                                        NamedMilitaryCustodyCommandKind kind,
                                        const CompleteExactWorldReadBindings *actual) {
  if (!world.catalog || !world.catalog->military)
    return {false, false,
            common_failure("NamedMilitaryCustodyWorldSourcesRequired", actor)
                .error};
  auto custody_selected = selected_named_military_custody(world.catalog->identity);
  if (!custody_selected.ok) return {false, false, std::move(custody_selected.error)};
  if (custody_selected.value && world.catalog->exact_time &&
      exact_complete_selected(*world.catalog->exact_time)) {
    return source_guard<bool>([&] {
      return with_original_custody<bool>(world, actual, [&](const auto &custody) {
        return take_source(read_named_military_custody_command_gate(
            custody.sources().custody, actor, kind)).has_value();
      });
    });
  }
  // 非 Complete 的既有入口原样；不把其缺完整释放源伪装为全域已接通。
  auto field_selected = selected_field_battle_version(world.catalog->identity);
  if (!field_selected.ok)
    return {false, false, std::move(field_selected.error)};
  // 只在显式 Field 选择下借同 World 的早期原叶；不重入 Complete/Interaction。
  // 本栈拥有这些 adapter 直到实际看管反读结束，绝不补造空完成记录。
  WorldCurrentMomentRead clock(world);
  OriginalWorldReadBoundary boundary(world, clock);
  std::optional<NativeFieldApplicationRead> field_applications;
  std::optional<MilitaryAccess> access;
  std::optional<NativeFormationSourceRead> formations;
  if (field_selected.value) {
    if (!world.field_battle)
      return {false, false,
              common_failure("NamedMilitaryCustodyFieldRootSelectionMismatch")
                  .error};
    field_applications.emplace(world, clock, boundary);
    access.emplace(world.services.military_access());
    auto selected = selected_native_military_state_versions(world.catalog->identity);
    if (!selected.ok)
      return {false, false, std::move(selected.error)};
    if (selected.value.military_action_time) {
      if (!world.military_action_time || !world.movement_exact ||
          !world.action_elapsed || !world.world_timeline)
        return {false, false,
                common_failure("NamedMilitaryCustodyFormationReadRequired")
                    .error};
      formations.emplace(
          world.common, world.military, *world.military_action_time,
          *world.movement_exact, *world.action_elapsed,
          ExactFormationSourceReadContext{
              *access, world.world_timeline->scale_ref, clock, {}});
    }
  }
  const NamedMilitaryCustodyContext source{
      world.common,
      world.military,
      *world.catalog->military,
      world.catalog->identity,
      world.field_battle ? &*world.field_battle : nullptr,
      world.catalog->naval ? &world.naval : nullptr,
      formations ? &*formations : nullptr,
      field_selected.value ? &clock : nullptr,
      access ? &*access : nullptr,
      field_applications ? &*field_applications : nullptr};
  auto result = read_named_military_custody_command_gate(source, actor, kind);
  if (!result.ok)
    return {false, false, std::move(result.error)};
  return {true, result.value.has_value(), {}};
}
Outcome<bool>
guard_native_military_custody_payload(const WorldBundle &world,
                                      const StableId &actor,
                                      const CommandPayload &command) {
  auto kind = NamedMilitaryCustodyCommandKind::other_world_change;
  const auto *advance = std::get_if<AdvanceCommand>(&command.data);
  const bool wait = std::holds_alternative<WaitCommand>(command.data);
  if ((advance && command.kind == "advance") ||
      (wait && command.kind == "wait"))
    kind = NamedMilitaryCustodyCommandKind::half_day_wait;
  else if (const auto *interaction =
               std::get_if<InteractionCommand>(&command.data)) {
    const auto *actual = std::visit(CustodyDialogueKind{}, *interaction);
    if (actual && command.kind == actual)
      kind = NamedMilitaryCustodyCommandKind::co_located_dialogue;
  }
  auto permission = read_native_military_custody_permission(world, actor, kind);
  if (!permission.ok || !permission.value)
    return permission;
  if (kind == NamedMilitaryCustodyCommandKind::half_day_wait) {
    auto valid = half_day_request(world, actor, advance);
    if (!valid.ok)
      return {false, false, std::move(valid.error)};
  }
  return permission;
}
Outcome<bool> guard_native_military_custody_common_kind(
    const WorldBundle &world, const StableId &actor, const StableId &kind) {
  const auto actual = kind == "wait" || kind == "advance"
                          ? NamedMilitaryCustodyCommandKind::half_day_wait
                      : dialogue_name(kind)
                          ? NamedMilitaryCustodyCommandKind::co_located_dialogue
                          : NamedMilitaryCustodyCommandKind::other_world_change;
  return read_native_military_custody_permission(world, actor, actual);
}
Status validate_native_named_military_custody_world_sources(const WorldBundle &world) {
  return source_guard<Done>([&] {
    require_source(world.catalog && world.catalog->military,
                   "NamedMilitaryCustodyWorldSourcesRequired");
    const bool selected = take_source(selected_named_military_custody(world.catalog->identity));
    if (!selected) {
      const NamedMilitaryCustodyContext original{world.common, world.military,
          *world.catalog->military, world.catalog->identity,
          world.field_battle ? &*world.field_battle : nullptr,
          world.catalog->naval ? &world.naval : nullptr};
      take_source(validate_named_military_custody_policy(original));
      take_source(validate_named_military_custody_terms(original));
      for (const auto &[id, head] : world.common.state().facts.rows())
        if (head.kind == "military_named_custody_released")
          throw CustodySourceFailure{common_failure("NamedMilitaryCustodyReleasePayloadRequired", id).error};
      return Done{};
    }
    require_source(world.catalog->exact_time && exact_complete_selected(*world.catalog->exact_time),
                   "NamedMilitaryCustodyCompleteOwnerRequired");
    return with_original_custody<Done>(world, nullptr, [&](const auto &custody) {
      take_source(custody.validate_sources());
      return Done{};
    });
  });
}
} // namespace hanmo::resident
