// Actual source-bound episode/effect reads. No inferred effect, expiry or
// grade.
#pragma once
#include "model.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct VerifiedBattleInjuryEffectV1 {
  const ConditionEpisodeV2 *episode = nullptr;
  const ConditionReceiptV2 *origin = nullptr;
  const ConditionEffectContribution *contribution = nullptr;
  const InjuryEffectItem *effect = nullptr;
};
Outcome<VerifiedBattleInjuryEffectV1>
read_battle_injury_effect_source_v1(const CharacterConditionSources &,
                                    const StableId &contribution_key);
} // namespace hanmo::resident
namespace hanmo::resident {
// Complete source/episode/permanent-contribution closure for the admitted map2
// flat permanent profile. It never admits unresolved recovery/lifecycle tags.
Status
validate_battle_injury_episode_sources_v1(const CharacterConditionSources &);
} // namespace hanmo::resident
