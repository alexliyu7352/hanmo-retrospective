#include "field_local_step.hpp"
#include "field_army_exit_selection.hpp"
#include "field_ai_driver.hpp"
#include "field_commander_step.hpp"
#include "field_deployment_commands.hpp"
#include "field_morale.hpp"
#include "field_reinforcement_geometry.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_routing.hpp"
#include "field_segment_settlement.hpp"
#include "field_stratagem_resolution.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <tuple>
namespace hanmo::resident {
namespace {
bool same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
} // namespace
Outcome<FieldBattleWindowSlice> read_field_window_at_actual_boundary(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const StableId &battle,
    const FieldSegmentApplicationRead &clock, const FieldBattleTime &to,
    const FieldBattleParameters &parameters) {
  auto fail = [&](const char *code) -> Outcome<FieldBattleWindowSlice> {
    return {false, {}, common_failure(code, battle).error};
  };
  if (!clock.bound_to(common, military, world))
    return fail("FieldWindowWorldMismatch");
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto canonical = compare_world_moments(now.value, now.value);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  auto closing = clock.read_field_window_closing_moment();
  if (!closing.ok)
    return {false, {}, closing.error};
  auto same_closing = compare_world_moments(now.value, closing.value);
  if (!same_closing.ok)
    return {false, {}, same_closing.error};
  if (same_closing.value != 0)
    return fail("FieldWindowClosingMomentMismatch");
  auto zero = compare_fractions(now.value.sub_half_day, CanonicalFraction{});
  if (!zero.ok)
    return {false, {}, zero.error};
  if (zero.value != 0 || now.value.half_day_tick < common.tick() ||
      __int128_t(now.value.half_day_tick) - common.tick() > 1)
    return fail("FieldWindowActualClosingBoundaryRequired");
  const auto session = world.sessions.find(battle);
  if (session == world.sessions.end())
    return fail("FieldBattleNotFound");
  return read_field_battle_window_slice(
      session->second, now.value.half_day_tick, to, parameters);
}
Outcome<FieldLocalStepResult> apply_field_local_step_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &field_catalog, FieldBattleWorldState &world,
    const StableId &battle, const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &abilities,
    const FieldSegmentApplicationRead &actual_clock,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto fail = [&](const char *code) -> Outcome<FieldLocalStepResult> {
    return {false, {}, common_failure(code, battle).error};
  };
  if (!actual_clock.bound_to(common, military, world))
    return fail("FieldStepWorldMismatch");
  auto actual_before = actual_clock.read_current_moment();
  if (!actual_before.ok)
    return {false, {}, actual_before.error};
  // Original S0 may expose its true closing M while Common still retains
  // the opening committed tick. Use the bound provider and same-owner
  // ability clock below, never reconstruct a WorldMoment from local seconds.
  auto canonical =
      compare_world_moments(actual_before.value, actual_before.value);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  // The ability/attribute path must observe the same actual S0 boundary as
  // the settlement/life path. Equal local seconds are not such a proof.
  auto ability_moment = read_field_actual_application_moment(common, abilities);
  if (!ability_moment.ok)
    return {false, {}, ability_moment.error};
  auto same_boundary =
      compare_world_moments(actual_before.value, ability_moment.value);
  if (!same_boundary.ok)
    return {false, {}, same_boundary.error};
  if (same_boundary.value != 0)
    return fail("FieldStepAbilityMomentMismatch");
  const auto committed_tick = common.tick();
  auto found = world.sessions.find(battle);
  if (found == world.sessions.end())
    return fail("FieldBattleNotFound");
  auto &session = found->second;
  if (field.field_hash != session.field_hash)
    return fail("FieldStepGeometryMismatch");
  auto allowed = read_field_window_at_actual_boundary(
      common, military, world, battle, actual_clock, step.to, field.parameters);
  if (!allowed.ok)
    return {false, {}, allowed.error};
  if (step.steps != 1 || allowed.value.steps != 1 ||
      !same(step.from, allowed.value.from) ||
      !same(step.to, allowed.value.to) ||
      step.segment != allowed.value.segment ||
      step.opens_next_segment != allowed.value.opens_next_segment ||
      step.closes_window != allowed.value.closes_window)
    return fail("FieldStepBoundaryMismatch");
  // A due entry cannot disappear behind a no-op provider. The outer actual
  // arrival producer must create each eligible formation and its real source.
  // A partial 31ef queue keeps its ORIGINAL due while its preceding circle
  // clears the entry. Blocking that movement would deadlock the first entrant.
  for (const auto &entry : session.pending_entries) {
    if (!before(entry.entry, step.to))
      continue;
    const auto boundary = world.boundary_contacts.find(entry.contact_id);
    if (boundary == world.boundary_contacts.end() ||
        boundary->second.battle_id != battle ||
        boundary->second.status != FieldBoundaryStatus::Entered ||
        !boundary->second.reinforcement_queue || !abilities.exact_contacts ||
        !abilities.original_rosters)
      return fail("FieldPendingEntryApplicationOrderMismatch");
    const auto &exact = *abilities.exact_contacts;
    if (&exact.common != &common || &exact.military != &military ||
        &exact.field != &world ||
        !exact.clock.bound_to(common, military, world))
      return fail("FieldPendingEntrySameWorldRequired");
    auto entry_now = exact.clock.read_current_moment();
    if (!entry_now.ok)
      return {false, {}, entry_now.error};
    auto same_now = compare_world_moments(entry_now.value, actual_before.value);
    if (!same_now.ok)
      return {false, {}, same_now.error};
    if (same_now.value)
      return fail("FieldPendingEntryActualMomentMismatch");


    const auto source_memory = CheckpointMemoryCharge(0);
    const SourceRef actual_contact{
        checkpoint_fresh_text_copy("military"),
        checkpoint_fresh_text_copy("field_battle_contact"),
        checkpoint_fresh_string_copy(entry.contact_id)};
    auto original = read_field_original_reinforcement_queue(
        exact, field_catalog, *abilities.original_rosters, actual_contact);
    if (!original.ok)
      return {false, {}, original.error};
    const auto &queue = *original.value;
    if (queue.admitted.empty() ||
        queue.admitted.size() >= queue.formation_ids.size() ||
        !boundary->second.army_id ||
        *boundary->second.army_id != entry.army_id || !boundary->second.side ||
        *boundary->second.side != entry.side ||
        !same(entry.entry, queue.admitted.front().at) ||
        entry.entry_point_dm.x != boundary->second.entry_point_dm.x ||
        entry.entry_point_dm.y != boundary->second.entry_point_dm.y)
      return fail("FieldPendingEntryApplicationOrderMismatch");
    const auto &last = queue.admitted.back();
    const auto formation = session.formations.find(last.formation_id);
    if (formation == session.formations.end())
      return fail("FieldPendingEntryPreviousAdmissionRequired");
    auto admission = read_field_reinforcement_admission_identity(
        common, world, session, formation->second);
    if (!admission.ok)
      return {false, {}, admission.error};
    if (admission.value != &last)
      return fail("FieldPendingEntryPreviousAdmissionRequired");
    auto cleared = read_field_reinforcement_entry_clear(
        session, entry.entry_point_dm, last.formation_id);
    if (!cleared.ok)
      return {false, {}, cleared.error};
    if (cleared.value)
      return fail("FieldPendingEntryApplicationOrderMismatch");
  }
  if (step.opens_next_segment) {
    session.segment = step.segment;
    session.window_complete = false;
  }
  // Subphases consume the genuinely opened segment, not its opening request.
  auto running_step = step;
  running_step.opens_next_segment = false;
  auto status = apply_field_control_arrivals_in_candidate(
      common, world, battle, field.parameters, running_step, owner,
      abilities.original_life, abilities.actual_clock,
      abilities.original_read_boundary);
  if (!status.ok)
    return {false, {}, status.error};
  status = resolve_field_stratagems_in_candidate(
      common, world, battle, field, abilities, running_step, owner);
  if (!status.ok)
    return {false, {}, status.error};
  status = apply_field_ai_missions_in_candidate(
      common, military, catalog, travel, field_catalog, world, battle, field,
      abilities, running_step, owner);
  if (!status.ok)
    return {false, {}, status.error};
  // 回调在原WaveReleased等from时刻采用之后、任何本步移动之前冻结真实选择。
  auto moved = apply_field_mission_step_in_candidate(
      world, session, field, running_step, owner, [&] {
        return ensure_field_required_army_exit_choices_in_candidate(
            common, world, field_catalog, abilities, battle, false, owner);
      });
  if (!moved.ok)
    return {false, {}, moved.error};
  auto contact =
      apply_field_contacts_in_candidate(session, field, running_step, owner);
  if (!contact.ok)
    return {false, {}, contact.error};
  status = reveal_field_stratagems_after_motion_in_candidate(
      common, world, battle, field, abilities, running_step, owner);
  if (!status.ok)
    return {false, {}, status.error};
  auto pulse = apply_field_casualty_pulse_in_candidate(
      session, field, catalog.battle_catalog, running_step, owner);
  if (!pulse.ok)
    return {false, {}, pulse.error};
  auto morale = apply_field_pulse_morale_in_candidate(
      common, session, field, pulse.value, running_step, owner);
  if (!morale.ok)
    return {false, {}, morale.error};
  auto routing =
      apply_field_routing_in_candidate(session, field, running_step, owner);
  if (!routing.ok)
    return {false, {}, routing.error};
  auto commander = apply_field_commander_step_in_candidate(
      common, military, world, battle, field, pulse.value, running_step,
      abilities, actual_clock, owner);
  if (!commander.ok)
    return {false, {}, commander.error};
  auto departed =
      apply_field_departures_in_candidate(world, session, field, running_step, owner);
  if (!departed.ok)
    return {false, {}, departed.error};
  // Only the original local clock moves, after every original phase succeeds.
  // World Common.tick and S0 are untouched; failures above discard the World.
  session.now = step.to;
  session.window_complete = step.closes_window;
  if (departed.value.terminal) {
    status = ensure_field_required_army_exit_choices_in_candidate(
        common, world, field_catalog, abilities, battle, true, owner);
    if (!status.ok) return {false, {}, status.error};
  }
  status = recover_field_tactic_points_in_candidate(session, field.parameters);
  if (!status.ok)
    return {false, {}, status.error};
  status = publish_field_order_reports_in_candidate(session);
  if (!status.ok)
    return {false, {}, status.error};
  auto actual_after = actual_clock.read_current_moment();
  if (!actual_after.ok)
    return {false, {}, actual_after.error};
  auto unchanged =
      compare_world_moments(actual_before.value, actual_after.value);
  if (!unchanged.ok)
    return {false, {}, unchanged.error};
  if (unchanged.value != 0 || common.tick() != committed_tick)
    return fail("FieldStepChangedWorldTime");
  const bool settle = step.closes_window || departed.value.terminal;
  return {true, {step, std::move(departed.value), settle}, {}};
}
} // namespace hanmo::resident
