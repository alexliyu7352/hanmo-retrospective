// 军阵实践的原开训基础资格；只读原人物与Study净贡献，不授战法或复制训练成绩。
#pragma once
#include "growth_skill_history.hpp"
#include "growth_skill_origin.hpp"
namespace hanmo::resident {
// 原完整Family/Creation/Birth、Growth与MilitaryTraining/drill史先验；调用方先核payload.start_tick。
// 仅 action:/military_training/military 实际开训头；稳定历史，禁止未来phase/任意业务头。
// 五原技艺 tactics/infantry/archery/cavalry/transport 的minimum下界，仅计严格早于start_tick的Study。
// 同tick迟答不猜先后；Study因果ID依Common原实序。不得把本返回当已习得战法或精确全量XP。
Outcome<GrowthSkillQualification> read_growth_military_training_qualification_before_start(const CommonStore&,const GrowthSkillOriginContext&,const GrowthState&,const StableId& actor,const StableId& skill,Integer start_tick,const StableId& actual_training_action);
}
