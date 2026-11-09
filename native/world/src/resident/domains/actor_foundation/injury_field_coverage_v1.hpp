// 野战每段 wounded 真来源双向覆盖；死亡由原生命所有者另验。
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct CharacterInjurySourcesStateV1;
struct MilitaryState;
struct FieldBattleWorldState;
Status validate_field_injury_source_coverage_v1(
    const CharacterConditionSources &, const CharacterInjurySourcesStateV1 &,
    const MilitaryState &, const FieldBattleWorldState &);
} // namespace hanmo::resident
