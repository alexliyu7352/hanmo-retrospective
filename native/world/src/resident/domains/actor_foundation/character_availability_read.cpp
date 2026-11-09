#include "character_availability_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
std::size_t position(const CommonStore &common, const SourceRef &ref) {
  const auto actual = read_actual_common_fact_position(common, ref);
  if (!actual.ok)
    fail("CharacterAvailabilityOriginalFactRequired", ref.id);
  return actual.value;
}
void sources(const CharacterConditionSources &body, const StableId &actor,
             const CurrentMomentRead &clock, bool complete) {
  if (!body.deaths || !body.life_sources)
    fail("CharacterAvailabilityLife4SourcesRequired", {},
         ErrorKind::unsupported_capability);
  const auto now = take(clock.read_current_moment());
  if (take(compare_world_moments(now, body.actual_now)))
    fail("CharacterAvailabilitySameWorldMomentRequired");
  take(validate_military_availability_actor_history(body, actor));
  if (complete) {
    take(validate_military_availability_coverage(body));
    take(validate_family_life4(body.common, *body.deaths, *body.life_sources,
                               body.actual_now, body.military_history_order));
  }
}
CharacterAvailabilityAtSource
before(const CharacterConditionSources &body, const StableId &actor,
       const SourceRef &boundary, const WorldMoment &at,
       const CurrentMomentRead &clock, bool complete,
       CharacterAvailabilityProvenanceSink *provenance = nullptr) {
  sources(body, actor, clock, complete);
  const auto cut = position(body.common, boundary);
  auto presence = take(read_character_presence_before_fact_v2(
      body.common, body.state, body.catalog, *body.deaths, *body.life_sources,
      actor, boundary, at, clock, body.family_catalog, body.shared,
      body.born_sources, provenance, body.military_history_order));
  const auto initial = take(read_character_initial_values_v2(
      body.common, body.state, body.catalog, body.family_catalog, body.shared,
      actor, body.actual_now, body.born_sources));
  CharacterAvailabilityAtSource out{
      initial.origin, presence.present,
      presence.alive, presence.present && presence.alive && initial.available,
      nullptr,        nullptr};
  bool available = initial.available;
  for (const auto &entry :
       body.state.character_condition_extensions.capacity_transitions.rows()) {
    const auto &row = entry.second;
    if (row.actor_id != actor)
      continue;
    if (!is_original_availability_transition(row))
      continue;
    const auto cause =
        take(read_original_availability_cause_position(body, row));
    const auto index = position(body.common, *cause.source);
    const auto compared =
        take(compare_world_moments(row.effective_at.exact_moment, at));
    if (index == cut)
      fail("CharacterAvailabilityBoundaryCannotBeOwnCause", boundary.id);
    if ((index < cut && compared > 0) || (index > cut && compared < 0))
      fail("CharacterAvailabilityOriginalTimeOrderMismatch", row.transition_id);
    if (index < cut) {
      if (row.from != available)
        fail("CharacterAvailabilityOriginalPredecessorMismatch",
             row.transition_id);
      available = row.to;
      out.latest_availability_transition = &row;
    }
    if (provenance) {
      take(provenance->append(
          CharacterAvailabilityProvenanceSlot::capacity_transition,
          {"actor_foundation", "capacity_transition", row.transition_id}, 2,
          index < cut));
      const auto &ref = *cause.source;
      take(provenance->append(
          CharacterAvailabilityProvenanceSlot::capacity_cause,
          {ref.domain, ref.kind, ref.id}, cause.schema, index < cut));
    }
  }
  if (presence.death_ref) {
    out.actual_death = body.deaths->deaths.find(presence.death_ref->id);
    if (!out.actual_death)
      fail("CharacterAvailabilityOriginalDeathRequired", actor);
  }
  out.available = out.present && out.alive && available;
  return out;
}
} // namespace
Outcome<CharacterAvailabilityAtSource>
read_character_availability_before_fact_v2(
    const CharacterConditionSources &body, const StableId &actor,
    const SourceRef &boundary, const WorldMoment &at,
    const CurrentMomentRead &clock) {
  try {
    return {true, before(body, actor, boundary, at, clock, true), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<CharacterAvailabilitySpanRead>
read_character_availability_span_after_fact_v2(
    const CharacterConditionSources &body, const StableId &actor,
    const SourceRef &adoption, const WorldMoment &adopted_at,
    const MomentSpan &span, const CurrentMomentRead &clock) {
  try {
    if (take(compare_world_moments(span.from, span.to)) >= 0 ||
        take(compare_world_moments(span.from, adopted_at)) < 0 ||
        take(compare_world_moments(span.to, body.actual_now)) > 0)
      fail("CharacterAvailabilityActualSpanInvalid", actor);
    auto initial = before(body, actor, adoption, adopted_at, clock, true);
    CharacterAvailabilitySpanRead out{initial, initial.present && initial.alive,
                                      initial.available, nullptr,
                                      initial.actual_death};
    if (!initial.present)
      return {true, out, {}};
    const auto cut = position(body.common, adoption);
    // 先归约到真正span.from；新释放只恢复它自己捕获前的值，窗口内损失仍使全窗不可用。
    bool current_available = initial.available;
    const CapacityTransitionV2 *current_loss =
        current_available ? nullptr : initial.latest_availability_transition;
    out.available_for_full_span = true;
    for (const auto &entry : body.state.character_condition_extensions
                                 .capacity_transitions.rows()) {
      const auto &row = entry.second;
      if (row.actor_id != actor || !is_original_availability_transition(row))
        continue;
      const auto cause =
          take(read_original_availability_cause_position(body, row));
      if (position(body.common, *cause.source) <= cut)
        continue;
      if (take(compare_world_moments(row.effective_at.exact_moment, span.to)) >=
          0)
        continue;
      if (take(compare_world_moments(row.effective_at.exact_moment,
                                     span.from)) <= 0) {
        current_available = row.to;
        current_loss = row.to ? nullptr : &row;
        continue;
      }
      if (!current_available) {
        out.available_for_full_span = false;
        if (!out.first_unavailable_transition)
          out.first_unavailable_transition = current_loss;
      }
      current_available = row.to;
      current_loss = row.to ? nullptr : &row;
      if (!current_available) {
        out.available_for_full_span = false;
        if (!out.first_unavailable_transition)
          out.first_unavailable_transition = &row;
      }
    }
    if (!current_available) {
      out.available_for_full_span = false;
      if (!out.first_unavailable_transition)
        out.first_unavailable_transition = current_loss;
    }
    if (initial.alive) {
      const auto *death = take(read_character_death_after_fact_v2(
          body.common, body.state, body.catalog, *body.deaths,
          *body.life_sources, actor, adoption, adopted_at, clock,
          body.family_catalog, body.shared, body.born_sources,
          body.military_history_order));
      if (death && take(compare_world_moments(death->precise->effective_at,
                                              span.to)) < 0) {
        out.death_before_span_end = death;
        out.alive_for_full_span = false;
        out.available_for_full_span = false;
      }
    }
    return {true, out, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<CharacterAvailabilityAtSource>
read_character_availability_before_fact_provenance_v2(
    const CharacterConditionSources &body, const StableId &actor,
    const SourceRef &boundary, const WorldMoment &at,
    const CurrentMomentRead &clock) {
  try {
    return {true, before(body, actor, boundary, at, clock, false), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<CharacterAvailabilityAtSource>
visit_character_availability_before_fact_provenance_v2(
    const CharacterConditionSources &body, const StableId &actor,
    const SourceRef &boundary, const WorldMoment &at,
    const CurrentMomentRead &clock,
    CharacterAvailabilityProvenanceSink &provenance) {
  try {
    return {
        true, before(body, actor, boundary, at, clock, false, &provenance), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
