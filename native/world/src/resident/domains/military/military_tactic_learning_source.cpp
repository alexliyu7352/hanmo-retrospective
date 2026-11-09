// 原事实顺序是唯一资格前界；教师/学员交叉学习不得依赖未来完成或当前列表循环自证。
#include "military_tactic_learning_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const StableId &code, const StableId &path = {}) {
  return {false, {}, common_failure(code, path).error};
}
template <class T>
const T *find(const std::vector<T> &rows, const StableId &id) {
  for (const auto &row : rows)
    if (row.id == id)
      return &row;
  return nullptr;
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
Outcome<Integer> upper(const CommonStore &c, const DynamicStepInput *p) {
  if (!p)
    return {true, c.tick(), {}};
  if (p->from_tick < 0 || p->from_tick != c.tick() ||
      p->from_tick == std::numeric_limits<Integer>::max() ||
      p->to_tick != p->from_tick + 1)
    return fail<Integer>("MilitaryTacticPhaseMismatch");
  return {true, p->to_tick, {}};
}
std::size_t position(const CommonStore &c, const StableId &id) {
  const auto &order = c.state().facts.order();
  return std::find(order.begin(), order.end(), id) - order.begin();
}
bool head(const CommonStore &c, const StableId &id, const StableId &prefix,
          const StableId &kind, const StableId &domain) {
  if (id.rfind(prefix, 0) != 0)
    return false;
  Integer n = 0;
  auto first = id.data() + prefix.size(), last = id.data() + id.size();
  auto parsed = std::from_chars(first, last, n);
  const auto *h = c.state().facts.find(id);
  return parsed.ec == std::errc{} && parsed.ptr == last && n > 0 &&
         n < c.state().next_instance_id && id == prefix + std::to_string(n) &&
         h && h->id == id && h->kind == kind && h->domain == domain &&
         position(c, id) < c.state().facts.size();
}
bool boundary(const CommonStore &c, const StableId &id) {
  return head(c, id, "tactic_lesson:", "tactic_lesson_started",
              "growth_tactic_lessons") ||
         head(c, id, "military_rout_entry:", "military_rout_entry",
              "military") ||
         head(c, id, "military_tactic:", "military_tactic_learned",
              "military_tactics") ||
         head(c, id, "field_battle:", "field_battle_session", "military") ||
         head(c, id, "field_contact:", "field_battle_contact", "military") ||
         head(c, id, "field_start:", "field_battle_started", "military") ||
         head(c, id, "field_entry:", "field_battle_entered", "military") ||
         head(c, id, "field_commander_changed:", "field_commander_changed",
              "military");
}
Integer level(Integer xp) {
  return xp >= 1200 ? 4 : xp >= 650 ? 3 : xp >= 300 ? 2 : xp >= 100 ? 1 : 0;
}
Status causals(const CommonStore &c, const std::vector<StableId> &ids,
               const StableId &before) {
  const auto end = position(c, before);
  std::size_t previous = 0;
  bool first = true;
  for (const auto &id : ids) {
    const auto at = position(c, id);
    if (!c.state().facts.find(id) || at >= end || (!first && at <= previous))
      return common_failure("MilitaryTacticQualificationCauseMismatch", id);
    first = false;
    previous = at;
  }
  return common_success();
}
} // namespace
Status validate_military_tactic_learning_structure(
    const CommonStore &c, const CommonCatalog &common,
    const MilitaryTacticLearningCatalog &cat,
    const MilitaryTacticLearningState &state, const DynamicStepInput *phase) {
  auto checked = validate_military_tactic_learning_catalog(cat);
  if (!checked.ok)
    return checked;
  checked = validate_military_tactic_learning_shape(state);
  if (!checked.ok)
    return checked;
  auto observed = upper(c, phase);
  if (!observed.ok)
    return {false, {}, observed.error};
  if (!cat.enabled)
    return state.acquisitions.empty() && state.practices.empty() &&
                   state.field_order.empty()
               ? common_success()
               : common_failure("UnexpectedMilitaryTacticLearning");
  std::set<StableId> seen;
  std::size_t previous = 0;
  bool first = true;
  for (const auto &f : state.acquisitions) {
    if (!head(c, f.id, "military_tactic:", "military_tactic_learned",
              "military_tactics") ||
        f.tick > observed.value || !c.actor(f.actor_id) ||
        !find(cat.definitions, f.tactic_id))
      return common_failure("MilitaryTacticAcquisitionSourceMismatch", f.id);
    const auto at = position(c, f.id);
    if (!first && at <= previous)
      return common_failure("MilitaryTacticHistoryOrderMismatch", f.id);
    first = false;
    previous = at;
    seen.insert(f.id);
    if (f.source_kind == MilitaryTacticLearningSourceKind::authored_initial) {
      const auto *grant = find(cat.initial_grants, f.source_id);
      if (!grant || grant->actor_id != f.actor_id ||
          grant->tactic_id != f.tactic_id ||
          std::find(common.actor_ids.begin(), common.actor_ids.end(),
                    f.actor_id) == common.actor_ids.end() ||
          f.tick != 0)
        return common_failure("MilitaryTacticInitialGrantMismatch", f.id);
    } else {
      const bool teaching =
          f.source_kind == MilitaryTacticLearningSourceKind::teaching;
      const auto prefix =
          teaching ? "tactic_lesson:" : "military_tactic_practice:";
      const auto kind = teaching ? "tactic_lesson_closed"
                                 : "military_tactic_practice_completed";
      const auto domain =
          teaching ? "growth_tactic_lessons" : "military_tactics";
      if (!head(c, f.source_id, prefix, kind, domain) ||
          position(c, f.source_id) >= at)
        return common_failure("MilitaryTacticCompletionSourceMismatch", f.id);
    }
  }
  std::set<StableId> practices;
  previous = 0;
  first = true;
  for (const auto &f : state.practices) {
    const auto at = position(c, f.id);
    if (!head(c, f.id, "military_tactic_practice:",
              "military_tactic_practice_completed", "military_tactics") ||
        !head(c, f.training_action_id, "action:", "military_training",
              "military") ||
        position(c, f.training_action_id) >= at || f.tick > observed.value ||
        !c.actor(f.actor_id) || !find(cat.definitions, f.tactic_id) ||
        (!first && at <= previous))
      return common_failure("MilitaryTacticPracticeSourceMismatch", f.id);
    practices.insert(f.id);
    first = false;
    previous = at;
  }
  for (const auto &row : c.state().facts.rows())
    if (row.second.domain == "military_tactics" &&
        row.second.kind == "military_tactic_practice_completed" &&
        !practices.count(row.first))
      return common_failure("MilitaryTacticPracticePayloadMissing", row.first);
  for (const auto &row : c.state().facts.rows())
    if (row.second.domain == "military_tactics" &&
        row.second.kind == "military_tactic_learned" && !seen.count(row.first))
      return common_failure("MilitaryTacticAcquisitionPayloadMissing",
                            row.first);
  for (const auto &g : cat.initial_grants) {
    std::size_t count = 0;
    for (const auto &f : state.acquisitions)
      if (f.source_kind == MilitaryTacticLearningSourceKind::authored_initial &&
          f.source_id == g.id)
        ++count;
    if (count != 1)
      return common_failure("MilitaryTacticInitialGrantMissing", g.id);
  }
  return common_success();
}
Outcome<MilitaryTacticTeachingTarget>
read_military_tactic_teaching_target(const CommonStore &c,
                                     const MilitaryTacticLearningCatalog &cat,
                                     const StableId &id) {
  auto checked = validate_military_tactic_learning_catalog(cat);
  if (!checked.ok)
    return {false, {}, checked.error};
  const auto *d = find(cat.definitions, id);
  if (!cat.enabled || !d)
    return fail<MilitaryTacticTeachingTarget>("UnknownMilitaryLearnedTactic",
                                              id);
  auto r = military_learned_tactic_rule(d->kind);
  if (!r.ok)
    return {false, {}, r.error};
  return {true,
          {c.state().identity, id, military_learned_tactic_kind_name(d->kind),
           r.value.prerequisite_skill_id, r.value.prerequisite_level,
           d->teacher_actor_ids},
          {}};
}
Outcome<std::optional<MilitaryTacticLearningObservation>>
read_military_learned_tactic(const CommonStore &c, const CommonCatalog &common,
                             const MilitaryTacticLearningCatalog &cat,
                             const MilitaryTacticLearningState &state,
                             const StableId &actor, const StableId &tactic,
                             Integer tick, const StableId &before,
                             const DynamicStepInput *phase) {
  using R = std::optional<MilitaryTacticLearningObservation>;
  auto checked =
      validate_military_tactic_learning_structure(c, common, cat, state, phase);
  if (!checked.ok)
    return {false, {}, checked.error};
  auto limit = upper(c, phase);
  if (!limit.ok)
    return {false, {}, limit.error};
  if (!cat.enabled || !c.actor(actor) || !find(cat.definitions, tactic) ||
      tick < 0 || tick > limit.value ||
      (before.empty() ? tick != limit.value : !boundary(c, before)))
    return fail<R>("MilitaryTacticReadBoundaryMismatch", before);
  const auto end =
      before.empty() ? c.state().facts.size() : position(c, before);
  for (const auto &f : state.acquisitions)
    if (f.actor_id == actor && f.tactic_id == tactic) {
      if (position(c, f.id) >= end || f.tick > tick)
        return {true, std::nullopt, {}};
      MilitaryTacticLearningObservation out;


      (out.copied_payload).grow(0);
      out.source_identity = c.state().identity;
      out.actor_id = actor;
      out.tactic_id = tactic;
      out.acquisition_fact_id = f.id;
      out.source_fact_id = f.source_id;
      out.source_kind = f.source_kind;
      if (f.source_kind == MilitaryTacticLearningSourceKind::authored_initial)
        out.initial_grant_id = f.source_id;
      out.before_fact_id = before;
      out.acquired_tick = f.tick;
      out.observed_tick = tick;
      return {true, std::move(out), {}};
    }
  return {true, std::nullopt, {}};
}
Outcome<MilitaryTacticLearningObservation>
read_military_tactic_teaching_acquisition(
    const CommonStore &c, const CommonCatalog &common,
    const MilitaryTacticLearningCatalog &cat,
    const MilitaryTacticLearningState &state, const StableId &closed) {
  auto checked =
      validate_military_tactic_learning_structure(c, common, cat, state);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (!head(c, closed, "tactic_lesson:", "tactic_lesson_closed",
            "growth_tactic_lessons"))
    return fail<MilitaryTacticLearningObservation>(
        "MilitaryTacticCompletionSourceMismatch", closed);
  const MilitaryTacticAcquisitionFact *selected = nullptr;
  for (const auto &f : state.acquisitions)
    if (f.source_kind == MilitaryTacticLearningSourceKind::teaching &&
        f.source_id == closed) {
      if (selected)
        return fail<MilitaryTacticLearningObservation>(
            "MilitaryTacticTeachingAcquisitionRepeated", closed);
      selected = &f;
    }
  if (!selected || position(c, closed) >= position(c, selected->id))
    return fail<MilitaryTacticLearningObservation>(
        "MilitaryTacticTeachingAcquisitionMissing", closed);
  MilitaryTacticLearningObservation out;


  (out.copied_payload).grow(0);
  out.source_identity = c.state().identity;
  out.actor_id = selected->actor_id;
  out.tactic_id = selected->tactic_id;
  out.acquisition_fact_id = selected->id;
  out.source_fact_id = closed;
  out.source_kind = selected->source_kind;
  out.acquired_tick = selected->tick;
  out.observed_tick = c.tick();
  return {true, std::move(out), {}};
}

namespace {
Status completion(const CommonStore &c, const CommonCatalog &common,
                  const MilitaryTacticLearningCatalog &cat,
                  const MilitaryTacticLearningState &state,
                  const MilitaryTacticAcquisitionFact &f,
                  const MilitaryTacticLearningSourceRead &read,
                  const DynamicStepInput *phase) {
  auto checked = common_success();
  const auto *def = find(cat.definitions, f.tactic_id);
  auto rule = military_learned_tactic_rule(def->kind);
  if (!rule.ok)
    return {false, {}, rule.error};
  if (f.source_kind == MilitaryTacticLearningSourceKind::teaching) {
    auto source = read.completed_tactic_teaching(f.source_id);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &p = source.value;
    if (!identity(p.source_identity, c.state().identity) ||
        p.source_fact_id != f.source_id || p.student_id != f.actor_id ||
        p.tactic_id != f.tactic_id || p.action_id.empty() ||
        p.teacher_id.empty() || p.teacher_id == p.student_id ||
        p.place_id.empty() || p.agreement_id.empty() ||
        p.teacher_say_source.empty() || p.started_tick < 0 ||
        p.completed_tick != f.tick || p.completed_tick <= p.started_tick ||
        p.qualification_observed_tick != p.started_tick ||
        p.prerequisite_skill_id != rule.value.prerequisite_skill_id ||
        p.student_minimum_experience < 0 ||
        p.student_minimum_experience > 1200 ||
        p.student_minimum_level != level(p.student_minimum_experience) ||
        p.student_minimum_level < rule.value.prerequisite_level ||
        std::find(def->teacher_actor_ids.begin(), def->teacher_actor_ids.end(),
                  p.teacher_id) == def->teacher_actor_ids.end() ||
        !head(c, p.started_fact_id, "tactic_lesson:", "tactic_lesson_started",
              "growth_tactic_lessons") ||
        position(c, p.started_fact_id) >= position(c, p.source_fact_id))
      return common_failure("MilitaryTacticTeachingSourceMismatch", f.id);
    checked = causals(c, p.qualification_causal_fact_ids, p.started_fact_id);
    if (!checked.ok)
      return checked;
    auto teacher = read_military_learned_tactic(
        c, common, cat, state, p.teacher_id, f.tactic_id, p.started_tick,
        p.started_fact_id, phase);
    if (!teacher.ok)
      return {false, {}, teacher.error};
    if (!teacher.value ||
        teacher.value->acquisition_fact_id != p.teacher_acquisition_fact_id ||
        teacher.value->initial_grant_id != p.teacher_initial_grant_id)
      return common_failure("MilitaryTacticTeacherKnowledgeMismatch", f.id);
  } else {
    auto source = read.completed_tactic_practice(f.source_id);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &p = source.value;
    if (!identity(p.source_identity, c.state().identity) ||
        p.source_fact_id != f.source_id || p.actor_id != f.actor_id ||
        p.tactic_id != f.tactic_id || p.action_id.empty() ||
        p.started_tick < 0 || p.completed_tick != f.tick ||
        p.completed_tick < p.started_tick ||
        p.qualification_observed_tick != p.started_tick ||
        p.prerequisite_skill_id != rule.value.prerequisite_skill_id ||
        p.actor_minimum_experience < 0 || p.actor_minimum_experience > 1200 ||
        p.actor_minimum_level != level(p.actor_minimum_experience) ||
        p.actor_minimum_level < rule.value.prerequisite_level ||
        p.score < def->practice_min_score || p.score > 100 ||
        std::find(def->practice_challenge_ids.begin(),
                  def->practice_challenge_ids.end(),
                  p.challenge_id) == def->practice_challenge_ids.end())
      return common_failure("MilitaryTacticPracticeSourceMismatch", f.id);
    checked = causals(c, p.qualification_causal_fact_ids, p.source_fact_id);
    if (!checked.ok)
      return checked;
  }
  return common_success();
}
} // namespace
Status validate_military_tactic_learning_sources(
    const CommonStore &c, const CommonCatalog &common,
    const MilitaryTacticLearningCatalog &cat,
    const MilitaryTacticLearningState &state,
    const MilitaryTacticLearningSourceRead &read,
    const DynamicStepInput *phase) {
  auto checked =
      validate_military_tactic_learning_structure(c, common, cat, state, phase);
  if (!checked.ok)
    return checked;
  for (const auto &f : state.acquisitions) {
    if (f.source_kind == MilitaryTacticLearningSourceKind::authored_initial)
      continue;
    checked = completion(c, common, cat, state, f, read, phase);
    if (!checked.ok)
      return checked;
  }
  for (const auto &p : state.practices) {
    std::size_t count = 0;
    for (const auto &f : state.acquisitions)
      if (f.source_kind == MilitaryTacticLearningSourceKind::practice &&
          f.source_id == p.id && f.actor_id == p.actor_id &&
          f.tactic_id == p.tactic_id)
        ++count;
    if (count != 1)
      return common_failure("MilitaryTacticPracticeAcquisitionMissing", p.id);
  }
  return common_success();
}
// 只供外层完整World候选：任何失败必须丢弃该候选，不在NPC已有savepoint内另开嵌套事务。
Status initialize_military_tactic_learning_in_candidate(
    CommonStore &c, const CommonCatalog &common,
    const MilitaryTacticLearningCatalog &cat,
    MilitaryTacticLearningState &state) {
  auto checked = validate_military_tactic_learning_catalog(cat);
  if (!checked.ok)
    return checked;
  if (!cat.enabled)
    return state.acquisitions.empty() && state.practices.empty() &&
                   state.field_order.empty()
               ? common_success()
               : common_failure("UnexpectedMilitaryTacticLearning");
  if (c.tick() != 0)
    return common_failure("MilitaryTacticInitialBoundaryMismatch");
  if (!state.acquisitions.empty())
    return validate_military_tactic_learning_structure(c, common, cat, state);
  for (const auto &g : cat.initial_grants) {
    if (!c.actor(g.actor_id) ||
        std::find(common.actor_ids.begin(), common.actor_ids.end(),
                  g.actor_id) == common.actor_ids.end())
      return common_failure("MilitaryTacticInitialActorRequired", g.actor_id);
    auto id = c.next_id("military_tactic");
    if (!id.ok)
      return {false, {}, id.error};
    checked = c.append_fact(
        {id.value, "military_tactic_learned", "military_tactics"});
    if (!checked.ok)
      return checked;
    state.acquisitions.push_back(
        {id.value,
         g.actor_id,
         g.tactic_id,
         g.id,
         MilitaryTacticLearningSourceKind::authored_initial,
         0,
         {}});
  }
  return validate_military_tactic_learning_structure(c, common, cat, state);
}
Outcome<MilitaryTacticLearningResult>
record_military_tactic_completion_in_candidate(
    CommonStore &c, const CommonCatalog &common,
    const MilitaryTacticLearningCatalog &cat,
    MilitaryTacticLearningState &state, MilitaryTacticLearningSourceKind kind,
    const StableId &source_id, const MilitaryTacticLearningSourceRead &read,
    const DynamicStepInput *phase) {
  using R = MilitaryTacticLearningResult;
  auto checked =
      validate_military_tactic_learning_structure(c, common, cat, state, phase);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (!cat.enabled ||
      kind == MilitaryTacticLearningSourceKind::authored_initial)
    return fail<R>("MilitaryTacticCompletionSourceRequired", source_id);
  auto observed = upper(c, phase);
  if (!observed.ok)
    return {false, {}, observed.error};
  MilitaryTacticAcquisitionFact fact;
  fact.source_kind = kind;
  fact.source_id = source_id;
  if (kind == MilitaryTacticLearningSourceKind::teaching) {
    auto p = read.completed_tactic_teaching(source_id);
    if (!p.ok)
      return {false, {}, p.error};
    fact.actor_id = p.value.student_id;
    fact.tactic_id = p.value.tactic_id;
    fact.tick = p.value.completed_tick;
  } else if (kind == MilitaryTacticLearningSourceKind::practice) {
    auto p = read.completed_tactic_practice(source_id);
    if (!p.ok)
      return {false, {}, p.error};
    fact.actor_id = p.value.actor_id;
    fact.tactic_id = p.value.tactic_id;
    fact.tick = p.value.completed_tick;
  } else
    return fail<R>("MilitaryTacticCompletionSourceRequired", source_id);
  const bool teaching = kind == MilitaryTacticLearningSourceKind::teaching;
  if (!head(c, source_id,
            teaching ? "tactic_lesson:" : "military_tactic_practice:",
            teaching ? "tactic_lesson_closed"
                     : "military_tactic_practice_completed",
            teaching ? "growth_tactic_lessons" : "military_tactics") ||
      !find(cat.definitions, fact.tactic_id))
    return fail<R>("MilitaryTacticCompletionSourceMismatch", source_id);
  checked = completion(c, common, cat, state, fact, read, phase);
  if (!checked.ok)
    return {false, {}, checked.error};
  for (const auto &f : state.acquisitions)
    if (f.actor_id == fact.actor_id && f.tactic_id == fact.tactic_id) {
      if (f.source_kind != kind || f.source_id != source_id)
        return fail<R>("MilitaryTacticAlreadyLearned", fact.tactic_id);
      return {true, {f.id, true}, {}};
    }
  const auto *actor = c.actor(fact.actor_id);
  if (fact.tick != observed.value || !actor || !actor->alive)
    return fail<R>("MilitaryTacticLearningCurrentCompletionRequired",
                   source_id);
  auto id = c.next_id("military_tactic");
  if (!id.ok)
    return {false, {}, id.error};
  checked =
      c.append_fact({id.value, "military_tactic_learned", "military_tactics"});
  if (!checked.ok)
    return {false, {}, checked.error};
  fact.id = id.value;
  state.acquisitions.push_back(std::move(fact));
  checked =
      validate_military_tactic_learning_structure(c, common, cat, state, phase);
  return checked.ok ? Outcome<R>{true, {id.value, false}, {}}
                    : Outcome<R>{false, {}, checked.error};
}
} // namespace hanmo::resident
