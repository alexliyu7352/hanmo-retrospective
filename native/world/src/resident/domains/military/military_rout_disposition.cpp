// 只有真实相邻存活敌部可接受被俘池；被堵两轮但无人看管不凭空成为俘虏或死者。
#include "military_rout_disposition.hpp"
#include "military_commander.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
namespace H=military_hex;
const BattleUnit* find(const MilitaryBattleRecord& b,const StableId& id){for(const auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
bool alive_force(const BattleUnit& u){return !u.withdrawn&&!u.fit.empty()&&u.morale>0&&military_commander_loss(u)!=2;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool owns_exit(const BattlefieldDefinition& b,const BattleUnit& u){for(const auto& e:b.exits)if(e.side==u.side&&e.position==u.position)return true;return false;}
}
Outcome<MilitaryRoutDisposition> resolve_military_rout_disposition(const MilitaryBattleRecord& before,const MilitaryBattleRecord& after,const MilitaryBattleCatalog& catalog,const MilitaryRoutRetreatRule& rule,const std::vector<MilitaryRoutProgress>& progress){
 using R=MilitaryRoutDisposition;const BattlefieldDefinition* board=nullptr;for(const auto& b:catalog.battlefields)if(b.id==before.battlefield_id)board=&b;
 if(!board||before.id.empty()||after.id!=before.id||after.battlefield_id!=before.battlefield_id||after.round!=before.round||before.phase!="planning"||after.phase!="planning"||before.units.size()!=after.units.size())return fail<R>("MilitaryRoutRoundSourceMismatch",before.id);auto checked=validate_military_rout_rule(rule,*board);if(!checked.ok)return {false,{},checked.error};
 std::map<StableId,MilitaryRoutProgress> sources;for(const auto& p:progress){const auto* u=find(before,p.unit_id);if(!u||u->withdrawn||u->fit.empty()||u->morale!=0||p.started_round<1||p.started_round>=before.round||p.blocked_rounds<0||p.blocked_rounds>rule.blocked_rounds_before_capture||!sources.emplace(p.unit_id,p).second)return fail<R>("MilitaryRoutProgressSourceMismatch",p.unit_id);}
 R out{after,{},{}};std::set<StableId> seen;
 for(auto& unit:out.battle.units){
  const auto* old=find(before,unit.unit_id);if(!old||!seen.insert(unit.unit_id).second||old->army_id!=unit.army_id||old->side!=unit.side||old->commander_actor_id!=unit.commander_actor_id||old->member_ids!=unit.member_ids)return fail<R>("MilitaryRoutRosterChanged",unit.unit_id);
  const bool previously_routed=!old->withdrawn&&!old->fit.empty()&&old->morale==0;
  if(previously_routed!=bool(sources.count(unit.unit_id)))return fail<R>("MilitaryRoutProgressMissing",unit.unit_id);
  if(unit.withdrawn||unit.fit.empty())continue;
  if(!previously_routed){if(unit.morale!=0)continue;out.progress.push_back({unit.unit_id,before.round,0});MilitaryBattleEvent event;event.kind="rout_started";event.unit_id=unit.unit_id;event.position=unit.position;event.code="MilitaryRoutRetreatPending";out.events.push_back(std::move(event));continue;}
  // 不允许等待/防御、原缺粮下限或友军鼓舞把正在溃退者直接恢复作战。
  if(unit.morale!=0)return fail<R>("MilitaryRoutCannotResumeAttack",unit.unit_id);auto quote=quote_military_rout_retreat(before,catalog,rule,unit.unit_id);if(!quote.ok)return {false,{},quote.error};
  if(std::find(quote.value.path.begin(),quote.value.path.end(),unit.position)==quote.value.path.end())return fail<R>("MilitaryRoutMovementSourceMismatch",unit.unit_id);
  if(owns_exit(*board,unit)){unit.withdrawn=true;if(std::find(out.battle.objective_state.escaped_unit_ids.begin(),out.battle.objective_state.escaped_unit_ids.end(),unit.unit_id)!=out.battle.objective_state.escaped_unit_ids.end())return fail<R>("MilitaryRoutExitRepeated",unit.unit_id);out.battle.objective_state.escaped_unit_ids.push_back(unit.unit_id);MilitaryBattleEvent event;event.kind="rout_retreat";event.unit_id=unit.unit_id;event.position=unit.position;event.code="MilitaryRoutReachedExit";out.events.push_back(std::move(event));continue;}
  auto current=sources.at(unit.unit_id);const bool moved=unit.position!=old->position;current.blocked_rounds=moved?0:std::min(rule.blocked_rounds_before_capture,current.blocked_rounds+1);const BattleUnit* captor=nullptr;
  if(!moved&&!quote.value.exit_reachable&&current.blocked_rounds==rule.blocked_rounds_before_capture)for(const auto& id:quote.value.adjacent_captor_unit_ids){const auto* candidate=find(after,id);if(candidate&&candidate->side!=unit.side&&alive_force(*candidate)&&H::distance(candidate->position,unit.position)==1){captor=candidate;break;}}
  if(captor){std::vector<StableId> taken=unit.fit;taken.insert(taken.end(),unit.wounded.begin(),unit.wounded.end());for(const auto& id:taken)if(std::find(unit.captured.begin(),unit.captured.end(),id)!=unit.captured.end())return fail<R>("MilitaryRoutCapturedTwice",id);unit.captured.insert(unit.captured.end(),taken.begin(),taken.end());unit.fit.clear();unit.wounded.clear();MilitaryBattleEvent event;event.kind="rout";event.unit_id=unit.unit_id;event.captured=Integer(taken.size());event.deserted=0;event.captured_ids=std::move(taken);event.captor_unit_id=captor->unit_id;event.captor_army_id=captor->army_id;event.code="MilitaryRoutBlockedCaptured";out.events.push_back(std::move(event));}
  else{out.progress.push_back(current);MilitaryBattleEvent event;event.kind="rout_retreat";event.unit_id=unit.unit_id;event.position=unit.position;event.code=moved?"MilitaryRoutRetreatMoved":"MilitaryRoutRetreatBlocked";out.events.push_back(std::move(event));}
 }
 std::sort(out.progress.begin(),out.progress.end(),[](const auto& a,const auto& b){return a.unit_id<b.unit_id;});std::sort(out.battle.objective_state.escaped_unit_ids.begin(),out.battle.objective_state.escaped_unit_ids.end());return {true,std::move(out),{}};
}
}
