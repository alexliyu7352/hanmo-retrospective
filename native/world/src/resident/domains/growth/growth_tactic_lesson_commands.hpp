// 本人原命令入口；报名只选择已在当前会话的真实教师提议，取消不替教师作新决定。
#pragma once
#include "growth_tactic_lesson_rules.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonCommand {StableId kind,definition_id,started_fact_id;};
struct GrowthTacticLessonCancellationQuote {WorldIdentity source_identity;StableId actor_id,started_fact_id,action_id;Integer provided_half_days=0,fee_paid=0,fee_released=0;};
struct GrowthTacticLessonCommandQuote {StableId kind;std::optional<GrowthTacticLessonQuote> enrollment;std::optional<GrowthTacticLessonCancellationQuote> cancellation;};
Outcome<GrowthTacticLessonCommand> decode_growth_tactic_lesson_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_growth_tactic_lesson_command(const GrowthTacticLessonCommand&);
Outcome<GrowthTacticLessonCancellationQuote> preview_growth_tactic_lesson_cancellation(const CommonStore&,const GrowthTacticLessonState&,const StableId& actor,const StableId& actual_start);
Status revalidate_growth_tactic_lesson_cancellation(const CommonStore&,const GrowthTacticLessonState&,const GrowthTacticLessonCancellationQuote&);
}
