// 只有原军级统帅实际所属部队的本轮退出/阵亡/被俘/溃退才触发；一次源跨轮保留。
#include "military_command_shock.hpp"
#include "military_commander.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>bool has(const std::vector<T>& rows,const T& value){return std::find(rows.begin(),rows.end(),value)!=rows.end();}
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(const auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
// 退出的条件与原round提交完全相同；此处只标实际已到出口的已接受命令，不另移动部队。
bool leaving(const BattleUnit& u,const MilitaryOrders& orders,const BattlefieldDefinition& board){
 if(u.withdrawn||u.fit.empty()||u.morale<=0||military_commander_loss(u)==2)return false;
 for(const auto& order:orders)if(order.first==u.unit_id&&order.second.action=="withdraw")for(const auto& e:board.exits)if(e.side==u.side&&e.position==u.position)return true;return false;
}
bool source_kind(MilitaryFieldCommandSourceKind k){return k==MilitaryFieldCommandSourceKind::army_formation||k==MilitaryFieldCommandSourceKind::field_assignment;}
bool same_members(const BattleUnit& a,const BattleUnit& b){return a.unit_id==b.unit_id&&a.army_id==b.army_id&&a.commander_actor_id==b.commander_actor_id&&a.side==b.side&&a.member_ids==b.member_ids;}
// 原本已经退离/被击溃的入场快照不再制造新震动；人物不在队列时只认独立军官标记。
StableId defeat(const BattleUnit& u,const StableId& actor){
 if(has(u.dead,actor)||(u.commander_actor_id==actor&&military_commander_loss(u)==2))return "commander_killed";
 if(has(u.captured,actor))return "commander_captured";if(has(u.deserted,actor))return "commander_deserted";
 if(u.withdrawn)return "unit_withdrawn";if(u.fit.empty())return "unit_defeated";if(u.morale==0)return "unit_routed";return {};
}
}
Status validate_military_command_shock_rule(const MilitaryCommandShockRule& r){return r.profile=="army_commander_once_v1"&&r.morale_loss==10?common_success():common_failure("MilitaryCommandShockRuleMismatch");}
Status validate_military_command_shock_shape(const MilitaryBattleRecord& b,const MilitaryCommandShockState& state){
 std::set<StableId> armies,actors;StableId prior;
 for(const auto& leader:state.leaders){if(leader.army_id.empty()||leader.actor_id.empty()||leader.command_source.id.empty()||!source_kind(leader.command_source.kind)||(!prior.empty()&&prior>=leader.army_id)||!armies.insert(leader.army_id).second||!actors.insert(leader.actor_id).second)return common_failure("MilitaryCommandShockLeaderShape",leader.army_id);prior=leader.army_id;bool present=false;StableId found;
  for(const auto& u:b.units)if(u.army_id==leader.army_id){present=true;if(u.commander_actor_id==leader.actor_id||has(u.member_ids,leader.actor_id)){if(!found.empty())return common_failure("MilitaryCommandShockLeaderAmbiguous",leader.actor_id);found=u.unit_id;}}
  if(!present||found!=leader.unit_id)return common_failure("MilitaryCommandShockLeaderUnitMismatch",leader.army_id);
 }
 std::set<StableId> actual;for(const auto& u:b.units)actual.insert(u.army_id);if(actual!=armies)return common_failure("MilitaryCommandShockArmyMissing",b.id);
 std::set<StableId> caused;Integer last=0;StableId last_army;
 for(const auto& cause:state.causes){const MilitaryCommandShockLeader* leader=nullptr;for(const auto& l:state.leaders)if(l.army_id==cause.army_id)leader=&l;const std::set<StableId> kinds{"commander_killed","commander_captured","commander_deserted","unit_withdrawn","unit_defeated","unit_routed"};if(!leader||leader->unit_id.empty()||cause.actor_id!=leader->actor_id||cause.unit_id!=leader->unit_id||!kinds.count(cause.kind)||cause.round<1||cause.round>b.round||cause.round<last||(cause.round==last&&cause.army_id<=last_army)||!caused.insert(cause.army_id).second)return common_failure("MilitaryCommandShockCauseShape",cause.army_id);last=cause.round;last_army=cause.army_id;}
 return common_success();
}
namespace {
Outcome<MilitaryCommandShockResolution> resolve_shock(const MilitaryBattleRecord& before,const MilitaryBattleRecord& after,const MilitaryOrders& orders,const MilitaryBattleCatalog& cat,const MilitaryCommandShockRule& rule,const MilitaryCommandShockState& state,bool sequential){
 using R=MilitaryCommandShockResolution;auto fail=[](const StableId& code,const StableId& id)->Outcome<R>{return {false,{},common_failure(code,id).error};};auto valid=validate_military_command_shock_rule(rule);if(!valid.ok)return {false,{},valid.error};valid=validate_military_command_shock_shape(before,state);if(!valid.ok)return {false,{},valid.error};const auto* board=find(cat.battlefields,before.battlefield_id);
 if(!board||before.id.empty()||before.id!=after.id||before.battlefield_id!=after.battlefield_id||before.phase!="planning"||after.phase!="planning"||before.round!=after.round||before.units.size()!=after.units.size())return fail("MilitaryCommandShockRoundMismatch",before.id);
 for(const auto& u:after.units){const auto* old=unit(before,u.unit_id);if(!old||!same_members(*old,u))return fail("MilitaryCommandShockRosterMismatch",u.unit_id);}
 R out{after,state,{}};std::set<StableId> used;for(const auto& f:state.causes){if(f.round>before.round||(!sequential&&f.round==before.round))return fail("MilitaryCommandShockPriorCauseMismatch",f.army_id);used.insert(f.army_id);}
 for(const auto& leader:state.leaders){if(leader.unit_id.empty()||used.count(leader.army_id))continue;const auto* old=unit(before,leader.unit_id);const auto* next=unit(after,leader.unit_id);if(!old||!next)return fail("MilitaryCommandShockLeaderUnitMismatch",leader.unit_id);if(!defeat(*old,leader.actor_id).empty())return fail("MilitaryCommandShockPriorCauseMissing",leader.army_id);auto kind=defeat(*next,leader.actor_id);if(kind.empty()&&leaving(*next,orders,*board))kind="unit_withdrawn";if(kind.empty())continue;
  out.state.causes.push_back({leader.army_id,leader.actor_id,leader.unit_id,kind,before.round,{}});
  for(auto& target:out.battle.units){if(target.army_id!=leader.army_id||target.withdrawn||target.fit.empty()||target.morale<=0||military_commander_loss(target)==2||leaving(target,orders,*board))continue;const auto original=target.morale;target.morale=std::max<Integer>(0,original-rule.morale_loss);MilitaryBattleEvent event;event.kind="command_shock";event.unit_id=target.unit_id;event.target_id=leader.unit_id;event.reason=kind;event.morale=target.morale-original;event.code="MilitaryArmyCommanderLost";out.events.push_back(std::move(event));}
 }
 if(sequential)std::sort(out.state.causes.begin(),out.state.causes.end(),[](const auto& a,const auto& b){return a.round!=b.round?a.round<b.round:a.army_id<b.army_id;});
 valid=validate_military_command_shock_shape(out.battle,out.state);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
}
}
Outcome<MilitaryCommandShockResolution> resolve_military_command_shock(const MilitaryBattleRecord& before,const MilitaryBattleRecord& after,const MilitaryOrders& orders,const MilitaryBattleCatalog& cat,const MilitaryCommandShockRule& rule,const MilitaryCommandShockState& state){return resolve_shock(before,after,orders,cat,rule,state,false);}
Outcome<MilitaryCommandShockResolution> resolve_military_sequential_command_shock(const MilitaryBattleRecord& before,const MilitaryBattleRecord& after,const MilitaryOrders& orders,const MilitaryBattleCatalog& cat,const MilitaryCommandShockRule& rule,const MilitaryCommandShockState& state){
 if(!cat.sequential_round_rule||cat.sequential_round_rule->profile!="entry_initiative_05_v1"||!before.commanded_damage||!before.rout_retreat_enabled)return {false,{},common_failure("MilitarySequentialPrerequisiteRequired",before.id).error};
 return resolve_shock(before,after,orders,cat,rule,state,true);
}

}
