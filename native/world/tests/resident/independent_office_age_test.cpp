// 独立任职年龄边界的真实共同人物只读门；不模拟任命成功、不改已有政治权限。
#include "resident/domains/growth/growth_actor_qualification.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// Release同样执行断言；首错保留正式规则码。
void check(bool value,const StableId& message){++checks;if(!value)throw std::runtime_error(message);}
// 同一共同人物出生日期与世界半日；真实只读结果不能改动原人物或时钟。
void qualify(CommonStore& common,const StableId& expected){
    const auto actor=*common.state().actors.find("person");
    const auto tick=common.tick(),next=common.state().next_instance_id;
    const auto result=growth_independent_office_age(common,"person");
    check(expected.empty()?result.ok:!result.ok&&result.error.original.code==expected,"expected "+expected+", got "+result.error.original.code);
    const auto& after=*common.state().actors.find("person");
    check(after.birth_tick==actor.birth_tick&&after.wallet_id==actor.wallet_id&&after.action_id==actor.action_id&&after.skills.rites==actor.skills.rites&&common.tick()==tick&&common.state().next_instance_id==next,"qualification preserves identity and world clock");
}
}
// 覆盖生日之前、当天、未知出生与越界输入，年龄门不承担活性/伤势/职权等其他领域判断。
int main(){try{
    auto catalog=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},catalog};ActorRecord person;person.id="person";person.wallet_id="original.wallet";person.action_id="original.duty";person.skills.rites=300;common.state().actors.insert(person.id,person);
    qualify(common,"ActorAgeUnknown");
    auto* actor=common.state().actors.find("person");actor->birth_tick=0;common.state().identity.tick=18*720-1;qualify(common,"OfficeRequiresAdult");
    common.state().identity.tick=18*720;qualify(common,{});
    common.state().identity.tick=0;actor->birth_tick=-18*720;qualify(common,{});
    actor->birth_tick=-120*720;qualify(common,{});
    actor->birth_tick=1;qualify(common,"ActorAgeUnknown");
    actor->birth_tick=std::numeric_limits<Integer>::min();qualify(common,"ActorAgeUnknown");
    actor->birth_tick=0;common.state().identity.tick=std::numeric_limits<Integer>::max();qualify(common,"ActorAgeUnknown");
    const auto missing=growth_independent_office_age(common,"missing");check(!missing.ok&&missing.error.original.code=="UnknownActor","unknown actor remains explicit");
    std::cout<<"INDEPENDENT_OFFICE_AGE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"INDEPENDENT_OFFICE_AGE FAIL "<<error.what()<<'\n';return 1;}}
