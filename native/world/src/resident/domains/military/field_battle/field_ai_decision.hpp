// 原 BW06 §6.2 有限场内判断；不是全局 NPC Director，不读取敌方未到达军令。
#pragma once
#include "field_view_model.hpp"
namespace hanmo::resident {
struct FieldAiMissionDecision {
  StableId formation_id;
  FieldMission mission;
  bool pursuit = false;
};
struct FieldAiDecisionRead {
  bool evaluated = false;
  std::vector<FieldAiMissionDecision> missions;
};
// 敌情只读真正本侧视图；session 仅借本侧初始名册、原采纳方案和计时状态。
// 返回决策尚未发令；唯一 driver 必须走原 Direct/Order 权限、延迟与来源写者。
Outcome<FieldAiDecisionRead>
read_field_ai_mission_decisions(const FieldBattleSession &,
                                const FieldBattleView &,
                                const GeneratedBattleField &, Integer side);
} // namespace hanmo::resident
