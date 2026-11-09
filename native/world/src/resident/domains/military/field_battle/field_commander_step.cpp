#include "field_commander_step.hpp"
#include "field_ability_sources.hpp"
#include "field_commander_change_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<FieldCommanderPulseRead> apply_field_commander_step_in_candidate(
    CommonStore &common, const MilitaryState &military,
    FieldBattleWorldState &world, const StableId &battle,
    const GeneratedBattleField &field, const FieldPulseRead &pulse,
    const FieldBattleWindowSlice &step,
    const FieldBattleAbilityReadContext &abilities,
    const FieldSegmentApplicationRead &clock, CheckpointMemoryCharge &owner) {
  if (!clock.bound_to(common, military, world))
    return {false,
            {},
            common_failure("FieldCommanderChangeWorldMismatch", battle).error};
  auto actual = world.sessions.find(battle);
  if (actual == world.sessions.end())
    return {false, {}, common_failure("FieldBattleNotFound", battle).error};
  auto outcome = apply_field_commander_pulse_in_candidate(actual->second, field,
                                                          pulse, step, owner);
  if (!outcome.ok)
    return outcome;
  // Source order follows the original formation-order exposure pass. Only a
  // real original-primary Lost with a still-serving predesignated deputy
  // creates a replacement; no secondary death ledger or inherited skill set.
  for (const auto &id : outcome.value.lost_formations) {
    const auto &f = actual->second.formations.at(id);
    if (!f.commander_lost_at || f.commander_lost_at->tick != step.to.tick ||
        f.commander_lost_at->t != step.to.t)
      continue;
    auto source = record_field_commander_change_source_in_candidate(
        common, military, world, battle, id, step.to, clock, owner);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value)
      continue;
    auto applied = apply_field_commander_ability_replacement_in_candidate(
        common, abilities, world, *source.value, field.parameters, owner);
    if (!applied.ok)
      return {false, {}, applied.error};
  }
  return outcome;
}
} // namespace hanmo::resident
