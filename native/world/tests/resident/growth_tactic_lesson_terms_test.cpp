// 有限课时费用守恒与边界；不把本纯算术组件当实际Common付款或完整教学。
#include "resident/domains/growth/growth_tactic_lessons.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}}
int main(){try{
 for(const Integer fee:{0LL,1LL,3LL,101LL,2147483647LL})for(const Integer duration:{1LL,2LL,6LL,720LL}){Integer sum=0;for(Integer i=1;i<=duration;++i){auto part=growth_tactic_lesson_fee_for_ordinal(fee,duration,i);check(part.ok&&part.value>=0,"bounded period fee");sum+=part.value;auto paid=growth_tactic_lesson_fee_through(fee,duration,i);check(paid.ok&&paid.value==sum&&fee-paid.value>=0,"partial fee and remaining reservation conserve total");}check(sum==fee,"complete fee is paid exactly once");}
 check(!growth_tactic_lesson_fee_for_ordinal(1,0,1).ok,"zero duration rejected");check(!growth_tactic_lesson_fee_for_ordinal(1,2,0).ok,"zero ordinal rejected");check(!growth_tactic_lesson_fee_through(1,2,3).ok,"over-provision rejected");check(!growth_tactic_lesson_fee_through(-1,2,1).ok,"negative fee rejected");check(!growth_tactic_lesson_fee_through(2147483648LL,2,1).ok,"unsupported cash bound rejected");
 GrowthSkillQualification source;source.skill_id="tactics";source.initial_prototype_id="student";source.initial_xp=290;source.minimum_xp=300;source.minimum_level=2;source.observed_tick=8;source.study_source_ids={"action:20"};auto q=freeze_growth_tactic_lesson_prerequisite(source,2);check(validate_growth_tactic_lesson_prerequisite_shape(q).ok,"explicit minimum qualifies");q.minimum_level=3;check(!validate_growth_tactic_lesson_prerequisite_shape(q).ok,"forged level rejected");q.minimum_level=2;q.study_source_ids.push_back("action:20");check(!validate_growth_tactic_lesson_prerequisite_shape(q).ok,"duplicate source refused");
 std::cout<<"GROWTH_TACTIC_LESSON_TERMS "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
