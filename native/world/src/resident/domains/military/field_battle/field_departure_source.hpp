// Actual local departure provenance for the single-formation S1 handoff.
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
struct FieldDepartureSourceRead {
  const FieldBattleSession *session = nullptr;
  const FieldFormation *formation = nullptr;
  const FieldBattleRoadBinding *binding = nullptr;
  const FieldBattleEvent *withdrawal = nullptr;
  // True surviving named original members only; never the rest of the Army.
  std::vector<const StableId *> surviving_actor_ids;
};
// Called after the actual step boundary is committed to the unpublished
// session. This does not grant S1 mutation or manufacture an exit application.
Outcome<FieldDepartureSourceRead> read_field_actual_departure_source(
    const CommonStore &, const FieldBattleWorldState &,
    const StableId &battle_id, const StableId &formation_id);
// Finite terminal-routing exception to reaching a qualified exit cell. Only
// retained Rout -> Withdrawn -> Ended events at the real terminal cut prove it.
// No new position/state is manufactured by this source-local predicate.
bool is_field_terminal_routing_departure(const FieldDepartureSourceRead &);
} // namespace hanmo::resident
