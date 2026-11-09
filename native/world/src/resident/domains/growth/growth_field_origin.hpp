// Field原contact阶段只放宽真实from→to的观察上界，不改变身份/属性来源算法。
#pragma once
#include "growth_attribute_origin.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 供同域属性叶借原身份内核，不是永久XP历史/新技能成长入口。
// actual_session非空时必须借同Common的真实field_battle_session原头。
Outcome<GrowthSkillOrigin> read_growth_skill_origin_at_field_contact(
    const CommonStore &, const GrowthSkillOriginContext &,
    const StableId &actor, const StableId &skill, const DynamicStepInput &,
    const FactRecord *actual_session = nullptr);
// 五属性仍取原author/creation/birth，并核今日不可变值未被未登记写者改动。
// 观察时间明确是phase.to，不将其偷改from；原出生/创建事实必须在真正session前。
Outcome<GrowthAttributeOrigin> read_growth_attribute_origin_at_field_contact(
    const CommonStore &, const GrowthSkillOriginContext &,
    const StableId &actor, const DynamicStepInput &,
    const FactRecord *actual_session = nullptr);
} // namespace hanmo::resident
