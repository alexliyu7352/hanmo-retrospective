// 原十项与显式六项扩展共用生产经验访问器；本门不链接旧ABI世界库或声称课程/银行已过。
#include "resident/domains/growth/growth_model.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 实际失败保留业务原因，Release不关闭检查。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
}
// 旧profile不自动增加六项，完整profile每项独立修改且严格保留原经验上限。
int main(){try{
    const auto& legacy=growth_skill_ids(false);const auto& complete=growth_skill_ids(true);
    check(legacy.size()==10&&complete.size()==16,"explicit skill profile sizes");
    check(std::set<StableId>(complete.begin(),complete.end()).size()==16,"complete skill identities are unique");
    ActorSkills old;old.rites=80;
    for(const auto* id:{"naval","espionage","fortification","agriculture","medicine","forging"}){
        check(!growth_skill_defined(id,false)&&growth_skill_defined(id,true),StableId{id}+" requires explicit profile");
        check(growth_skill_xp(old,id)==-1,"absent extension is not zero experience");
        const auto result=growth_set_skill(old,id,100);check(!result.ok&&result.error.original.code=="SkillProfileRequired"&&!old.extended&&old.rites==80,"writing old profile cannot silently create extension");
    }
    check(growth_set_skill(old,"rites",300).ok&&old.rites==300&&!old.extended,"old ten skills remain writable without extension");
    ActorSkills full;full.extended=ActorExtendedSkills{};
    Integer value=0;
    for(const auto& id:complete){value+=50;check(growth_set_skill(full,id,value).ok,"write each actual skill");check(growth_skill_xp(full,id)==value,"read exact skill XP");}
    value=0;for(const auto& id:complete){value+=50;check(growth_skill_xp(full,id)==value,"later writes do not overwrite other skills");}
    for(const auto* id:{"naval","espionage","fortification","agriculture","medicine","forging"}){
        const auto before=growth_skill_xp(full,id);check(!growth_set_skill(full,id,-1).ok&&!growth_set_skill(full,id,1201).ok&&growth_skill_xp(full,id)==before,"out of range experience has no partial write");
        check(growth_set_skill(full,id,1200).ok&&growth_skill_level(growth_skill_xp(full,id))==4,"new skill uses existing maximum and levels");
    }
    check(!growth_skill_defined("medicine ",true)&&!growth_skill_defined("unknown",true)&&growth_skill_xp(full,"unknown")==-1,"unknown IDs are not normalized");
    check(!growth_set_skill(full,"unknown",20).ok,"unknown skill cannot create arbitrary state");
    for(const auto& row:std::vector<std::pair<Integer,Integer>>{{0,0},{99,0},{100,1},{299,1},{300,2},{649,2},{650,3},{1199,3},{1200,4},{1201,-1},{-1,-1}})check(growth_skill_level(row.first)==row.second,"unchanged cumulative thresholds");
    std::cout<<"EXTENDED_GROWTH_SKILLS "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"EXTENDED_GROWTH_SKILLS FAIL "<<error.what()<<'\n';return 1;}}
