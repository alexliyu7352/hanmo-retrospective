#include "native_field_military_consumers.hpp"
#include "formation_spatial.hpp"
#include "military_group_movement.hpp"
#include "military_roster_history.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <iterator>
#include <new>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code).error};
}
void bound(WorldBundle &w, const NativeMilitaryWorldConstructionContext &x,
           ExactPhaseWriteContext &writes) {
  require(&x.world == &w && w.active_exact_write_context() == &writes &&
              w.catalog && w.catalog->military && w.field_battle &&
              !w.military_exact && w.military_action_time && w.action_elapsed,
          "FieldMilitaryConsumerSameCandidateRequired");
  const auto actual = take(read_military_exact_phase_selection(*w.catalog));
  require(actual.field_original && !actual.dynamic_schema &&
              !actual.actions.army_return && !actual.actions.military_train &&
              !actual.actions.army_resupply && !actual.naval_repair &&
              !actual.station && x.selected.field_original &&
              !x.selected.dynamic_schema &&
              actual.actions.army_form == x.selected.actions.army_form &&
              actual.actions.office_accept == x.selected.actions.office_accept,
          "FieldMilitaryConsumerSelectionMismatch");
  require(take(compare_world_moments(x.at, take(x.clock.read_current_moment()),
                                     x.budget)) == 0 &&
              take(compare_world_moments(x.at, take(w.actual_phase_moment()),
                                         x.budget)) == 0,
          "FieldMilitaryConsumerActualMomentRequired");
  take(validate_selected_military_roster_root(w.common, w.catalog->identity,
                                              nullptr));
  take(validate_selected_military_field_history(
      w.common, w.military, w.catalog->identity, x.field_history));
}
} // namespace
Status release_native_field_military_action_in_candidate(
    WorldBundle &w, const NativeMilitaryWorldConstructionContext &x,
    ExactPhaseWriteContext &writes, const StableId &id) {
  try {
    bound(w, x, writes);
    const auto *fact = w.military_action_time->dynamic_action_facts.find(id);
    const auto *action = w.common.action(id);
    require(
        fact && action && fact->id == id && fact->phase == "completed" &&
            fact->completed_at && fact->terminal_source_ref &&
            action->kind == fact->command &&
            action->actor_id == fact->actor_id &&
            ((fact->command == "office_accept" &&
              x.selected.actions.office_accept) ||
             (fact->command == "army_form" && x.selected.actions.army_form)),
        "FieldMilitaryOriginalTerminalActionRequired");
    require(take(compare_world_moments(*fact->completed_at, x.at, x.budget)) ==
                0,
            "FieldMilitaryTerminalMomentMismatch");
    const auto &ref = *fact->terminal_source_ref;
    const auto *head = w.common.state().facts.find(ref.id);
    require(head && head->id == ref.id && head->domain == ref.domain &&
                head->kind == ref.kind,
            "FieldMilitaryTerminalHeadRequired");
    std::size_t terminals = 0;
    for (const auto &row : w.action_elapsed->fixed_service_transitions)
      if (row.action_ref == id && row.kind == FixedTransitionKind::terminal) {
        require(row.terminal_disposition.has_value() &&
                    take(compare_world_moments(row.occurred_at, x.at,
                                               x.budget)) == 0,
                "FieldMilitaryOriginalTerminalTransitionMismatch");
        ++terminals;
      }
    require(terminals == 1, "FieldMilitaryOriginalTerminalTransitionRequired");
    // The original owner has already adopted the real elapsed terminal. This
    // releases actual reservations/occupation; it does not erase elapsed
    // history.
    return release_action(w, id);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldMilitaryConsumerResourceExhausted").error};
  }
}
Status adopt_native_field_formation_locations_in_candidate(
    WorldBundle &w, const NativeMilitaryWorldConstructionContext &x,
    ExactPhaseWriteContext &writes, const DynamicMilitaryActionFact2 &fact,
    Integer first) {
  try {
    bound(w, x, writes);
    require(x.formation && x.selected.actions.army_form && w.movement_exact &&
                w.military_action_time->dynamic_action_facts.find(fact.id) ==
                    &fact &&
                fact.command == "army_form" && fact.phase == "completed" &&
                fact.success && fact.completed_at &&
                w.common.actor_location_capture_identity(),
            "FieldMilitaryActualFormationRequired");
    auto actual = take(adopt_exact_formation_spatial_bindings_in_candidate(
        w.common, *w.movement_exact, w.military, *w.military_action_time,
        *w.action_elapsed, *x.formation, fact.id, first));
    for (const auto &next : actual)
      for (const auto &old : writes.spatial_location_writes)
        require(next.capture_identity != old.capture_identity ||
                    next.capture_sequence != old.capture_sequence,
                "FieldMilitaryFormationCaptureAlreadyAdopted");
    for (auto &next : actual)
      writes.spatial_location_writes.emplace_back(std::move(next));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldMilitaryConsumerResourceExhausted").error};
  }
}
Outcome<StableId> start_native_field_campaign_departure_in_candidate(
    WorldBundle &w, const NativeMilitaryWorldConstructionContext &x,
    ExactPhaseWriteContext &writes, const GroupMovementContext &group,
    const CurrentReadStampRead &stamps,
    const MilitaryMarchAuthorSelection &author, const StableId &campaign,
    const StableId &actor) {
  try {
    bound(w, x, writes);
    require(
        w.movement_exact && w.spatial_carriers && w.group_movement_executions &&
            w.catalog->travel && &group.travel_catalog == &*w.catalog->travel &&
            &group.travel_services == &w.services &&
            &x.body.common == &w.common && x.body.military == &w.military &&
            !x.body.military_exact &&
            take(compare_world_moments(x.body.actual_now, x.at, x.budget)) ==
                0 &&
            take(compare_world_moments(take(stamps.read_current_moment()), x.at,
                                       x.budget)) == 0,
        "FieldMilitaryDepartureOriginalGroupRequired");
    const MilitarySelectedHistoryRead selected{w.catalog->identity, nullptr,
                                               x.field_history};
    // This original producer previews and adopts CampaignDepart itself at the
    // real S1 opening; no calendar row/return target is forged into a command.
    return start_selected_military_campaign_group_journey_in_candidate(
        w.common, w.military, *w.catalog->military, selected, x.access, stamps,
        x.body, author, *w.spatial_carriers, *w.movement_exact,
        *w.group_movement_executions, group, campaign, actor,
        writes.spatial_location_writes,
        w.population_military_enabled() ? &w.population_military : nullptr,
        w.population_military_enabled() ? &*w.catalog->population_military
                                        : nullptr,
        x.budget, &x.formations, &x.returns, &x.periods);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldMilitaryConsumerResourceExhausted").error};
  }
}
} // namespace hanmo::resident
