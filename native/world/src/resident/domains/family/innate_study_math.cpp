// 纯学习数学不读世界、不抽随机；所有增益仍由原课程真实结算。
#include "innate_study_model.hpp"
#include <tuple>
namespace hanmo::resident {
bool innate_study_source_equal(const InnateStudySource& a,const InnateStudySource& b){return std::tie(a.actor_id,a.skill_id,a.origin_kind,a.origin_fact_id,a.observed_tick,a.bonus_bp)==std::tie(b.actor_id,b.skill_id,b.origin_kind,b.origin_fact_id,b.observed_tick,b.bonus_bp)&&a.known_levels.rows()==b.known_levels.rows();}
Outcome<Integer> innate_study_lesson_xp(Integer base,Integer bonus){if(base<0||base>1000000||bonus<0||bonus>1500)return {false,{},common_failure("InvalidInnateStudyBonus").error};return {true,base+base*bonus/10000,{}};}
}
