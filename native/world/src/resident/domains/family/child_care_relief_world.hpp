// 原World的具名照护接手组合口；本域只写自己的有限责任与同一Common的真实私粮/Action。
#pragma once
#include "child_care_relief_commands.hpp"
#include "child_care_relief_completion.hpp"
#include "child_care_relief_exclusion.hpp"
#include "child_care_relief_responsibilities.hpp"
namespace hanmo::resident {
struct WorldBundle;struct AdvanceDetails;struct ActionCompletion;
bool child_care_relief_command_kind(const StableId&);
Status validate_child_care_relief_fact_header(const ChildCareReliefState&,const FactRecord&);
// 完整Birth/Life及军事来源先验，不递归World；严核显式profile、缓存和实际新分配来源。
Status validate_child_care_relief_world(const WorldBundle&);
// clock_commit和FamilyLife死亡之后消费本轮终态；零时取消也走同一个释放/等待对白口。
Status finish_child_care_relief_world(WorldBundle&,AdvanceDetails&);
Status release_completed_child_care_relief(WorldBundle&,const StableId& action_id,const ActionCompletion&);
}
