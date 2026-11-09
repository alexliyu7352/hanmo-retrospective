// 教学真实完工只向原军事掌握账提供来源；不另存战法列表、XP或军战资源。
#pragma once
#include "growth_tactic_lesson_receipts.hpp"
#include "resident/domains/military/military_tactic_learning_contract.hpp"
namespace hanmo::resident {
// 完整教学报名/课时/中断史及实际结果绑定先验；不回调S2完整习得验证。
Outcome<MilitaryTacticTeachingCompletion> read_growth_tactic_teaching_completion(const CommonStore&,const GrowthTacticLessonState&,const StableId& exact_closed);
// 唯一热用：clock_commit后，本次课时函数返回成功closed后立即登记S2，实际结果尚未生成。
// 调用者持同候选完整事务且传真正新增closed；不可用于冷档或旧未绑定载荷恢复。
Outcome<MilitaryTacticTeachingCompletion> read_growth_tactic_teaching_completion_after_half_day(const CommonStore&,const GrowthTacticLessonState&,const StableId& exact_closed,const DynamicStepInput& committed_step);
using GrowthTacticTeachingAcquiredRead=std::function<Outcome<MilitaryTacticLearningObservation>(const StableId& exact_closed)>;
// 完整S2晚习得源验之后再反查每个成功closed，精确来源必须唯一落账；当前已会不代这个因果。
Status validate_growth_tactic_teaching_acquisition_coverage(const CommonStore&,const GrowthTacticLessonState&,const GrowthTacticTeachingAcquiredRead&);
}
