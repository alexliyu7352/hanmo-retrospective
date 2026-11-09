// 源码永久事实提供原始XP与逐笔净增；唯一阶段合同决定同半日自动教学和零时补答的先后。
#include "growth_naval_history.hpp"
#include "growth_field_history.hpp"
#include "growth_internal.hpp"
#include "growth_military_entry_history.hpp"
#include "growth_personal_entry_history.hpp"
#include "growth_personal_mastery_history.hpp"
#include "growth_siege_engineering_history.hpp"
#include "growth_tactics_history.hpp"
#include "resident/domains/family/family_birth.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 初始统率只取不可变创建来源；当前属性仅用于核对没有未解释的写者。
struct Origin {
  Integer xp = 0, tick = 0;
  StableId authored;
  std::vector<StableId> sources;
  Integer leadership = 0, martial = 0;
};
enum class EntryBoundary {
  naval_preparation,
  land_entry,
  siege_engineering,
  personal_entry,
  field_contact,
  field_exact_contact,
  field_current_preview,
  field_zero_time
};
struct Credit {
  Integer tick = 0, before = 0, gained = 0;
  bool zero_time = false;
  StableId source;
  std::size_t source_order = 0;
};
// 实例号只检真格式与分配范围，先后始终取Common原事实序。
bool allocated(const CommonStore &c, const StableId &id,
               const StableId &prefix) {
  const auto head = prefix + ":";
  if (id.rfind(head, 0) != 0)
    return false;
  const auto digits = id.substr(head.size());
  Integer n = 0;
  auto p = std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return p.ec == std::errc{} && p.ptr == digits.data() + digits.size() &&
         n > 0 && n < c.state().next_instance_id && std::to_string(n) == digits;
}
std::size_t position(const CommonStore &c, const StableId &id) {
  const auto &order = c.state().facts.order();
  return std::size_t(std::find(order.begin(), order.end(), id) - order.begin());
}
bool header(const CommonStore &c, const StableId &id, const StableId &kind,
            const StableId &domain) {
  const auto *h = c.state().facts.find(id);
  return h && h->id == id && h->kind == kind && h->domain == domain &&
         position(c, id) < c.state().facts.size();
}
// 作者、本人真实创建、旧收养实例和生物出生分别有独立源；当前prototype字段不足以证明初始XP。
Outcome<Origin> origin(const CommonStore &c, const GrowthNavalHistoryContext &x,
                       const StableId &actor, Integer tick,
                       std::size_t boundary, const StableId &skill,
                       bool same_tick_creation = false) {
  auto bad = [&]() -> Outcome<Origin> {
    return {false,
            {},
            common_failure("GrowthNavalSkillOriginUnavailable", actor).error};
  };
  const auto *person = c.state().actors.find(actor);
  if (!person || (skill == "naval" && !x.common.extended_skills))
    return bad();
  if (std::find(x.common.actor_ids.begin(), x.common.actor_ids.end(), actor) !=
      x.common.actor_ids.end()) {
    const auto *p = x.family_catalog.prototypes.find(actor);
    if (!p || !x.common.actor_initial_birth.find(actor) ||
        (person->prototype_id && *person->prototype_id != actor) ||
        (person->created_tick && *person->created_tick != 0) ||
        x.family.facts.find(actor))
      return bad();
    const auto xp = growth_skill_xp(p->skills, skill);
    if (xp < 0 || xp > 1200)
      return bad();
    return {true,
            {xp, 0, actor, {}, p->attributes.leadership, p->attributes.martial},
            {}};
  }
  if (!allocated(c, actor, "actor") || !person->created_tick ||
      !person->prototype_id || *person->created_tick < 0 ||
      *person->created_tick > tick || position(c, actor) >= boundary ||
      (!same_tick_creation && boundary < c.state().facts.size() &&
       *person->created_tick >= tick))
    return bad();
  const auto *prototype =
      x.family_catalog.prototypes.find(*person->prototype_id);
  if (!prototype)
    return bad();
  Origin out;
  out.tick = *person->created_tick;
  out.leadership = prototype->attributes.leadership;
  out.martial = prototype->attributes.martial;
  const auto *value = x.family.facts.find(actor);
  const auto *creation =
      value ? std::get_if<IdentityCreationFact>(value) : nullptr;
  if (creation) {
    if (!header(c, actor, "actor_creation", "family") ||
        creation->actor_id != actor ||
        creation->prototype_id != *person->prototype_id ||
        creation->created_tick != out.tick ||
        position(c, creation->source) >= position(c, actor))
      return bad();
    const auto *source = x.family.facts.find(creation->source);
    if (!source)
      return bad();
    if (const auto *made = std::get_if<CharacterCreationFact>(source)) {
      if (!header(c, creation->source, "family_character_creation", "family") ||
          !allocated(c, creation->source, "family") ||
          made->actor_id != actor || made->created_tick != out.tick ||
          made->prototype_actor_id != creation->prototype_id ||
          made->scenario_id != c.state().metadata.scenario_id)
        return bad();
      out.xp = growth_skill_xp(made->draft.skills, skill);
      out.leadership = made->draft.attributes.leadership;
      out.martial = made->draft.attributes.martial;
    } else if (const auto *adopted = std::get_if<AdoptionFact>(source)) {
      if (!header(c, creation->source, "family_adoption", "family") ||
          !adopted->success || adopted->kind != "family_adoption" ||
          adopted->child_actor_id != actor ||
          adopted->completed_tick != out.tick ||
          adopted->terms.prototype_id != creation->prototype_id)
        return bad();
      out.xp = growth_skill_xp(prototype->skills, skill);
    } else
      return bad();
    out.sources = {creation->source, actor};
  } else {
    if (!x.births || !header(c, actor, "actor_creation", "family_birth"))
      return bad();
    const FamilyBirthRecord *birth = nullptr;
    StableId id;
    for (const auto &row : x.births->births.rows())
      if (row.second.actor_id == actor) {
        if (birth)
          return bad();
        birth = &row.second;
        id = row.first;
      }
    if (!birth || !header(c, id, "family_birth_recorded", "family_birth") ||
        !allocated(c, id, "family_birth") || birth->tick != out.tick ||
        birth->prototype_id != *person->prototype_id ||
        position(c, id) >= position(c, actor))
      return bad();
    out.xp = growth_skill_xp(prototype->skills, skill);
    out.sources = {id, actor};
  }
  if (out.xp < 0 || out.xp > 1200)
    return bad();
  return {true, std::move(out), {}};
}
// 同一只读内核同时重建全部今日链并截取真实准备前缀；任何无法解释的当前XP差额均拒绝。
Outcome<GrowthNavalSkillObservation>
read(const CommonStore &c, const GrowthNavalHistoryContext &x,
     const GrowthState &state, const StableId &actor, Integer observed,
     const StableId &boundary_id, const DynamicStepInput *phase,
     const StableId &skill, EntryBoundary boundary_kind,
     Integer *leadership = nullptr, Integer *martial = nullptr) {
  auto bad = [&](const StableId &code,
                 const StableId &ref =
                     StableId{}) -> Outcome<GrowthNavalSkillObservation> {
    return {false, {}, common_failure(code, ref.empty() ? actor : ref).error};
  };
  // 只有下方具名入口在读取真实 S0 proof 后能选择此私有当前预览分支。
  // 所有原课开始/净增仍整链验证，不容许 phase.to 的未来值倒灌当前。
  Integer through = boundary_kind == EntryBoundary::field_current_preview
                        ? observed
                        : c.tick();
  if (phase) {
    if (c.tick() < 0 || c.tick() >= 9007199254740991LL ||
        phase->from_tick != c.tick() || phase->to_tick != c.tick() + 1)
      return bad("GrowthNavalSkillPhaseInvalid");
    through = phase->to_tick;
  }
  if (observed < 0 || observed > through)
    return bad("GrowthNavalSkillBoundaryUnavailable");
  const bool historical = !boundary_id.empty();
  std::size_t boundary = c.state().facts.size();
  if (historical) {
    const bool exact =
        boundary_kind == EntryBoundary::naval_preparation
            ? allocated(c, boundary_id, "naval_skill") &&
                  header(c, boundary_id, "naval_gunnery_preparation",
                         "military_naval")
        : boundary_kind == EntryBoundary::land_entry
            ? allocated(c, boundary_id, "military_rout_entry") &&
                  header(c, boundary_id, "military_rout_entry", "military")
        : boundary_kind == EntryBoundary::siege_engineering
            ? allocated(c, boundary_id, "military_siege_engineering") &&
                  header(c, boundary_id, "military_siege_engineering_prepared",
                         "military")
        : boundary_kind == EntryBoundary::field_contact
            ? (header(c, boundary_id, "field_battle_session", "military") ||
               header(c, boundary_id, "field_battle_started", "military") ||
               header(c, boundary_id, "military_council", "military"))
        : boundary_kind == EntryBoundary::field_exact_contact
            ? (header(c, boundary_id, "field_battle_started", "military") ||
               header(c, boundary_id, "military_council", "military"))
        : boundary_kind == EntryBoundary::field_zero_time
            ? (header(c, boundary_id, "field_battle_started", "military") ||
               header(c, boundary_id, "field_battle_entered", "military") ||
               header(c, boundary_id, "military_council", "military") ||
               header(c, boundary_id, "field_battle_stratagem", "military") ||
               header(c, boundary_id, "field_commander_changed", "military"))
            : allocated(c, boundary_id, "personal_entry") &&
                  header(c, boundary_id, "personal_combat_entered", "personal");
    if ((observed < 1 && boundary_kind != EntryBoundary::personal_entry &&
         boundary_kind != EntryBoundary::field_zero_time &&
         boundary_kind != EntryBoundary::field_exact_contact) ||
        !exact)
      return bad("GrowthNavalSkillBoundaryUnavailable");
    boundary = position(c, boundary_id);
  }
  // 围城准备在actions_step前；热当前借口连新身份也只可观察from。旧接敌入口仍观察to。
  const bool early_siege = boundary_kind == EntryBoundary::siege_engineering &&
                           (historical || phase);
  const auto origin_tick =
      early_siege && !historical ? phase->from_tick : observed;
  auto initial = origin(c, x, actor, origin_tick, boundary, skill,
                        boundary_kind == EntryBoundary::personal_entry ||
                            boundary_kind == EntryBoundary::field_zero_time);
  if (!initial.ok)
    return {false, {}, initial.error};
  const auto *person = c.state().actors.find(actor);
  const auto today = growth_skill_xp(person->skills, skill);
  if (today < initial.value.xp || today > 1200)
    return bad("GrowthNavalSkillHistoryIncomplete");
  if (leadership) {
    if (initial.value.leadership < 1 || initial.value.leadership > 110 ||
        person->attributes.leadership != initial.value.leadership)
      return bad("GrowthMilitaryEntryLeadershipHistoryIncomplete");
    *leadership = initial.value.leadership;
  }
  if (martial) {
    if (initial.value.martial < 1 || initial.value.martial > 110 ||
        person->attributes.martial != initial.value.martial)
      return bad("GrowthPersonalEntryMartialHistoryIncomplete");
    *martial = initial.value.martial;
  }
  std::vector<Credit> credits;
  for (const auto &row : c.state().facts.rows())
    if (row.second.kind == "skill_study") {
      const auto *f = state.facts.find(row.first);
      if (!f || !std::get_if<StudyFact>(f))
        return bad("GrowthNavalSkillStudySourceMissing", row.first);
    }
  for (const auto &row : state.facts.rows()) {
    const auto *f = std::get_if<StudyFact>(&row.second);
    if (!f || f->terms.student_id != actor || f->terms.skill_id != skill)
      continue;
    if (!allocated(c, row.first, "action") ||
        !header(c, row.first, "skill_study", "cultivation") ||
        f->actor_id != actor || f->start_tick < initial.value.tick ||
        f->start_tick > through ||
        (f->phase != "started" && f->phase != "completed"))
      return bad("GrowthNavalSkillStudySourceInvalid", row.first);
    // 防止已准许的零时入场稍后被同tick取消/迟答反向弄成不可重放：尚可补XP的原课先收口。
    // 新开的课程同tick尚无课时，取消净增为零，不应让更早的合法入场失效。
    if ((boundary_kind == EntryBoundary::personal_entry ||
         boundary_kind == EntryBoundary::field_zero_time) &&
        !historical && f->phase == "started") {
      if (!growth::study_segmented(*f) && !f->lessons.empty() &&
          growth::study_xp_gained(*f) > 0)
        return bad(boundary_kind == EntryBoundary::field_zero_time
                       ? "GrowthFieldUnsettledStudy"
                       : "GrowthPersonalEntryUnsettledStudy",
                   row.first);
      if (growth::study_segmented(*f) &&
          (!f->challenge || !f->challenge->result))
        for (const auto &lesson : f->lessons)
          if (lesson.segment && lesson.segment->performance_pending &&
              !lesson.segment->adjustment && lesson.xp > 0)
            return bad(boundary_kind == EntryBoundary::field_zero_time
                           ? "GrowthFieldUnsettledStudy"
                           : "GrowthPersonalEntryUnsettledStudy",
                       row.first);
    }
    auto add = [&](Integer tick, Integer before, Integer gain,
                   bool zero) -> Status {
      if (tick < f->start_tick || tick > through ||
          (!zero && tick <= f->start_tick) || (zero && tick > c.tick()) ||
          before < 0 || before > 1200 || gain < 0 || gain > 1200 - before)
        return common_failure("GrowthNavalSkillCreditInvalid", row.first);
      credits.push_back(
          {tick, before, gain, zero, row.first, position(c, row.first)});
      return common_success();
    };
    if (growth::study_segmented(*f)) {
      for (const auto &lesson : f->lessons) {
        if (!lesson.segment)
          return bad("GrowthNavalSkillStudySourceInvalid", row.first);
        const auto &s = *lesson.segment;
        auto v = add(s.settled_tick, s.xp_before, s.xp_gained, false);
        if (!v.ok)
          return {false, {}, v.error};
        if (s.adjustment) {
          const auto &a = *s.adjustment;
          if (a.settled_tick < s.settled_tick)
            return bad("GrowthNavalSkillCreditInvalid", row.first);
          v = add(a.settled_tick, a.xp_before, a.xp_gained, true);
          if (!v.ok)
            return {false, {}, v.error};
        }
      }
    } else if (f->phase == "completed") {
      auto v = add(f->completed_tick,
                   f->xp_settlement_before.value_or(f->terms.xp_before),
                   growth::study_xp_gained(*f), f->code == "StudyCancelled");
      if (!v.ok)
        return {false, {}, v.error};
    }
  }
  // 一个半日先运行全部原Action，再执行该tick的零时命令；相同阶段以真实XP前值链接，零增量先行。
  std::stable_sort(credits.begin(), credits.end(),
                   [](const Credit &a, const Credit &b) {
                     return std::tie(a.tick, a.zero_time, a.before, a.gained,
                                     a.source_order) <
                            std::tie(b.tick, b.zero_time, b.before, b.gained,
                                     b.source_order);
                   });
  GrowthNavalSkillObservation result;
  result.source_identity = c.state().identity;
  result.actor_id = actor;
  result.skill_id = skill;
  result.before_fact_id = boundary_id;
  result.authored_actor_id = initial.value.authored;
  result.observed_tick = observed;
  result.experience = initial.value.xp;
  result.causal_fact_ids = initial.value.sources;
  Integer actual = initial.value.xp;
  std::set<StableId> causes(result.causal_fact_ids.begin(),
                            result.causal_fact_ids.end());
  for (const auto &event : credits) {
    if (event.before != actual)
      return bad("GrowthNavalSkillHistoryIncomplete", event.source);
    actual += event.gained;
    // 个人战是零时入口，课程开始头不证明同tick取消/迟答在入场前还是后；冷热同时拒该歧义。
    if ((boundary_kind == EntryBoundary::personal_entry ||
         boundary_kind == EntryBoundary::field_zero_time) &&
        event.gained == 0)
      continue;
    if (boundary_kind == EntryBoundary::personal_entry &&
        event.tick == observed && event.zero_time)
      return bad("GrowthPersonalEntryZeroTimeHistoryUnavailable", event.source);
    // Field零时支没有Study实际采用头，连自动净增也不以同tick替代Common先后。
    // 这里只拒无法证明的正净增，原零增量/异tick链仍由同一个reducer完整反验。
    if (boundary_kind == EntryBoundary::field_zero_time &&
        event.tick == observed)
      return bad("GrowthFieldSameTickApplicationSourceRequired", event.source);
    const bool included = early_siege
                              ? event.tick < observed
                              : (!historical || event.tick < observed ||
                                 (event.tick == observed && !event.zero_time));
    if (included) {
      if (event.source_order >= boundary)
        return bad("GrowthNavalSkillHistoryOrderInvalid", event.source);
      result.experience += event.gained;
      if (causes.insert(event.source).second)
        result.causal_fact_ids.push_back(event.source);
    }
  }
  if (actual != today || result.experience > today)
    return bad("GrowthNavalSkillHistoryIncomplete");
  result.level = growth_skill_level(result.experience);
  return {true, std::move(result), {}};
}
} // namespace
namespace {
// 只读Field原四技艺，不将此入口推广为任意skill/新玩法的动态注册口。
Status field_skill(const StableId &skill) {
  return skill == "infantry" || skill == "cavalry" || skill == "archery" ||
                 skill == "tactics"
             ? common_success()
             : common_failure("GrowthFieldOriginalSkillRequired", skill);
}
// 原来源集合排序只给消费方确定顺序，XP数值仍取原逐笔reducer，不按ID猜因果。
Outcome<GrowthFieldSkillObservation>
field_read(const CommonStore &c, const GrowthFieldHistoryContext &x,
           const GrowthState &state, const StableId &actor,
           const StableId &skill, Integer tick, const StableId &boundary,
           const DynamicStepInput *phase, EntryBoundary kind) {
  auto valid = field_skill(skill);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto out = read(c, x, state, actor, tick, boundary, phase, skill, kind);
  if (!out.ok)
    return out;
  std::sort(out.value.causal_fact_ids.begin(), out.value.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  return out;
}
} // namespace
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_at_contact(
    const CommonStore &c, const GrowthFieldHistoryContext &x,
    const GrowthState &state, const StableId &actor, const StableId &skill,
    const DynamicStepInput &phase) {
  return field_read(c, x, state, actor, skill, phase.to_tick, {}, &phase,
                    EntryBoundary::field_contact);
}
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_current_preview(
    const CommonStore &c, const GrowthFieldHistoryContext &x,
    const GrowthState &state, const StableId &actor, const StableId &skill,
    const OriginalWorldReadBoundary &boundary, const CurrentMomentRead &clock) {
  if (!boundary.bound_to(c, clock))
    return {false,
            {},
            common_failure("GrowthFieldCurrentBoundaryBindingRequired", actor)
                .error};
  auto observed = boundary.read_observed_moment();
  if (!observed.ok)
    return {false, {}, observed.error};
  return field_read(c, x, state, actor, skill, observed.value.half_day_tick, {},
                    nullptr, EntryBoundary::field_current_preview);
}
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_before_zero_time(
    const CommonStore &c, const GrowthFieldHistoryContext &x,
    const GrowthState &state, const StableId &actor, const StableId &skill) {
  return field_read(c, x, state, actor, skill, c.tick(), {}, nullptr,
                    EntryBoundary::field_zero_time);
}
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_before_fact(
    const CommonStore &c, const GrowthFieldHistoryContext &x,
    const GrowthState &state, const StableId &actor, const StableId &skill,
    Integer observed, const FactRecord &actual, GrowthFieldBoundaryKind kind,
    const DynamicStepInput *source_phase,
    const DynamicStepInput *current_growth_phase,
    const WorldMoment *actual_source_moment,
    const CurrentMomentRead *actual_clock) {
  const auto valid_phase = [&](const DynamicStepInput *phase) {
    return !phase ||
           (c.tick() >= 0 && c.tick() < 9007199254740991LL &&
            phase->from_tick == c.tick() && phase->to_tick == c.tick() + 1);
  };
  const bool npc_start = kind == GrowthFieldBoundaryKind::npc_started;
  const bool npc_council = kind == GrowthFieldBoundaryKind::npc_council_adopted;
  const bool old_contact = kind == GrowthFieldBoundaryKind::npc_contact_session;
  const bool exact_contact = npc_start || npc_council;
  const bool contact = old_contact || exact_contact;
  const bool start =
      kind == GrowthFieldBoundaryKind::player_started || npc_start;
  const bool phase_entry =
      kind == GrowthFieldBoundaryKind::actor_entered_at_contact;
  const bool entry =
      kind == GrowthFieldBoundaryKind::actor_entered || phase_entry;
  const bool council =
      kind == GrowthFieldBoundaryKind::council_adopted || npc_council;
  const bool stratagem = kind == GrowthFieldBoundaryKind::stratagem_adopted;
  const bool commander = kind == GrowthFieldBoundaryKind::commander_changed;
  if ((!contact && !start && !entry && !council && !stratagem && !commander) ||
      c.state().facts.find(actual.id) != &actual ||
      actual.domain != "military" ||
      actual.kind != (old_contact ? "field_battle_session"
                      : start     ? "field_battle_started"
                      : entry     ? "field_battle_entered"
                      : council   ? "military_council"
                      : stratagem ? "field_battle_stratagem"
                                  : "field_commander_changed") ||
      (!contact && !phase_entry && source_phase) ||
      (old_contact && source_phase && observed != source_phase->to_tick) ||
      !valid_phase(source_phase) || !valid_phase(current_growth_phase) ||
      (observed > c.tick() && ((!contact && !phase_entry) || !source_phase ||
                               observed != source_phase->to_tick)))
    return {
        false,
        {},
        common_failure("GrowthFieldActualBoundaryRequired", actual.id).error};
  if (phase_entry || exact_contact) {
    if (!actual_source_moment || !actual_clock ||
        actual_source_moment->half_day_tick != observed)
      return {
          false,
          {},
          common_failure("GrowthFieldExactApplicationMomentRequired", actual.id)
              .error};
    auto now = actual_clock->read_current_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto not_future = compare_world_moments(*actual_source_moment, now.value);
    if (!not_future.ok)
      return {false, {}, std::move(not_future.error)};
    if (not_future.value > 0)
      return {
          false,
          {},
          common_failure("GrowthFieldExactApplicationFutureMoment", actual.id)
              .error};
    if (source_phase) {
      auto lower = compare_world_moments(
          *actual_source_moment, WorldMoment{source_phase->from_tick, {}});
      auto upper = compare_world_moments(
          *actual_source_moment, WorldMoment{source_phase->to_tick, {}});
      if (!lower.ok)
        return {false, {}, std::move(lower.error)};
      if (!upper.ok)
        return {false, {}, std::move(upper.error)};
      if (lower.value < 0 || upper.value > 0)
        return {false,
                {},
                common_failure("GrowthFieldExactApplicationPhaseMomentMismatch",
                               actual.id)
                    .error};
    }
  }
  // Field owner verifies original source body/actor/observed S0 tick first.
  // commander_changed uses the real deputy at that application header; local
  // battle time is not a WorldMoment and never replaces observed_tick.
  // This reader supplies four skill XP/levels only, not learned tactic cards.
  // military_council的Speak不是采用；只有S2已核Adopt原row可调用council边界。
  // 本域不读取/制造旧MilitaryState.battles，也不替Field决定参与资格。
  // 今日账可已含本轮actions_step的to净增；它只扩完整反验上界，不移动旧source
  // cut。
  const auto *ledger_phase =
      current_growth_phase ? current_growth_phase : source_phase;
  return field_read(c, x, state, actor, skill, observed, actual.id,
                    ledger_phase,
                    exact_contact ? EntryBoundary::field_exact_contact
                    : old_contact ? EntryBoundary::field_contact
                                  : EntryBoundary::field_zero_time);
}
Outcome<GrowthNavalSkillObservation>
read_growth_naval_skill(const CommonStore &c,
                        const GrowthNavalHistoryContext &x,
                        const GrowthState &s, const StableId &actor,
                        const DynamicStepInput *phase) {
  return read(c, x, s, actor, phase ? phase->to_tick : c.tick(), {}, phase,
              "naval", EntryBoundary::naval_preparation);
}
Outcome<GrowthNavalSkillObservation> read_growth_naval_skill_before_preparation(
    const CommonStore &c, const GrowthNavalHistoryContext &x,
    const GrowthState &s, const StableId &actor, Integer tick,
    const StableId &fact, const DynamicStepInput *phase) {
  if (fact.empty())
    return {
        false, {}, common_failure("GrowthNavalSkillBoundaryUnavailable").error};
  return read(c, x, s, actor, tick, fact, phase, "naval",
              EntryBoundary::naval_preparation);
}

namespace {
// 陆战沿同一课程结算链读取；对外因果集合另按Common真实事实序排列，不改变XP重放次序。
Outcome<GrowthMilitaryEntryQualification>
military_entry(const CommonStore &c, const GrowthMilitaryEntryContext &x,
               const GrowthState &state, const StableId &actor,
               const StableId &skill, Integer tick, const StableId &boundary,
               const DynamicStepInput *phase) {
  if (skill != "infantry" && skill != "archery" && skill != "cavalry")
    return {false,
            {},
            common_failure("GrowthMilitaryEntrySkillUnsupported", skill).error};
  Integer leadership = 0;
  auto exact = read(c, x, state, actor, tick, boundary, phase, skill,
                    EntryBoundary::land_entry, &leadership);
  if (!exact.ok)
    return {false, {}, exact.error};
  GrowthMilitaryEntryQualification out;
  out.source_identity = exact.value.source_identity;
  out.actor_id = actor;
  out.skill_id = skill;
  out.before_fact_id = boundary;
  out.authored_actor_id = exact.value.authored_actor_id;
  out.observed_tick = exact.value.observed_tick;
  out.experience = exact.value.experience;
  out.level = exact.value.level;
  out.leadership = leadership;
  out.causal_fact_ids = std::move(exact.value.causal_fact_ids);
  std::sort(out.causal_fact_ids.begin(), out.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  return {true, std::move(out), {}};
}
} // namespace
// 当前资格仍解释完整永久链；phase只供原contact_install同栈借用。
Outcome<GrowthMilitaryEntryQualification>
read_growth_military_entry_qualification(const CommonStore &c,
                                         const GrowthMilitaryEntryContext &x,
                                         const GrowthState &state,
                                         const StableId &actor,
                                         const StableId &skill,
                                         const DynamicStepInput *phase) {
  return military_entry(c, x, state, actor, skill,
                        phase ? phase->to_tick : c.tick(), {}, phase);
}
// 实际入战头已存在时才读取前界，不收任意tick或空边界冒历史证明。
Outcome<GrowthMilitaryEntryQualification>
read_growth_military_entry_qualification_before_fact(
    const CommonStore &c, const GrowthMilitaryEntryContext &x,
    const GrowthState &state, const StableId &actor, const StableId &skill,
    Integer tick, const StableId &boundary, const DynamicStepInput *phase) {
  if (boundary.empty())
    return {false,
            {},
            common_failure("GrowthMilitaryEntryBoundaryUnavailable").error};
  return military_entry(c, x, state, actor, skill, tick, boundary, phase);
}
// 兵法不带统率条件；仍使用同一技能初态和Study净增内核，并单独核真实入战边界。
namespace {
Outcome<GrowthTacticsSkillObservation>
tactics_entry(const CommonStore &c, const GrowthTacticsHistoryContext &x,
              const GrowthState &state, const StableId &actor, Integer tick,
              const StableId &boundary, const DynamicStepInput *phase) {
  auto out = read(c, x, state, actor, tick, boundary, phase, "tactics",
                  EntryBoundary::land_entry);
  if (!out.ok)
    return out;
  std::sort(out.value.causal_fact_ids.begin(), out.value.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  return out;
}
} // namespace
Outcome<GrowthTacticsSkillObservation>
read_growth_tactics_skill(const CommonStore &c,
                          const GrowthTacticsHistoryContext &x,
                          const GrowthState &s, const StableId &actor,
                          const DynamicStepInput *phase) {
  return tactics_entry(c, x, s, actor, phase ? phase->to_tick : c.tick(), {},
                       phase);
}
Outcome<GrowthTacticsSkillObservation> read_growth_tactics_skill_before_entry(
    const CommonStore &c, const GrowthTacticsHistoryContext &x,
    const GrowthState &s, const StableId &actor, Integer tick,
    const StableId &fact, const DynamicStepInput *phase) {
  if (fact.empty())
    return {
        false, {}, common_failure("GrowthTacticsBoundaryUnavailable").error};
  return tactics_entry(c, x, s, actor, tick, fact, phase);
}
// 筑城具名只读严格复用原完整XP链；仅新围城阶段排除本轮所有课程结算，不复制成长账。
namespace {
Outcome<GrowthSiegeEngineeringSkillObservation>
siege_engineering(const CommonStore &c,
                  const GrowthSiegeEngineeringHistoryContext &x,
                  const GrowthState &s, const StableId &actor, Integer tick,
                  const StableId &fact, const DynamicStepInput *phase) {
  if (!x.common.extended_skills)
    return {
        false,
        {},
        common_failure("GrowthSiegeEngineeringExtendedSkillsRequired", actor)
            .error};
  auto out = read(c, x, s, actor, tick, fact, phase, "fortification",
                  EntryBoundary::siege_engineering);
  if (!out.ok)
    return out;
  std::sort(out.value.causal_fact_ids.begin(), out.value.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  return out;
}
} // namespace
Outcome<GrowthSiegeEngineeringSkillObservation>
read_growth_siege_engineering_skill(
    const CommonStore &c, const GrowthSiegeEngineeringHistoryContext &x,
    const GrowthState &s, const StableId &actor,
    const DynamicStepInput *phase) {
  return siege_engineering(c, x, s, actor, phase ? phase->to_tick : c.tick(),
                           {}, phase);
}
Outcome<GrowthSiegeEngineeringSkillObservation>
read_growth_siege_engineering_skill_before_preparation(
    const CommonStore &c, const GrowthSiegeEngineeringHistoryContext &x,
    const GrowthState &s, const StableId &actor, Integer tick,
    const StableId &fact, const DynamicStepInput *phase) {
  if (fact.empty())
    return {false,
            {},
            common_failure("GrowthSiegeEngineeringBoundaryUnavailable").error};
  return siege_engineering(c, x, s, actor, tick, fact, phase);
}
namespace {
// 两原技艺沿同核独立重放，合并只输出真实来源并集；武力仍取原创建而非装备或当前有效攻击值。
Outcome<GrowthPersonalEntryQualification>
personal_entry(const CommonStore &c, const GrowthPersonalEntryContext &x,
               const GrowthState &s, const StableId &actor, Integer tick,
               const StableId &fact) {
  Integer martial = 0;
  auto fighting = read(c, x, s, actor, tick, fact, nullptr, "martial",
                       EntryBoundary::personal_entry, nullptr, &martial);
  if (!fighting.ok)
    return {false, {}, fighting.error};
  auto archery = read(c, x, s, actor, tick, fact, nullptr, "archery",
                      EntryBoundary::personal_entry);
  if (!archery.ok)
    return {false, {}, archery.error};
  if (fighting.value.authored_actor_id != archery.value.authored_actor_id)
    return {false,
            {},
            common_failure("GrowthPersonalEntryOriginMismatch", actor).error};
  GrowthPersonalEntryQualification out;
  out.source_identity = c.state().identity;
  out.actor_id = actor;
  out.authored_actor_id = fighting.value.authored_actor_id;
  out.before_fact_id = fact;
  out.observed_tick = tick;
  out.base_martial = martial;
  out.martial_experience = fighting.value.experience;
  out.martial_level = fighting.value.level;
  out.archery_experience = archery.value.experience;
  out.archery_level = archery.value.level;
  std::set<StableId> sources(fighting.value.causal_fact_ids.begin(),
                             fighting.value.causal_fact_ids.end());
  sources.insert(archery.value.causal_fact_ids.begin(),
                 archery.value.causal_fact_ids.end());
  out.causal_fact_ids.assign(sources.begin(), sources.end());
  std::sort(out.causal_fact_ids.begin(), out.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  if (!fact.empty())
    for (const auto &source : out.causal_fact_ids) {
      const auto colon = source.rfind(':');
      Integer n = 0, b = 0;
      const auto digits =
          colon == StableId::npos ? StableId{} : source.substr(colon + 1);
      const auto before = fact.substr(fact.rfind(':') + 1);
      auto a = std::from_chars(digits.data(), digits.data() + digits.size(), n);
      auto z = std::from_chars(before.data(), before.data() + before.size(), b);
      if (a.ec != std::errc{} || a.ptr != digits.data() + digits.size() ||
          z.ec != std::errc{} || z.ptr != before.data() + before.size() ||
          n <= 0 || n >= b || position(c, source) >= position(c, fact))
        return {false,
                {},
                common_failure("GrowthPersonalEntrySourceOrderInvalid", source)
                    .error};
    }
  return {true, std::move(out), {}};
}
} // namespace
Outcome<GrowthPersonalEntryQualification>
read_growth_personal_entry_qualification(const CommonStore &c,
                                         const GrowthPersonalEntryContext &x,
                                         const GrowthState &s,
                                         const StableId &actor) {
  return personal_entry(c, x, s, actor, c.tick(), {});
}
Outcome<GrowthPersonalEntryQualification>
read_growth_personal_entry_qualification_before_fact(
    const CommonStore &c, const GrowthPersonalEntryContext &x,
    const GrowthState &s, const StableId &actor, Integer tick,
    const StableId &fact) {
  if (fact.empty())
    return {
        false,
        {},
        common_failure("GrowthPersonalEntryBoundaryUnavailable", actor).error};
  return personal_entry(c, x, s, actor, tick, fact);
}

namespace {
// 原Training唯一写掌握度，零时只改装备/心法选择；按真正完工tick重放而不借行动分配号充当完工序。
struct MasteryActor {
  Origin original;
  OrderedTable<Integer> learned;
  std::map<StableId, std::set<StableId>> causes;
};
struct MasteryEvent {
  Integer tick = 0;
  bool start = false;
  StableId id;
  const TrainingFact *fact = nullptr;
};
Integer allocation_number(const StableId &id) {
  const auto colon = id.rfind(':');
  if (colon == StableId::npos)
    return 0;
  const auto digits = id.substr(colon + 1);
  Integer n = 0;
  auto p = std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return p.ec == std::errc{} && p.ptr == digits.data() + digits.size() &&
                 n > 0 && std::to_string(n) == digits
             ? n
             : 0;
}
bool mastery_equal(const OrderedTable<Integer> &a,
                   const OrderedTable<Integer> &b) {
  if (a.size() != b.size())
    return false;
  for (const auto &row : a.rows()) {
    const auto *other = b.find(row.first);
    if (!other || *other != row.second)
      return false;
  }
  return true;
}
Outcome<GrowthPersonalMasteryObservation>
personal_mastery(const CommonStore &c, const GrowthPersonalMasteryContext &x,
                 const GrowthState &state, const StableId &actor, Integer tick,
                 const StableId &boundary) {
  auto bad = [&](const StableId &code,
                 const StableId &source =
                     StableId{}) -> Outcome<GrowthPersonalMasteryObservation> {
    return {
        false, {}, common_failure(code, source.empty() ? actor : source).error};
  };
  const bool historical = !boundary.empty();
  const auto limit =
      historical ? position(c, boundary) : c.state().facts.size();
  if (tick < 0 || tick > c.tick() || x.initial.scenario_id.empty() ||
      x.initial.scenario_id != c.state().metadata.scenario_id ||
      !x.growth.cultivation || !x.personal.enabled)
    return bad("GrowthMasteryContextInvalid");
  if (historical &&
      (!allocated(c, boundary, "personal_entry") ||
       !header(c, boundary, "personal_combat_entered", "personal")))
    return bad("GrowthMasteryBoundaryUnavailable");
  const std::set<StableId> expected(x.origins.common.actor_ids.begin(),
                                    x.origins.common.actor_ids.end());
  if (expected.size() != x.origins.common.actor_ids.size() ||
      expected.size() != x.initial.actors.size())
    return bad("GrowthMasteryInitialCatalogMismatch");
  for (const auto &row : x.initial.actors.rows()) {
    if (!expected.count(row.first))
      return bad("GrowthMasteryInitialCatalogMismatch", row.first);
    for (const auto &skill : row.second.rows()) {
      const auto *p = x.personal.skills.find(skill.first);
      if (!p || (p->kind != "technique" && p->kind != "heart") ||
          skill.second < 0 || skill.second > 100)
        return bad("GrowthMasteryInitialCatalogMismatch", row.first);
    }
  }
  auto same_side = [&](const StableId &source, const StableId &before) {
    return allocation_number(source) > 0 &&
           allocation_number(source) < allocation_number(before) &&
           position(c, source) < position(c, before);
  };
  std::map<StableId, MasteryActor> people;
  auto prepare = [&](const StableId &id) -> Status {
    if (people.count(id))
      return common_success();
    auto initial = origin(c, x.origins, id, c.tick(), c.state().facts.size(),
                          "martial", true);
    if (!initial.ok)
      return {false, {}, initial.error};
    MasteryActor p;
    p.original = std::move(initial.value);
    if (!p.original.authored.empty()) {
      const auto *learned = x.initial.actors.find(id);
      if (!learned)
        return common_failure("GrowthMasteryInitialCatalogMismatch", id);
      p.learned = *learned;
    }
    // 原CharacterCreation/Adoption/Birth均默认构造ActorCombat；新writer若赋初值必须新增真实初态来源。
    for (const auto &source : p.original.sources)
      if (allocation_number(source) <= 0)
        return common_failure("GrowthMasteryOriginOrderInvalid", source);
    people.emplace(id, std::move(p));
    return common_success();
  };
  auto prepared = prepare(actor);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  const auto &initial = people.at(actor).original;
  if (initial.tick > tick)
    return bad("GrowthMasteryActorNotCreated");
  for (const auto &source : initial.sources)
    if (position(c, source) >= limit ||
        (historical && !same_side(source, boundary)))
      return bad("GrowthMasteryOriginOrderInvalid", source);
  std::vector<MasteryEvent> events;
  for (const auto &row : c.state().facts.rows())
    if (row.second.kind == "training") {
      const auto *fact = state.facts.find(row.first);
      if (!fact || !std::get_if<TrainingFact>(fact))
        return bad("GrowthMasteryTrainingMissing", row.first);
    }
  for (const auto &row : state.facts.rows()) {
    const auto *f = std::get_if<TrainingFact>(&row.second);
    if (!f)
      continue;
    const auto &t = f->training;
    const auto *skill = x.personal.skills.find(t.skill_id);
    if (!allocated(c, row.first, "action") ||
        !header(c, row.first, "training", "cultivation") || !skill ||
        (skill->kind != "technique" && skill->kind != "heart") ||
        f->start_tick < 0 || f->start_tick > c.tick() || t.duration < 1 ||
        t.duration > 30 || f->start_tick > 9007199254740991LL - t.duration ||
        t.mastery_before < -1 || t.mastery_before > 100 ||
        t.mastery_after < 0 || t.mastery_after > 100)
      return bad("GrowthMasteryTrainingInvalid", row.first);
    if (f->action_kind != "learn_skill" && f->action_kind != "teach_skill" &&
        f->action_kind != "practice_skill")
      return bad("GrowthMasteryTrainingInvalid", row.first);
    if (f->actor_id !=
            (f->action_kind == "teach_skill" ? t.teacher_id : t.student_id) ||
        t.student_id.empty())
      return bad("GrowthMasteryTrainingInvalid", row.first);
    for (const auto &id : {t.student_id, t.teacher_id})
      if (!id.empty()) {
        prepared = prepare(id);
        if (!prepared.ok)
          return {false, {}, prepared.error};
        const auto &p = people.at(id);
        if (p.original.tick > f->start_tick)
          return bad("GrowthMasteryTrainingBeforeCreation", row.first);
        for (const auto &source : p.original.sources)
          if (!same_side(source, row.first))
            return bad("GrowthMasteryOriginOrderInvalid", row.first);
      }
    events.push_back({f->start_tick, true, row.first, f});
    if (f->phase == "completed") {
      if (f->completed_tick != f->start_tick + t.duration ||
          f->completed_tick > c.tick() || c.action(row.first) != nullptr)
        return bad("GrowthMasteryTrainingTimeInvalid", row.first);
      events.push_back({f->completed_tick, false, row.first, f});
    } else if (f->phase != "started" || f->completed_tick != -1 || f->success ||
               !c.action(row.first) || !state.actions.find(row.first))
      return bad("GrowthMasteryTrainingTimeInvalid", row.first);
  }
  std::sort(events.begin(), events.end(),
            [&](const MasteryEvent &a, const MasteryEvent &b) {
              return std::tuple{a.tick, a.start, position(c, a.id)} <
                     std::tuple{b.tick, b.start, position(c, b.id)};
            });
  GrowthPersonalMasteryObservation out;
  out.source_identity = c.state().identity;
  out.actor_id = actor;
  out.authored_actor_id = initial.authored;
  out.before_fact_id = boundary;
  out.observed_tick = tick;
  out.learned = people.at(actor).learned;
  std::set<StableId> selected(initial.sources.begin(), initial.sources.end());
  std::map<StableId, StableId> occupied;
  std::map<StableId, std::set<StableId>> start_causes;
  std::set<StableId> ambiguous_practice;
  auto mastery = [](const MasteryActor &p, const StableId &skill) {
    const auto *value = p.learned.find(skill);
    return value ? *value : Integer{-1};
  };
  for (const auto &e : events) {
    const auto &f = *e.fact;
    const auto &t = f.training;
    auto &student = people.at(t.student_id);
    if (e.start) {
      if (mastery(student, t.skill_id) != t.mastery_before)
        return bad("GrowthMasteryTrainingChainMismatch", e.id);
      for (const auto &id : {t.student_id, t.teacher_id})
        if (!id.empty()) {
          if (occupied.count(id))
            return bad("GrowthMasteryTrainingOverlap", e.id);
          occupied.emplace(id, e.id);
        }
      auto &causes = start_causes[e.id];
      causes.insert(student.original.sources.begin(),
                    student.original.sources.end());
      const auto own = student.causes.find(t.skill_id);
      if (own != student.causes.end())
        causes.insert(own->second.begin(), own->second.end());
      if (f.action_kind == "practice_skill") {
        const auto practice =
            x.personal.skills.find(t.skill_id)->practice_ticks;
        if (!t.teacher_id.empty() || f.context || t.duration > 8 ||
            t.mastery_before < 0 || t.mastery_before >= 100 || practice < 1 ||
            practice > 9007199254740991LL - 100 ||
            t.student_fatigue != t.duration * 6 || t.teacher_fatigue != 0)
          return bad("GrowthMasteryPracticeInvalid", e.id);
        const auto gain = ((100 + practice - 1) / practice) * t.duration;
        const auto rested = std::min<Integer>(100, t.mastery_before + gain),
                   tired = std::min<Integer>(
                       100, t.mastery_before + std::max<Integer>(1, gain / 2));
        if (t.mastery_after != rested && t.mastery_after != tired)
          return bad("GrowthMasteryPracticeInvalid", e.id);
        // 旧Training未存开始时疲劳来源；不能让自存after在两个原分支中任选而冒精确成长。
        // 两分支数值相同（最低1点或封顶）才无需新的疲劳历史口；其他练习明确保留缺口。
        if (f.success && rested != tired)
          ambiguous_practice.insert(e.id);
      } else {
        if (t.teacher_id.empty() || t.teacher_id == t.student_id ||
            !f.context || f.context->kind ||
            !x.origins.common.roles.find(f.context->role_id) ||
            f.context->place_id != t.place_id ||
            f.context->provider_actor_id != (f.action_kind == "learn_skill"
                                                 ? t.teacher_id
                                                 : t.student_id) ||
            t.student_fatigue != t.duration * 4 ||
            t.teacher_fatigue != t.duration * 2)
          return bad("GrowthMasteryTeacherInvalid", e.id);
        const auto &teacher = people.at(t.teacher_id);
        const auto known = mastery(teacher, t.skill_id);
        if (known < 80)
          return bad("GrowthMasteryTeacherHistoryIncomplete", e.id);
        causes.insert(teacher.original.sources.begin(),
                      teacher.original.sources.end());
        const auto taught = teacher.causes.find(t.skill_id);
        if (taught != teacher.causes.end())
          causes.insert(taught->second.begin(), taught->second.end());
        if (f.action_kind == "learn_skill") {
          const auto *role = x.growth.roles.find(f.context->role_id);
          bool offered = false;
          if (role)
            for (const auto &lesson : role->lessons)
              if (lesson.skill_id == t.skill_id &&
                  lesson.study_ticks == t.duration && lesson.fee == t.fee &&
                  lesson.wallet_id == t.recipient_wallet_id)
                offered = true;
          if (t.mastery_before != -1 || t.mastery_after != 0 || !offered ||
              t.payer_wallet_id.empty() || t.recipient_wallet_id.empty() ||
              t.payer_wallet_id == t.recipient_wallet_id)
            return bad("GrowthMasteryLessonInvalid", e.id);
        } else if (t.duration != 2 || t.mastery_before >= known ||
                   t.mastery_after !=
                       std::min<Integer>(
                           known, std::max<Integer>(0, t.mastery_before) + 10))
          return bad("GrowthMasteryTeachingInvalid", e.id);
      }
      if (f.action_kind != "learn_skill" &&
          (t.fee != 0 || !t.payer_wallet_id.empty() ||
           !t.recipient_wallet_id.empty()))
        return bad("GrowthMasteryUnexpectedPayment", e.id);
      for (const auto &source : causes)
        if (!same_side(source, e.id))
          return bad("GrowthMasteryTrainingSourceOrderInvalid", e.id);
      causes.insert(e.id);
    } else {
      for (const auto &id : {t.student_id, t.teacher_id})
        if (!id.empty()) {
          const auto found = occupied.find(id);
          if (found == occupied.end() || found->second != e.id)
            return bad("GrowthMasteryTrainingOverlap", e.id);
          occupied.erase(found);
        }
      if (mastery(student, t.skill_id) != t.mastery_before)
        return bad("GrowthMasteryTrainingChainMismatch", e.id);
      if (f.success) {
        student.learned.set(t.skill_id, t.mastery_after);
        student.causes[t.skill_id] = start_causes.at(e.id);
        if (t.student_id == actor && e.tick <= tick) {
          out.learned.set(t.skill_id, t.mastery_after);
          selected.insert(start_causes.at(e.id).begin(),
                          start_causes.at(e.id).end());
        }
      }
    }
  }
  for (const auto &row : people) {
    const auto *person = c.state().actors.find(row.first);
    if (!person || !mastery_equal(person->combat.learned, row.second.learned))
      return bad("GrowthMasteryHistoryIncomplete", row.first);
  }
  for (const auto &source : selected) {
    if (position(c, source) >= limit ||
        (historical && !same_side(source, boundary)))
      return bad("GrowthMasterySourceOrderInvalid", source);
    if (ambiguous_practice.count(source))
      return bad("GrowthMasteryPracticeFatigueSourceRequired", source);
  }
  out.causal_fact_ids.assign(selected.begin(), selected.end());
  std::sort(out.causal_fact_ids.begin(), out.causal_fact_ids.end(),
            [&](const StableId &a, const StableId &b) {
              return position(c, a) < position(c, b);
            });
  return {true, std::move(out), {}};
}
} // namespace
Outcome<GrowthPersonalMasteryObservation>
read_growth_personal_mastery(const CommonStore &c,
                             const GrowthPersonalMasteryContext &x,
                             const GrowthState &s, const StableId &actor) {
  return personal_mastery(c, x, s, actor, c.tick(), {});
}
Outcome<GrowthPersonalMasteryObservation>
read_growth_personal_mastery_before_entry(const CommonStore &c,
                                          const GrowthPersonalMasteryContext &x,
                                          const GrowthState &s,
                                          const StableId &actor, Integer tick,
                                          const StableId &entry) {
  if (entry.empty())
    return {false,
            {},
            common_failure("GrowthMasteryBoundaryUnavailable", actor).error};
  return personal_mastery(c, x, s, actor, tick, entry);
}
} // namespace hanmo::resident
