// 个人战共享服务与开战半日协调；不创建第二任务、战斗或共同人物状态。
#include "exact_complete_world.hpp"
#include "exact_phase_graph.hpp"
#include "native_character_family_world.hpp"
#include "native_character_injury_state.hpp"
#include "native_complete_world_core.hpp"
#include "native_personal_battle_lifecycle.hpp"
#include "personal_integration.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/injury_progress_replay_v1.hpp"
#include "resident/domains/actor_foundation/personal_initial_attributes_v1.hpp"
#include "resident/domains/actor_foundation/personal_injury_write_v1.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "task_cargo_access.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
// 护送选择与结果从当前任务借用，任务只保存原battle ID及原履约后果。
PersonalAccess BundleServices::personal_access() const {
  PersonalAccess access;
  access.pauses_world = [this]() {
    return personal_pauses_world(world_.personal);
  };
  access.occupied = [this](const StableId &actor) {
    return in_open_battle(actor);
  };
  access.escort_objective = [this](const StableId &actor,
                                   const OrderedTable<Integer> &sides)
      -> std::optional<std::pair<StableId, StableId>> {
    std::optional<std::pair<StableId, StableId>> selected;
    for (const auto &row : world_.tasks.tasks.rows()) {
      const auto &task = row.second;
      if (task.assignee != actor || task.status != "active")
        continue;
      const auto *definition =
          world_.catalog->tasks->offers.find(task.offer_id);
      if (!definition || definition->template_id != "escort")
        continue;
      const auto *side = sides.find(definition->goal.actor_id);
      if (side && *side == 0)
        selected = std::make_pair(definition->goal.actor_id,
                                  definition->goal.objective_id);
    }
    return selected;
  };
  access.record_escort = [this](const PersonalBattle &battle) {
    TaskProtectedBattle observed;
    observed.id = battle.id;
    observed.controller_id = battle.controller_id;
    observed.outcome = battle.outcome;
    observed.applied = battle.applied;
    for (const auto &row : battle.units.rows())
      if (row.second.protected_actor)
        observed.protected_actor_ids.push_back(row.first);
    return record_task_battle(world_.common, world_.tasks,
                              *world_.catalog->tasks, observed);
  };
  access.on_battle_started = [this](const PersonalBattle &battle) {
    TaskCargoAccess source(world_.common, world_.personal,
                           *world_.catalog->personal, world_.interaction);
    if (!world_.dynamic_enabled()) {
      auto observed = source.task_cargo_source(battle.id);
      if (!observed.ok)
        return Status{false, {}, observed.error};
      return observed.value
                 ? common_failure("TaskCargoRequiresDynamicProfile", battle.id,
                                  ErrorKind::unsupported_capability)
                 : common_success();
    }
    return freeze_task_cargo_exposure(world_.common, world_.tasks, battle.id,
                                      source);
  };
  access.on_battle_result = [this](const PersonalBattle &battle) {
    TaskCargoAccess source(world_.common, world_.personal,
                           *world_.catalog->personal, world_.interaction);
    if (!world_.dynamic_enabled()) {
      auto observed = source.task_cargo_source(battle.id);
      if (!observed.ok)
        return Status{false, {}, observed.error};
      return observed.value
                 ? common_failure("TaskCargoRequiresDynamicProfile", battle.id,
                                  ErrorKind::unsupported_capability)
                 : common_success();
    }
    return apply_task_cargo_result(world_.common, world_.tasks, battle.id,
                                   source);
  };
  return access;
}
namespace {
// 同一次原命令借同World Complete图；先有owner，再有身体快照和引用它的读口。
// 不回写根、不缓存跨advance的M，也不把非空指针当source1选择。
class PersonalOriginalScope final : public CheckpointAllocatedObject {
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  const WorldBundle &world_;
  CharacterConditionSources conditions_;
  const NativeCompleteWorldCoreSources &core_;
  GrowthPermanentSources growth_;
  CharacterInjuryReplaySourcesV1 injury_;
  NativePersonalInitialAttributesRead initial_;

  PersonalOriginalScope(const WorldBundle &world,
                        std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                        CharacterConditionSources conditions)
      : owner_(std::move(owner)), world_(world),
        conditions_(std::move(conditions)),
        core_(*owner_->reads().original_core),
        growth_{conditions_, *world.catalog->growth_permanent,
                *world.growth_permanent,
                core_.characters.family.work_practice()},
        injury_{conditions_, *world.character_injury_sources, &world.personal,
                &*world.field_battle},
        initial_(conditions_, growth_,
                 *core_.characters.early.attribute_absence,
                 core_.characters.early.clock, world.personal,
                 *world.catalog->personal) {}

public:
  PersonalOriginalScope(const PersonalOriginalScope &) = delete;
  PersonalOriginalScope &operator=(const PersonalOriginalScope &) = delete;
  PersonalOriginalScope(PersonalOriginalScope &&) = delete;
  PersonalOriginalScope &operator=(PersonalOriginalScope &&) = delete;

  static Outcome<std::unique_ptr<PersonalOriginalScope>>
  create(const WorldBundle &world, WorldBundle *candidate) {
    auto layout = validate_native_character_injury_layout(world);
    if (!layout.ok)
      return {false, {}, std::move(layout.error)};
    if ((candidate && candidate != &world) || !world.catalog ||
        !world.catalog->personal || !world.catalog->growth_permanent ||
        !world.growth_permanent || !world.actor_foundation ||
        !world.character_injury_sources || !world.field_battle)
      return {false,
              {},
              common_failure("PersonalOriginalWorldRootsRequired").error};
    auto owner = candidate ? candidate->complete_exact_time_bindings()
                           : world.complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, std::move(owner.error)};
    if (!owner.value || !owner.value->reads().original_core)
      return {false,
              {},
              common_failure("PersonalOriginalCompleteOwnerRequired").error};
    const auto &reads = owner.value->reads();
    const auto &core = *reads.original_core;
    if (&core.characters.early.world != &world ||
        &core.characters.early.complete != owner.value.get() ||
        &core.characters.family.body() != &reads.character_conditions ||
        !core.characters.early.attribute_absence)
      return {false,
              {},
              common_failure("PersonalOriginalSameWorldOwnerRequired").error};
    auto body = reads.character_conditions.sources();
    if (!body.ok)
      return {false, {}, std::move(body.error)};
    if (&body.value.common != &world.common ||
        &body.value.state != &*world.actor_foundation ||
        body.value.injury_sources != &*world.character_injury_sources ||
        body.value.injury_personal != &world.personal ||
        body.value.injury_field != &*world.field_battle ||
        body.value.personal_catalog != world.catalog->personal.get() ||
        body.value.permanent_growth != &*world.growth_permanent ||
        body.value.permanent_growth_catalog !=
            world.catalog->growth_permanent.get() ||
        body.value.work_practice != &core.characters.family.work_practice())
      return {false,
              {},
              common_failure("PersonalOriginalSameWorldBodyRequired").error};
    auto result =
        std::unique_ptr<PersonalOriginalScope>(new PersonalOriginalScope(
            world, std::move(owner.value), std::move(body.value)));
    auto valid = result->initial_.validate_context();
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    return {true, std::move(result), {}};
  }

  const PersonalInitialAttributesRead &initial() const noexcept {
    return initial_;
  }
  const CharacterInjuryReplaySourcesV1 &injury() const noexcept {
    return injury_;
  }
  GrowthCapabilityRevisionWrite *capability() noexcept {
    return owner_->growth_capability_writer();
  }
};

// 只衔接原terminal算法返回的真实token到当前capture；不执行第二次身体或能力写。
class PersonalTerminalCaptureWrite final
    : public PersonalTerminalConditionWrite {
  WorldBundle &world_;
  const CharacterInjuryReplaySourcesV1 &sources_;
  const PersonalBattleLifecycleWriteContext &clock_;
  GrowthCapabilityRevisionWrite &capability_;

public:
  PersonalTerminalCaptureWrite(WorldBundle &world,
                               const CharacterInjuryReplaySourcesV1 &sources,
                               const PersonalBattleLifecycleWriteContext &clock,
                               GrowthCapabilityRevisionWrite &capability)
      : world_(world), sources_(sources), clock_(clock),
        capability_(capability) {}

  bool bound_to(const CommonStore &common, const PersonalState &personal,
                const PersonalCatalog &catalog) const noexcept override {
    return world_.actor_foundation && world_.character_injury_sources &&
           world_.growth_permanent && &world_.common == &common &&
           &world_.personal == &personal &&
           world_.catalog->personal.get() == &catalog &&
           &clock_.common == &common && &clock_.state == &personal &&
           &sources_.conditions.common == &common &&
           &sources_.conditions.state == &*world_.actor_foundation &&
           sources_.conditions.injury_sources ==
               &*world_.character_injury_sources &&
           &sources_.injury == &*world_.character_injury_sources &&
           sources_.personal == &personal &&
           sources_.conditions.personal_catalog == &catalog &&
           sources_.conditions.permanent_growth == &*world_.growth_permanent &&
           capability_.bound_to(common, *world_.growth_permanent);
  }

  Status apply_actual_terminal(const StableId &battle,
                               const StableId &actor) override {
    auto *writes = world_.active_exact_write_context();
    if (!writes || !world_.common.actor_condition_capture_identity() ||
        !bound_to(world_.common, world_.personal, *world_.catalog->personal))
      return common_failure("PersonalTerminalActualCandidateCaptureRequired",
                            actor, ErrorKind::internal);


    auto memory = CheckpointMemoryCharge(0);
    const SourceRef original{checkpoint_fresh_text_copy("personal"),
                             checkpoint_fresh_text_copy("personal_battle"),
                             checkpoint_fresh_string_copy(battle)};
    auto result = apply_personal_terminal_condition_in_candidate_v1(
        world_.common, *world_.actor_foundation,
        *world_.character_injury_sources, sources_, original, actor, clock_,
        *world_.growth_permanent, capability_);
    if (!result.ok)
      return {false, {}, std::move(result.error)};
    writes->condition_writes.push_back(std::move(result.value));
    return common_success();
  }
};
} // namespace

// 预览只构造同源领域报价，不预走半日或写作者/任务数据。
Outcome<PersonalQuote>
preview_personal_integration(const WorldBundle &world, const StableId &actor,
                             const PersonalCommand &command) {
  auto lifecycle = validate_native_personal_battle_lifecycle_layout(world);
  if (!lifecycle.ok)
    return {false, {}, std::move(lifecycle.error)};
  auto selected =
      selected_native_personal_battle_lifecycle(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto injury =
      selected_native_character_injury_source_version(world.catalog->identity);
  if (!injury.ok)
    return {false, {}, std::move(injury.error)};
  auto interaction = world.services.interaction_access();
  auto personal = world.services.personal_access();
  personal.selected_battle_lifecycle = selected.value;
  personal.selected_injury_sources = injury.value;
  std::unique_ptr<PersonalOriginalScope> original;
  if (injury.value) {
    auto source = PersonalOriginalScope::create(world, nullptr);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
    original = std::move(source.value);
    personal.initial_attributes = &original->initial();
  }
  return preview_personal(
      world.common, world.personal, *world.catalog->personal, world.interaction,
      *world.catalog->interaction, interaction, personal, actor, command);
}

// 原duel_start先hold双方推进半日；真实advance后重新取得同World来源和时刻。
Outcome<PersonalReceipt>
apply_personal_integration(WorldBundle &world, const StableId &actor,
                           const PersonalCommand &command,
                           const PersonalQuote &quote) {
  auto lifecycle = validate_native_personal_battle_lifecycle_layout(world);
  if (!lifecycle.ok)
    return {false, {}, std::move(lifecycle.error)};
  if ((command.kind == "duel_start" || command.kind == "encounter_fight") &&
      quote.duration > 0) {
    auto elapsed = advance_world(world, quote.sides.order());
    if (!elapsed.ok)
      return {false, {}, elapsed.error};
    auto ready = preview_personal_integration(world, actor, command);
    if (!ready.ok)
      return {false, {}, ready.error};
  }
  auto selected =
      selected_native_personal_battle_lifecycle(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto injury =
      selected_native_character_injury_source_version(world.catalog->identity);
  if (!injury.ok)
    return {false, {}, std::move(injury.error)};
  auto interaction = world.services.interaction_access();
  auto personal = world.services.personal_access();
  WorldCurrentMomentRead lifecycle_clock(world);
  PersonalBattleLifecycleWriteContext lifecycle_context{
      world.common, world.personal, lifecycle_clock};
  personal.selected_battle_lifecycle = selected.value;
  personal.battle_lifecycle = selected.value ? &lifecycle_context : nullptr;
  personal.selected_injury_sources = injury.value;
  std::unique_ptr<PersonalOriginalScope> original;
  std::optional<PersonalTerminalCaptureWrite> terminal;
  if (injury.value) {
    auto source = PersonalOriginalScope::create(world, &world);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
    original = std::move(source.value);
    auto *capability = original->capability();
    if (!capability || !world.active_exact_write_context() ||
        !world.common.actor_condition_capture_identity())
      return {false,
              {},
              common_failure("PersonalTerminalActualCandidateCaptureRequired",
                             actor, ErrorKind::internal)
                  .error};
    personal.initial_attributes = &original->initial();
    terminal.emplace(world, original->injury(), lifecycle_context, *capability);
    personal.terminal_condition = &*terminal;
  }
  return apply_personal(world.common, world.personal, *world.catalog->personal,
                        world.interaction, *world.catalog->interaction,
                        interaction, personal, actor, quote);
}

// 原whole和冷恢复检查真实started头冻结的初值；旧未选不承担source1的新义务。
Status
validate_native_personal_initial_attributes_sources(const WorldBundle &world) {
  auto selected =
      selected_native_character_injury_source_version(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value)
    return common_success();
  auto original = PersonalOriginalScope::create(world, nullptr);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  return validate_personal_initial_attributes_sources_v1(
      world.common, world.personal, *world.catalog->personal,
      original.value->initial());
}
} // namespace hanmo::resident
