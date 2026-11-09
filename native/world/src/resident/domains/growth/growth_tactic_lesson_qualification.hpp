// 战法报名基础资格只消费原永久课程下界；不会因达到技能等级自动获得任何战法。
#pragma once
#include "growth_skill_history.hpp"
#include "growth_skill_origin.hpp"
namespace hanmo::resident {
// 仅tactics/infantry/archery/cavalry/transport，结果明确minimum_xp/minimum_level。
// 完整Family/Creation/Birth及Growth先验；只计严格早于本半日的Study净贡献。
// 同tick原课程迟答没有独立结算头，因此本半日刚达门槛须下一半日报名，不猜先后。
Outcome<GrowthSkillQualification> read_growth_tactic_lesson_qualification(const CommonStore&,const GrowthSkillOriginContext&,const GrowthState&,const StableId& actor,const StableId& skill);
// 仅真实tactic_lesson:/tactic_lesson_started/growth_tactic_lessons；调用者先核原payload.tick。
// 不接受军战准备头或未来phase。初始来源和Study来源分别保留，Study列表按Common实序唯一。
Outcome<GrowthSkillQualification> read_growth_tactic_lesson_qualification_before_start(const CommonStore&,const GrowthSkillOriginContext&,const GrowthState&,const StableId& actor,const StableId& skill,Integer start_tick,const StableId& actual_start_fact);
}
