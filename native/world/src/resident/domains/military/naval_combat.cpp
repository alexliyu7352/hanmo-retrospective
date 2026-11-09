// 水上溃退保留原船兵员；只有实际登船或显式投降才产接管来源，失能不转船主。
#include "military_attack_damage_core.hpp"
#include "naval_combat.hpp"
#include "naval_commander.hpp"
#include "naval_gunnery_rules.hpp"
#include "naval_gunnery_source.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 所有引用来自同一冻结轮；缺失引用拒绝，绝不补临时船或船员。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
BattleUnit* unit(NavalTacticalFrame& f,const StableId& id){for(auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
const NavalBattleBinding* binding(const NavalTacticalFrame& f,const StableId& id){for(const auto& b:f.bindings)if(b.unit_id==id)return &b;return nullptr;}
const NavalRamDefinition* ram(const NavalCombatRules& rules,const StableId& id){for(const auto& r:rules.ram)if(r.hull_id==id)return &r;return nullptr;}
bool active(const BattleUnit& u){return !u.withdrawn&&u.morale>0&&!u.fit.empty()&&naval_commander_loss(u)!=2;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 生还者仍是原unit名册子集，排序只用于原同轮稳定结算。
std::vector<StableId> survivors(const BattleUnit& u){auto ids=u.fit;ids.insert(ids.end(),u.wounded.begin(),u.wounded.end());std::sort(ids.begin(),ids.end());return ids;}
}
// 船型本身不偷偷带战斗数值；显式冲撞能力严格有限，未知结果profile拒绝。
Status validate_naval_combat_rules(const NavalCombatRules& rules,const NavalAssetCatalog& catalog){
 if(rules.gunnery){auto checked=validate_naval_gunnery_policy(*rules.gunnery,catalog);if(!checked.ok)return checked;}
 if(rules.tactical_navigation){auto checked=validate_naval_tactical_navigation(*rules.tactical_navigation);if(!checked.ok)return checked;}
 if(rules.profile!="boarding_ram_v1")return common_failure("UnsupportedNavalCombatProfile");
 std::set<StableId> seen;for(const auto& r:rules.ram)if(!find(catalog.hulls,r.hull_id)||!seen.insert(r.hull_id).second||r.target_damage<1||r.target_damage>1000000||r.self_damage<0||r.self_damage>1000000)return common_failure("InvalidNavalRamDefinition",r.hull_id);
 if(rules.commander_losses&&(rules.commander_losses->kind!="boarding_exposure_v1"||rules.commander_losses->exposure_bp<0||rules.commander_losses->exposure_bp>10000))return common_failure("InvalidNavalCommanderLossRule");
 return common_success();
}
// 先同步移动，再冻结交战双方伤害；不因数组次序让先受伤单位少打一次。
Outcome<NavalCombatResult> resolve_naval_combat(const NavalAssetState& assets,const NavalAssetCatalog& catalog,const NavalCombatRules& rules,const MilitaryBattleCatalog& military,const NavalTacticalFrame& frame,const NavalTacticalOrders& orders,const StableId& seed,const StableId& state,const MilitaryBattleRandom& random,const NavalGunneryPreparation* prepared){
 auto checked=validate_naval_combat_rules(rules,catalog);if(!checked.ok)return {false,{},checked.error};if(bool(rules.gunnery)!=(prepared!=nullptr))return fail<NavalCombatResult>("NavalGunneryPreparationRequired",frame.id);if(prepared){checked=validate_naval_gunnery_frame_source(*prepared,frame);if(!checked.ok)return {false,{},checked.error};}
 for(const auto& u:frame.units)if(!valid_naval_commander_loss(u)||(!rules.commander_losses&&!u.statuses.empty()))return fail<NavalCombatResult>("NavalCommanderLossProfileRequired",u.unit_id);
 const auto* board=find(military.battlefields,frame.battlefield_id);if(!board)return fail<NavalCombatResult>("UnknownNavalBattlefield",frame.battlefield_id);
 for(const auto& p:orders)if(p.second.action=="ram"){const auto* link=binding(frame,p.first);const auto* ship=link?find(assets.vessels,link->vessel_id):nullptr;if(!ship||!ram(rules,ship->hull_id))return fail<NavalCombatResult>("NavalRamNotAuthored",p.first);}
 auto moved=resolve_naval_maneuvers(assets,catalog,*board,frame,orders,rules.tactical_navigation?&*rules.tactical_navigation:nullptr,rules.gunnery?&*rules.gunnery:nullptr);if(!moved.ok)return {false,{},moved.error};
 NavalCombatResult out;out.assets=assets;out.frame=moved.value.frame;out.maneuvers=std::move(moved.value.events);out.rng_seed=seed;out.rng_state=state;
 const auto frozen=out.frame;
 std::map<StableId,Integer> hull_damage;
 auto impacts=moved.value.ram_requests;std::sort(impacts.begin(),impacts.end());
 for(const auto& p:impacts){const auto* a=binding(frame,p.first);const auto* z=binding(frame,p.second);const auto* ship=find(assets.vessels,a->vessel_id);const auto* definition=ram(rules,ship->hull_id);if(!definition)return fail<NavalCombatResult>("NavalRamNotAuthored",ship->hull_id);
  const auto actual_damage=std::min(definition->target_damage,find(assets.vessels,z->vessel_id)->integrity-hull_damage[z->vessel_id]);const auto actual_self=std::min(definition->self_damage,ship->integrity-hull_damage[a->vessel_id]);
  hull_damage[z->vessel_id]+=actual_damage;hull_damage[a->vessel_id]+=actual_self;
  out.events.push_back({"ram",p.first,p.second,"NavalHullImpact",0,actual_damage,actual_self,100});
 }
 for(auto& vessel:out.assets.vessels)if(hull_damage.count(vessel.id))vessel.integrity=std::max<Integer>(0,vessel.integrity-hull_damage.at(vessel.id));
 NavalGunneryResult shooting;std::set<StableId> gunners;
 if(rules.gunnery){auto fired=resolve_naval_gunnery(out.assets,catalog,*rules.gunnery,military,*prepared,frozen,moved.value.gunnery_requests,out.rng_seed,out.rng_state,random);if(!fired.ok)return {false,{},fired.error};shooting=std::move(fired.value);out.rng_seed=shooting.rng_seed;out.rng_state=shooting.rng_state;out.events.insert(out.events.end(),shooting.events.begin(),shooting.events.end());for(const auto& r:moved.value.gunnery_requests)gunners.insert(r.first);for(auto& v:out.assets.vessels)if(shooting.hull_damage.count(v.id))v.integrity=std::max<Integer>(0,v.integrity-shooting.hull_damage.at(v.id));}
 // 每部队每轮仍只有一次近战攻击；防守反击优先已有接舷目标，否则稳定选一个来犯者。
 std::map<StableId,StableId> attacks;std::set<StableId> surrendering;
 for(const auto& p:moved.value.surrender_requests)surrendering.insert(p.first);
 auto boarding=moved.value.boarding_requests;std::sort(boarding.begin(),boarding.end());
 for(const auto& p:boarding)if(!surrendering.count(p.first))attacks.emplace(p.first,p.second);
 for(const auto& p:boarding)if(!surrendering.count(p.second)&&!gunners.count(p.second)&&active(*unit(frozen,p.second)))attacks.emplace(p.second,p.first);
 std::map<StableId,Integer> casualties=shooting.casualties,morale=shooting.morale;std::map<std::pair<StableId,StableId>,Integer> damage_by_attacker;
 for(const auto& p:attacks){const auto* attacker=unit(frozen,p.first);const auto* defender=unit(frozen,p.second);
  auto q=preview_military_close_attack(*attacker,*defender,military,frame.battlefield_id,frame.round);if(!q.ok)return {false,{},q.error};
  if(!random)return fail<NavalCombatResult>("MilitaryRandomServiceRequired");
  auto draw=random(out.rng_seed,out.rng_state,1,100);if(!draw.ok)return {false,{},draw.error};if(draw.value<1||draw.value>100)return fail<NavalCombatResult>("InvalidMilitaryRandomDraw");
  const auto expected=original_military_casualty_expectation(q.value.potential,q.value.hit_chance);Integer count=original_military_sampled_casualties(expected,draw.value);
  count=std::min<Integer>(count,std::max<Integer>(0,Integer(defender->fit.size())-casualties[p.second]));casualties[p.second]+=count;damage_by_attacker[p]=count;
  if(count>0)morale[p.second]+=original_military_casualty_morale_loss(count,Integer(defender->fit.size()),false,q.value.flank);
  out.events.push_back({"board_attack",p.first,p.second,count?"Hit":"Miss",count,0,0,q.value.hit_chance});
 }
 // 复用原军旅人员伤损分配，唯一差异是低士气不执行陆上deserted/rout转换。
 for(const auto& p:casualties){auto* crew=unit(out.frame,p.first);auto fit=crew->fit;std::sort(fit.begin(),fit.end());for(Integer i=0;i<p.second;++i){crew->fit.erase(std::find(crew->fit.begin(),crew->fit.end(),fit[i]));(original_military_casualty_is_wounded(i,p.second,false)?crew->wounded:crew->dead).push_back(fit[i]);}crew->morale=std::max<Integer>(0,crew->morale-morale[p.first]);}
 // 军官按同一移动后快照、真实本部兵损和原随机流独立结算；不会增加或扣减兵员 token。
 if(rules.commander_losses){std::vector<StableId> ids;for(const auto& u:frozen.units)ids.push_back(u.unit_id);std::sort(ids.begin(),ids.end());
  for(const auto& id:ids){const auto* before=unit(frozen,id);auto* after=unit(out.frame,id);if(naval_commander_loss(*before)==2||before->fit.empty())continue;
   Integer loss=0;const bool rostered=std::find(before->member_ids.begin(),before->member_ids.end(),before->commander_actor_id)!=before->member_ids.end();
   if(rostered){if(std::find(after->dead.begin(),after->dead.end(),before->commander_actor_id)!=after->dead.end())loss=2;else if(std::find(after->wounded.begin(),after->wounded.end(),before->commander_actor_id)!=after->wounded.end())loss=1;}
   else {const auto dead=Integer(after->dead.size())-Integer(before->dead.size()),wounded=Integer(after->wounded.size())-Integer(before->wounded.size());if(dead+wounded==0||rules.commander_losses->exposure_bp==0)continue;
    if(!random)return fail<NavalCombatResult>("MilitaryRandomServiceRequired");auto draw=random(out.rng_seed,out.rng_state,1,10000);if(!draw.ok)return {false,{},draw.error};if(draw.value<1||draw.value>10000)return fail<NavalCombatResult>("InvalidMilitaryRandomDraw");
    loss=original_military_commander_exposure_loss(dead,wounded,Integer(before->fit.size()),rules.commander_losses->exposure_bp,draw.value);
   }
   if(loss>naval_commander_loss(*before)){mark_naval_commander_loss(*after,loss);out.events.push_back({"commander_loss",id,id,loss==2?"NavalCommanderKilled":"NavalCommanderWounded",0,0,0,0});}
  }
 }
 // 显式投降仍要求本轮结束后真实接收者能接管；双方同归于尽不能互造俘虏。
 std::map<StableId,NavalCaptureClaim> claims;
 for(const auto& p:moved.value.surrender_requests){const auto* taker=unit(out.frame,p.second);const auto* yielding=unit(out.frame,p.first);if(active(*taker)&&!surrendering.count(taker->unit_id))claims.emplace(p.first,NavalCaptureClaim{p.first,p.second,taker->army_id,"surrender",frame.round,survivors(*yielding)});}
 for(const auto& p:boarding){const auto* taker=unit(out.frame,p.first);const auto* defeated=unit(out.frame,p.second);if(!active(*taker)||surrendering.count(taker->unit_id)||defeated->withdrawn||(!defeated->fit.empty()&&defeated->morale>0))continue;
  auto at=claims.find(p.second);if(at!=claims.end()&&(at->second.kind=="surrender"||damage_by_attacker[{at->second.captor_unit_id,p.second}]>=damage_by_attacker[p]))continue;
  claims[p.second]={p.second,p.first,taker->army_id,"boarding_win",frame.round,survivors(*defeated)};
 }
 for(auto& p:claims)out.capture_claims.push_back(std::move(p.second));
 out.frame.phase="settlement";checked=validate_naval_tactical_frame(out.assets,catalog,*board,out.frame);if(!checked.ok)return {false,{},checked.error};
 return {true,std::move(out),{}};
}
}
