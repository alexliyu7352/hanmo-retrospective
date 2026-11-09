// Actual local Lost is not an S4 DeathRef and must not advance the world clock.
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
Outcome<std::optional<FieldBattleTime>> read_field_original_player_local_loss(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, Integer side, const StableId &original_player);
} // namespace hanmo::resident
