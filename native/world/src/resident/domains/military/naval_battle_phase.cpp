#include "resident/domains/military/military_model.hpp"
// 仍在结算页的战场必须先完成真实请求或明确放弃；不能重放上一轮规避资产责任拒绝。
#include "naval_battle_phase.hpp"
#include "naval_plan_command.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
Status preflight_naval_battles(const CommonRead& common,const NavalState& naval,const NavalCatalog& ships,Integer tick){
 if(!ships.combat_policy)return common_failure("NavalCombatProfileRequired");if(common.tick()==std::numeric_limits<Integer>::max()||tick!=common.tick()+1)return common_failure("NavalHalfDayRequired");
 for(const auto& b:naval.battle.battles){if(b.frame.phase=="closed")continue;if(b.last_tick!=common.tick())return common_failure("NavalBattleTickMismatch",b.frame.id);if(b.frame.phase=="settlement")return common_failure("NavalSettlementPending",b.frame.id);if(b.frame.phase!="planning")return common_failure("InvalidNavalBattlePhase",b.frame.id);if(!b.frame.controller_actor_id.empty()&&!b.frame.plan_submitted)return common_failure("NavalPlanPending",b.frame.id);auto plan=validate_naval_pending_plan(naval,b);if(!plan.ok)return plan;}
 return common_success();
}
// 每场仅一次真实供给后的回合写入，失败由原整体候选回滚所有已处理战场。
Outcome<std::vector<StableId>> step_naval_battles(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,NavalState& naval,const NavalCatalog& ships,Integer tick,const NavalCapturedJourneyEnd& end_journey,const MilitaryTravelEndWithSource& end_with_source){
 auto ready=preflight_naval_battles(common,naval,ships,tick);if(!ready.ok)return {false,{},ready.error};std::vector<StableId> ids,out;for(const auto& b:naval.battle.battles)if(b.frame.phase=="planning"&&b.created_tick<=common.tick())ids.push_back(b.frame.id);std::sort(ids.begin(),ids.end());for(const auto& id:ids){auto step=apply_naval_round(common,military,commands,access,naval,ships,id,tick,end_journey,end_with_source);if(!step.ok)return {false,{},step.error};out.push_back(step.value.id);}return {true,std::move(out),{}};
}
}
