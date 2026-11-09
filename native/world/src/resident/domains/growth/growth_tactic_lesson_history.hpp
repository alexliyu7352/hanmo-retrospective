// 冷史按原Common事实序重验真实授课；资格、死亡和军事占用各借原领域，不倒读今日位置。
#pragma once
#include "growth_tactic_lesson_rules.hpp"
#include "growth_tactic_lesson_receipts.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonHistoryAccess {
 const GrowthTacticLessonSourceAccess& enrollment;
 FamilyDeathOriginRead deaths;
 // 仅原具名Military历史空闲读；World核双方、实际before头/tick与history_complete。
 // 原开课当面地点已独立核实；任何不可证占用/位置变化必须错误，不拿今日空闲倒填。
 std::function<Status(const GrowthTacticLessonStart&,const StableId& started_fact_id,Integer tick,const StableId& actual_before)> period_before;
 std::function<Outcome<std::optional<GrowthTacticLessonInterruption>>(const GrowthTacticLessonStart&,const StableId& started_fact_id,Integer tick,const StableId& actual_before)> interruption_before;
};
// 完整Common/Interaction/Family/Creation/Birth/Life/Growth及Military早结构先验；不反入S2晚教学来源。
// 稳定World冷/热后验，包含所有实际result绑定；S2晚习得与成功源反向coverage在它之后。
Status validate_growth_tactic_lesson_history(const CommonStore&,const InteractionCatalog&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonState&,const GrowthTacticLessonHistoryAccess&);
}
