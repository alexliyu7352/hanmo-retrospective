// 请求裁定复用当前公开局面和原六角报价，不读玩家草案、不抽随机、不提前结算响应动作。
#include "battle_requests.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 所有引用只在本次纯读期间有效，不缓存跨事务对象。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 战斗单位按其业务ID查找，不把数组序当作身份。
const BattleUnit* unit(const MilitaryBattleRecord& battle,const StableId& id){for(const auto& row:battle.units)if(row.unit_id==id)return &row;return nullptr;}
// 有效响应者必须仍在本场战斗且未溃散。
bool active(const BattleUnit& value){return !value.withdrawn&&!value.fit.empty()&&value.morale>0;}
// 输入名册保持显式，不把空字符串视为成员。
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
// 非法输入沿原规则错误出口返回；有效请求被统帅拒绝则另返普通裁定。
Outcome<MilitaryRequestDecision> error(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// 回覆保原意图；没有实际合法响应时不伪造AI动作。
Outcome<MilitaryRequestDecision> refuse(const MilitaryBattleRequest& request,const StableId& reason){MilitaryRequestDecision decision;decision.request_kind=request.request_kind;decision.reason=reason;return {true,std::move(decision),{}};}
// 接受仍只是候选，世界层须与回执及计划在同一事务发布。
Outcome<MilitaryRequestDecision> accept(const MilitaryBattleRequest& request,const StableId& reason,const StableId& responder,const MilitaryOrder& order){MilitaryRequestDecision decision;decision.accepted=true;decision.request_kind=request.request_kind;decision.reason=reason;decision.response_unit_id=responder;decision.response_order=order;return {true,std::move(decision),{}};}
// 冻结AI名单是响应的必要条件；玩家单位不因请求转给AI。
bool has_ai_plan(const MilitaryBattleRecord& battle,const StableId& id){for(const auto& row:battle.ai_plans)if(row.first==id)return true;return false;}
// 当前明确守点或撤离目标不为另一条提议被暗中放弃。
bool committed_to_objective(const MilitaryBattleRecord& battle,const BattleObjectiveDefinition& goal,const BattleUnit& value){if(value.side!=goal.side)return false;if(goal.kind=="hold"&&goal.point&&value.position==*goal.point)return true;return contains(battle.objective_unit_ids,value.unit_id)&&(goal.kind=="escort"||goal.kind=="breakthrough"||goal.kind=="cover_retreat");}
// 撤退意愿沿原AI的低士气与撤离目标条件，未新增任意概率或处分。
bool withdrawal_intended(const MilitaryBattleRecord& battle,const BattleObjectiveDefinition& goal,const BattleUnit& value){if(value.morale<25)return true;if(value.side!=goal.side)return false;if(goal.kind=="breakthrough"||contains(battle.objective_unit_ids,value.unit_id))return true;if(goal.kind!="cover_retreat")return false;for(const auto& id:battle.objective_unit_ids){const auto* target=unit(battle,id);if(!target||!target->withdrawn)return false;}return true;}
// 本轮原路径必须真实到达己方出口；同意不产生传送或即时退出。
bool reachable_exit(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog,const BattlefieldDefinition& board,const BattleUnit& value){auto options=military_movement_options(battle,catalog,value.unit_id);if(!options.ok)return false;for(const auto& exit:board.exits)if(exit.side==value.side)for(const auto& path:options.value)if(path.position==exit.position)return true;return false;}
}
// 三种请求均只使用已经冻结的公开战场；非法权源范围不能退化成一般拒绝而被持久接纳。
Outcome<MilitaryRequestDecision> consider_military_request(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog,const MilitaryBattleRequest& request,const std::vector<StableId>& authorized_ai_units,const MilitaryDamageSources& sources){
 auto valid=validate_military_battle(battle,catalog,true);if(!valid.ok)return {false,{},valid.error};
 if(battle.phase!="planning"||battle.plan_submitted)return error("MilitaryRequestBoundaryClosed");
 if(battle.command_scope!="unit"||battle.controller_actor_id.empty())return error("MilitaryRequestRequiresSubordinate");
 if(!request.fields_valid)return error("InvalidMilitaryRequest");
 if(request.battle_id!=battle.id||request.request_key.empty()||request.request_key.size()>4096)return error("InvalidMilitaryRequest");
 if(request.request_kind!="support"&&request.request_kind!="withdrawal"&&request.request_kind!="focus_target")return error("UnknownMilitaryRequest");
 const auto* requester=unit(battle,request.unit_id);
 if(!requester||!active(*requester)||!contains(battle.command_unit_ids,request.unit_id)||requester->commander_actor_id!=battle.controller_actor_id)return error("MilitaryUnitUnauthorized",request.unit_id);
 const auto* goal=find(catalog.objectives,battle.objective_id);const auto* board=find(catalog.battlefields,battle.battlefield_id);
 const BattleUnit* target=nullptr;
 if(request.request_kind=="focus_target"){target=unit(battle,request.target_unit_id);if(!target||!active(*target)||target->side==requester->side)return error("MilitaryRequestEnemyRequired",request.target_unit_id);}
 else if(!request.target_unit_id.empty())return error("MilitaryRequestUnexpectedTarget");
 std::set<StableId> responders;
 if(authorized_ai_units.size()>64)return error("MilitaryRequestInvalidAuthority");
 for(const auto& id:authorized_ai_units){const auto* responder=unit(battle,id);if(!responder||responder->army_id!=requester->army_id||responder->side!=requester->side||contains(battle.command_unit_ids,id)||!has_ai_plan(battle,id)||!responders.insert(id).second)return error("MilitaryRequestInvalidAuthority",id);}
 if(request.request_kind=="withdrawal"){
  if(!goal->allow_withdrawal)return refuse(request,"MilitaryWithdrawalForbidden");
  if(!withdrawal_intended(battle,*goal,*requester))return refuse(request,"MilitaryCommanderMaintainsObjective");
  if(!reachable_exit(battle,catalog,*board,*requester))return refuse(request,"MilitaryRetreatExitUnreachable");
 }
 if(request.request_kind=="support"||request.request_kind=="withdrawal"){
  for(const auto& id:responders){const auto* responder=unit(battle,id);if(!active(*responder)||committed_to_objective(battle,*goal,*responder))continue;const auto* troop=find(catalog.troop_types,responder->troop_type_id);std::vector<const MilitaryTactic*> tactics;for(const auto& tactic:troop->tactics)if(tactic.kind=="support")tactics.push_back(&tactic);std::sort(tactics.begin(),tactics.end(),[](const auto* a,const auto* b){return a->id<b->id;});
   for(const auto* tactic:tactics){MilitaryOrder order;order.action="tactic";order.destination=responder->position;order.heading=responder->heading;order.target_id=request.unit_id;order.tactic_id=tactic->id;auto quote=preview_military_order(battle,catalog,id,order,sources);if(quote.ok)return accept(request,request.request_kind=="withdrawal"?"MilitaryWithdrawalApprovedWithCover":"MilitarySupportAccepted",id,quote.value.order);}
  }
  return refuse(request,"MilitaryNoLegalSupport");
 }
 bool found=false;Integer best_damage=-1,best_cost=0;StableId best_unit;MilitaryOrder best_order;
 for(const auto& id:responders){const auto* responder=unit(battle,id);if(!active(*responder)||committed_to_objective(battle,*goal,*responder))continue;auto positions=military_movement_options(battle,catalog,id);if(!positions.ok)return {false,{},positions.error};std::sort(positions.value.begin(),positions.value.end(),[](const auto& a,const auto& b){return a.position.q!=b.position.q?a.position.q<b.position.q:a.position.r<b.position.r;});
  for(const auto& position:positions.value){MilitaryOrder order;order.action="attack";order.destination=position.position;order.heading=military_hex::direction(position.position,target->position);order.target_id=target->unit_id;auto quote=preview_military_order(battle,catalog,id,order,sources);if(!quote.ok)continue;const auto damage=quote.value.casualties_min.value_or(0)+quote.value.casualties_max.value_or(0);if(!found||damage>best_damage||(damage==best_damage&&quote.value.cost<best_cost)){found=true;best_damage=damage;best_cost=quote.value.cost;best_unit=id;best_order=quote.value.order;}}
 }
 return found?accept(request,"MilitaryFocusAccepted",best_unit,best_order):refuse(request,"MilitaryNoLegalAttack");
}
}
