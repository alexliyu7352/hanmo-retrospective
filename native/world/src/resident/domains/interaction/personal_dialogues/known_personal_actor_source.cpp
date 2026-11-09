// 完整原人物/年龄/死亡源前置；历史使用确切实际事实，不以今天生死否定先前合法会面。
#include "known_personal_actor_source.hpp"
#include "resident/domains/family/actor_age_read.hpp"
#include "resident/domains/family/family_continuation_people4.hpp"
#include <charconv>
namespace hanmo::resident {
namespace {
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool ordered_before(const CommonStore &c, const StableId &a,
                    const StableId &b) {
  auto number = [&](const StableId &id) -> std::optional<Integer> {
    auto colon = id.find(':');
    if (colon == StableId::npos)
      return {};
    const auto text = id.substr(colon + 1);
    Integer n = 0;
    const auto p = std::from_chars(text.data(), text.data() + text.size(), n);
    if (p.ec != std::errc{} || p.ptr != text.data() + text.size() || n < 1 ||
        n >= c.state().next_instance_id || std::to_string(n) != text)
      return {};
    return n;
  };
  const auto x = number(a), y = number(b);
  const auto &order = c.state().facts.order();
  const auto i = std::find(order.begin(), order.end(), a),
             j = std::find(order.begin(), order.end(), b);
  return x && y && *x < *y && i != order.end() && j != order.end() && i < j;
}
Outcome<KnownPersonalActorOrigin>
read(const CommonStore &c, const CommonCatalog &catalog,
     const KnownPersonalDialogueSources &sources,
     const KnownPersonalDialogueSelector &selector, const StableId &actor,
     Integer tick, const StableId &before, bool historical) {
  auto fail = [&](const StableId &code) -> Outcome<KnownPersonalActorOrigin> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (actor.empty() || tick < 0 || tick > c.tick() ||
      (historical ? before.empty() : (!before.empty() || tick != c.tick())))
    return fail("KnownPersonalActorBoundaryRequired");
  // Complete4 声明年龄无伪生日；同根原年龄引用替代旧整数生日算法。
  std::optional<ActorAgeRead> legacy_age;
  std::optional<LifeAgeBasisSnapshot> complete_age;
  if (sources.complete_people) {
    if (!sources.complete_people->bound_to(c))
      return fail("KnownPersonalActorPeopleCandidateMismatch");
    const auto *boundary = historical ? c.state().facts.find(before) : nullptr;
    if (historical && !boundary)
      return fail("KnownPersonalActorBoundaryRequired");
    auto age =
        historical
            ? sources.complete_people->read_age_before_original_fact(
                  actor, tick, {boundary->domain, boundary->kind, before})
            : sources.complete_people->read_current_age(actor);
    if (!age.ok)
      return {false, {}, age.error};
    if (age.value.age_years < 18)
      return fail("KnownPersonalActorAdultRequired");
    complete_age = std::move(age.value);
  } else {
    auto age = historical
                   ? read_actor_age_before_fact(c, catalog, actor, tick, before)
                   : read_actor_age(c, catalog, actor);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return fail("KnownPersonalActorAdultRequired");
    legacy_age = std::move(age.value);
  }
  if (historical ? !bool(sources.presence_before) : !bool(sources.presence))
    return fail("KnownPersonalActorPresenceRequired");
  auto presence = historical ? sources.presence_before(actor, tick, before)
                             : sources.presence(actor);
  if (!presence.ok)
    return {false, {}, presence.error};
  const auto &p = presence.value;
  if (p.actor_id != actor || p.before_fact_id != before ||
      p.observed_tick != tick || !p.alive ||
      !identity(p.source_identity, c.state().identity))
    return fail("KnownPersonalActorPresenceMismatch");
  const bool installed =
      std::find(catalog.actor_ids.begin(), catalog.actor_ids.end(), actor) !=
      catalog.actor_ids.end();
  if (selector.kind == "installed_actor") {
    if (selector.source_id != actor || !installed ||
        !p.creation_fact_id.empty())
      return fail("KnownPersonalActorInstalledSourceRequired");
    return {true, {"installed_actor", actor, {}, {}, {}}, {}};
  }
  if (selector.kind != "born_prototype" || selector.source_id.empty() ||
      installed ||
      (historical ? !bool(sources.born_before) : !bool(sources.born)))
    return fail("KnownPersonalActorBirthSourceRequired");
  auto born = historical ? sources.born_before(actor, tick, before)
                         : sources.born(actor);
  if (!born.ok)
    return {false, {}, born.error};
  if (!born.value)
    return fail("KnownPersonalActorBirthSourceRequired");
  const auto &b = *born.value;
  if (b.actor_id != actor || b.prototype_id != selector.source_id ||
      b.birth_fact_id.empty() || b.creation_fact_id != actor ||
      b.creation_fact_id != p.creation_fact_id || !b.adult || !b.alive ||
      (legacy_age && b.birth_tick != legacy_age->birth_tick) ||
      b.observed_tick != tick || b.before_fact_id != before ||
      !identity(b.source_identity, c.state().identity))
    return fail("KnownPersonalActorBirthSourceMismatch");
  if (complete_age) {
    const auto *birth = std::get_if<BirthFactRef>(&complete_age->ref);
    if (!birth || birth->birth_fact_id != b.birth_fact_id)
      return fail("KnownPersonalActorBirthAgeSourceMismatch");
  }
  if (historical && (!ordered_before(c, b.birth_fact_id, before) ||
                     !ordered_before(c, b.creation_fact_id, before)))
    return fail("KnownPersonalActorBirthBoundaryMismatch");
  return {true,
          {"recorded_birth", actor, b.birth_fact_id, b.creation_fact_id,
           b.prototype_id},
          {}};
}
} // namespace
Outcome<KnownPersonalActorOrigin> read_known_personal_actor_origin(
    const CommonStore &c, const CommonCatalog &catalog,
    const KnownPersonalDialogueSources &sources,
    const KnownPersonalDialogueSelector &selector, const StableId &actor) {
  return read(c, catalog, sources, selector, actor, c.tick(), {}, false);
}
Outcome<KnownPersonalActorOrigin> read_known_personal_actor_origin_before_fact(
    const CommonStore &c, const CommonCatalog &catalog,
    const KnownPersonalDialogueSources &sources,
    const KnownPersonalDialogueSelector &selector, const StableId &actor,
    Integer tick, const StableId &fact) {
  return read(c, catalog, sources, selector, actor, tick, fact, true);
}
bool same_known_personal_actor_origin(const KnownPersonalActorOrigin &a,
                                      const KnownPersonalActorOrigin &b) {
  return a.kind == b.kind && a.actor_id == b.actor_id &&
         a.birth_fact_id == b.birth_fact_id &&
         a.creation_fact_id == b.creation_fact_id &&
         a.prototype_id == b.prototype_id;
}
} // namespace hanmo::resident
