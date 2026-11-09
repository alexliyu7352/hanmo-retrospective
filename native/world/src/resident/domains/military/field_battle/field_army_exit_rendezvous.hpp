// 10-31整军单口修订的几何叶：终局后沿原A*/步速/碰撞读一步，不改变胜负时刻。
#pragma once
#include "field_navigation.hpp"
namespace hanmo::resident {
// selected_exit须由整军原选择owner证明；本叶仅核它确属本场该侧出口。
// 返回几何结果不是选择授权或实际移动事实；不写Formation、Common、S1或result.ended。
// 已真正Withdrawn者严格拒绝，不能借汇合撤销旧Exit或把其原世界位置拉回战场。
Outcome<FieldNavigationStep> read_field_post_result_exit_step(
    const FieldBattleSession &, const GeneratedBattleField &,
    const StableId &formation_id, std::size_t selected_exit,
    const FieldBattleWindowSlice &actual_step);
} // namespace hanmo::resident
