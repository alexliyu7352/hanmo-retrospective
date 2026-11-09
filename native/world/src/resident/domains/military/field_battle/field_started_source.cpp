#include "field_started_source.hpp"
#include "field_contact_origin.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <tuple>
namespace hanmo::resident {
Outcome<FieldBattleStartedSourceRead> read_field_battle_started_source(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const SourceRef &source,
    const FieldSegmentApplicationRead &clock,
    const MilitaryFormationSourceRead &formations,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldBattleStartedSourceRead> {
    return {false, {}, common_failure(code, source.id).error};
  };
  if (!clock.bound_to(common, military, world) ||
      !formations.bound_to(common.state(), military) ||
      source.domain != "military" || source.kind != "field_battle_started")
    return fail("FieldStartedOriginalOwnerRequired");
  auto start_position = read_actual_common_fact_position(common, source);
  if (!start_position.ok)
    return {false, {}, start_position.error};
  const FieldBattleSession *session = nullptr;
  for (const auto &[id, value] : world.sessions) {
    if (!value.started_source ||
        !same_source_ref(*value.started_source, source))
      continue;
    if (session || id != value.battle_id)
      return fail("FieldStartedSourceAmbiguous");
    session = &value;
  }
  if (!session || !session->started_at || !session->started_actual_at ||
      (session->state != FieldBattleState::Running &&
       session->state != FieldBattleState::Ended) ||
      session->started_actual_at->half_day_tick !=
          session->started_observed_tick ||
      session->started_observed_tick < session->created_tick ||
      std::tie(session->started_at->tick, session->started_at->t) >
          std::tie(session->now.tick, session->now.t) ||
      (session->started_by_actor_id &&
       !common.actor(*session->started_by_actor_id)))
    return fail("FieldStartedOriginalApplicationRequired");
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto not_future =
      compare_world_moments(*session->started_actual_at, now.value, budget);
  if (!not_future.ok)
    return {false, {}, not_future.error};
  if (not_future.value > 0)
    return fail("FieldStartedApplicationInFuture");
  const FieldBattleContactRecord *origin = nullptr;
  for (const auto &[id, contact] : world.contacts) {
    if (contact.battle_id != session->battle_id)
      continue;
    if (origin || id != contact.contact_id)
      return fail("FieldStartedOriginalContactAmbiguous");
    auto origin_head = validate_field_contact_origin_head(common, contact);
    if (!origin_head.ok)
      return {false, {}, origin_head.error};
    origin = &contact;
  }
  if (!origin)
    return fail("FieldStartedOriginalContactRequired");
  auto parent = read_actual_common_fact_position(
      common,
      SourceRef{"military", "field_battle_contact", origin->contact_id});
  auto installed = read_actual_common_fact_position(
      common,
      SourceRef{"military", "field_battle_session", session->battle_id});
  if (!parent.ok)
    return {false, {}, parent.error};
  if (!installed.ok)
    return {false, {}, installed.error};
  if (parent.value >= installed.value ||
      installed.value >= start_position.value)
    return fail("FieldStartedOriginalSourceOrderMismatch");
  for (Integer side = 0; side < 2; ++side) {
    const auto &faction = session->sides[side];
    if (faction.side != side || faction.faction_id.empty() ||
        !session->councils[side].closed)
      return fail("FieldStartedOriginalSideRequired");
    const ArmyRecord *army = nullptr;
    // Army IDs/formation sources are retained identities; faction authority is
    // the original completed formation terms, not a new battle's live binding.
    for (const auto &row : military.armies) {
      if (row.id != origin->original_bindings[0].army_id &&
          row.id != origin->original_bindings[1].army_id)
        continue;
      auto original = read_original_military_formation(
          common.state(), military, row, *session->started_actual_at,
          &formations, budget);
      if (!original.ok)
        return {false, {}, original.error};
      if (!original.value.original_terms)
        return fail("FieldStartedOriginalFormationRequired");
      if (original.value.original_terms->organization_id != faction.faction_id)
        continue;
      if (army)
        return fail("FieldStartedOriginalArmyAmbiguous");
      auto formed = read_actual_common_fact_position(
          common, original.value.completed_source.source_ref);
      if (!formed.ok)
        return {false, {}, formed.error};
      if (formed.value >= parent.value)
        return fail("FieldStartedOriginalFormationOrderMismatch");
      army = &row;
    }
    if (!army)
      return fail("FieldStartedOriginalFactionRequired");
    bool has_original_formation = false;
    for (const auto &[id, formation] : session->formations)
      if (formation.army_id == army->id) {
        if (formation.side != side)
          return fail("FieldStartedOriginalSideMismatch");
        has_original_formation = true;
      }
    if (!has_original_formation)
      return fail("FieldStartedOriginalFormationRequired");
    const FieldCouncilFact *adoption = nullptr;
    for (const auto &[id, row] : world.council_facts) {
      if (row.kind != FieldCouncilFactKind::Adopted ||
          row.battle_id != session->battle_id || row.side != side)
        continue;
      if (adoption || row.fact_id != id || !row.adopted_actual_at ||
          !row.council.closed)
        return fail("FieldStartedOriginalAdoptionAmbiguous");
      auto position = read_actual_common_fact_position(
          common, SourceRef{"military", "military_council", id});
      if (!position.ok)
        return {false, {}, position.error};
      auto moment = compare_world_moments(*row.adopted_actual_at,
                                          *session->started_actual_at, budget);
      if (!moment.ok)
        return {false, {}, moment.error};
      if (position.value <= installed.value ||
          position.value >= start_position.value || moment.value > 0)
        return fail("FieldStartedOriginalAdoptionOrderMismatch");
      adoption = &row;
    }
    if (!adoption)
      return fail("FieldStartedOriginalAdoptionRequired");
  }
  if (session->sides[0].faction_id == session->sides[1].faction_id)
    return fail("FieldStartedSidesSameFaction");
  return {true,
          {session,
           &*session->started_source,
           &*session->started_actual_at,
           {&session->sides[0].faction_id, &session->sides[1].faction_id},
           1},
          {}};
}
} // namespace hanmo::resident
