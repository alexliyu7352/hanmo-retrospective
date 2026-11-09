// 成功原休养/治疗终局反扫；中断不发点，long_rest仍走旧逐期身体规则。
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct CharacterInjurySourcesStateV1;
// 显式原模块选择与原目录必须一致；未选不是空治疗provider。
Status validate_injury_recovery_selection_v1(const CharacterConditionSources &);
Status validate_injury_service_source_coverage_v1(
    const CharacterConditionSources &, const CharacterInjurySourcesStateV1 &);
} // namespace hanmo::resident
