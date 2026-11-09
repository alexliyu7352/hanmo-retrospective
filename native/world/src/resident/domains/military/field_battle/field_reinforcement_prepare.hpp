// Actual pending admission inputs. This does not install a formation or invent
// a placement/mission when the complete entry producer has not yet run.
#pragma once
#include "field_boundary_source.hpp"
#include "field_session.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
struct FieldBattleBoundaryRecord;
struct FieldReinforcementPreparation {
  CheckpointMemoryCharge copied_formations;
  const FieldBattleBoundaryRecord *boundary = nullptr;
  const FieldBattleSession *session = nullptr;
  const FieldPendingEntry *pending = nullptr;
  FieldBoundaryOriginalRead original;
  WorldMoment actual_at;
  std::vector<FieldFormation> original_formations;
};
Outcome<FieldReinforcementPreparation>
prepare_field_reinforcement_at_original_entry(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &actual_field_contact,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
