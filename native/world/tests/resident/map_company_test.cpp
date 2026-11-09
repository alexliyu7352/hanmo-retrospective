// 本人军团地图摘要的资格和隐私门；军令/军队移动不由测试读页触发。
#include "resident/views/map_company.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 保留失败来源，任何未知军团不能当作空页通过。
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
// 只有实际本人统帅允许解析姓名；任何敌方读取都会使本门失败。
struct Names:PresentationRead { mutable std::vector<StableId> read;Outcome<std::string> display_name(const StableId& id)const override{if(id!="hero"&&id!="leader")throw std::runtime_error("hidden commander name read");read.push_back(id);return {true,"Name "+id,{}};}Outcome<std::string> text(const StableId&)const override{throw std::runtime_error("unexpected text");} };
// 本测试仅提供typed成员关系，非正式作者世界或军事生命周期验收。
struct Fixture {
    std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},catalog};MilitaryState military;Names names;
    Fixture(){common.state().metadata.player_actor_id="hero";common.state().identity.execution_epoch=2;common.state().identity.revision=5;common.state().identity.tick=9;for(const auto id:{"hero","leader"}){ActorRecord person;person.id=id;person.alive=true;person.available=true;person.location=SettlementLocation{"town","gate"};common.state().actors.insert(id,person);}}
    Outcome<MapCompanyPage> read(){return query_map_company(common,military,names,"hero");}
    void army(bool commander){ArmyRecord army;army.id="own";army.commander_actor_id=commander?"hero":"leader";army.member_actor_ids={"hero","leader"};military.armies.push_back(army);if(!commander)common.state().actors.find("hero")->location=InArmyLocation{"own"};}
};
}
// 精确本人来源、真实空页、退役/错绑及全态非变都在一个有限只读门内验证。
int main(){try{
    Fixture f;auto empty=f.read();check(empty.ok&&!empty.value.army&&f.names.read.empty(),"solitary observer has no imaginary army");
    f.army(false);const auto before=render_boundary_json(encode_common_state(f.common.state())).value;auto member=f.read();check(member.ok&&member.value.army->army_id=="own"&&!member.value.army->commanded_by_observer&&member.value.army->commander_name=="Name leader","actual army member sees own commander only");
    const auto original=render_boundary_json(encode_map_company(member.value)).value;
    for(int i=0;i<250;++i){ArmyRecord enemy;enemy.id="remote:"+std::to_string(i);enemy.commander_actor_id="hidden";enemy.member_actor_ids={"hidden"};enemy.wallet_id="secret-wallet";enemy.inventory_id="secret-inventory";f.military.armies.push_back(enemy);}
    auto repeated=f.read();check(repeated.ok&&render_boundary_json(encode_map_company(repeated.value)).value==original,"remote armies do not change finite own page");
    check(f.names.read==std::vector<StableId>({"leader","leader"}),"no distant name access");
    check(original.find("secret")==std::string::npos&&original.find("member_actor_ids")==std::string::npos&&original.find("position")==std::string::npos,"page has no private cargo force or new coordinates");
    check(render_boundary_json(encode_common_state(f.common.state())).value==before,"query does not mutate common state");
    check(!query_map_company(f.common,f.military,f.names,"leader").ok,"arbitrary observer rejected");
    f.common.state().actors.find("hero")->location=InArmyLocation{"other"};check(!f.read().ok,"mismatched raw attachment rejected");
    f.common.state().actors.find("hero")->location=InArmyLocation{"own"};f.military.armies[0].status="returned";check(!f.read().ok,"stale attachment is not a current army");
    Fixture commander;commander.army(true);auto own=commander.read();check(own.ok&&own.value.army->commanded_by_observer&&own.value.army->commander_actor_id=="hero","commander identity is actual current actor");
    commander.common.state().actors.find("hero")->alive=false;check(!commander.read().ok,"dead observer rejected");
    std::cout<<"MAP_COMPANY_CHECKS="<<checks<<" FAILURES=0 SOURCE=FINITE_TYPED_FIXTURE COMMANDS=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"MAP_COMPANY_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
