// 学习与个人战快照必须消费同一人物的十六技艺前置；本门不发卡、不改熟练度或战斗数值。
#include "resident/domains/personal/personal_model.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 所有检查在Release生效，错误码与原学习记录一起核实。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
}
// 六项新前置只由明确扩展人物提供；旧人物/未知技能不能隐式取得资格。
int main(){try{
    for(const auto* id:{"naval","espionage","fortification","agriculture","medicine","forging"}){
        ActorRecord actor;actor.id="student";actor.skills.extended=ActorExtendedSkills{};
        PersonalSkill skill;skill.id="ability.sample";skill.kind="heart";skill.required_skill_level=PersonalSkillRequirement{id,2,{}};
        actor.combat.learned.insert(skill.id,80);PersonalCatalog catalog;catalog.skills.insert(skill.id,skill);
        auto result=personal_skill_eligible(actor,skill);
        check(!result.ok&&result.error.original.code=="SkillLevelRequired"&&result.error.actual_level==0&&result.error.required_level==2,"new prerequisite reports actual zero level");
        check(!personal_learned_snapshot(actor,catalog).find(skill.id)&&*actor.combat.learned.find(skill.id)==80,"ineligible snapshot does not delete actual learned skill");
        check(growth_set_skill(actor.skills,id,300).ok,"explicit real skill XP can reach required level");
        result=personal_skill_eligible(actor,skill);check(result.ok,"same new-skill qualification is usable for learning");
        auto snapshot=personal_learned_snapshot(actor,catalog);check(snapshot.find(skill.id)&&*snapshot.find(skill.id)==80,"qualified snapshot preserves actual mastery");
        skill.required_skill_level->level=5;check(!personal_skill_eligible(actor,skill).ok,"undefined required level rejected");
        actor.skills.extended.reset();skill.required_skill_level->level=2;result=personal_skill_eligible(actor,skill);check(!result.ok&&!actor.skills.extended,"old profile cannot synthesize six new skills");
    }
    ActorRecord old;old.skills.martial=300;PersonalSkill skill;skill.required_skill_level=PersonalSkillRequirement{"martial",2,{}};check(personal_skill_eligible(old,skill).ok,"existing ten-skill requirement unchanged");
    skill.required_skill_level=PersonalSkillRequirement{"unknown",1,{}};auto result=personal_skill_eligible(old,skill);check(!result.ok&&result.error.original.code=="InvalidSkillRequirement","unknown names remain invalid");
    skill.required_skill_level.reset();check(personal_skill_eligible(old,skill).ok,"old skills without prerequisites retain their contract");
    std::cout<<"GROWTH_CARD_PREREQUISITES "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"GROWTH_CARD_PREREQUISITES FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
