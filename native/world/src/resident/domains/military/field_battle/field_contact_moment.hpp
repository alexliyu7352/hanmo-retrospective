#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct FieldBattleWorldState;
struct FieldBattleCatalog;
struct FieldExactContactReadContext;
struct FieldBattleContactRecord;
struct FactRecord;
struct FieldInitialContactMomentRead {
  const FieldBattleContactRecord *field_contact = nullptr;
  const FactRecord *actual_field_contact_head = nullptr;
  const SourceRef *original_movement_contact = nullptr;
  const WorldMoment *occurred_at = nullptr;
};
// Initial exact S1 two-Army contact only. Does not read P3 or runtime control,
// so a before-contact politics consumer cannot recurse into its own state.
// A legacy ratio or pending boundary contact is not a substitute for this
// actual retained S1 application moment.
Outcome<FieldInitialContactMomentRead> read_field_initial_contact_actual_moment(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldBattleCatalog &, const SourceRef &actual_field_contact,
    const FieldExactContactReadContext &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
