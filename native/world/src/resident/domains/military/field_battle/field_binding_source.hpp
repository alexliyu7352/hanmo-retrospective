#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
// Retained original contact/entry identity only; no claim about today's hold.
Outcome<const FieldBattleRoadBinding *> read_field_original_battle_binding(
    const CommonStore &, const FieldBattleWorldState &, const StableId &battle,
    const StableId &army);
} // namespace hanmo::resident
