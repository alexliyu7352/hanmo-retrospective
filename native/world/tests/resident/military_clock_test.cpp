// 普通随军推进的有限资格门；本夹具不代替正规编组、存读或Main军旅验收。
#include "resident/domains/military/military_clock.hpp"
#include "resident/storage/common_codec.hpp"
#include "../../src/resident/domains/military/camp_views.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 每次拒绝核其具体来源，避免将错误码变化藏在布尔成功里。
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
// 使用真实共同存储类型，所有初态仅供域内查询边界测试。
struct Fixture {
    std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},catalog};MilitaryState military;
    Fixture(){
        for(const auto id:{"player","commander","outsider"}){ActorRecord actor;actor.id=id;actor.hp=100;actor.alive=true;actor.available=true;actor.location=SettlementLocation{"town","camp"};common.state().actors.insert(id,actor);}
        ArmyRecord army;army.id="army:1";army.commander_actor_id="commander";army.member_actor_ids={"commander","player"};military.armies.push_back(army);
        common.state().actors.find("player")->location=InArmyLocation{"army:1"};
    }
    Outcome<StableId> read(){return military_clock_leader(common,military,"player");}
    // 行军行动仍由实际统帅发起，共同占用两个真实人物。
    void march(){ActionCoreView action;action.id="action:2";action.kind="travel";action.actor_id="commander";action.participants={"commander","player"};action.remaining_ticks=2;common.state().actions.insert(action.id,action);for(const auto id:{"player","commander"})common.state().actors.find(id)->action_id=action.id;common.state().actors.find("commander")->location=WildernessLocation{"world","town",{1,2}};}
};
}
// 入军身份、俘虏排除、真实行军占用、无写入和非法目标逐项覆盖。
int main(){try{
    Fixture service;service.common.state().metadata.player_actor_id="player";
    CampaignRecord campaign;campaign.id="campaign:3";campaign.template_id="order.supply";campaign.status="marching";service.military.campaigns.push_back(campaign);service.military.armies[0].order_id=campaign.id;
    MilitaryCatalog military_catalog;MilitaryOrderDefinition order;order.id=campaign.template_id;order.display_name="Supply march";military_catalog.orders.push_back(order);
    MilitaryUnitRecord own;own.id="unit.player";own.commander_actor_id="player";own.fit={"soldier:1"};own.wounded={"soldier:2"};MilitaryUnitRecord other;other.id="unit.other";other.commander_actor_id="commander";other.fit={"hidden:1","hidden:2"};service.military.military_units={own,other};service.military.armies[0].unit_ids={own.id,other.id};
    auto page=military_service_page(service.common,service.military,military_catalog,*service.catalog);check(page.ok,"actual member service page succeeds");
    const auto service_text=render_boundary_json(page.value).value;check(service_text.find("unit.player")!=StableId::npos&&service_text.find("unit.other")==StableId::npos&&service_text.find("hidden:")==StableId::npos,"service page includes only personally controlled unit");
    check(boundary::get_string(page.value,"clock_command","page")=="wait"&&boundary::get_string(page.value,"control_scope","page")=="unit","idle member clock and actual unit scope");service.march();page=military_service_page(service.common,service.military,military_catalog,*service.catalog);check(page.ok&&boundary::get_string(page.value,"clock_command","page")=="advance","travelling service preserves advance command");
    service.military.armies[0].prisoner_ids={"player"};check(!military_service_page(service.common,service.military,military_catalog,*service.catalog).ok,"prisoner cannot inspect a false service role");
    Fixture f;const auto before=render_boundary_json(encode_common_state(f.common.state())).value;
    auto result=f.read();check(result.ok&&result.value=="commander","idle genuine follower may advance the shared clock");
    check(render_boundary_json(encode_common_state(f.common.state())).value==before,"clock qualification does not mutate common state");
    check(!military_clock_leader(f.common,f.military,"missing").ok,"missing actor rejected");
    check(!military_clock_leader(f.common,f.military,"outsider").ok,"unattached actor does not gain army authority");
    f.military.armies[0].member_actor_ids={"commander"};check(f.read().error.original.code=="ArmyMembershipRequired","raw attachment alone cannot establish membership");
    f.military.armies[0].member_actor_ids.push_back("player");f.military.armies[0].prisoner_ids={"player"};check(f.read().error.original.code=="ArmyMembershipRequired","prisoner cannot advance as an enlisted member");
    f.military.armies[0].prisoner_ids.clear();f.military.armies[0].status="closed";check(!f.read().ok,"closed army rejected");f.military.armies[0].status="active";
    f.common.state().actors.find("player")->alive=false;check(f.read().error.original.code=="ActorUnavailable","dead follower rejected");f.common.state().actors.find("player")->alive=true;
    f.common.state().actors.find("commander")->available=false;check(f.read().ok,"commander availability changes are resolved by original world phases");f.common.state().actors.find("commander")->available=true;
    f.common.state().actors.find("commander")->location=FollowingLocation{"party:3"};check(!f.read().ok,"nested attachment rejected");f.common.state().actors.find("commander")->location=SettlementLocation{"town","camp"};
    ActionCoreView leader_only;leader_only.id="action:leader_only";leader_only.actor_id="commander";leader_only.kind="wait";leader_only.participants={"commander"};leader_only.remaining_ticks=1;f.common.state().actions.insert(leader_only.id,leader_only);
    f.common.state().actors.find("commander")->action_id=leader_only.id;
    check(f.read().ok,"an idle subordinate can let an independently busy commander finish");
    f.common.state().actors.find("commander")->action_id.clear();f.common.state().actions.erase(leader_only.id);
    f.march();check(f.read().ok,"actual shared march allows follower clock advancement");
    const auto march_before=render_boundary_json(encode_common_state(f.common.state())).value;check(f.read().ok&&render_boundary_json(encode_common_state(f.common.state())).value==march_before,"march check preserves location action and clock");
    f.common.state().actors.find("player")->action_id="action:other";check(f.read().error.original.code=="ArmyMemberActionMismatch","foreign actor action rejected");f.common.state().actors.find("player")->action_id="action:2";
    f.common.state().actions.find("action:2")->participants={"commander"};check(!f.read().ok,"action participants must contain the follower");f.common.state().actions.find("action:2")->participants.push_back("player");
    f.common.state().actions.find("action:2")->kind="wait";check(f.read().ok,"shared logistics occupation may finish on the original world clock");
    std::cout<<"MILITARY_CLOCK_CHECKS="<<checks<<" FAILURES=0 SOURCE=FINITE_TYPED_FIXTURE COMMANDS=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"MILITARY_CLOCK_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
