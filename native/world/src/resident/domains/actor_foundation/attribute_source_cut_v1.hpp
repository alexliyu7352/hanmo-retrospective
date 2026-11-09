// Real source composition inputs, not a full capability permission. Temporary
// professional effects/recovery are separate original owners, never defaulted.
#pragma once
#include "battle_injury_episode_v1.hpp"
#include "created_origin.hpp"
namespace hanmo::resident {
struct GrowthPermanentSources;
struct AttributeMilestone;
class CharacterAttributeComponentVisitorV1 {
public:
  virtual ~CharacterAttributeComponentVisitorV1() = default;
  virtual Status initial(const CharacterInitialValuesV2 &) = 0;
  virtual Status permanent_growth(const AttributeMilestone &) = 0;
  virtual Status permanent_injury(const VerifiedBattleInjuryEffectV1 &) = 0;
};
// Business owner proves the actual consumer payload/actor/M first. This reader
// proves complete original base, Growth milestone and admitted source1
// permanent injury inputs strictly before that actual Common header. All
// callbacks borrow existing owned rows. Retain only a private result until the
// final Status succeeds. It does not authorize the consumer, erase temporary
// effects or release injury.
Status visit_character_base_growth_injury_before_fact_v1(
    const CharacterConditionSources &, const GrowthPermanentSources &,
    const StableId &actor, const SourceRef &actual_consumer,
    const WorldMoment &actual_consumer_at,
    CharacterAttributeComponentVisitorV1 &);
class CurrentMomentRead;
// Genuine current same-candidate read for preview, not a fabricated consumer
// header or historical cut. The original S0 clock / Common prefix must stay
// unchanged throughout; partial visitor outputs remain unpublished on failure.
Status visit_current_character_base_growth_injury_v1(
    const CharacterConditionSources &, const GrowthPermanentSources &,
    const CurrentMomentRead &, const StableId &actor,
    CharacterAttributeComponentVisitorV1 &);
} // namespace hanmo::resident
