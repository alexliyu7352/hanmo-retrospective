// 军议的同一热冷结构门；不读 UI，也不从保存内容创建缺失任职或将领。
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
class FamilyLifeHistoryRead4;
// 仅在原 Session/名册/生成格已核后调用，计划可未定案，但闭议必须完整。
Status validate_field_council_records(const CommonStore &,
                                      const FieldBattleSession &,
                                      const GeneratedBattleField &);
// Common 头与本域事实互相覆盖；不允许未实现事件种类伪装成已落业务事实。
Status validate_field_council_facts(
    const CommonStore &, const FieldBattleWorldState &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
// 情报三级必须确为对方已采纳的真实计划，不能只是同形的另一份建议。
bool same_field_council_plan(const FieldCouncilPlan &,
                             const FieldCouncilPlan &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Adoption-time assignment eligibility only. Terrain/scout/intellect determine
// success afterwards and must not turn a paid failed attempt into a free void.
bool field_council_stratagem_assignment_eligible(
    const FieldCouncilStratagemPlan &, const FieldCouncilPlan &);
} // namespace hanmo::resident
