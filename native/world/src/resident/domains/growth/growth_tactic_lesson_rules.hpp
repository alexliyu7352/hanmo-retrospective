// 原Action与本域永久课时同候选更新；所有in_candidate入口由外层完整保存点统一回滚。
#pragma once
#include "growth_tactic_lesson_source.hpp"
#include "growth_tactic_lesson_resources.hpp"
#include "resident/domains/family/family_death.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonInterruption {StableId actor_id,cause_fact_id;Integer tick=0;};
struct GrowthTacticLessonRuntimeAccess {
    FamilyDeathOriginRead deaths;
    // 只允许原Military真实俘获/入战源；当前不可用或移动本身不足以生成历史中断。
    std::function<Outcome<std::optional<GrowthTacticLessonInterruption>>(const GrowthTacticLessonStart&,const StableId& started_fact_id)> interruption;
};
struct GrowthTacticLessonEffect {StableId action_id,started_fact_id,source_fact_id,reason;bool completed=false,success=false;Integer provided_half_days=0,fee_paid=0,fee_released=0;};
// 新建唯一原Action，双方共同占用；实际等待对白由World沿这个action绑定，不另建会话。
Outcome<StableId> start_growth_tactic_lesson_in_candidate(CommonStore&,GrowthTacticLessonState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonSourceAccess&,const GrowthTacticLessonQuote&);
// 中央actions_step只对原Action减一；clock_commit后本口按实际to与剩余值核唯一课时。
// 先真实死亡/军事中断，后付款与课时；结束后由中央唯一release_action清占用并恢复原对白。
Outcome<GrowthTacticLessonEffect> finish_growth_tactic_lesson_half_day_in_candidate(CommonStore&,GrowthTacticLessonState&,const GrowthTacticLessonRuntimeAccess&,const StableId& started_fact_id,const DynamicStepInput& completed_step);
Outcome<GrowthTacticLessonEffect> cancel_growth_tactic_lesson_in_candidate(CommonStore&,GrowthTacticLessonState&,const StableId& actor,const StableId& started_fact_id);
// 零时命令造成的真实死亡也必须释放本原Action，后续实际result绑定与Plan消费仍走原提交尾。
Outcome<GrowthTacticLessonEffect> close_growth_tactic_lesson_after_death_in_candidate(CommonStore&,GrowthTacticLessonState&,const GrowthTacticLessonRuntimeAccess&,const StableId& started_fact_id);
}
