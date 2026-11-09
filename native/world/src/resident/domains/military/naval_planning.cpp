// 可重建AI只读本轮公开局势；既存计划是轮次事实，不在玩家草案变化后偷偷重算。
#include "naval_planning.hpp"
#include "naval_commander.hpp"
#include "naval_gunnery_rules.hpp"
#include "naval_gunnery_source.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 只在当前已验证目录中查具名对象。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& frame,const StableId& id){for(const auto& u:frame.units)if(u.unit_id==id)return &u;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 新作者策略只协调已经冻结的同轮AI路径；不读取玩家计划，原占格优先仍由同步微步裁定。
bool clashes(const std::vector<HexPoint>& path,const std::vector<std::vector<HexPoint>>& chosen){for(const auto& previous:chosen){const auto count=std::max(path.size(),previous.size());for(std::size_t tick=1;tick<count;++tick){const auto now=path[std::min(tick,path.size()-1)],old=path[std::min(tick-1,path.size()-1)],other=previous[std::min(tick,previous.size()-1)],other_old=previous[std::min(tick-1,previous.size()-1)];if(now==other||now==other_old||old==other)return true;}}return false;}
// 报价同时约束显式政策，不让未实现或未作者化的动作在提交后才失败。
Status allowed(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const BattlefieldDefinition& board,const NavalTacticalFrame& frame,const StableId& id,const NavalTacticalOrder& order,const MilitaryBattleCatalog& military,const NavalGunneryPreparation* prepared){
 if(!policy.combat.commander_losses&&!frame.units.empty())for(const auto& u:frame.units)if(!u.statuses.empty())return common_failure("NavalCommanderLossProfileRequired",u.unit_id);
 if(order.action=="withdraw"&&!policy.allow_withdrawal)return common_failure("NavalWithdrawalForbidden",id);
 if(order.action=="ram"){const NavalVesselRecord* vessel=nullptr;for(const auto& binding:frame.bindings)if(binding.unit_id==id)vessel=find(assets.vessels,binding.vessel_id);bool authored=false;if(vessel)for(const auto& r:policy.combat.ram)if(r.hull_id==vessel->hull_id)authored=true;if(!authored)return common_failure("NavalRamNotAuthored",id);}
 auto quote=preview_naval_tactical_order(assets,ships,board,frame,id,order,policy.combat.tactical_navigation?&*policy.combat.tactical_navigation:nullptr,policy.combat.gunnery?&*policy.combat.gunnery:nullptr);if(!quote.ok)return {false,{},quote.error};if(bool(policy.combat.gunnery)!=(prepared!=nullptr))return common_failure("NavalGunneryPreparationRequired",frame.id);if(order.action=="hull_attack"){auto at=frame;for(auto& u:at.units)if(u.unit_id==id){u.position=order.destination;u.heading=order.heading;}auto fire=preview_naval_gunnery(assets,ships,*policy.combat.gunnery,military,*prepared,at,id,order.target_unit_id);if(!fire.ok)return {false,{},fire.error};}return common_success();
}
}
// 原六角入口和出口必须为实际水面；没有显式入口的船不能借陆军入口出现在河里。
Status validate_naval_battle_policy(const NavalBattlePolicy& policy,const NavalAssetCatalog& ships,const MilitaryBattleCatalog& military){
 if(!policy.ai_maneuver_policy.empty()&&policy.ai_maneuver_policy!="alternating_approach_v1")return common_failure("UnknownNavalManeuverPolicy");
 if(policy.kind!="boarding_ram_v1"||policy.max_rounds<1||policy.max_rounds>9999||policy.ai_surrender_morale<0||policy.ai_surrender_morale>100)return common_failure("InvalidNavalBattlePolicy");
 auto checked=validate_naval_combat_rules(policy.combat,ships);if(!checked.ok)return checked;const auto* board=find(military.battlefields,policy.battlefield_id);if(!board)return common_failure("UnknownNavalBattlefield",policy.battlefield_id);checked=military_hex::validate(*board);if(!checked.ok)return checked;if(policy.combat.tactical_navigation){checked=validate_naval_tactical_navigation(*policy.combat.tactical_navigation,board);if(!checked.ok)return checked;}
 std::set<Integer> exits;for(const auto* rows:{&board->spawns,&board->exits})for(const auto& entry:*rows){if(military_hex::movement_cost(*board,entry.position,std::map<StableId,Integer>{{"water",1}})<0)return common_failure("NavalWaterEntryRequired",policy.battlefield_id);if(rows==&board->exits)exits.insert(entry.side);}
 if(policy.allow_withdrawal&&exits.size()!=2)return common_failure("NavalBothSideExitsRequired");return common_success();
}
// 单位稳定排序仲裁同分候选；低士气投降也是具名AI命令，不从船损直接产俘虏。
Outcome<NavalTacticalOrders> make_naval_ai(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& military,const NavalTacticalFrame& frame,const NavalGunneryPreparation* prepared){
 auto checked=validate_naval_battle_policy(policy,ships,military);if(!checked.ok)return {false,{},checked.error};if(bool(policy.combat.gunnery)!=(prepared!=nullptr))return fail<NavalTacticalOrders>("NavalGunneryPreparationRequired",frame.id);if(prepared){checked=validate_naval_gunnery_frame_source(*prepared,frame);if(!checked.ok)return {false,{},checked.error};}const auto* board=find(military.battlefields,policy.battlefield_id);if(frame.battlefield_id!=board->id||frame.phase!="planning"||frame.plan_submitted)return fail<NavalTacticalOrders>("NavalBattleNotPlanning");checked=validate_naval_tactical_frame(assets,ships,*board,frame);if(!checked.ok)return {false,{},checked.error};
 std::vector<StableId> ids;for(const auto& u:frame.units)ids.push_back(u.unit_id);std::sort(ids.begin(),ids.end());NavalTacticalOrders result;const bool alternating=policy.ai_maneuver_policy=="alternating_approach_v1";std::vector<std::vector<HexPoint>> chosen;
 for(const auto& id:ids){if(has(frame.controlled_unit_ids,id))continue;const auto* actor=unit(frame,id);NavalTacticalOrder best{"hold",{},actor->position,actor->heading};Integer best_score=-1000000;
  if(!actor->withdrawn){auto options=naval_movement_options(assets,ships,*board,frame,id,policy.combat.tactical_navigation?&*policy.combat.tactical_navigation:nullptr);if(!options.ok)return {false,{},options.error};
   for(const auto& enemy_id:ids){const auto* enemy=unit(frame,enemy_id);if(enemy->side==actor->side||enemy->withdrawn)continue;
    if(policy.ai_surrender_morale>0&&actor->morale<=policy.ai_surrender_morale&&enemy->morale>0&&enemy->fit.size()>actor->fit.size()){NavalTacticalOrder surrender{"surrender",enemy_id,actor->position,actor->heading};if(allowed(assets,ships,policy,*board,frame,id,surrender,military,prepared).ok){best=surrender;best_score=1000000;break;}}
    for(const auto& path:options.value){if(alternating&&((actor->side!=(frame.round-1)%2&&path.position!=actor->position)||(!policy.combat.tactical_navigation&&clashes(path.path,chosen))))continue;const auto distance=military_hex::distance(path.position,enemy->position);auto heading=military_hex::direction(path.position,enemy->position);NavalTacticalOrder move{path.position==actor->position?"hold":"move",{},path.position,heading};Integer score=-distance*100-path.cost;
     if(distance==1){NavalTacticalOrder board_order{"board",enemy_id,path.position,heading};if(allowed(assets,ships,policy,*board,frame,id,board_order,military,prepared).ok){move=board_order;score=10000-path.cost;}}
     if(policy.combat.gunnery&&prepared&&move.action!="board"){NavalTacticalOrder fire{"hull_attack",enemy_id,path.position,heading};if(allowed(assets,ships,policy,*board,frame,id,fire,military,prepared).ok){auto at=frame;for(auto& u:at.units)if(u.unit_id==id){u.position=path.position;u.heading=heading;}auto shot=preview_naval_gunnery(assets,ships,*policy.combat.gunnery,military,*prepared,at,id,enemy_id);if(!shot.ok)return {false,{},shot.error};move=fire;score=8000+std::min<Integer>(100,shot.value.hull_damage)+shot.value.personnel.potential-path.cost;}}
     // 新航向规则的最终朝向可能选择另一条同格路径；AI评分和避让必须核最终同一报价。
     if(policy.combat.tactical_navigation){auto actual=preview_naval_tactical_order(assets,ships,*board,frame,id,move,&*policy.combat.tactical_navigation,policy.combat.gunnery?&*policy.combat.gunnery:nullptr);if(!actual.ok||(alternating&&clashes(actual.value.path,chosen)))continue;score=move.action=="board"?10000-actual.value.cost:move.action=="hull_attack"?score+path.cost-actual.value.cost:-distance*100-actual.value.cost;}
     if(score>best_score&&allowed(assets,ships,policy,*board,frame,id,move,military,prepared).ok){best=std::move(move);best_score=score;}
    }
   }
  }
  checked=allowed(assets,ships,policy,*board,frame,id,best,military,prepared);if(!checked.ok)return {false,{},checked.error};if(alternating&&!actor->withdrawn){auto quote=preview_naval_tactical_order(assets,ships,*board,frame,id,best,policy.combat.tactical_navigation?&*policy.combat.tactical_navigation:nullptr,policy.combat.gunnery?&*policy.combat.gunnery:nullptr);if(!quote.ok)return {false,{},quote.error};chosen.push_back(std::move(quote.value.path));}result.emplace_back(id,std::move(best));
 }
 return {true,std::move(result),{}};
}
// 真实actor/控制集合已由World权源生成；纯提交层拒冒人、漏部队、混入AI和重复unit。
Outcome<NavalBattleRecord> submit_naval_plan(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& military,const NavalBattleRecord& current,const StableId& actor,const NavalTacticalOrders& orders,const NavalGunneryPreparation* prepared){
 const auto& frame=current.frame;if(prepared){auto checked=validate_naval_gunnery_frame_source(*prepared,frame);if(!checked.ok)return {false,{},checked.error};}if(actor.empty()||actor!=frame.controller_actor_id)return fail<NavalBattleRecord>("NavalCommanderUnauthorized");if(frame.phase!="planning"||frame.plan_submitted||!current.pending_orders.empty())return fail<NavalBattleRecord>("NavalPlanAlreadySubmitted");
 const auto* board=find(military.battlefields,policy.battlefield_id);if(!board)return fail<NavalBattleRecord>("UnknownNavalBattlefield");std::set<StableId> seen;
 for(const auto& p:orders){if(!has(frame.controlled_unit_ids,p.first)||!seen.insert(p.first).second)return fail<NavalBattleRecord>("NavalUnitUnauthorized",p.first);auto checked=allowed(assets,ships,policy,*board,frame,p.first,p.second,military,prepared);if(!checked.ok)return {false,{},checked.error};}
 if(seen.size()!=frame.controlled_unit_ids.size())return fail<NavalBattleRecord>("IncompleteNavalOrders");auto next=current;next.pending_orders=orders;std::sort(next.pending_orders.begin(),next.pending_orders.end(),[](const auto& a,const auto& b){return a.first<b.first;});next.frame.plan_submitted=true;
 auto checked=frozen_naval_orders(assets,ships,policy,military,next,prepared);if(!checked.ok)return {false,{},checked.error};return {true,std::move(next),{}};
}
// 这里合并已保存AI和已提交玩家计划，不调用AI函数，因此不能偷看新草案。
Outcome<NavalTacticalOrders> frozen_naval_orders(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& military,const NavalBattleRecord& battle,const NavalGunneryPreparation* prepared){
 auto checked=validate_naval_battle_policy(policy,ships,military);if(!checked.ok)return {false,{},checked.error};const auto* board=find(military.battlefields,policy.battlefield_id);const auto& frame=battle.frame;if(bool(policy.combat.gunnery)!=(prepared!=nullptr))return fail<NavalTacticalOrders>("NavalGunneryPreparationRequired",frame.id);if(prepared){checked=validate_naval_gunnery_frame_source(*prepared,frame);if(!checked.ok)return {false,{},checked.error};}
 if(frame.phase!="planning"||frame.battlefield_id!=board->id||(!frame.controller_actor_id.empty()&&!frame.plan_submitted))return fail<NavalTacticalOrders>("NavalPlanRequired");if(frame.controller_actor_id.empty()&&(!frame.controlled_unit_ids.empty()||!battle.pending_orders.empty()))return fail<NavalTacticalOrders>("NavalAutonomousAuthorityMismatch");
 std::set<StableId> seen;NavalTacticalOrders out;
 for(const auto* rows:{&battle.ai_orders,&battle.pending_orders})for(const auto& p:*rows){const auto* u=unit(frame,p.first);if(!u||!seen.insert(p.first).second||has(frame.controlled_unit_ids,p.first)!=(rows==&battle.pending_orders))return fail<NavalTacticalOrders>("NavalFrozenPlanMismatch",p.first);checked=allowed(assets,ships,policy,*board,frame,p.first,p.second,military,prepared);if(!checked.ok)return {false,{},checked.error};out.push_back(p);}
 if(seen.size()!=frame.units.size())return fail<NavalTacticalOrders>("IncompleteNavalOrders");std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.first<b.first;});return {true,std::move(out),{}};
}
}
