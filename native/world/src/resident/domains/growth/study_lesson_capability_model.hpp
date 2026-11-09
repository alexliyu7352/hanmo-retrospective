// v1b§5原逐课来源值；同一Growth持有，值不含XP副本或新进度。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "growth_fixed_service_model.hpp"
namespace hanmo::resident {

// F课堂引用原FSL历史，绝不把F窗冒充整数WorkPeriodRef/1。
struct StudyFixedLessonRef {StableId action_id;Integer lesson_ordinal=0;SourceRef original_fsl_source;Integer source_schema=1;};
struct FrozenLessonCapability {
 StableId actor_id;ActorFoundationRuleRef purpose_ref;Integer condition_revision=0;StableId origin_ref;
 std::vector<StableId> attribute_milestone_refs;
 std::vector<CharacterConditionSourceRef> condition_source_refs;
 std::vector<CharacterEfficiencyComponent> registered_efficiency_components;
 Integer frozen_efficiency_bp=0;ActorFoundationRuleRef admission_rule_ref;
};
struct LessonCapabilitySource {
 StableId action_id;Integer lesson_ordinal=0;StudyFixedLessonRef work_period_ref;
 WorldMoment provided_from,provided_to;FrozenLessonCapability student_capability;
 std::optional<FrozenLessonCapability> teacher_capability;ActorFoundationRuleRef xp_rule_ref;
 std::optional<LifeOwnedFieldRef> innate_source_ref;std::vector<SourceRef> fatigue_effect_use_refs;
 ConditionReceiptRefV2 student_condition_receipt_ref;std::optional<ConditionReceiptRefV2> teacher_condition_receipt_ref;
 StudyLessonResultRef original_lesson_result_ref;
};
// 按原action和ordinal定位，不分配第二课时/成长身份。

}
