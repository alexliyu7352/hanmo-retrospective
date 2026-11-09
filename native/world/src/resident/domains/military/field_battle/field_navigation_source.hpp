#pragma once
#include "field_navigation.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
class CommonStore;
struct MilitaryState;
struct FieldBattleWorldState;
struct FieldBattleAbilityReadContext;
// Target comes only from the original mission/current actual local prefix.
// No caller-supplied target or replacement mission is accepted.
Status observe_field_navigation_target_in_candidate(
    const FieldBattleWorldState &, FieldBattleSession &, FieldFormation &, const GeneratedBattleField &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
// Internal exit of the original sole move writer, not an arbitrary position
// setter.
Status record_field_original_motion_application(FieldBattleSession &,
                                                FieldFormation &,
                                                const GeneratedBattleField &,
                                                const FieldBattleWindowSlice &,
                                                const FieldNavigationStep &,
                                                CheckpointMemoryCharge &);
Status validate_field_navigation_sources(const CommonStore &,
                                         const MilitaryState &,
                                         const FieldBattleWorldState &,
                                         const FieldBattleSession &,
                                         const GeneratedBattleField &,
                                         const FieldBattleAbilityReadContext &);
} // namespace hanmo::resident
