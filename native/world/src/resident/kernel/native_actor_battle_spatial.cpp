// field 局部 dm / BattleTime 原样返回；Common Location 仅保留既有语义快照。
#include "native_actor_battle_spatial.hpp"
#include "native_military_state.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/field_battle/field_binding_source.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_queue.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
Status required(const char *code, const StableId &id = {}) {
  return common_failure(code, id, ErrorKind::internal);
}
Status layout(const WorldBundle &world) {
  if (!world.catalog)
    return required("FieldActorSpatialWorldRootsRequired");
  return validate_field_battle_layout(
      world.catalog->identity, world.catalog->field_battle.get(),
      world.field_battle ? &*world.field_battle : nullptr);
}
Status header(const WorldBundle &world, const StableId &id, const char *kind) {
  const auto *fact = world.common.state().facts.find(id);
  if (!fact || fact->id != id || fact->domain != "military" ||
      fact->kind != kind)
    return required("FieldActorSpatialSourceHeaderRequired", id);
  return common_success();
}
} // namespace
NativeFieldBattleActorSpatialRead::NativeFieldBattleActorSpatialRead(
    const WorldBundle &world)
    : world_(world) {}
Outcome<std::unique_ptr<NativeFieldBattleActorSpatialRead>>
NativeFieldBattleActorSpatialRead::create(const WorldBundle &world) {
  auto owner = std::unique_ptr<NativeFieldBattleActorSpatialRead>(
      new NativeFieldBattleActorSpatialRead(world));
  auto valid = owner->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(owner), {}};
}
bool NativeFieldBattleActorSpatialRead::bound_to(
    const CommonRead &common,
    const MovementExactState &movement) const noexcept {
  return &common == static_cast<const CommonRead *>(&world_.common) &&
         world_.movement_exact && &movement == &*world_.movement_exact;
}
Status NativeFieldBattleActorSpatialRead::validate_bindings() const {
  auto valid = layout(world_);
  if (!valid.ok)
    return valid;
  auto selected = selected_field_battle_version(world_.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 1)
    return required("FieldActorSpatialSelectionRequired");
  return common_success();
}
Outcome<std::optional<ActorSpatialLocationView>>
NativeFieldBattleActorSpatialRead::read_field_actor_location(
    const StableId &actor_id) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  // 委托原 Session/军团/具名成员校验；坏绑定和未摆位沿原错误返回。
  auto current = world_.services.field_battle_position(actor_id);
  if (!current.ok)
    return {false, {}, std::move(current.error)};
  if (!current.value)
    return {true, std::nullopt, {}};
  const auto *actor = world_.common.actor(actor_id);
  const auto binding =
      world_.field_battle->army_bindings.find(current.value->army_id);
  if (!actor || binding == world_.field_battle->army_bindings.end() ||
      binding->second.battle_id != current.value->battle_id)
    return {false,
            {},
            required("FieldActorSpatialBindingMismatch", actor_id).error};
  ActorSpatialLocationView out;
  out.actor_id = actor_id;
  out.location_snapshot = actor->location;
  // 初始接触与援军入场各保自己的真实源；未入队成员不能借军团身份取位置。
  if (const auto *initial =
          std::get_if<FieldInitialContactBinding>(&binding->second.source)) {
    valid = header(world_, initial->contact_id, "field_battle_contact");
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    out.actual_location_sources.push_back(
        {{"military", "field_battle_contact", initial->contact_id}, 1});
  } else {
    const auto *boundary =
        std::get_if<FieldBoundaryContactBinding>(&binding->second.source);
    const auto session =
        world_.field_battle->sessions.find(current.value->battle_id);
    if (!boundary || session == world_.field_battle->sessions.end())
      return {false, {},
              required("FieldActorBoundarySpatialBindingRequired", actor_id)
                  .error};
    const auto formation =
        session->second.formations.find(current.value->formation_id);
    if (formation == session->second.formations.end())
      return {false, {},
              required("FieldActorBoundarySpatialFormationRequired", actor_id)
                  .error};
    auto original = read_field_original_battle_binding(
        world_.common, *world_.field_battle, binding->second.battle_id,
        binding->second.army_id);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    const auto *origin =
        std::get_if<FieldBoundaryContactBinding>(&original.value->source);
    if (!origin || origin->contact_id != boundary->contact_id)
      return {false, {},
              required("FieldActorBoundarySpatialOriginMismatch", actor_id)
                  .error};
    auto entry = read_field_reinforcement_admission_identity(
        world_.common, *world_.field_battle, session->second,
        formation->second);
    if (!entry.ok)
      return {false, {}, std::move(entry.error)};
    out.actual_location_sources.push_back(
        {{"military", "field_battle_contact", boundary->contact_id}, 1});
    out.actual_location_sources.push_back({entry.value->application_source, 1});
  }
  valid = header(world_, binding->second.battle_id, "field_battle_session");
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  out.actual_location_sources.push_back(
      {{"military", "field_battle_session", binding->second.battle_id}, 1});
  out.actual_anchor = std::move(*current.value);
  return {true, std::move(out), {}};
}
Status validate_native_actor_battle_spatial_sources(
    const WorldBundle &world, const ActorBattleSpatialSources &sources) {
  auto valid = layout(world);
  if (!valid.ok)
    return valid;
  if (!world.movement_exact)
    return required("CompleteActorSpatialMovementRootRequired");
  valid = validate_native_military_state_layout(world);
  if (!valid.ok)
    return valid;
  auto legacy =
      selected_native_military_state_versions(world.catalog->identity);
  if (!legacy.ok)
    return {false, {}, std::move(legacy.error)};
  auto field = selected_field_battle_version(world.catalog->identity);
  if (!field.ok)
    return {false, {}, std::move(field.error)};
  if (bool(legacy.value.dynamic_military) != bool(sources.legacy))
    return required(legacy.value.dynamic_military
                        ? "CompleteOriginalBattleActorSpatialReadRequired"
                        : "UnselectedOriginalBattleActorSpatialRead");
  if (bool(field.value) != bool(sources.field))
    return required(field.value ? "CompleteFieldActorSpatialReadRequired"
                                : "UnselectedFieldActorSpatialRead");
  if ((sources.legacy &&
       !sources.legacy->bound_to(world.common, *world.movement_exact)) ||
      (sources.field &&
       !sources.field->bound_to(world.common, *world.movement_exact)))
    return required("CompleteBattleActorSpatialWorldMismatch");
  return common_success();
}
NativeActorBattleSpatialOwner::NativeActorBattleSpatialOwner(
    const WorldBundle &world, const BattleActorSpatialRead *legacy)
    : world_(world), sources_(legacy, nullptr) {}
Outcome<std::unique_ptr<NativeActorBattleSpatialOwner>>
NativeActorBattleSpatialOwner::create(const WorldBundle &world,
                                      const BattleActorSpatialRead *legacy) {
  auto valid = layout(world);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto selected = selected_field_battle_version(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto owner = std::unique_ptr<NativeActorBattleSpatialOwner>(
      new NativeActorBattleSpatialOwner(world, legacy));
  if (selected.value) {
    auto field = NativeFieldBattleActorSpatialRead::create(world);
    if (!field.ok)
      return {false, {}, std::move(field.error)};
    owner->field_ = std::move(field.value);
    owner->sources_ = ActorBattleSpatialSources(legacy, owner->field_.get());
  }
  valid = owner->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(owner), {}};
}
Status NativeActorBattleSpatialOwner::validate_bindings() const {
  if (sources_.field != field_.get())
    return required("FieldActorSpatialOwnerMismatch");
  return validate_native_actor_battle_spatial_sources(world_, sources_);
}
} // namespace hanmo::resident
