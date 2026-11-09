// Pure original §6.1 rally geometry. Only actually entered formations occur in
// session.formations; pending queues are not combatants or weight contributors.
#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
struct FieldReinforcementRallyRead {
  PointI rally_dm;
  PointI commander_dm;
};
Outcome<FieldReinforcementRallyRead>
read_field_reinforcement_rally(const FieldBattleSession &,
                               const GeneratedBattleField &, Integer side,
                               FieldTroopCategory arriving_category);
struct FieldMotionPrefixRead;
// Same rule over the caller's actual local-event replay prefix. This does not
// grant authenticity to arbitrary supplied positions or create another state.
Outcome<FieldReinforcementRallyRead> read_field_reinforcement_rally_at_prefix(
    const FieldBattleSession &, const GeneratedBattleField &, Integer side,
    FieldTroopCategory arriving_category, const FieldMotionPrefixRead &);
// A queue head may enter only after the preceding formation's original
// collision circle no longer contains the actual entry point.
Outcome<bool> read_field_reinforcement_entry_clear(
    const FieldBattleSession &, PointI actual_entry_dm,
    const StableId &actual_preceding_formation);
} // namespace hanmo::resident
