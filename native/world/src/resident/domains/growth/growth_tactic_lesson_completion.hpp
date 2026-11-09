// 教学完成只读精确原Action与永久终态；没有Action绝不推定成功。
#pragma once
#include "growth_tactic_lesson_codec.hpp"
namespace hanmo::resident {
struct ActionCompletion;
struct GrowthTacticLessonCompletion {
 StableId source_fact_id,started_fact_id,action_id,actor_id,provider_actor_id,reason,code;
 Integer tick=0,provided_half_days=0,fee_paid=0,fee_released=0;
 bool success=false;
};
// Plan只借具名原工单；保存原报价中的教师say/session/context，不拿后来会话替代。
struct GrowthTacticLessonActionSource {
 WorldIdentity source_identity;
 StableId started_fact_id,action_id;
 GrowthTacticLessonAgreement agreement;
 std::optional<GrowthTacticLessonCompletion> completion;
};
// 完整Lesson及actual-result史先验；exact_action必须唯一属于student。缺Action只在有唯一真实closure时合法。
Outcome<GrowthTacticLessonActionSource> read_growth_tactic_lesson_action_source(const CommonStore&,const GrowthTacticLessonState&,const StableId& student,const StableId& exact_action);
// 调用前完整报名/课时/中断史；这里不反入World、不核S2晚习得而形成递归。
Outcome<GrowthTacticLessonCompletion> read_growth_tactic_lesson_completion(const CommonStore&,const GrowthTacticLessonState&,const StableId& exact_closure);
// 新领域只使用原plain外层完成字段，所有存在位与冷热JSON一致；拒混入任何别域效果。
Status validate_growth_tactic_lesson_action_completion(const GrowthTacticLessonCompletion&,const ActionCompletion&);
}
