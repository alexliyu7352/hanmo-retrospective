// LC01 原计划只保存真实来源地址。此叶不新增计划状态，也不代替采用后全来源校验。
#pragma once
#include "resident/domains/director/director_subject2.hpp"
namespace hanmo::resident {
class DirectorActorCommandEffectRead;
struct InteractionState;
// 当前真实旁支 A/B 支的有限形状；其它 LC01 owner 尚缺时明确拒绝，不空过。
Status validate_family_director_plan_fields5(const CommonStore &,
                                             const DirectorCatalog &,
                                             const FinitePlan &);
// 原 actor effect 只读已封 result/history，不回未来 adoption 或 P10。
// 实际 B 必须交叉指回同一 A，不能两个各自合法的源拼成一条计划。
Status validate_family_director_plan_source5(
    const CommonStore &, const DirectorState &, const DirectorCatalog &,
    const InteractionState &, const CatalogIdentity &actual_identity,
    const FinitePlan &, const DirectorActorCommandEffectRead &);
// 调用者须已核本次真实执行路由/seq。借回原 actual_binding 中的 A ID，
// 首写绝不预分配，B 不覆为自己的 ID。非 Family typed 命令明确拒绝。
Outcome<const StableId *>
read_family_director_applied_goal5(const CommonStore &,
                                   const SubjectDecisionProposal2 &,
                                   const SubjectDecisionApplied2 &);
} // namespace hanmo::resident
