// 同一原battle候选逐实际先后执行；零兵/已退/新溃退者不能补原攻击，最终仍只生成原一次round。
#include "military_attack_damage_core.hpp"
#include "military_sequential_execution.hpp"
#include "military_sequential_rout.hpp"
#include "military_command_shock.hpp"
#include "military_command_resources.hpp"
#include "military_field_terrain.hpp"
#include "military_commander.hpp"
#include "military_rout_orders.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
namespace D=military_sequential_detail;
BattleUnit* unit(MilitaryBattleRecord& b,const StableId& id){for(auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(const auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
void status(BattleUnit& u,const StableId& key,Integer until){for(auto& s:u.statuses)if(s.first==key){s.second=std::max(s.second,until);return;}u.statuses.emplace_back(key,until);}
bool active(const BattleUnit& u){return !u.withdrawn&&!u.fit.empty()&&u.morale>0&&military_commander_loss(u)!=2;}
// 每次真实攻击以当刻有效池分出60%伤与余数死，不触及既有伤/俘/离队者。
void damage(BattleUnit& u,Integer count,Integer extra_morale=0){const Integer before=u.fit.size();count=std::clamp<Integer>(count,0,before);if(!count)return;auto ids=u.fit;std::sort(ids.begin(),ids.end());for(Integer i=0;i<count;++i){u.fit.erase(std::find(u.fit.begin(),u.fit.end(),ids[i]));(original_military_casualty_is_wounded(i,count,true)?u.wounded:u.dead).push_back(ids[i]);}u.morale=std::max<Integer>(0,u.morale-original_military_casualty_morale_loss(count,before,true)-extra_morale);}
Outcome<MilitaryBattleRecord> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
}
Outcome<MilitaryBattleRecord> resolve_military_sequential_round(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& actor,const MilitaryOrders& orders,const MilitaryBattleRandom& random,const MilitaryDamageSources& sources){
 auto valid=validate_military_battle(b,c,true);if(!valid.ok)return {false,{},valid.error};auto queue=query_military_sequential_turns(b,c);if(!queue.ok)return {false,{},queue.error};auto player=validate_military_orders(b,c,actor,orders);if(!player.ok)return {false,{},player.error};
 MilitaryOrders plans=std::move(player.value);plans.insert(plans.end(),b.ai_plans.begin(),b.ai_plans.end());std::map<StableId,MilitaryOrder> selected;std::map<StableId,std::vector<HexPoint>> paths;
 for(const auto& p:plans){if(!selected.emplace(p.first,p.second).second)return fail("MilitarySequentialRepeatedOrder",p.first);auto quoted=preview_military_order(b,c,p.first,p.second,sources);if(!quoted.ok)return {false,{},quoted.error};paths.emplace(p.first,std::move(quoted.value.path));}
 MilitaryBattleRecord next=b;next.last_events.clear();std::map<StableId,Integer> guards,incoming;std::map<StableId,std::vector<StableId>> supports;std::set<StableId> consumed;const auto* board=find(c.battlefields,b.battlefield_id);
 for(const auto& turn:queue.value){
  auto* u=unit(next,turn.unit_id);if(!u)return fail("MilitarySequentialUnitSourceMismatch",turn.unit_id);
  if(u->withdrawn||u->fit.empty()||(military_commander_loss(*u)==2&&!military_unit_routing(*u))){MilitaryBattleEvent e;e.kind="action_cancelled";e.unit_id=turn.unit_id;e.code="MilitarySequentialUnitUnavailable";next.last_events.push_back(std::move(e));continue;}
  MilitaryBattleRecord before=next;MilitaryOrders actual;
  // 佯退风险持续到本人下一实际行动开始，跨轮保存仍由原状态事实回放。
  if(next.learned_tactics)u->statuses.erase(std::remove_if(u->statuses.begin(),u->statuses.end(),[](const auto& s){return s.first=="military_feint_until_turn_v1";}),u->statuses.end());
  // 已跳过者不恢复；每个实际排到的本部回合只在此写一次，溃退回合仍无战法权限。
  if(next.command_resources){auto budget=query_military_command_resources(next,*next.command_resources,u->unit_id);if(!budget.ok)return {false,{},budget.error};u->tactic_points=budget.value.available;}

  if(military_unit_routing(*u)){
   auto routed=resolve_military_sequential_rout_turn(next,c,u->unit_id);if(!routed.ok)return {false,{},routed.error};next=std::move(routed.value.battle);next.last_events.insert(next.last_events.end(),routed.value.events.begin(),routed.value.events.end());
  }else{
   auto it=selected.find(u->unit_id);if(it==selected.end())return fail("MissingMilitaryUnitOrder",u->unit_id);const auto& p=it->second;const auto* troop=find(c.troop_types,u->troop_type_id);const auto* t=p.action=="tactic"?find(troop->tactics,p.tactic_id):nullptr;const StableId kind=t?t->kind:StableId{};actual.emplace_back(u->unit_id,p);
   const bool terrain_retreat=paths.at(u->unit_id).size()>1&&military_field_retreat_required(next,c,u->unit_id,kind);auto moved=D::move_committed_path(next,c,u->unit_id,paths.at(u->unit_id),kind);if(!moved.ok)return {false,{},moved.error};u=unit(next,turn.unit_id);u->heading=p.heading;
   if(p.action=="wait"||p.action=="defend"){if(!next.command_resources)u->tactic_points=std::min(troop->tactic_capacity,u->tactic_points+(p.action=="wait"?3:2));u->morale=std::min<Integer>(100,u->morale+(p.action=="wait"?1:3));if(p.action=="defend")guards[u->unit_id]=1;}
   if(t){if(u->tactic_points<t->point_cost)return fail("NotEnoughMilitaryTacticPoints",u->unit_id);u->tactic_points-=t->point_cost;
    if(kind=="rally"){for(auto& ally:next.units)if(active(ally)&&ally.side==u->side&&military_hex::distance(ally.position,u->position)<=1&&(!next.learned_tactics||(ally.unit_id!=u->unit_id&&std::none_of(ally.statuses.begin(),ally.statuses.end(),[&](const auto& s){return s.first=="military_rally_received_v1"&&s.second==next.round;})))){ally.morale=std::min<Integer>(100,ally.morale+t->morale_effect);if(next.learned_tactics)status(ally,"military_rally_received_v1",next.round);else status(ally,"inspired",next.round+t->status_rounds-1);MilitaryBattleEvent e;e.kind="rally";e.unit_id=u->unit_id;e.target_id=ally.unit_id;e.morale=ally.morale;next.last_events.push_back(std::move(e));}}
    else if(kind=="charge")status(*u,"exposed",next.round+t->status_rounds);
    else if(kind=="feint"){incoming[u->unit_id]=11000;if(next.learned_tactics)status(*u,"military_feint_until_turn_v1",next.round);}
    else if(kind=="guard")guards[u->unit_id]=2;
    else if(kind=="reform"){u->statuses.erase(std::remove_if(u->statuses.begin(),u->statuses.end(),[](const auto& s){return s.first=="confused";}),u->statuses.end());MilitaryBattleEvent e;e.kind="reform";e.unit_id=u->unit_id;e.tactic_id=t->id;next.last_events.push_back(std::move(e));}
    else if(kind=="support"){auto* target=unit(next,p.target_id);if(target&&active(*target)&&target->side==u->side&&military_hex::distance(target->position,u->position)==1)supports[target->unit_id].push_back(u->unit_id);}
   }
   if(p.action=="attack"||kind=="charge"||kind=="volley"||kind=="flank"||kind=="suppress"){
    auto quote=terrain_retreat?Outcome<MilitaryAttackQuote>{false,{},common_failure("MilitaryZoneRetreatCannotAttack",u->unit_id).error}:D::attack_quote(next,c,u->unit_id,p,guards[p.target_id],moved.value,incoming.count(p.target_id)?incoming[p.target_id]:10000,sources);MilitaryBattleEvent event;event.kind="attack";event.unit_id=u->unit_id;event.target_id=p.target_id;event.tactic_id=p.tactic_id;event.casualties=0;
    if(!quote.ok)event.code=quote.error.original.code;
    else{auto* target=unit(next,p.target_id);const Integer count=std::min<Integer>(quote.value.potential,target->fit.size());Integer share=0;BattleUnit* supporter=nullptr;
     if(count>0&&!consumed.count(p.target_id)){consumed.insert(p.target_id);auto candidates=supports[p.target_id];std::sort(candidates.begin(),candidates.end());for(const auto& id:candidates){auto* candidate=unit(next,id);if(candidate&&active(*candidate)&&candidate->side==target->side&&military_hex::distance(candidate->position,target->position)==1){supporter=candidate;break;}}if(supporter)share=original_military_support_share(count,Integer(supporter->fit.size()));}
     damage(*target,count-share,t?t->morale_effect:0);if(count>0&&kind=="volley")status(*target,"suppressed",next.round+t->status_rounds);if(count>0&&kind=="suppress")status(*target,"hindered",next.round+1);event.casualties=count-share;event.hit_chance=100;event.code=count>0?"Hit":"Miss";
     next.last_events.push_back(std::move(event));if(share>0){damage(*supporter,share);MilitaryBattleEvent e;e.kind="support";e.unit_id=supporter->unit_id;e.target_id=p.target_id;e.attacker_id=u->unit_id;e.casualties=share;e.original_casualties=count;next.last_events.push_back(std::move(e));}event.kind.clear();
    }
    if(!event.kind.empty())next.last_events.push_back(std::move(event));
   }
   if(p.action=="withdraw"&&active(*u))for(const auto& exit:board->exits)if(exit.side==u->side&&exit.position==u->position){u->withdrawn=true;next.objective_state.escaped_unit_ids.push_back(u->unit_id);std::sort(next.objective_state.escaped_unit_ids.begin(),next.objective_state.escaped_unit_ids.end());MilitaryBattleEvent e;e.kind="withdraw";e.unit_id=u->unit_id;e.position=u->position;next.last_events.push_back(std::move(e));break;}
   // 原独立军官暴露仍读这一真实兵损差分；本显式策略在下一部行动前落定。
   if(board->commander_losses){valid=resolve_military_commander_losses(before,next,*board->commander_losses,random);if(!valid.ok)return {false,{},valid.error};}
  }
  if(next.command_shock){auto shock=resolve_military_sequential_command_shock(before,next,actual,c,*c.command_shock_rule,*before.command_shock);if(!shock.ok)return {false,{},shock.error};next=std::move(shock.value.battle);next.command_shock=std::move(shock.value.state);next.last_events.insert(next.last_events.end(),shock.value.events.begin(),shock.value.events.end());}
  valid=synchronize_military_sequential_rout(next,c);if(!valid.ok)return {false,{},valid.error};next.result=D::objective(next,c,false);
  if(!next.result&&!next.controller_actor_id.empty())for(const auto& row:next.units)if(row.commander_actor_id==next.controller_actor_id&&military_commander_loss(row)==2)next.result=MilitaryBattleResult{-1,"MilitaryCommanderKilled",{}};
  if(next.result)break;
 }
 if(!next.result)next.result=D::objective(next,c,true);
 if(next.result){next.phase="result";next.ai_plans.clear();MilitaryBattleEvent event;event.kind="result";event.winner_side=next.result->winner_side;event.reason=next.result->reason;next.last_events.push_back(std::move(event));}
 else{++next.round;for(auto& u:next.units)u.statuses.erase(std::remove_if(u.statuses.begin(),u.statuses.end(),[&](const auto& s){return s.first!="military_commander_loss_v1"&&s.first!="military_feint_until_turn_v1"&&s.first!="confused"&&!military_rout_status_key(s.first)&&s.second<next.round;}),u.statuses.end());auto ai=make_military_ai(next,c,sources);if(!ai.ok)return {false,{},ai.error};next.ai_plans=std::move(ai.value);}
 valid=validate_military_battle(next,c);if(!valid.ok)return {false,{},valid.error};return {true,std::move(next),{}};
}
}
