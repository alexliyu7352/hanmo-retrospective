// 原逐课XP的净贡献窄读，供同一ReadSkillQualificationV2合并；不重授、不镜像XP。
#pragma once
#include "study_lesson_capability.hpp"
#include "study_adjustment_time2.hpp"
namespace hanmo::resident {
struct StudyLessonPermanentContributionView {
 const StudyFact* fact=nullptr;const StudyLesson* lesson=nullptr;const LessonCapabilitySource* original_capability_source=nullptr;
 Integer xp_before=0,xp_gained=0,xp_after=0;WorldMoment effective_at;
 // 原唯一来源表的插入序，仅同一原逐课writer域内可作因果顺序；不是ID排序。
 std::size_t actual_source_ordinal=0;
};
Outcome<StudyLessonPermanentContributionView> read_study_lesson_permanent_contribution(const CharacterConditionSources&,const GrowthState&,const StudyLessonResultRef&);
struct StudyAdjustmentPermanentContributionView {StudyAdjustmentSourceView source;Integer xp_before=0,xp_gained=0,xp_after=0;};
Outcome<StudyAdjustmentPermanentContributionView> read_study_adjustment_permanent_contribution(const CommonStore&,const GrowthState&,const GrowthCatalog&,const StudyPerformanceAdjustmentRef&,const WorldMoment& actual_now);
}
namespace hanmo::resident {
// 只验证这两种真实Study贡献的原net链；不冒六分支完整source2，其他writer由总reader合并。
struct StudyVerifiedSkillNet {Integer initial_xp=0,study_net=0,current_xp=0;};
Outcome<StudyVerifiedSkillNet> read_verified_study_skill_net(const CharacterConditionSources&,const GrowthState&,const StableId& actor_id,const StableId& skill_id);
}
