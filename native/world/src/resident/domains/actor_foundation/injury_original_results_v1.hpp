// Original result checks, not a second progress or effective-attribute owner.
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct CharacterInjurySourcesStateV1;
struct ConditionReceiptV2;
struct PersonalState;
struct PersonalCatalog;
struct MilitaryState;
struct FieldBattleWorldState;
// Each accepts only the actual receipt/root identities supplied by the world
// owner. These prove original result M and source-specific math, not the earned
// points BEFORE this receipt: full replay must independently establish that
// prefix, as well as rule/episode/effect/capacity and raw-source reverse
// coverage.
Status validate_personal_injury_original_result_v1(
    const CharacterConditionSources &, const CharacterInjurySourcesStateV1 &,
    const ConditionReceiptV2 &, const PersonalState &, const PersonalCatalog &,
    Integer selected_lifecycle);
Status validate_field_injury_original_result_v1(
    const CharacterConditionSources &, const CharacterInjurySourcesStateV1 &,
    const ConditionReceiptV2 &, const MilitaryState &,
    const FieldBattleWorldState &);
Status validate_treatment_injury_original_result_v1(
    const CharacterConditionSources &, const CharacterInjurySourcesStateV1 &,
    const ConditionReceiptV2 &);
// 普通休养原服务与已定保点规则；long_rest不走此入口。
Status
validate_rest_injury_original_result_v1(const CharacterConditionSources &,
                                        const CharacterInjurySourcesStateV1 &,
                                        const ConditionReceiptV2 &);
} // namespace hanmo::resident
