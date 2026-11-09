// 显式新模块的严格冷入口；独立尾与原Common头/Action参数双向对应。
#pragma once
#include "growth_tactic_lesson_commands.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status emit_growth_tactic_lesson_fact_payloads_fields(ProjectionWriter&,const CommonStore&,const GrowthTacticLessonState*);
Status emit_growth_tactic_lesson_action_payloads_fields(ProjectionWriter&,const CommonStore&,const GrowthTacticLessonState*);
inline constexpr const char* growth_tactic_lesson_module="growth_tactic_lessons";
Outcome<bool> decode_growth_tactic_lesson_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_growth_tactic_lesson_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const GrowthTacticLessonCatalog*,const GrowthTacticLessonState*);
Outcome<BoundaryValue> growth_tactic_lesson_fact_payloads(const CommonStore&,const GrowthTacticLessonState*);
Outcome<BoundaryValue> growth_tactic_lesson_action_payloads(const CommonStore&,const GrowthTacticLessonState*);
Status validate_growth_tactic_lesson_json_payloads(const CommonStore&,const GrowthTacticLessonState*,const BoundaryValue& original_common_json);
BoundaryValue encode_growth_tactic_lesson_quote(const GrowthTacticLessonCommandQuote&);
}
