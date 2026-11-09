// Source-only injury/earned-point projection, not full Condition authorization.
#pragma once
#include "actor_foundation_refs.hpp"
#include "resident/domains/growth/growth_recovery_progress.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct CharacterInjurySourcesStateV1;
struct ActorConditionHead;
struct PersonalState;
struct FieldBattleWorldState;
struct CharacterInjuryReplaySourcesV1 {
  const CharacterConditionSources &conditions;
  const CharacterInjurySourcesStateV1 &injury;
  const PersonalState *personal = nullptr;
  const FieldBattleWorldState *field = nullptr;
};
// Replays from authentic Origin2 initial injury and zero earned points, through
// the exact original Condition prefix. No today's-body difference inference.
// Real source result/math and source once links are checked; callers still need
// full body/episode/capacity validation and reverse original-source coverage.
// 此投影不能单独授权身体写入；野战必须接10-27已定每段原来源后继。
Outcome<GrowthRecoveryProgress> read_character_injury_progress_at_head_v1(
    const CharacterInjuryReplaySourcesV1 &, const ActorConditionHead &,
    const WorldMoment &actual_boundary);
} // namespace hanmo::resident
