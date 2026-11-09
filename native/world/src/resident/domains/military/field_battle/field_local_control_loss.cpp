#include "field_event_legacy_kinds.hpp"
#include "field_local_control_loss.hpp"
#include "field_binding_source.hpp"
#include "field_reinforcement_queue.hpp"
#include "resident/storage/common_store.hpp"
#include <tuple>
namespace hanmo::resident {
Outcome<std::optional<FieldBattleTime>> read_field_original_player_local_loss(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, Integer side, const StableId &actor) {
  auto fail = [&]() -> Outcome<std::optional<FieldBattleTime>> {
    return {false,
            {},
            common_failure("FieldOriginalLocalLossMismatch", actor).error};
  };
  const auto actual = world.sessions.find(session.battle_id);
  if (actual == world.sessions.end() || &actual->second != &session ||
      side < 0 || side > 1)
    return fail();
  const bool original_player = session.councils[side].player_actor_id == actor;
  const FieldCouncilFact *adopted = nullptr;
  if (original_player) {
    for (const auto &[id, row] : world.council_facts)
      if (row.battle_id == session.battle_id && row.side == side &&
          row.kind == FieldCouncilFactKind::Adopted) {
        const auto *head = common.state().facts.find(id);
        if (adopted || row.fact_id != id || !head ||
            head->domain != "military" || head->kind != "military_council" ||
            row.council.player_actor_id != actor || !row.council.closed)
          return fail();
        adopted = &row;
      }
    if (!adopted)
      return fail();
  }
  std::optional<FieldBattleTime> result;
  for (const auto &[id, f] : session.formations) {
    if (f.side != side || f.commander_actor_id != actor)
      continue;
    FieldBattleTime participation;
    if (adopted) {
      bool participant = false;
      for (const auto &row : adopted->council.participants)
        if (row.actor_id == actor && row.formation_id == id)
          participant = true;
      if (!participant)
        return fail();
      participation = adopted->at;
    } else {
      // A later participant has its own real entry, not a forged copy of the
      // original council/player-controller identity.
      auto binding = read_field_original_battle_binding(
          common, world, session.battle_id, f.army_id);
      if (!binding.ok)
        return {false, {}, binding.error};
      if (!std::holds_alternative<FieldBoundaryContactBinding>(
              binding.value->source))
        continue; // Original non-player initial participant, not today's
                  // metadata.
      auto entry = read_field_reinforcement_admission_identity(common, world,
                                                               session, f);
      if (!entry.ok)
        return {false, {}, entry.error};
      if (entry.value->ai_mission && !entry.value->pending_player_entry_order)
        continue;
      if (!entry.value->pending_player_entry_order || entry.value->ai_mission)
        return fail();
      participation = entry.value->at;
    }
    const FieldCommanderCasualty *death = nullptr;
    for (const auto &row : f.commander_casualties)
      if (row.actor_id == actor && row.loss == 2) {
        if (death)
          return fail();
        death = &row;
      }
    if (!death) {
      if (f.commander_lost_at)
        return fail();
      continue;
    }
    if (result || !f.commander_lost_at ||
        std::tie(f.commander_lost_at->tick, f.commander_lost_at->t) !=
            std::tie(death->at.tick, death->at.t) ||
        std::tie(death->at.tick, death->at.t) >
            std::tie(session.now.tick, session.now.t) ||
        std::tie(death->at.tick, death->at.t) <
            std::tie(participation.tick, participation.t))
      return fail();
    Integer events = 0;
    for (const auto &event : session.events)
      if (event.kind == field_event_kind::CommanderLost &&
          event.formation_id == id &&
          std::tie(event.at.tick, event.at.t) ==
              std::tie(death->at.tick, death->at.t)) {
        if (event.side != side || event.count != 2)
          return fail();
        ++events;
      }
    if (events != 1)
      return fail();
    result = death->at;
  }
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
