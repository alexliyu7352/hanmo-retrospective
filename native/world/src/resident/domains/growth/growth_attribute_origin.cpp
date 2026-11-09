#include "growth_attribute_origin.hpp"
#include "growth_field_origin.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
namespace {
Integer serial(const StableId &id) {
  const auto at = id.rfind(':');
  if (at == StableId::npos)
    return -1;
  const auto tail = id.substr(at + 1);
  Integer n = 0;
  const auto p = std::from_chars(tail.data(), tail.data() + tail.size(), n);
  return p.ec == std::errc{} && p.ptr == tail.data() + tail.size() && n > 0 &&
                 std::to_string(n) == tail
             ? n
             : -1;
}
std::size_t position(const CommonStore &c, const StableId &id) {
  const auto &v = c.state().facts.order();
  return std::size_t(std::find(v.begin(), v.end(), id) - v.begin());
}
bool valid(const ActorAttributes &a) {
  for (auto v :
       {a.administration, a.charisma, a.intellect, a.leadership, a.martial})
    if (v < 1 || v > 110)
      return false;
  return true;
}
bool equal(const ActorAttributes &a, const ActorAttributes &b) {
  return a.administration == b.administration && a.charisma == b.charisma &&
         a.intellect == b.intellect && a.leadership == b.leadership &&
         a.martial == b.martial;
}
Outcome<GrowthAttributeOrigin>
read(const CommonStore &c, const GrowthSkillOriginContext &context,
     const StableId &actor, Integer tick, const StableId &boundary,
     const DynamicStepInput *field_phase = nullptr) {
  auto fail = [&](const StableId &code) -> Outcome<GrowthAttributeOrigin> {
    return {false, {}, common_failure(code, actor).error};
  };
  auto proof =
      field_phase
          ? read_growth_skill_origin_at_field_contact(
                c, context, actor, "commerce", *field_phase,
                boundary.empty() ? nullptr : c.state().facts.find(boundary))
      : boundary.empty()
          ? read_growth_skill_origin(c, context, actor, "commerce")
          : read_growth_skill_origin_before_fact(c, context, actor, "commerce",
                                                 tick, boundary);
  if (!proof.ok)
    return {false, {}, proof.error};
  const auto &source = proof.value;
  const auto *prototype =
      context.family_catalog.prototypes.find(source.prototype_actor_id);
  const auto *person = c.state().actors.find(actor);
  if (!prototype || !person)
    return fail("GrowthAttributeOriginRequired");
  const ActorAttributes *attributes = &prototype->attributes;
  if (source.origin_kind == "character_creation") {
    if (source.source_fact_ids.size() != 2)
      return fail("GrowthAttributeOriginRequired");
    const auto *value =
        context.family.facts.find(source.source_fact_ids.front());
    const auto *made =
        value ? std::get_if<CharacterCreationFact>(value) : nullptr;
    if (!made || made->actor_id != actor)
      return fail("GrowthAttributeOriginRequired");
    attributes = &made->draft.attributes;
  } else if (source.origin_kind != "effective_authored_actor" &&
             source.origin_kind != "adoption_creation" &&
             source.origin_kind != "biological_birth")
    return fail("GrowthAttributeOriginRequired");
  if (!valid(*attributes) || !equal(*attributes, person->attributes))
    return fail("GrowthAttributeHistoryIncomplete");
  const auto edge =
      boundary.empty() ? c.state().facts.size() : position(c, boundary);
  const auto edge_id =
      boundary.empty() ? c.state().next_instance_id : serial(boundary);
  if (edge_id < 1 || edge_id > c.state().next_instance_id)
    return fail("GrowthAttributeBoundaryInvalid");
  Integer previous = 0;
  std::size_t previous_position = 0;
  for (const auto &id : source.source_fact_ids) {
    const auto n = serial(id);
    const auto at = position(c, id);
    if (n <= previous || n >= edge_id || n >= c.state().next_instance_id ||
        at >= edge || (previous && at <= previous_position))
      return fail("GrowthAttributeOriginOrderInvalid");
    previous = n;
    previous_position = at;
  }
  GrowthAttributeOrigin out;
  out.source_identity = c.state().identity;
  out.actor_id = actor;
  out.origin_kind = source.origin_kind;
  out.authored_actor_id = source.authored_actor_id;
  out.prototype_actor_id = source.prototype_actor_id;
  out.before_fact_id = boundary;
  out.created_tick = source.created_tick;
  out.observed_tick = tick;
  out.attributes = *attributes;
  out.source_fact_ids = source.source_fact_ids;
  return {true, std::move(out), {}};
}
} // namespace
Outcome<GrowthAttributeOrigin>
read_growth_attribute_origin(const CommonStore &c,
                             const GrowthSkillOriginContext &x,
                             const StableId &actor) {
  return read(c, x, actor, c.tick(), {});
}
Outcome<GrowthAttributeOrigin> read_growth_attribute_origin_before_fact(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const StableId &actor, Integer tick, const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthAttributeBoundaryInvalid", actor).error};
  return read(c, x, actor, tick, fact);
}
// 五属性沿唯一身份算法取原值；只把实际contact phase传给原观察上界。
Outcome<GrowthAttributeOrigin> read_growth_attribute_origin_at_field_contact(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const StableId &actor, const DynamicStepInput &phase,
    const FactRecord *actual) {
  if (actual &&
      (c.state().facts.find(actual->id) != actual ||
       actual->kind != "field_battle_session" || actual->domain != "military"))
    return {false,
            {},
            common_failure("GrowthFieldActualSessionRequired", actor).error};
  return read(c, x, actor, phase.to_tick, actual ? actual->id : StableId{},
              &phase);
}

} // namespace hanmo::resident
