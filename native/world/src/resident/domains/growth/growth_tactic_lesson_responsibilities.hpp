// 原学费承诺的私人责任读；死亡或暂时无余额都不能消灭未结教学预约。
#pragma once
#include "growth_tactic_lesson_receipts.hpp"
namespace hanmo::resident {
struct GrowthTacticLessonResponsibilities {WorldIdentity source_identity;StableId actor_id,before_fact_id;Integer observed_tick=0;std::vector<StableId> pending_source_ids;};
// 完整教学史/实际结果绑定先验。任一方未终止原课都阻断，包含零学费的本人履约责任。
Outcome<GrowthTacticLessonResponsibilities> read_growth_tactic_lesson_responsibilities(const CommonStore&,const GrowthTacticLessonState&,const StableId& actor);
// 仅原family_inheritance:/family_inheritance_settled/family_inheritance；调用者先验实际payload.tick。
// 后来闭课不能倒清早期责任，后来开课也不倒填旧交割；不读取今日钱包推断清偿。
Outcome<GrowthTacticLessonResponsibilities> read_growth_tactic_lesson_responsibilities_before_inheritance(const CommonStore&,const GrowthTacticLessonState&,const StableId& actor,Integer tick,const StableId& actual_inheritance);
}
