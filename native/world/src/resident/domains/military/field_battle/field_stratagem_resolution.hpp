#pragma once
#include "field_ability_sources.hpp"
#include "field_world.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
// True council adoption, before any Running view may expose the formation.
Status install_field_adopted_stratagems_in_candidate(
    CommonStore &, FieldBattleWorldState &, const StableId &adopted_fact_id,
    const GeneratedBattleField &, const FieldBattleAbilityReadContext &,
    CheckpointMemoryCharge &);
// Original adopted-source records only. Call at arrivals, then again after
// actual movement for concealment reveal; resolved records never re-roll.
Status resolve_field_stratagems_in_candidate(
    CommonStore &, FieldBattleWorldState &, const StableId &battle,
    const GeneratedBattleField &, const FieldBattleAbilityReadContext &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
// Same true records after movement; a newly revealed source is stamped at
// the actual completed movement boundary, not before this step's Contact.
Status reveal_field_stratagems_after_motion_in_candidate(
    CommonStore &, FieldBattleWorldState &, const StableId &battle,
    const GeneratedBattleField &, const FieldBattleAbilityReadContext &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
} // namespace hanmo::resident

namespace hanmo::resident {
Status validate_field_stratagem_sources(const CommonStore &,
                                        const FieldBattleWorldState &,
                                        const FieldBattleSession &,
                                        const GeneratedBattleField &,
                                        const FieldBattleAbilityReadContext &);
}
