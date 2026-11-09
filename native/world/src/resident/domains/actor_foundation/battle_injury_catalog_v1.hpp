// Original scenario.actor_foundation author port, explicit source1 successors.
// Selection does not install an episode or authorize a body write.
#pragma once
#include "model.hpp"
namespace hanmo::resident {
bool battle_injury_source1_selected(const CatalogIdentity &) noexcept;
Status validate_battle_injury_catalog_v1(const ActorFoundationCatalog &);
struct CharacterConditionSources;
struct CharacterBattleReceiptSourceV1;
struct PersonalState;
struct FieldBattleWorldState;
struct SelectedBattleInjurySourceV1 {
  const OriginalBattleInjuryRuleV1 *rule = nullptr;
  const InjuryCapabilityMapEntry *mapping = nullptr;
  const WorldMoment *occurred_at = nullptr;
  Integer incoming_injury = 0;
};
Outcome<SelectedBattleInjurySourceV1> read_selected_battle_injury_source_v1(
    const CharacterConditionSources &, const PersonalState *,
    const FieldBattleWorldState *, const CharacterBattleReceiptSourceV1 &,
    const StableId &actor_id);
} // namespace hanmo::resident

namespace hanmo::resident {
Status
validate_battle_injury_selected_result_v1(const CharacterConditionSources &,
                                          const ConditionReceiptV2 &);
}
