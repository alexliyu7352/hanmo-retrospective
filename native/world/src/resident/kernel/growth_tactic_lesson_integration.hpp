// 教学只借同候选原人物、军事和会话权威；工厂持值回调，不能跨World clone保存。
#pragma once
#include "resident/domains/growth/growth_tactic_lesson_history.hpp"
#include "resident/domains/growth/growth_tactic_lesson_commands.hpp"
#include "resident/domains/growth/growth_tactic_lesson_military.hpp"
#include "resident/domains/growth/growth_tactic_lesson_views.hpp"
namespace hanmo::resident {
struct WorldBundle;struct ReceiptDetails;struct AdvanceDetails;
// 完整旧人物/课程/军史先验；不递归军事晚教学来源或整World。
Status validate_growth_tactic_lessons_world(const WorldBundle&);
Outcome<GrowthTacticLessonCommandQuote> preview_growth_tactic_lesson_world(const WorldBundle&,const StableId&,const GrowthTacticLessonCommand&);
Outcome<ReceiptDetails> apply_growth_tactic_lesson_world(WorldBundle&,const StableId&,const GrowthTacticLessonCommand&,const GrowthTacticLessonCommandQuote&);
Outcome<BoundaryValue> query_growth_tactic_lesson_world(const WorldBundle&,Integer page);
// actions_step只减剩余值；clock后且真实死亡覆盖已登记才结本轮课时和军事唯一取得。
Status finish_growth_tactic_lessons_world(WorldBundle&,const DynamicStepInput& committed_step,AdvanceDetails&);
Status finish_growth_tactic_lesson_boundary_world(WorldBundle&);
Status bind_growth_tactic_lesson_result_world(WorldBundle&,const GrowthTacticLessonState* before,const StableId&,Integer,const CommitResult&,Integer from_tick);
// 禁止通用release丢弃尚未关闭的预约/承诺；必须先产生精确本域closure。
Status guard_growth_tactic_lesson_release(const WorldBundle&,const StableId& action_id);
}
