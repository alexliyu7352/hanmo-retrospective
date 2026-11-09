// 军团优先、军俘独行、队伍跟随与clone重绑的窄共同读取合同。
#include "resident/storage/common_store.hpp"
// 发布优化构建也必须执行本测试断言，不能让NDEBUG形成假绿。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 夹具直接给出活动军团唯一数组；生产适配器由C借用真实MilitaryState。
struct ArmyFixture final:MembershipResolver {bool active=true;StableId leader="a";std::vector<StableId> members{"a","c"};const std::vector<StableId>* active_army_members(const StableId& id)const override{return active&&(id=="a"||id=="c")?&members:nullptr;}const StableId* army_leader(const StableId& id)const override{return active&&(id=="a"||id=="c"||id=="p")?&leader:nullptr;}};
int main(){CommonState seed;for(auto id:{"a","b","c","p"}){ActorRecord a;a.id=id;a.location=SettlementLocation{"city","gate"};seed.actors.insert(id,a);}seed.actors.find("b")->location=FollowingLocation{"party"};seed.actors.find("c")->location=InArmyLocation{"army"};seed.actors.find("p")->location=InArmyLocation{"army"};PartyRecord party;party.id="party";party.leader_id="a";party.members={"a","b"};seed.parties.insert("party",party);CommonStore common(seed,std::make_shared<CommonCatalog>());ArmyFixture army;common.bind_membership(&army);assert(*common.members("a")==army.members);assert(*common.members("b")==party.members);assert(*common.members("p")==std::vector<StableId>{"p"});assert(!common.members("missing"));auto captured=common.resolved_location("p");assert(captured.ok&&std::get<SettlementLocation>(captured.value).place_id=="gate");auto copied=common;assert(!copied.resolved_location("c").ok);copied.bind_membership(&army);assert(copied.resolved_location("c").ok);army.active=false;common.invalidate({ResourceKey::Kind::army,"army"});assert(*common.members("a")==party.members&&common.read_version({ResourceKey::Kind::army,"army"}).version==1);auto rng=common.random_bounded("world",1,3);assert(!rng.ok&&rng.error.kind==ErrorKind::internal&&rng.error.original.code=="UnknownRandomStream"&&rng.error.source_id=="world"&&common.state().rng.empty());Owner a{Owner::Kind::actor,"a",{}, {"id","kind"}},b{Owner::Kind::actor,"a",{}, {"kind","id"}};assert(owner_equal(a,b));std::cout<<"state_membership: PASS (army priority/prisoner exclusion/party location/clone rebind/version/missing RNG)\n";}
