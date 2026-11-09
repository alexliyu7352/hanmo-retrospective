// 原成果稳定子键；不分配第二成果ID或复制XP。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct TrainingResultRef {StableId action_id;};
struct StudyLessonResultRef {StableId action_id;Integer lesson_ordinal=0;};
struct StudyPerformanceAdjustmentRef {StableId action_id;Integer lesson_ordinal=0;};
struct GroupLessonResultRef {StableId course_id;Integer period_ordinal=0;};
struct PracticeAwardRef {StableId award_id;};
struct AttributeMilestoneRef {StableId milestone_result_id;};
struct GrowthTradeGrantRef {StableId action_id;};
using LifeGrowthContributionRef=std::variant<TrainingResultRef,StudyLessonResultRef,StudyPerformanceAdjustmentRef,GroupLessonResultRef,PracticeAwardRef,AttributeMilestoneRef,GrowthTradeGrantRef>;
}
