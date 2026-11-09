// 永久课程下界的必要纯读门：同tick不倒填、后来课程不改旧开店、缺/错来源和读不写。
#include "resident/domains/growth/growth_skill_history.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;
// 本组件构造的是已验证Growth前置的typed来源；不会宣称真实上课或完整World通过。
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
StudyFact course(const StableId& actor,Integer before,Integer tick,Integer gain){StudyFact f;f.actor_id=actor;f.start_tick=1;f.phase="completed";f.completed_tick=tick;f.success=true;f.terms.student_id=actor;f.terms.skill_id="commerce";f.terms.xp_before=before;f.terms.xp_limit=650;f.terms.settlement_policy="per_lesson_v1";StudyLesson lesson;lesson.xp=gain;lesson.segment=StudySegment{1,tick,before,10000,gain,false,{},{}};f.lessons.push_back(lesson);return f;}
}
int main(){try{
    auto catalog=std::make_shared<CommonCatalog>();catalog->actor_ids={"student"};catalog->actor_initial_birth.insert("student",-14400);FamilyCatalog family;IdentityPrototype prototype;prototype.skills.commerce=290;family.prototypes.insert("student",prototype);
    CommonState seed;seed.identity.world_id[0]=1;seed.identity.tick=4;seed.identity.revision=9;seed.identity.execution_epoch=2;seed.next_instance_id=100;ActorRecord actor;actor.id="student";actor.skills.commerce=320;seed.actors.insert(actor.id,actor);CommonStore common(seed,catalog);GrowthState growth;growth.facts.insert("action:8",course("student",290,3,10));common.state().facts.insert("action:8",{"action:8","skill_study","cultivation"});common.state().facts.insert("private_shop:4",{"private_shop:4","private_shop_opened","private_shops"});
    const auto before=encode_common_state(common.state());auto current=read_commerce_study_qualification(common,*catalog,family,growth,"student");check(current.ok&&current.value.initial_xp==290&&current.value.minimum_xp==300&&current.value.minimum_level==2&&current.value.study_source_ids==std::vector<StableId>{"action:8"},"actual prior study proves level2 lower bound");
    auto history=read_commerce_study_qualification_before_opening(common,*catalog,family,growth,"student",4,"private_shop:4");check(history.ok&&history.value.minimum_xp==300,"actual order not numeric id determines boundary");
    auto same=read_commerce_study_qualification_before_opening(common,*catalog,family,growth,"student",3,"private_shop:4");check(same.ok&&same.value.minimum_xp==290&&same.value.minimum_level==1,"same tick lacks independent completion ordering");
    growth.facts.insert("action:2",course("student",300,4,20));common.state().facts.insert("action:2",{"action:2","skill_study","cultivation"});history=read_commerce_study_qualification_before_opening(common,*catalog,family,growth,"student",4,"private_shop:4");check(history.ok&&history.value.minimum_xp==300,"later same tick course cannot fill earlier qualification");
    auto* later=std::get_if<StudyFact>(growth.facts.find("action:2"));later->lessons[0].segment->settled_tick=2;check(!read_commerce_study_qualification_before_opening(common,*catalog,family,growth,"student",4,"private_shop:4").ok,"source appended after boundary but claims earlier credit rejected");later->lessons[0].segment->settled_tick=4;
    common.state().facts.find("action:8")->domain="interaction";check(!read_commerce_study_qualification(common,*catalog,family,growth,"student").ok,"wrong source domain rejected");common.state().facts.find("action:8")->domain="cultivation";
    common.state().actors.find("student")->skills.commerce=299;check(!read_commerce_study_qualification(common,*catalog,family,growth,"student").ok,"current cannot be below permanent contributions");common.state().actors.find("student")->skills.commerce=320;
    check(!read_commerce_study_qualification_before_opening(common,*catalog,family,growth,"student",4,"private_shop:999").ok,"missing real opening rejected");
    common.state().facts.erase("action:2");growth.facts.erase("action:2");check(compare_boundary(before,encode_common_state(common.state())).exact(),"pure reads changed money actors facts or IDs");
    std::cout<<"GROWTH_SKILL_HISTORY "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_SKILL_HISTORY "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
