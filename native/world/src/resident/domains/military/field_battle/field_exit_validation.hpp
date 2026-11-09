#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct FieldBattleAbilityReadContext;
struct FieldBattleCatalog;
struct TravelCatalog;
// Domain side of the original S1 handshake. S1 independently checks its actual
// location/binding history; this never substitutes an exit for adopted release.
Status validate_field_original_exit_handoffs(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext *);
} // namespace hanmo::resident
