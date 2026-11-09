// True Field start source for P3; never queries current diplomacy to prove itself.
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
class FieldSegmentApplicationRead;
class MilitaryFormationSourceRead;
struct FieldBattleStartedSourceRead {
  const FieldBattleSession *session = nullptr;
  const SourceRef *actual_source = nullptr;
  const WorldMoment *started_actual_at = nullptr;
  std::array<const StableId *, 2> factions{};
  Integer source_schema = 1;
};
Outcome<FieldBattleStartedSourceRead> read_field_battle_started_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &, const FieldSegmentApplicationRead &,
    const MilitaryFormationSourceRead &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
