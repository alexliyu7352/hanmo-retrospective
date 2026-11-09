// 失败完成只引用已落的同场因果；同tick的旧源由调用者的实际Common序边界排除。
#include "naval_journey_termination.hpp"
#include "naval_casualties.hpp"
#include "naval_commander.hpp"
#include "naval_navigation_history.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>Outcome<T> fail(const StableId& id){return {false,{},common_failure("NavalJourneyTerminationSourceMismatch",id).error};}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
bool precedes(const CommonStore& c,const StableId& a,const StableId& b){const auto& ids=c.state().facts.order();auto first=std::find(ids.begin(),ids.end(),a),last=std::find(ids.begin(),ids.end(),b);return first!=ids.end()&&last!=ids.end()&&first<last;}
// 一个接敌的每条原行旅只有一位真实领航者；被俘乘员不是整条行旅的拥有者。
const MilitaryMotionSource* motion(const NavalContactFact& contact,const StableId& action){const MilitaryMotionSource* result=nullptr;for(const auto* row:{&contact.first_motion,&contact.second_motion})if(!row->stationary&&row->action_id==action){if(result)return nullptr;result=row;}return result;}
}
static Outcome<std::vector<NavalJourneyTermination>> direct_terminations(const CommonStore& c,const MilitaryState& m,const NavalState& n,const StableId& id){
 const auto* header=c.state().facts.find(id);if(!header||header->id!=id||header->domain!="military_naval")return fail<std::vector<NavalJourneyTermination>>(id);
 const NavalRoundFact* round=nullptr;const NavalCaptureFact* capture=nullptr;const NavalContinueFact* continued=nullptr;StableId battle_id,casualty_id,reason;Integer tick=-1;std::vector<StableId> actions;
 if(header->kind=="naval_round"){round=find(n.battle.rounds,id);if(!round)return fail<std::vector<NavalJourneyTermination>>(id);battle_id=round->battle_id;tick=round->tick;casualty_id=round->casualty_source_id;reason="NavalCommanderKilled";}
 else if(header->kind=="naval_capture"){capture=find(n.battle.captures,id);if(!capture)return fail<std::vector<NavalJourneyTermination>>(id);round=find(n.battle.rounds,capture->round_source_id);battle_id=capture->battle_id;tick=capture->tick;casualty_id=capture->casualty_source_id;actions=capture->interrupted_action_ids;reason="NavalCommanderCaptured";}
 else if(header->kind=="naval_continue"){continued=find(n.battle.continuations,id);if(!continued)return fail<std::vector<NavalJourneyTermination>>(id);round=find(n.battle.rounds,continued->round_source_id);battle_id=continued->battle_id;tick=continued->tick;actions=continued->interrupted_action_ids;reason="NavalTravelUnserviceable";}
 else return fail<std::vector<NavalJourneyTermination>>(id);
 const NavalBattleRecord* battle=nullptr;for(const auto& row:n.battle.battles)if(row.frame.id==battle_id){if(battle)return fail<std::vector<NavalJourneyTermination>>(id);battle=&row;}
 const auto* contact=battle?find(n.battle.contacts,battle->contact_source_id):nullptr;const auto* contact_header=contact?c.state().facts.find(contact->id):nullptr;const auto* round_header=round?c.state().facts.find(round->id):nullptr;
 if(tick<0||tick>c.tick()||!round||round->battle_id!=battle_id||round->tick>tick||!round_header||round_header->kind!="naval_round"||round_header->domain!="military_naval"||!contact||contact->battle_id!=battle_id||contact->tick>tick||!contact_header||contact_header->kind!="naval_contact"||contact_header->domain!="military_naval"||!precedes(c,contact->id,round->id)||(round->id!=id&&!precedes(c,round->id,id)))return fail<std::vector<NavalJourneyTermination>>(id);
 const MilitaryCasualtyFact* casualty=nullptr;
 if(!casualty_id.empty()){auto checked=validate_naval_casualty_receipt(c,m,n,casualty_id,c.tick());if(!checked.ok)return {false,{},checked.error};casualty=find(m.casualty_facts,casualty_id);if(!casualty||casualty->battle_id!=id||casualty->tick!=tick)return fail<std::vector<NavalJourneyTermination>>(id);}
 if(header->kind=="naval_round"){
  if(!casualty)return fail<std::vector<NavalJourneyTermination>>(id);
  for(const auto* trip:{&contact->first_motion,&contact->second_motion}){if(trip->stationary||trip->action_id.empty())continue;for(const auto& change:casualty->actor_changes)if(change.actor_id==trip->actor_id&&change.status=="dead"){
    bool actual=false;for(const auto& unit:round->after.units)if(unit.army_id==change.army_id&&unit.commander_actor_id==trip->actor_id&&naval_commander_loss(unit)==2)actual=true;
    if(!actual)return fail<std::vector<NavalJourneyTermination>>(id);
    actions.push_back(trip->action_id);
   }}
 }
 if(continued&&!actions.empty()&&(continued->after.phase!="closed"||!has({"opposition_left","crews_inactive","round_limit"},continued->reason)))return fail<std::vector<NavalJourneyTermination>>(id);
 std::set<StableId> seen;std::vector<NavalJourneyTermination> out;
 for(const auto& action:actions){const auto* trip=motion(*contact,action);if(action.empty()||!seen.insert(action).second||!trip||trip->actor_id.empty()||trip->plan_id.empty()||!c.actor(trip->actor_id)||c.action(action))return fail<std::vector<NavalJourneyTermination>>(id);
  if(capture){if(!casualty||!has(capture->prisoner_ids,trip->actor_id))return fail<std::vector<NavalJourneyTermination>>(id);Integer changed=0;for(const auto& row:casualty->actor_changes)if(row.actor_id==trip->actor_id&&row.status=="captured")++changed;if(changed!=1)return fail<std::vector<NavalJourneyTermination>>(id);}
  out.push_back({action,trip->actor_id,battle_id,id,casualty_id,reason,tick});
 }
 return {true,std::move(out),{}};
}
// 原接敵motion永久保留；仅最早真正终止者拥有完成回执，后来死亡/继续不重复结束旧行动。
Outcome<std::vector<NavalJourneyTermination>> query_naval_journey_terminations(const CommonStore& c,const MilitaryState& m,const NavalState& n,const StableId& id){
 auto actual=direct_terminations(c,m,n,id);if(!actual.ok)return actual;std::vector<NavalJourneyTermination> out;
 for(const auto& end:actual.value){bool ended_before=false;
  auto navigation=query_naval_navigation_termination_before(c,n.navigation,end.action_id,end.actor_id,id,end.tick);if(!navigation.ok)return {false,{},navigation.error};ended_before=navigation.value.has_value();
  std::vector<StableId> sources;for(const auto& f:n.battle.captures)if(has(f.interrupted_action_ids,end.action_id))sources.push_back(f.id);for(const auto& f:n.battle.continuations)if(has(f.interrupted_action_ids,end.action_id))sources.push_back(f.id);
  for(const auto& f:n.battle.rounds){const auto* loss=find(m.casualty_facts,f.casualty_source_id);if(loss)for(const auto& change:loss->actor_changes)if(change.actor_id==end.actor_id&&change.status=="dead"){sources.push_back(f.id);break;}}
  // 直接核每份原来源而不递归总史，避免长档把递归深度当游戏上限。
  for(const auto& source:sources){if(!c.state().facts.find(source))return fail<std::vector<NavalJourneyTermination>>(source);if(!precedes(c,source,id))continue;auto earlier=direct_terminations(c,m,n,source);if(!earlier.ok)return earlier;for(const auto& row:earlier.value)if(row.action_id==end.action_id){if(row.actor_id!=end.actor_id||row.tick>end.tick)return fail<std::vector<NavalJourneyTermination>>(source);ended_before=true;}}
  if(!ended_before)out.push_back(end);
 }
 return {true,std::move(out),{}};
}
Outcome<std::optional<NavalJourneyTermination>> query_naval_journey_termination(const CommonStore& c,const MilitaryState& m,const NavalState& n,const StableId& source,const StableId& action,const StableId& actor){
 if(action.empty()||actor.empty()||!c.actor(actor))return fail<std::optional<NavalJourneyTermination>>(source);auto rows=query_naval_journey_terminations(c,m,n,source);if(!rows.ok)return {false,{},rows.error};for(const auto& row:rows.value)if(row.action_id==action){if(row.actor_id!=actor)return fail<std::optional<NavalJourneyTermination>>(source);return {true,row,{}};}return {true,std::nullopt,{}};
}
}
