// 新溃退立即停攻，至本人排定行动才自动退路；原进度状态和唯一俘虏事件沿M10永久帧。
#include "military_sequential_rout.hpp"
#include "military_sequential_rounds.hpp"
#include "military_rout_status.hpp"
#include "military_commander.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
Status synchronize_military_sequential_rout(MilitaryBattleRecord& b,const MilitaryBattleCatalog& c){
 if(!military_sequential_round_in_use(b,c))return common_failure("MilitarySequentialPrerequisiteRequired",b.id);
 std::vector<MilitaryRoutProgress> progress;
 for(const auto& u:b.units){if(u.withdrawn||u.fit.empty()||u.morale>0)continue;std::optional<Integer> started,blocked;
  for(const auto& s:u.statuses){if(s.first=="military_rout_started_v1")started=s.second;else if(s.first=="military_rout_blocked_v1")blocked=s.second;}
  if(started.has_value()!=blocked.has_value())return common_failure("MilitaryRoutProgressMissing",u.unit_id);
  if(started)progress.push_back({u.unit_id,*started,*blocked});else{progress.push_back({u.unit_id,b.round,0});MilitaryBattleEvent e;e.kind="rout_started";e.unit_id=u.unit_id;e.position=u.position;e.code="MilitaryRoutRetreatPending";b.last_events.push_back(std::move(e));}
 }
 return write_military_rout_progress(b,progress);
}
Outcome<MilitaryRoutDisposition> resolve_military_sequential_rout_turn(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& id){
 using R=MilitaryRoutDisposition;if(!military_sequential_round_in_use(b,c))return {false,{},common_failure("MilitarySequentialPrerequisiteRequired",b.id).error};const auto* rule=military_rout_rule_in_use(b,c);if(!rule)return {false,{},common_failure("MilitaryRoutProfileRequired",id).error};
 auto state=read_military_rout_progress(b,c);if(!state.ok)return {false,{},state.error};auto q=quote_military_rout_retreat(b,c,*rule,id);if(!q.ok)return {false,{},q.error};R out{b,std::move(state.value),{}};BattleUnit* unit=nullptr;for(auto& u:out.battle.units)if(u.unit_id==id)unit=&u;auto row=std::find_if(out.progress.begin(),out.progress.end(),[&](const auto& r){return r.unit_id==id;});if(!unit||row==out.progress.end())return {false,{},common_failure("MilitaryRoutProgressMissing",id).error};
 unit->position=q.value.destination;unit->heading=q.value.heading;const bool moved=unit->position!=q.value.origin;MilitaryBattleEvent event;event.kind="rout_retreat";event.unit_id=id;event.position=unit->position;
 if(q.value.at_exit){unit->withdrawn=true;out.battle.objective_state.escaped_unit_ids.push_back(id);std::sort(out.battle.objective_state.escaped_unit_ids.begin(),out.battle.objective_state.escaped_unit_ids.end());out.progress.erase(row);event.code="MilitaryRoutReachedExit";}
 else{row->blocked_rounds=moved?0:std::min(rule->blocked_rounds_before_capture,row->blocked_rounds+1);const BattleUnit* captor=nullptr;
  if(!moved&&!q.value.exit_reachable&&row->blocked_rounds==rule->blocked_rounds_before_capture)for(const auto& candidate:q.value.adjacent_captor_unit_ids){for(const auto& u:b.units)if(u.unit_id==candidate&&!u.withdrawn&&!u.fit.empty()&&u.morale>0&&military_commander_loss(u)!=2&&u.side!=unit->side){captor=&u;break;}if(captor)break;}
  if(captor){std::vector<StableId> taken=unit->fit;taken.insert(taken.end(),unit->wounded.begin(),unit->wounded.end());unit->captured.insert(unit->captured.end(),taken.begin(),taken.end());unit->fit.clear();unit->wounded.clear();out.progress.erase(row);event.kind="rout";event.captured=Integer(taken.size());event.deserted=0;event.captured_ids=std::move(taken);event.captor_unit_id=captor->unit_id;event.captor_army_id=captor->army_id;event.code="MilitaryRoutBlockedCaptured";}
  else event.code=moved?"MilitaryRoutRetreatMoved":"MilitaryRoutRetreatBlocked";
 }
 auto valid=write_military_rout_progress(out.battle,out.progress);if(!valid.ok)return {false,{},valid.error};out.events.push_back(std::move(event));return {true,std::move(out),{}};
}
}
