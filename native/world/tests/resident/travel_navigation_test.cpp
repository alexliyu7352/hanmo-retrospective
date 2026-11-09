// 验证治所/场所导航只提出一个原命令，不在测试中把场所移动或任命当成成功业务。
#include "resident/domains/travel/travel_navigation.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 原拒绝码和下一步命令均作明确断言，失败返回非零。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 路网和资格仅是有限夹具，CommonStore和原plan_travel/preview_move_local是真实生产代码。
struct Fixture final:TravelServices {
    CommonStore common{CommonState{},std::make_shared<CommonCatalog>()};TravelState state;TravelCatalog catalog;std::set<StableId> denied;bool permission_error=false;
    Fixture(){
        common.state().metadata.player_actor_id="actor";ActorRecord actor;actor.id="actor";actor.alive=true;actor.available=true;actor.hp=50;actor.wallet_id="wallet";actor.location=SettlementLocation{"a","room"};common.state().actors.insert(actor.id,actor);
        WalletView wallet;wallet.id="wallet";wallet.owner={Owner::Kind::actor,"actor"};wallet.balance=100;common.state().wallets.insert(wallet.id,wallet);common.state().rng.insert("world",{17,71,{}});common.state().next_instance_id=93;
        catalog.settlements.insert("a",{"a","gate",{0,0}});catalog.settlements.insert("b",{"b","b.gate",{100,0}});
        catalog.places.insert("room",{"a",{"hall"}});catalog.places.insert("hall",{"a",{"room","gate"}});catalog.places.insert("gate",{"a",{"hall","locked","market"}});
        catalog.places.insert("locked",{"a",{"office"}});catalog.places.insert("market",{"a",{"office"}});catalog.places.insert("office",{"a",{}});catalog.places.insert("isolated",{"a",{}});
        catalog.places.insert("b.gate",{"b",{"b.square"}});catalog.places.insert("b.square",{"b",{"b.office"}});catalog.places.insert("b.office",{"b",{}});
        catalog.roads.insert("road",{"road","a","b",2,8,5,0,0,{}});
    }
    // 场所变更只用于摆放下一个独立测试输入，不模拟生产移动或时间推进。
    void at(const StableId& town,const StableId& place){common.state().actors.find("actor")->location=SettlementLocation{town,place};}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
    Outcome<bool> admission_allowed(const StableId& actor,const StableId& place)const override{if(permission_error)return {false,false,common_failure("PermissionSourceUnavailable").error};return {true,!denied.count(actor+":"+place),{}};}
    StableId party_id(const StableId&)const override{return {};}
    Integer party_extra_cost(const StableId&)const override{return 0;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{return {true,std::nullopt,{}};}
    bool talking(const StableId&)const override{return false;}
    const terrain::Flags& terrain_flags()const override{throw std::logic_error("unexpected terrain flags in road fixture");}
    const TravelScheduleDefinition* schedule(const StableId&)const override{throw std::logic_error("future schedule read");}
    Outcome<TravelNextStep> next(const StableId& place){return next_travel_command(common,state,catalog,*this,"actor",place);}
};
// 从typed结果取原地方移动命令；绝不从文字说明反推目的地。
StableId local_target(const Outcome<TravelNextStep>& result){if(!result.ok||!result.value.command)return {};const auto* command=std::get_if<MoveLocalCommand>(&*result.value.command);return command?command->place_id:StableId{};}
// 当前世界冷编码仅供非变比较。
std::string bytes(const Fixture& fixture){return render_boundary_json(encode_common_state(fixture.common.state())).value;}
// 多级房间、出口、跨城和到城后的治所逐段仍由原命令处理。
void next_steps(){
    Fixture f;const auto before=bytes(f);
    check(local_target(f.next("office"))=="hall","same-town multiroom path uses first real passage");
    check(local_target(f.next("b.office"))=="hall","cross-town departure first walks out of the room");
    check(bytes(f)==before,"navigation mutates no position money time IDs or RNG");
    f.at("a","gate");auto travel=f.next("b.office");
    check(travel.ok&&travel.value.command&&std::holds_alternative<TravelCommand>(*travel.value.command)&&std::get<StableId>(std::get<TravelCommand>(*travel.value.command).destination)=="b","at exit returns original travel command to actual settlement");
    auto quote=plan_travel(f.common,f.state,f.catalog,f,"actor",TravelDestination{StableId{"b"}});
    check(travel.ok&&travel.value.travel_quote&&travel.value.travel_quote->duration==quote.value.duration&&travel.value.travel_quote->cost==quote.value.cost&&travel.value.travel_quote->road->path==quote.value.road->path,"cross-town quote is unchanged plan_travel result");
    f.at("b","b.gate");check(local_target(f.next("b.office"))=="b.square","arrival at town entry still walks to actual office");
    f.at("b","b.office");auto arrived=f.next("b.office");check(arrived.ok&&!arrived.value.command&&!arrived.value.travel_quote,"already at target proposes no command");
}
// 当前队伍每位人物的准入都参与路线，拒绝或资金不足不会被转换成成功移动。
void refusals(){
    Fixture f;f.at("a","gate");check(local_target(f.next("office"))=="locked","equal-length ties keep author passage order");
    f.denied.insert("actor:locked");check(local_target(f.next("office"))=="market","denied intermediate doorway selects real permitted alternative");
    f.denied.insert("actor:office");check(f.next("office").error.original.code=="AudienceDenied","no permitted route preserves admission refusal");
    check(f.next("isolated").error.original.code=="NoLocalPassage","disconnected place is distinct from admission failure");
    f.denied.clear();f.permission_error=true;check(f.next("office").error.original.code=="PermissionSourceUnavailable","admission provider failure propagates");f.permission_error=false;
    ActorRecord member;member.id="follower";member.alive=true;member.available=true;member.location=FollowingLocation{"party"};f.common.state().actors.insert(member.id,member);PartyRecord party;party.id="party";party.leader_id="actor";party.members={"actor","follower"};f.common.state().parties.insert("party",party);
    f.denied.insert("follower:locked");check(local_target(f.next("office"))=="market","follower admission cannot be bypassed by leader access");
    f.common.state().actors.find("follower")->action_id="busy";check(f.next("office").error.original.code=="ParticipantBusy","occupied companion blocks the next movement");f.common.state().actors.find("follower")->action_id={};
    f.common.state().wallets.find("wallet")->balance=0;check(f.next("b.office").error.original.code=="InsufficientFunds","cross-town money refusal remains original quote failure");
    check(f.next("missing").error.original.code=="UnknownPlace","unknown target is not fabricated");
    f.at("a","room");f.catalog.places.find("hall")->passages={"b.office"};check(!f.next("b.office").ok,"local navigation never uses a cross-town passage to teleport");
}
}
// 只运行受影响导航与原报价，完整任用/行旅提交链由所属真实入口验收。
int main(){try{next_steps();refusals();}catch(const std::exception& error){check(false,error.what());}std::cout<<"TRAVEL_NAVIGATION_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
