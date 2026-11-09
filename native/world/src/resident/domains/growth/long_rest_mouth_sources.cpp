#include "resident/domains/growth/growth_model.hpp"
#include "long_rest_mouth_sources.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "resident/domains/actor_foundation/character_condition_replay.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
void selected(const CharacterConditionSources &body,
              const LongRestActualPeriod &period) {
  if (!body.growth_catalog ||
      !body.growth_catalog->complete_exact_rest_mouth_sources ||
      !body.growth_catalog->complete_exact_command_cuts || !body.rest_estates ||
      &body.rest_estates->common != &body.common || !body.fixed_spatial ||
      !body.shared)
    fail("GrowthExactRestMouthSourcesRequired");
  take(read_estate_rest_at_adoption(*body.rest_estates, period.estate_basis));
  if (take(compare_world_moments(period.applied_at,
                                 period.estate_basis.adopted_at)) ||
      take(compare_world_moments(period.applied_at, body.actual_now)) > 0)
    fail("LongRestMouthActualAdoptionRequired");
}
std::size_t source_index(const CommonStore &common, const SourceRef &ref) {
  const auto *head = common.state().facts.find(ref.id);
  const auto &order = common.state().facts.order();
  auto at = std::find(order.begin(), order.end(), ref.id);
  if (!head || head->domain != ref.domain || head->kind != ref.kind ||
      at == order.end())
    fail("LongRestMouthOriginalHeaderRequired", ref.id);
  return static_cast<std::size_t>(at - order.begin());
}
struct Candidate {
  StableId actor;
  std::optional<MomentOwnerSource> adoption;
};
std::vector<Candidate> candidates(const CharacterConditionSources &body,
                                  const RestFact &rest,
                                  const LongRestActualPeriod &period) {
  selected(body, period);
  const auto &family = body.rest_estates->family;
  std::vector<Candidate> values{{rest.actor_id, {}}};
  for (const auto &row : family.facts.rows()) {
    const auto *adoption = std::get_if<AdoptionFact>(&row.second);
    if (!adoption || adoption->kind != "family_adoption" ||
        adoption->guardian_actor_id != rest.actor_id)
      continue;
    const auto *index = family.children.find(adoption->child_actor_id);
    const auto *raw = family.facts.find(adoption->child_actor_id);
    const auto *created =
        raw ? std::get_if<IdentityCreationFact>(raw) : nullptr;
    if (!adoption->success || !index || *index != row.first || !created ||
        created->source != row.first ||
        created->actor_id != adoption->child_actor_id ||
        created->prototype_id != adoption->terms.prototype_id ||
        created->created_tick != adoption->completed_tick ||
        adoption->start_tick < 0 || adoption->terms.duration < 1 ||
        adoption->start_tick >
            std::numeric_limits<Integer>::max() - adoption->terms.duration ||
        adoption->completed_tick !=
            adoption->start_tick + adoption->terms.duration)
      fail("LongRestOriginalAdoptionSourceRequired", row.first);
    source_index(body.common, {"family", "family_adoption", row.first});
    const auto completion = source_index(
        body.common, {"family", "actor_creation", created->actor_id});
    // This is the original legacy creation completion source, explicitly /1.
    // A selected precise adoption successor must supply its own actual source,
    // never reuse the earlier pending Action head as the completion cut.
    const auto happened =
        take(normalize_world_moment(created->created_tick, {}));
    const auto relation =
        take(compare_world_moments(happened, period.applied_at));
    const bool before = completion < static_cast<std::size_t>(
                                         period.estate_basis.common_fact_count);
    if ((before && relation > 0) || (!before && relation < 0))
      fail("LongRestAdoptionActualCutMismatch", row.first);
    if (!before)
      continue;
    values.push_back(
        {adoption->child_actor_id,
         MomentOwnerSource{{"family", "family_adoption", row.first}, 1}});
  }
  for (const auto &child : family.children.rows()) {
    const auto *raw = family.facts.find(child.second);
    const auto *adoption = raw ? std::get_if<AdoptionFact>(raw) : nullptr;
    if (adoption && adoption->guardian_actor_id == rest.actor_id &&
        (adoption->kind != "family_adoption" || !adoption->success ||
         adoption->child_actor_id != child.first))
      fail("LongRestOriginalChildIndexMismatch", child.first);
  }
  std::sort(values.begin(), values.end(),
            [](const auto &a, const auto &b) { return a.actor < b.actor; });
  for (std::size_t n = 1; n < values.size(); ++n)
    if (values[n - 1].actor == values[n].actor)
      fail("LongRestOriginalCandidateRepeated", values[n].actor);
  return values;
}
bool available_at(const CharacterConditionSources &body,
                  const LongRestActualPeriod &period, const StableId &actor,
                  bool alive) {
  take(validate_military_availability_coverage(body));
  auto original = take(read_character_initial_values_v2(
      body.common, body.state, body.catalog, body.family_catalog, body.shared,
      actor, body.actual_now, body.born_sources));
  bool value = original.available;
  for (const auto &row :
       body.state.character_condition_extensions.capacity_transitions.rows())
    if (row.second.actor_id == actor)
      if (is_original_availability_transition(row.second)) {
        const auto &transition = row.second;
        const auto original =
            take(read_original_availability_cause_position(body, transition));
        const bool before =
            source_index(body.common, *original.source) <
            static_cast<std::size_t>(period.estate_basis.common_fact_count);
        const auto relation = take(compare_world_moments(
            transition.effective_at.exact_moment, period.applied_at));
        if ((before && relation > 0) || (!before && relation < 0))
          fail("LongRestAvailabilityActualCutMismatch", row.first);
        if (before) {
          if (transition.from != value)
            fail("LongRestAvailabilityPredecessorMismatch", row.first);
          value = transition.to;
        }
      }
  return alive && value;
}
ActorSpatialLocationView location_at(const CharacterConditionSources &body,
                                     const LongRestActualPeriod &period,
                                     const StableId &actor) {
  const auto &spatial = *body.fixed_spatial;
  take(validate_growth_fixed_spatial_source_moment(spatial, body.actual_now));
  return take(read_actor_spatial_location_at_actual_fact_prefix(
      body.common, *body.shared, spatial.movement, spatial.carriers,
      spatial.registrations, body.state, body.catalog, spatial.original, actor,
      period.applied_at,
      ActorSpatialActualFactPrefix{period.estate_basis.common_fact_count,
                                   period.estate_basis.last_common_fact},
      spatial.military_handoffs, {}, body.family_catalog, spatial.births,
      spatial.military_actions, spatial.custody_releases, spatial.field_exits));
}
void verify_candidate(const LongRestMouthInputs &input,
                      const Candidate &candidate) {
  if (input.actor_id != candidate.actor ||
      bool(input.original_adoption) != bool(candidate.adoption) ||
      (input.original_adoption && (!same(input.original_adoption->source_ref,
                                         candidate.adoption->source_ref) ||
                                   input.original_adoption->source_schema !=
                                       candidate.adoption->source_schema)))
    fail("LongRestMouthOriginalCandidateMismatch", candidate.actor);
}
} // namespace
Outcome<std::vector<LongRestMouthInputs>> capture_long_rest_mouth_inputs(
    const CharacterConditionSources &body, const RestFact &rest,
    const LongRestActualPeriod &period, const RestEstateView &estate) {
  try {
    if (take(compare_world_moments(period.applied_at, body.actual_now)) ||
        !period.condition_receipt_refs.empty())
      fail("LongRestMouthActualCaptureRequired", rest.actor_id);
    auto original = candidates(body, rest, period);
    std::vector<LongRestMouthInputs> values;
    values.reserve(original.size());
    std::vector<StableId> eligible;
    for (auto &candidate : original) {
      LongRestMouthInputs input;
      input.actor_id = candidate.actor;
      input.original_adoption = std::move(candidate.adoption);
      auto condition =
          take(read_current_character_condition(body, input.actor_id));
      input.condition_before = std::move(condition.head);
      auto position = location_at(body, period, input.actor_id);
      for (auto &source : position.actual_location_sources)
        input.actual_location_sources.push_back(
            {std::move(source.source), source.schema});
      const auto *place =
          std::get_if<SettlementLocation>(&position.location_snapshot);
      const bool resident =
          std::find(estate.resident_actor_ids.begin(),
                    estate.resident_actor_ids.end(),
                    input.actor_id) != estate.resident_actor_ids.end();
      if (condition.body.alive &&
          available_at(body, period, input.actor_id, condition.body.alive) &&
          (input.actor_id == rest.actor_id || !resident) && place &&
          place->place_id == estate.place_id)
        eligible.push_back(input.actor_id);
      values.push_back(std::move(input));
    }
    if (eligible != period.mouth_actor_ids)
      fail("LongRestActualMouthSelectionMismatch", rest.actor_id);
    return {true, std::move(values), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_long_rest_mouth_inputs(const CharacterConditionSources &body,
                                       const RestFact &rest,
                                       const LongRestActualPeriod &period,
                                       const RestEstateView &estate) {
  try {
    auto original = candidates(body, rest, period);
    if (period.mouth_inputs.size() != original.size() ||
        period.condition_receipt_refs.empty())
      fail("LongRestMouthInputCoverageMismatch", rest.actor_id);
    const auto &receipts =
        body.state.character_condition_extensions.applied_receipts.rows();
    std::size_t cut = receipts.size();
    for (std::size_t n = 0; n < receipts.size(); ++n)
      if (receipts[n].first ==
          period.condition_receipt_refs.front().result_id) {
        cut = n;
        break;
      }
    if (cut == receipts.size())
      fail("LongRestActualConditionCutRequired", rest.actor_id);
    for (std::size_t n = 0; n < period.condition_receipt_refs.size(); ++n)
      if (cut + n >= receipts.size() ||
          receipts[cut + n].first != period.condition_receipt_refs[n].result_id)
        fail("LongRestActualConditionApplicationOrderMismatch", rest.actor_id);
    std::vector<StableId> eligible;
    for (std::size_t n = 0; n < original.size(); ++n) {
      const auto &input = period.mouth_inputs[n];
      verify_candidate(input, original[n]);
      auto origin = take(read_character_initial_values_v2(
          body.common, body.state, body.catalog, body.family_catalog,
          body.shared, input.actor_id, body.actual_now, body.born_sources));
      ActorConditionHead expected{
          input.actor_id, 0, origin.origin->initial_condition_seed_ref, {}};
      for (std::size_t i = 0; i < cut; ++i)
        if (receipts[i].second.actor_id == input.actor_id) {
          expected.condition_revision =
              receipts[i].second.condition_revision_after;
          expected.latest_receipt_ref =
              ConditionReceiptRefV2{receipts[i].first};
        }
      if (!same(input.condition_before, expected))
        fail("LongRestMouthActualBodyCutMismatch", input.actor_id);
      auto condition = take(read_character_condition_chain_cut(
          body, input.condition_before, period.applied_at));
      auto position = location_at(body, period, input.actor_id);
      if (input.actual_location_sources.size() !=
          position.actual_location_sources.size())
        fail("LongRestMouthActualLocationMismatch", input.actor_id);
      for (std::size_t i = 0; i < input.actual_location_sources.size(); ++i)
        if (!same(input.actual_location_sources[i].source_ref,
                  position.actual_location_sources[i].source) ||
            input.actual_location_sources[i].source_schema !=
                position.actual_location_sources[i].schema)
          fail("LongRestMouthActualLocationMismatch", input.actor_id);
      const auto *place =
          std::get_if<SettlementLocation>(&position.location_snapshot);
      const bool resident =
          std::find(estate.resident_actor_ids.begin(),
                    estate.resident_actor_ids.end(),
                    input.actor_id) != estate.resident_actor_ids.end();
      if (condition.body.alive &&
          available_at(body, period, input.actor_id, condition.body.alive) &&
          (input.actor_id == rest.actor_id || !resident) && place &&
          place->place_id == estate.place_id)
        eligible.push_back(input.actor_id);
    }
    if (eligible != period.mouth_actor_ids)
      fail("LongRestActualMouthSelectionMismatch", rest.actor_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
