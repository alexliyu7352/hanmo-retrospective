#include "native_personal_dialogue_sources4.hpp"
#include "resident/domains/family/born_character_source.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
namespace hanmo::resident {
namespace {
// 历史投影必须持真实 Common 头；此处不猜头中未存的原业务时间。
Outcome<SourceRef> actual_boundary(const CommonStore &common,
                                   const StableId &id) {
  const auto *h = common.state().facts.find(id);
  if (!h || h->id != id)
    return {false,
            {},
            common_failure("PersonalDialogue4OriginalBoundaryRequired", id,
                           ErrorKind::internal)
                .error};
  return {true, {h->domain, h->kind, h->id}, {}};
}
} // namespace
// 每个回调只借本 holder 的稳定原根；晚绑后才能交给原会面 Access 使用。
NativePersonalDialogueSources4::NativePersonalDialogueSources4(
    const CommonStore &common, const InteractionState &interactions,
    const FamilyContinuationPeopleRead4 &people,
    const ProfileControlEligibilitySources &control,
    const BornCharacterSourceRead *born)
    : common_(common), interactions_(interactions), people_(people),
      control_(control), born_(born),
      born_sources_{
          [this](const StableId &actor) {
            return read_born(actor, nullptr, 0);
          },
          [this](const StableId &actor, Integer tick, const StableId &fact)
              -> Outcome<std::optional<BornActorSource>> {
            auto boundary = actual_boundary(common_, fact);
            if (!boundary.ok)
              return {false, {}, boundary.error};
            return read_born(actor, &boundary.value, tick);
          },
          [this](const StableId &actor, Integer tick,
                 const StableId &fact) -> Outcome<ActorPresenceRead> {
            auto valid = validate_bindings();
            if (!valid.ok)
              return {false, {}, valid.error};
            auto boundary = actual_boundary(common_, fact);
            if (!boundary.ok)
              return {false, {}, boundary.error};
            return people_.read_presence_before_original_fact(actor, tick,
                                                              boundary.value);
          },
          [this](Integer tick, const StableId &fact) {
            return player_before(tick, fact, true);
          }},
      known_sources_{
          born_sources_.born,
          born_sources_.born_before,
          [this](const StableId &actor) -> Outcome<ActorPresenceRead> {
            auto valid = validate_bindings();
            if (!valid.ok)
              return {false, {}, valid.error};
            return people_.read_current_presence(actor);
          },
          born_sources_.presence_before,
          [this](Integer tick, const StableId &fact) {
            return player_before(tick, fact, false);
          },
          &people_} {}
// 必须同一人物生命与控制根，Complete4 绝不因漏接而退回旧生日/军伤算法。
Status NativePersonalDialogueSources4::validate_bindings() const {
  if (!people_.bound_to(common_) || &control_.common != &common_ ||
      !control_.actual_clock ||
      people_.control_continuation() != control_.control_continuation ||
      known_sources_.complete_people != &people_ ||
      (born_ && !born_->bound_to(common_)))
    return common_failure("PersonalDialogue4OriginalRootsRequired", {},
                          ErrorKind::internal);
  auto at = people_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto control_at = control_.actual_clock->read_current_moment();
  if (!control_at.ok)
    return {false, {}, control_at.error};
  auto same = compare_world_moments(at.value, control_at.value);
  if (!same.ok)
    return {false, {}, same.error};
  return same.value ? common_failure("PersonalDialogue4PhaseMismatch", {},
                                     ErrorKind::internal)
                    : common_success();
}
// 原年龄 owner 唯一选择声明或 Birth 来源；只有真正 Birth 才返回出生 DTO。
Outcome<std::optional<BornActorSource>>
NativePersonalDialogueSources4::read_born(const StableId &actor,
                                          const SourceRef *boundary,
                                          Integer tick) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  auto presence =
      boundary
          ? people_.read_presence_before_original_fact(actor, tick, *boundary)
          : people_.read_current_presence(actor);
  if (!presence.ok)
    return {false, {}, presence.error};
  auto age = boundary
                 ? people_.read_age_before_original_fact(actor, tick, *boundary)
                 : people_.read_current_age(actor);
  if (!age.ok)
    return {false, {}, age.error};
  const auto *ref = std::get_if<BirthFactRef>(&age.value.ref);
  if (!ref)
    return {true, std::nullopt, {}};
  if (!born_)
    return {false,
            {},
            common_failure("PersonalDialogue4BornOriginalRequired", actor,
                           ErrorKind::unsupported_capability)
                .error};
  auto actual = born_->read_original_birth(*ref);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &source = actual.value;
  const auto *birth = source.birth;
  if (!birth || !source.original_fact ||
      source.original_fact->id != ref->birth_fact_id ||
      birth->actor_id != actor || presence.value.creation_fact_id != actor)
    return {false,
            {},
            common_failure("PersonalDialogue4BornSourceMismatch", actor,
                           ErrorKind::internal)
                .error};
  BornActorSource out;
  out.source_identity = common_.state().identity;
  out.actor_id = actor;
  out.birth_fact_id = ref->birth_fact_id;
  out.creation_fact_id = presence.value.creation_fact_id;
  out.prototype_id = birth->prototype_id;
  out.before_fact_id = boundary ? boundary->id : StableId{};
  out.biological_parent_ids = birth->biological_parent_ids;
  out.birth_tick = birth->tick;
  out.observed_tick = presence.value.observed_tick;
  out.adult = age.value.age_years >= 18;
  out.alive = presence.value.alive;
  return {true, std::move(out), {}};
}
// 仅原 opening 可成为此控制读口的切点；外部消费者仍先核自己保存的
// payload.tick。
Outcome<StableId> NativePersonalDialogueSources4::player_before(
    Integer tick, const StableId &fact, bool born_opening) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  auto boundary = actual_boundary(common_, fact);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  const auto &ref = boundary.value;
  if (tick < 0 || tick > common_.tick() ||
      ref.domain != (born_opening ? born_personal_dialogue_module
                                  : known_personal_dialogue_module) ||
      ref.kind != (born_opening ? born_personal_dialogue_opened_kind
                                : known_personal_dialogue_opened_kind))
    return {
        false,
        {},
        common_failure("PersonalDialogue4ControlBoundaryRequired", fact).error};
  // 本读口自己重读 opening 的保存时间，不能用 caller tick 换历史边界。
  if (born_opening) {
    const auto *opening =
        interactions_.born_personal_dialogues
            ? interactions_.born_personal_dialogues->openings.find(fact)
            : nullptr;
    if (!opening || opening->fact_id != fact || opening->tick != tick)
      return {
          false,
          {},
          common_failure("PersonalDialogue4OpeningTimeMismatch", fact).error};
  } else {
    const auto *opening =
        interactions_.known_personal_dialogues
            ? interactions_.known_personal_dialogues->openings.find(fact)
            : nullptr;
    if (!opening || opening->fact_id != fact || opening->tick != tick)
      return {
          false,
          {},
          common_failure("PersonalDialogue4OpeningTimeMismatch", fact).error};
  }
  auto original = read_profile_control_before_fact(control_, ref);
  if (!original.ok)
    return {false, {}, original.error};
  return {true, std::move(original.value.actor_id), {}};
}
} // namespace hanmo::resident
