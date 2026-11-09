// 师承学习页只投影原 Growth 永久课程；没有第二份 XP、费用、课时或教师能力账。
#pragma once
#include "family_apprenticeship.hpp"
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
struct ApprenticeshipCourseRead {
    StableId study_fact_id,phase,code;
    Integer start_tick=0,completed_tick=-1,actual_lessons=0,credited_xp=0;
};
struct ApprenticeshipCoursesPage {
    WorldIdentity source_identity;
    StableId observer_actor_id,bond_id,teacher_actor_id,student_actor_id,skill_id;
    Integer page=0,total=0;
    std::vector<ApprenticeshipCourseRead> rows;
};
// 须已完整验证本候选 Growth 与师承/提交源。只含关系成立后、退出前真正开工的原课；
// 退出不取消先前课程，其后来实际结课仍显示。当前 XP 或初态技能不作为这些课程成果。
Outcome<ApprenticeshipCoursesPage> read_apprenticeship_courses(const CommonStore&,const GrowthState&,const FamilyApprenticeshipState&,const StableId& observer,const StableId& bond_id,Integer page);
}
