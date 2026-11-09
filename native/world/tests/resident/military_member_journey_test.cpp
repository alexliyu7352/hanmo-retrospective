// 附属行程投影有限门；合法成员来源由注入的共同接口表达，不把夹具冒充生产军团。
#include "resident/views/journey_motion.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 检查不受NDEBUG影响，失败立即保存首个具体边界。
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
// 只用于有限CommonRead边界测试，真正军籍另由原NPC行军bank覆盖。
struct Members final:MembershipResolver {
 std::vector<StableId> ids{"leader","player"};StableId leader="leader";
 const std::vector<StableId>* active_army_members(const StableId& id)const override{return std::find(ids.begin(),ids.end(),id)!=ids.end()?&ids:nullptr;}
 const StableId* army_leader(const StableId& id)const override{return active_army_members(id)?&leader:nullptr;}
};
// 构造一个原travel行动的两名真实参与者，费用和位置不由视图回写。
struct Fixture {
 std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},catalog};TravelState travel;Members membership;
 Fixture(){for(const auto* id:{"player","leader"}){ActorRecord actor;actor.id=id;actor.action_id="action:1";actor.location=WildernessLocation{"map","home",{20,40}};common.state().actors.insert(id,actor);}common.state().actors.find("player")->location=InArmyLocation{"army:1"};common.bind_membership(&membership);ActionCoreView action;action.id="action:1";action.kind="travel";action.actor_id="leader";action.participants={"leader","player"};action.remaining_ticks=4;common.state().actions.insert(action.id,action);TerrainTravelParams params;params.map_id="map";params.points={{180,40}};params.destination=StableId("target");travel.actions.insert(action.id,params);}
};
}
// 两个页共用严格来源，正常独行、道路与跟随投影也保持原字段含义。
int main(){try{
 Fixture f;const auto before=render_boundary_json(encode_common_state(f.common.state())).value;
 auto motion=query_journey_travel(f.common,f.travel,"player");check(motion.ok&&motion.value.active&&motion.value.terrain&&motion.value.terrain->remaining_ticks==4,"member receives actual shared travel summary");
 auto route=query_journey_route(f.common,f.travel,"player");check(route.ok&&route.value.active&&route.value.points.size()==2&&route.value.points[0].x==20&&route.value.points[1].x==180,"member route uses real leader point and saved suffix");
 check(query_journey_location(f.common,"player").value.kind=="InArmy","raw member location remains InArmy");
 f.common.state().actors.find("leader")->action_id="other";check(!query_journey_travel(f.common,f.travel,"player").ok&&!query_journey_route(f.common,f.travel,"player").ok,"foreign leader action is rejected by both pages");f.common.state().actors.find("leader")->action_id="action:1";
 f.membership.ids={"leader"};check(!query_journey_travel(f.common,f.travel,"player").ok&&!query_journey_route(f.common,f.travel,"player").ok,"raw kind cannot replace real roster membership");f.membership.ids={"leader","player"};
 f.common.state().actions.find("action:1")->participants={"leader"};check(!query_journey_travel(f.common,f.travel,"player").ok&&!query_journey_route(f.common,f.travel,"player").ok,"missing action participant is rejected by both pages");f.common.state().actions.find("action:1")->participants={"leader","player"};
 f.common.state().actors.find("leader")->location=InArmyLocation{"army:other"};check(!query_journey_travel(f.common,f.travel,"player").ok,"nested leader attachment is rejected");f.common.state().actors.find("leader")->location=WildernessLocation{"map","home",{20,40}};
 check(query_journey_travel(f.common,f.travel,"leader").ok&&query_journey_route(f.common,f.travel,"leader").ok,"ordinary commander retains original projection");
 f.membership.ids.clear();PartyRecord party;party.id="party";party.leader_id="leader";party.members={"leader","player"};f.common.state().parties.insert(party.id,party);f.common.state().actors.find("player")->location=FollowingLocation{"party"};check(query_journey_travel(f.common,f.travel,"player").ok&&query_journey_route(f.common,f.travel,"player").ok,"same proven shared action can project Following without giving control");f.common.state().actors.find("player")->location=InArmyLocation{"army:1"};f.common.state().parties.erase("party");f.membership.ids={"leader","player"};
 check(before==render_boundary_json(encode_common_state(f.common.state())).value,"all restored fixture state and raw location stay byte-identical");
 std::cout<<"MILITARY_MEMBER_JOURNEY_CHECKS="<<checks<<" FAILURES=0 SOURCE=FINITE_TYPED_RULE COMMANDS=0\n";return 0;
 }catch(const std::exception& error){std::cerr<<"MILITARY_MEMBER_JOURNEY_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
