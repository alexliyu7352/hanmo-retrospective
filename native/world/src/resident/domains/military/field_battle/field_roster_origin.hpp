// Original military history reducer at the actual participation prefix.
// No persisted Field copy of the original military roster is introduced.
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct MilitaryExactState;
struct MilitaryUnitRecord;
struct MilitaryCatalog;
struct PopulationMilitaryState;
struct PopulationMilitaryCatalog;
class MilitaryFormationSourceRead;
class MilitaryReturnSourceRead;
class ExactTrainingPeriodRead;
class CurrentMomentRead;
class FieldSegmentApplicationRead;
struct FieldOriginalRosterReadContext {
  const CommonStore &common;
  const MilitaryState &military;
  const MilitaryExactState *exact;
  const CatalogIdentity &identity;
  const MilitaryFormationSourceRead &formations;
  const MilitaryReturnSourceRead &returns;
  const ExactTrainingPeriodRead &periods;
  const CurrentMomentRead &clock;
  ExactArithmeticBudget budget;
  const PopulationMilitaryState *population = nullptr;
  const PopulationMilitaryCatalog *population_catalog = nullptr;
  const FieldSegmentApplicationRead *field_applications = nullptr;
};
// All references borrow the original Complete military owners; no Naval or
// dynamic battle module is selected merely to read this original history.
Outcome<MilitaryUnitRecord> read_field_original_participation_roster(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldFormation &, const FieldOriginalRosterReadContext &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Replays only this battle's actual ordered local casualty/rout/withdrawal
// outputs from the original participation roster. It does not advance RNG or
// world time and does not replace combat/contact qualification validation.
Status validate_field_local_roster_outputs(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldFormation &, const FieldOriginalRosterReadContext &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 只读原参与名册与已存在局部输出的指定前缀；不重放战斗、RNG或写今日名册。
Outcome<MilitaryUnitRecord> read_field_local_roster_at_event_prefix(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldFormation &, const FieldOriginalRosterReadContext &,
    Integer last_event_sequence);
} // namespace hanmo::resident
