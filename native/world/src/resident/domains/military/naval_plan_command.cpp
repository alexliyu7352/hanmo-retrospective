// 即时军令不另建行动或推进时间；原业务键成功后永久重放同一决定。
#include "naval_plan_command.hpp"
#include "military_field_command_source.hpp"
#include "battle_world.hpp"
#include "resident/model/common_operations.hpp"
#include "naval_gunnery_source.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
const NavalBattleRecord* battle(const NavalState& state,const StableId& id){for(const auto& row:state.battle.battles)if(row.frame.id==id)return &row;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// 字典插入序不改变命令业务值，重复单位仍明确拒绝，不能由排序吞掉。
bool orders_equal(NavalTacticalOrders a,NavalTacticalOrders b){
 auto order=[](const auto& x,const auto& y){return x.first<y.first;};std::sort(a.begin(),a.end(),order);std::sort(b.begin(),b.end(),order);if(a.size()!=b.size())return false;
 for(std::size_t i=0;i<a.size();++i){if(i&&a[i-1].first==a[i].first)return false;const auto& x=a[i].second;const auto& y=b[i].second;if(a[i].first!=b[i].first||x.action!=y.action||x.target_unit_id!=y.target_unit_id||!(x.destination==y.destination)||x.heading!=y.heading)return false;}return true;
}
std::vector<StableId> sorted(std::vector<StableId> rows){std::sort(rows.begin(),rows.end());return rows;}
}
// 新请求重新检查真实任职或本人本部；重放只返回永久结果，不因后来失权再次执行。
Outcome<NavalPlanQuote> preview_naval_plan(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalPlanCommand& command){
 if(!ships.combat_policy)return fail<NavalPlanQuote>("NavalCombatProfileRequired");if(!naval_request_key_valid(command.request_key))return fail<NavalPlanQuote>("InvalidNavalRequestKey");
 for(const auto& old:naval.battle.plans)if(old.actor_id==actor&&old.request_key==command.request_key){if(old.battle_id!=command.battle_id||!orders_equal(old.orders,command.orders))return fail<NavalPlanQuote>("NavalIdempotencyConflict",command.request_key);NavalPlanQuote out;out.replayed=true;out.fact=old;return {true,std::move(out),{}};}
 const auto* source=battle(naval,command.battle_id);if(!source)return fail<NavalPlanQuote>("UnknownNavalBattle",command.battle_id);if(source->last_tick!=common.tick()||!source->pending_plan_source.empty())return fail<NavalPlanQuote>("NavalPlanSourceMismatch",command.battle_id);
 auto control=military_battle_control(common,military,commands,source->frame.units,actor);if(!control.ok)return {false,{},control.error};if(control.value.actor_id!=source->frame.controller_actor_id||control.value.side!=source->frame.controller_side||sorted(control.value.unit_ids)!=sorted(source->frame.controlled_unit_ids))return fail<NavalPlanQuote>("NavalAuthorityChanged",actor);
 auto next=submit_naval_plan(naval,ships,*ships.combat_policy,commands.battle_catalog,*source,actor,command.orders,naval_gunnery_preparation(naval.gunnery,source->frame.id));if(!next.ok)return {false,{},next.error};
 NavalPlanQuote out;out.candidate=std::move(next.value);auto& fact=out.fact;fact.battle_id=command.battle_id;fact.actor_id=actor;fact.request_key=command.request_key;fact.tick=common.tick();fact.round=source->frame.round;fact.orders=out.candidate.pending_orders;fact.scope=control.value.scope;fact.office_id=control.value.office_id;
 std::set<StableId> armies;for(const auto& unit:source->frame.units)if(std::find(control.value.unit_ids.begin(),control.value.unit_ids.end(),unit.unit_id)!=control.value.unit_ids.end())armies.insert(unit.army_id);if(armies.size()!=1)return fail<NavalPlanQuote>("NavalAuthorityArmyMismatch",actor);fact.army_id=*armies.begin();const auto* army=find(military.armies,fact.army_id);if(!army||army->status!="active")return fail<NavalPlanQuote>("NavalActiveArmyMismatch",fact.army_id);
 if(fact.scope=="army"){if(const auto* field=current_military_field_army_assignment(military,commands,army->id,actor))fact.authority_source=field->id;else{const auto* office=find(military.offices,fact.office_id);if(!office||!office->enabled||office->holder_actor_id!=actor||office->appointment_id.empty())return fail<NavalPlanQuote>("NavalAuthorityChanged",actor);fact.authority_source=office->appointment_id;}}else if(fact.scope=="unit")fact.authority_source=army->formation_source;else return fail<NavalPlanQuote>("NavalAuthorityScopeMismatch",actor);
 return {true,std::move(out),{}};
}
// 分配和 append 之后才替换域候选；原 Common savepoint 与世界候选共同承担晚失败回滚。
Outcome<NavalPlanFact> apply_naval_plan(CommonWrite& common,const MilitaryState& military,const MilitaryCatalog& commands,NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalPlanCommand& command){
 auto quote=preview_naval_plan(common,military,commands,naval,ships,actor,command);if(!quote.ok)return {false,{},quote.error};if(quote.value.replayed)return {true,quote.value.fact,{}};auto id=common.next_id("naval_plan");if(!id.ok)return {false,{},id.error};quote.value.fact.id=id.value;quote.value.candidate.pending_plan_source=id.value;
 auto appended=common.append_fact({id.value,"naval_plan","military_naval"});if(!appended.ok)return {false,{},appended.error};for(auto& current:naval.battle.battles)if(current.frame.id==command.battle_id){current=std::move(quote.value.candidate);naval.battle.plans.push_back(quote.value.fact);return {true,std::move(quote.value.fact),{}};}return fail<NavalPlanFact>("UnknownNavalBattle",command.battle_id);
}
// 完整冷史另核 Common 头和历史军权；本门保证半日不会消费一份无来源的玩家草案。
Status validate_naval_pending_plan(const NavalState& naval,const NavalBattleRecord& current){
 if(current.frame.controller_actor_id.empty())return current.pending_plan_source.empty()&&current.pending_orders.empty()?common_success():common_failure("NavalAutonomousPlanSourceMismatch",current.frame.id);
 const auto* fact=find(naval.battle.plans,current.pending_plan_source);if(!fact||fact->battle_id!=current.frame.id||fact->actor_id!=current.frame.controller_actor_id||fact->round!=current.frame.round||fact->tick!=current.last_tick||!current.frame.plan_submitted||!orders_equal(fact->orders,current.pending_orders))return common_failure("NavalPlanSourceRequired",current.frame.id);return common_success();
}
}
