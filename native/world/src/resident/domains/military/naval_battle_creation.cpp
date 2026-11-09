#include "resident/domains/military/military_model.hpp"
// 真实船与兵员原样绑定到水域入口；军种没有变成船型，舰队源也不会变成新军团。
#include "naval_battle_creation.hpp"
#include "naval_commander.hpp"
#include "naval_gunnery_source.hpp"
#include "battle_world.hpp"
#include "resident/model/common_rng.hpp"
#include "resident/model/common_string_hash.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// 缺少原军团、实船或船员引用时拒绝，而非补默认角色或船。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
bool id_valid(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return false;Integer n=0;const auto suffix=id.substr(prefix.size());auto p=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);return p.ec==std::errc{}&&p.ptr==suffix.data()+suffix.size()&&n>0&&std::to_string(n)==suffix;}
bool same_point(const WildernessLocation& a,const Location& location){const auto* b=std::get_if<WildernessLocation>(&location);return b&&a.map_id==b->map_id&&a.point.x==b->point.x&&a.point.y==b->point.y;}
}
// 截停后建场；这一步失败时外层丢弃同一World候选，原Travel截停和ID也不能单独落地。
Outcome<NavalBattleRecord> create_naval_battle_frame(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& naval,const NavalCatalog& ships,const NavalContactFact& contact,const StableId& battle_id){
 if(!ships.combat_policy)return fail<NavalBattleRecord>("NavalCombatProfileRequired");const auto& policy=*ships.combat_policy;auto checked=validate_naval_battle_policy(policy,ships,commands.battle_catalog);if(!checked.ok)return {false,{},checked.error};
 if(!id_valid(contact.id,"naval_contact:")||!id_valid(battle_id,"naval_battle:")||contact.battle_id!=battle_id||contact.tick<common.tick()||contact.tick-common.tick()>1||contact.time_den<1||contact.time_num<0||contact.time_num>contact.time_den||contact.first_fleet_id==contact.second_fleet_id)return fail<NavalBattleRecord>("InvalidNavalContactSource");
 const auto* first=find(naval.fleets,contact.first_fleet_id);const auto* second=find(naval.fleets,contact.second_fleet_id);const auto* a=first?find(military.armies,first->army_id):nullptr;const auto* b=second?find(military.armies,second->army_id):nullptr;
 if(!a||!b||!military_armies_hostile(military,commands,*a,*b))return fail<NavalBattleRecord>("NavalContactNotHostile");
 for(const auto& previous:naval.battle.battles){if(previous.frame.id==battle_id||previous.contact_source_id==contact.id)return fail<NavalBattleRecord>("NavalContactAlreadyUsed");if(previous.frame.phase!="closed")for(const auto& link:previous.frame.bindings)if(link.fleet_id==first->id||link.fleet_id==second->id)return fail<NavalBattleRecord>("NavalFleetAlreadyEngaged",link.fleet_id);}
 NavalBattleRecord next;next.frame.id=battle_id;next.frame.source_id=contact.id;next.contact_source_id=contact.id;next.frame.battlefield_id=policy.battlefield_id;next.created_tick=next.last_tick=contact.tick;
 const auto* board=find(commands.battle_catalog.battlefields,policy.battlefield_id);std::set<StableId> units,people;
 for(Integer side=0;side<2;++side){const auto* fleet=side==0?first:second;const auto* army=side==0?a:b;const auto& anchor=side==0?contact.first_anchor:contact.second_anchor;const auto& motion=side==0?contact.first_motion:contact.second_motion;const auto* leader=common.actor(army->commander_actor_id);
  if(fleet->status!="active"||army->status!="active"||!leader||!leader->alive||!leader->available||leader->id!=fleet->commander_actor_id||!same_point(anchor,leader->location)||motion.actor_id!=leader->id||!same_point(anchor,motion.location))return fail<NavalBattleRecord>("NavalContactAnchorMismatch",fleet->id);
  std::vector<NavalVesselAssignment> assignments=fleet->assignments;std::sort(assignments.begin(),assignments.end(),[](const auto& x,const auto& y){return x.unit_id<y.unit_id;});std::vector<HexPoint> spawn;for(const auto& entry:board->spawns)if(entry.side==side)spawn.push_back(entry.position);if(spawn.size()<assignments.size())return fail<NavalBattleRecord>("NavalBattlefieldCapacity",fleet->id);
  for(std::size_t i=0;i<assignments.size();++i){const auto& assignment=assignments[i];const auto* actual=find(military.military_units,assignment.unit_id);const auto* vessel=find(naval.vessels,assignment.vessel_id);
   if(!actual||actual->army_id!=army->id||!vessel||vessel->fleet_id!=fleet->id||!units.insert(actual->id).second||!actual->mobilizing_action_id.empty()||(actual->training_action_id&&!actual->training_action_id->empty()))return fail<NavalBattleRecord>("NavalUnitSourceMismatch",assignment.unit_id);
   BattleUnit u;u.unit_id=actual->id;u.army_id=actual->army_id;u.commander_actor_id=actual->commander_actor_id;u.troop_type_id=actual->troop_type_id;u.side=side;u.position=spawn[i];u.heading=side==0?0:3;u.morale=actual->morale;u.member_ids=actual->member_ids;u.fit=actual->fit;u.wounded=actual->wounded;u.dead=actual->dead;u.captured=actual->captured;u.deserted=actual->deserted;
   const auto* captain=common.actor(u.commander_actor_id);if(!captain)return fail<NavalBattleRecord>("NavalCaptainSourceRequired",u.commander_actor_id);if(!captain->alive){Integer deaths=0;for(const auto& fact:military.casualty_facts)if(fact.tick<=contact.tick)for(const auto& change:fact.actor_changes)if(change.actor_id==captain->id&&change.status=="dead"&&change.army_id==army->id)++deaths;if(!policy.combat.commander_losses||deaths!=1)return fail<NavalBattleRecord>("NavalCommanderLossSourceRequired",captain->id);mark_naval_commander_loss(u,2);}
   next.frame.units.push_back(u);next.frame.bindings.push_back({vessel->id,u.unit_id,fleet->id,{}});
  }
  for(const auto& id:army->member_actor_ids){const auto* person=common.actor(id);if(!person||!person->alive||!people.insert(id).second||(access.in_open_battle&&access.in_open_battle(id)))return fail<NavalBattleRecord>("NavalActorAlreadyEngaged",id);if(!person->action_id.empty()){const auto* action=common.action(person->action_id);if(!action||action->kind!="travel"||action->actor_id!=leader->id||std::find(action->participants.begin(),action->participants.end(),id)==action->participants.end())return fail<NavalBattleRecord>("NavalPersonOccupied",id);}}
 }
 // 玩家仅可得到与陆战相同的真实本军或本人本部权限；未参与时整场照常由NPC推进。
 bool player_present=a->commander_actor_id==access.player_actor_id||b->commander_actor_id==access.player_actor_id;for(const auto& u:next.frame.units)if(u.commander_actor_id==access.player_actor_id)player_present=true;
 auto control=military_battle_control(common,military,commands,next.frame.units,player_present?access.player_actor_id:StableId{});if(control.ok){next.frame.controller_actor_id=control.value.actor_id;next.frame.controller_side=control.value.side;next.frame.controlled_unit_ids=control.value.unit_ids;}
 else if(control.error.original.code!="MilitaryCommanderNotPresent")return {false,{},control.error};
 checked=validate_naval_tactical_frame(naval,ships,*board,next.frame);if(!checked.ok)return {false,{},checked.error};
 auto hash=godot_string_hash_utf8(battle_id),source_hash=godot_string_hash_utf8(contact.id);if(!hash.ok||!source_hash.ok)return {false,{},hash.ok?source_hash.error:hash.error};Integer seed=access.battle_seed^hash.value^source_hash.value;next.rng_seed=std::to_string(seed);next.rng_state=std::to_string(godot_pcg32_seed_state(seed));return {true,std::move(next),{}};
}
// 旧profile仍一次建场；新profile只有原准备来源已核后才产生可提交的AI集合。
Outcome<NavalBattleRecord> create_naval_battle_record(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& naval,const NavalCatalog& ships,const NavalContactFact& contact,const StableId& id,const NavalGunneryPreparation* prepared){
 auto created=create_naval_battle_frame(common,military,commands,access,naval,ships,contact,id);if(!created.ok)return created;if(bool(ships.combat_policy->combat.gunnery)!=(prepared!=nullptr))return fail<NavalBattleRecord>("NavalGunneryPreparationRequired",id);if(prepared){auto checked=validate_naval_gunnery_initial_bindings(*prepared,created.value.frame);if(!checked.ok)return {false,{},checked.error};}auto ai=make_naval_ai(naval,ships,*ships.combat_policy,commands.battle_catalog,created.value.frame,prepared);if(!ai.ok)return {false,{},ai.error};created.value.ai_orders=std::move(ai.value);return created;
}

}
