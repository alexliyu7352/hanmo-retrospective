#pragma once
#include "field_exact_contact_source.hpp"
namespace hanmo::resident {
struct MovementBoundaryContact;
struct MovementPrefixState;
struct MovementUnitState;
struct TravelExertionBasis;
struct FieldBoundaryOriginalRead {
  const MovementBoundaryContact *contact = nullptr;
  const MovementPrefixState *prefix = nullptr;
  const MovementUnitState *unit = nullptr;
  const TravelExertionBasis *basis = nullptr;
  const MovementUnitCloseSource *closed = nullptr;
  // Civilian traffic has no invented military identity or alliance side.
  const ArmyRecord *army = nullptr;
  const StableId *faction_id = nullptr;
  const GroupMovementJourney *journey = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const MilitaryGroupOrderAcceptance *order = nullptr;
  PointI entry_point_dm;
  std::optional<Integer> incoming_facing_mdeg;
  FieldBattleTime entry;
};
// Verifies one actual S1 stopped prefix. This is not admission or a relation
// decision; Field later records its separate actual contact/application head.
Outcome<FieldBoundaryOriginalRead> read_field_boundary_original_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &actual_movement_boundary, ExactArithmeticBudget = {});
} // namespace hanmo::resident
