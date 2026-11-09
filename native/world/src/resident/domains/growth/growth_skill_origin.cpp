#include "growth_skill_api.hpp"
// 每种动态身份只沿自己原来的永久分配事实；旧收养是身份创建，成年收养不重置既有人物技艺。
#include "growth_skill_origin.hpp"
#include "growth_field_origin.hpp"
#include "resident/domains/family/family_birth.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
namespace {
bool allocated(const CommonStore &c, const StableId &id,
               std::string_view prefix) {
  if (id.size() <= prefix.size() + 1 ||
      std::string_view(id).substr(0, prefix.size()) != prefix ||
      id[prefix.size()] != ':')
    return false;
  const auto digits = std::string_view(id).substr(prefix.size() + 1);
  Integer n = 0;
  auto r = std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return r.ec == std::errc{} && r.ptr == digits.data() + digits.size() &&
         n > 0 && n < c.state().next_instance_id && digits.front() != '0';
}
std::size_t position(const CommonStore &c, const StableId &id) {
  const auto &order = c.state().facts.order();
  return static_cast<std::size_t>(std::find(order.begin(), order.end(), id) -
                                  order.begin());
}
bool header(const CommonStore &c, const StableId &id, std::string_view kind,
            std::string_view domain) {
  const auto *h = c.state().facts.find(id);
  return h && h->id == id && h->kind == kind && h->domain == domain &&
         position(c, id) < c.state().facts.size();
}
Outcome<GrowthSkillOriginBorrowed>
read_borrowed(const CommonStore &c, const GrowthSkillOriginContext &x,
              const StableId &actor, const StableId &skill, Integer tick,
              const StableId &boundary,
              const DynamicStepInput *field_phase = nullptr) {
  auto fail = [&](const StableId &code = "GrowthSkillOriginUnavailable")
      -> Outcome<GrowthSkillOriginBorrowed> {
    return {false, {}, common_failure(code, actor).error};
  };
  Integer through = c.tick();
  if (field_phase) {
    if (c.tick() < 0 || c.tick() >= 9007199254740991LL ||
        field_phase->from_tick != c.tick() ||
        field_phase->to_tick != c.tick() + 1 || tick != field_phase->to_tick)
      return fail("GrowthFieldOriginPhaseInvalid");
    through = field_phase->to_tick;
  }
  if (tick < 0 || tick > through)
    return fail("GrowthSkillOriginBoundaryInvalid");
  auto cutoff = c.state().facts.size();
  if (!boundary.empty()) {
    const auto *h = c.state().facts.find(boundary);
    cutoff = position(c, boundary);
    if (!h || h->id != boundary || cutoff == c.state().facts.size())
      return fail("GrowthSkillOriginBoundaryInvalid");
  }
  const auto *person = c.state().actors.find(actor);
  if (!person)
    return fail();
  GrowthSkillOriginBorrowed out;
  out.source_identity = c.state().identity;
  out.actor_id = actor;
  out.skill_id = skill;
  const ActorSkills *original = nullptr;
  if (std::find(x.common.actor_ids.begin(), x.common.actor_ids.end(), actor) !=
      x.common.actor_ids.end()) {
    const auto *p = x.family_catalog.prototypes.find(actor);
    if (!p || !x.common.actor_initial_birth.find(actor) ||
        (person->prototype_id && *person->prototype_id != actor) ||
        (person->created_tick && *person->created_tick != 0) ||
        x.family.facts.find(actor))
      return fail();
    original = &p->skills;
    out.origin_kind = "effective_authored_actor";
    out.authored_actor_id = actor;
    out.prototype_actor_id = actor;
  } else {
    if (!allocated(c, actor, "actor") || !person->created_tick ||
        !person->birth_tick || !person->prototype_id ||
        *person->created_tick < 0 || *person->created_tick > tick ||
        position(c, actor) >= cutoff)
      return fail();
    const auto *prototype =
        x.family_catalog.prototypes.find(*person->prototype_id);
    if (!prototype)
      return fail();
    out.created_tick = *person->created_tick;
    out.prototype_actor_id = *person->prototype_id;
    const auto *value = x.family.facts.find(actor);
    const auto *creation =
        value ? std::get_if<IdentityCreationFact>(value) : nullptr;
    if (creation) {
      if (!header(c, actor, "actor_creation", "family") ||
          creation->actor_id != actor ||
          creation->prototype_id != out.prototype_actor_id ||
          creation->created_tick != out.created_tick ||
          creation->birth_tick != *person->birth_tick ||
          position(c, creation->source) >= position(c, actor))
        return fail();
      const auto *source = x.family.facts.find(creation->source);
      if (!source)
        return fail();
      if (const auto *made = std::get_if<CharacterCreationFact>(source)) {
        if (!allocated(c, creation->source, "family") ||
            !header(c, creation->source, "family_character_creation",
                    "family") ||
            made->actor_id != actor || made->created_tick != out.created_tick ||
            made->prototype_actor_id != out.prototype_actor_id ||
            made->scenario_id != c.state().metadata.scenario_id)
          return fail();
        original = &made->draft.skills;
        out.origin_kind = "character_creation";
      } else if (const auto *adopted = std::get_if<AdoptionFact>(source)) {
        if (!allocated(c, creation->source, "action") ||
            !header(c, creation->source, "family_adoption", "family") ||
            adopted->kind != "family_adoption" || !adopted->success ||
            adopted->child_actor_id != actor ||
            adopted->completed_tick != out.created_tick ||
            adopted->terms.prototype_id != out.prototype_actor_id ||
            adopted->terms.birth_tick != *person->birth_tick)
          return fail();
        original = &prototype->skills;
        out.origin_kind = "adoption_creation";
      } else
        return fail();
      out.source_fact_ids = {creation->source, actor};
    } else {
      if (value || !x.births ||
          !header(c, actor, "actor_creation", "family_birth"))
        return fail();
      const FamilyBirthRecord *birth = nullptr;
      const StableId *source = nullptr;
      for (const auto &row : x.births->births.rows())
        if (row.second.actor_id == actor) {
          if (birth)
            return fail();
          birth = &row.second;
          source = &row.first;
        }
      if (!birth || !source || !allocated(c, *source, "family_birth") ||
          !header(c, *source, "family_birth_recorded", "family_birth") ||
          birth->kind != "family_birth_recorded" ||
          birth->prototype_id != out.prototype_actor_id ||
          birth->tick != out.created_tick ||
          birth->tick != *person->birth_tick ||
          position(c, *source) >= position(c, actor))
        return fail();
      original = &prototype->skills;
      out.origin_kind = "biological_birth";
      out.source_fact_ids = {*source, actor};
    }
  }
  // 原十艺/扩展六艺都由唯一growth_skill_xp处理；未知技能及缺扩展源不猜0级。
  if ((skill == "naval" || skill == "espionage" || skill == "fortification" ||
       skill == "agriculture" || skill == "medicine" || skill == "forging") &&
      (!x.common.extended_skills || !original->extended))
    return fail();
  out.initial_xp = growth_skill_xp(*original, skill);
  if (out.initial_xp < 0 || out.initial_xp > 1200)
    return fail();
  return {true, std::move(out), {}};
}
GrowthSkillOrigin legacy_copy(const GrowthSkillOriginBorrowed &v) {
  GrowthSkillOrigin out;
  out.source_identity = v.source_identity;
  out.actor_id = v.actor_id;
  out.skill_id = v.skill_id;
  out.origin_kind = v.origin_kind;
  out.authored_actor_id = v.authored_actor_id;
  out.prototype_actor_id = v.prototype_actor_id;
  out.initial_xp = v.initial_xp;
  out.created_tick = v.created_tick;
  for (auto id : v.source_fact_ids)
    out.source_fact_ids.emplace_back(id);
  return out;
}
Outcome<GrowthSkillOrigin> read(const CommonStore &c,
                                const GrowthSkillOriginContext &x,
                                const StableId &actor, const StableId &skill,
                                Integer tick, const StableId &boundary,
                                const DynamicStepInput *phase = nullptr) {
  auto out = read_borrowed(c, x, actor, skill, tick, boundary, phase);
  if (!out.ok)
    return {false, {}, std::move(out.error)};
  return {true, legacy_copy(out.value), {}};
}
} // namespace
Outcome<GrowthSkillOrigin>
read_growth_skill_origin(const CommonStore &c,
                         const GrowthSkillOriginContext &x,
                         const StableId &actor, const StableId &skill) {
  return read(c, x, actor, skill, c.tick(), {});
}
Outcome<GrowthSkillOrigin> read_growth_skill_origin_before_fact(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const StableId &actor, const StableId &skill, Integer tick,
    const StableId &fact) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSkillOriginBoundaryInvalid", actor).error};
  return read(c, x, actor, skill, tick, fact);
}
// 只有Field唯一contact安装栈可传此明确phase，stable通用口仍拒未来tick。
Outcome<GrowthSkillOrigin> read_growth_skill_origin_at_field_contact(
    const CommonStore &c, const GrowthSkillOriginContext &x,
    const StableId &actor, const StableId &skill, const DynamicStepInput &phase,
    const FactRecord *actual) {
  if (actual &&
      (c.state().facts.find(actual->id) != actual ||
       actual->kind != "field_battle_session" || actual->domain != "military"))
    return {false,
            {},
            common_failure("GrowthFieldActualSessionRequired", actor).error};
  return read(c, x, actor, skill, phase.to_tick,
              actual ? actual->id : StableId{}, &phase);
}

} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthSkillOriginBorrowed>
read_growth_skill_origin_borrowed(const CommonStore &c,
                                  const GrowthSkillOriginContext &x,
                                  const StableId &a, const StableId &s) {
  return read_borrowed(c, x, a, s, c.tick(), {});
}
Outcome<GrowthSkillOriginBorrowed>
read_growth_skill_origin_before_fact_borrowed(const CommonStore &c,
                                              const GrowthSkillOriginContext &x,
                                              const StableId &a,
                                              const StableId &s, Integer tick,
                                              const StableId &fact) {
  if (fact.empty())
    return {
        false, {}, common_failure("GrowthSkillOriginBoundaryInvalid", a).error};
  return read_borrowed(c, x, a, s, tick, fact);
}
} // namespace hanmo::resident
