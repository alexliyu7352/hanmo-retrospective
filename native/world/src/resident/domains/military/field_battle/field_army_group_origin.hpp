#pragma once
#include "field_group_handoff.hpp"
namespace hanmo::resident {
struct FieldOriginalArmyGroupOrigin {
  const FieldBattleRoadBinding *battle_binding = nullptr;
  const GroupMovementJourney *journey = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const MovementUnitCloseSource *closed = nullptr;
  const SourceRef *original_contact = nullptr;
};
// Actual frozen world carrier origin of an Army admitted to this battle.
// It does not infer a current corpse/captor position from a local formation,
// nor does it assert that a particular queued formation entered the field.
Outcome<FieldOriginalArmyGroupOrigin>
read_field_original_army_group_origin(const FieldGroupHandoffReadContext &,
                                      const FieldBattleRoadBinding &,
                                      ExactArithmeticBudget = {});
} // namespace hanmo::resident
