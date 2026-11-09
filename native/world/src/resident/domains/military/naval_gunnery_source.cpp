#include "resident/domains/military/military_model.hpp"
// 舰上普通攻击的技艺只认入战时一次原来源；冷读取后来成长必须经exact fact前读重建。
#include "naval_gunnery_source.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
Integer sequence(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return 0;Integer n=0;auto b=id.data()+prefix.size();auto r=std::from_chars(b,id.data()+id.size(),n);return r.ec==std::errc{}&&r.ptr==id.data()+id.size()&&n>0&&id==prefix+std::to_string(n)?n:0;}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
bool source(const CommonStore& c,const StableId& id,const StableId& prefix,const StableId& kind){auto n=sequence(id,prefix);auto h=c.state().facts.find(id);return n>0&&n<c.state().next_instance_id&&h&&h->kind==kind&&h->domain=="military_naval";}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
const NavalBattleBinding* binding(const NavalTacticalFrame& f,const StableId& id){for(const auto& b:f.bindings)if(b.unit_id==id)return &b;return nullptr;}
// 仅字段顺序有完整等值含义，空顺序表示本域新建；不允许重复或遗漏键。
bool shape(const FieldOrder& order,std::initializer_list<StableId> keys){if(order.empty())return true;std::set<StableId> fields(keys);if(order.size()!=fields.size())return false;for(const auto& key:order)if(fields.erase(key)!=1)return false;return fields.empty();}
Status observation(const CommonStore& c,const NavalGunnerySkillObservation& s,const StableId& actor,Integer tick,const StableId& boundary){
 if(!identity(s.source_identity,c.state().identity)||s.actor_id!=actor||s.skill_id!="naval"||s.before_fact_id!=boundary||s.observed_tick!=tick||s.experience<0||s.level<0||s.level>1000000||(s.authored_actor_id.empty()&&s.causal_fact_ids.empty()))return common_failure("NavalGunnerySkillSourceMismatch",actor);
 std::set<StableId> ids;for(const auto& id:s.causal_fact_ids)if(id.empty()||!ids.insert(id).second||!c.state().facts.find(id)||(!boundary.empty()&&!before(c,id,boundary)))return common_failure("NavalGunnerySkillCauseMismatch",id);return common_success();
}
}
// 源记录尺寸沿实际战场128部队上限；所有技能值仍必须在完整历史门由Growth同源重算。
Status validate_naval_gunnery_shape(const NavalGunneryState& s){
 if(!shape(s.field_order,{"preparations"}))return common_failure("InvalidNavalGunneryFieldOrder");std::set<StableId> ids,battles,contacts;
 for(const auto& f:s.preparations){if(!shape(f.field_order,{"id","battle_id","contact_source_id","tick","captains"})||!sequence(f.id,"naval_skill:")||!sequence(f.battle_id,"naval_battle:")||!sequence(f.contact_source_id,"naval_contact:")||!ids.insert(f.id).second||!battles.insert(f.battle_id).second||!contacts.insert(f.contact_source_id).second||f.tick<1||f.captains.empty()||f.captains.size()>128)return common_failure("InvalidNavalGunneryPreparation",f.id);
  std::set<StableId> units,vessels,people;StableId prior;
  for(const auto& r:f.captains){if(!shape(r.field_order,{"unit_id","vessel_id","fleet_id","army_id","captain_actor_id","authored_actor_id","experience","level","causal_fact_ids"})||r.unit_id.empty()||!units.insert(r.unit_id).second||(!prior.empty()&&r.unit_id<=prior)||r.vessel_id.empty()||!vessels.insert(r.vessel_id).second||!sequence(r.fleet_id,"fleet:")||!sequence(r.army_id,"army:")||r.captain_actor_id.empty()||!people.insert(r.captain_actor_id).second||r.experience<0||r.level<0||r.level>1000000||(r.authored_actor_id.empty()&&r.causal_fact_ids.empty()))return common_failure("InvalidNavalGunneryCaptain",f.id);std::set<StableId> causes;for(const auto& id:r.causal_fact_ids)if(id.empty()||!causes.insert(id).second)return common_failure("InvalidNavalGunnerySkillCause",f.id);prior=r.unit_id;}
 }return common_success();
}
// 一一对应原初始frame全部绑定，不能漏掉对手或把另一艘船的高技能船长挂到本船。
Status validate_naval_gunnery_initial_bindings(const NavalGunneryPreparation& p,const NavalTacticalFrame& f){
 if(p.battle_id!=f.id||p.contact_source_id!=f.source_id||f.round!=1||f.phase!="planning"||f.plan_submitted||p.captains.size()!=f.units.size()||p.captains.size()!=f.bindings.size())return common_failure("NavalGunneryInitialFrameMismatch",p.id);
 for(const auto& r:p.captains){auto u=unit(f,r.unit_id);auto b=binding(f,r.unit_id);if(!u||!b||u->army_id!=r.army_id||u->commander_actor_id!=r.captain_actor_id||b->vessel_id!=r.vessel_id||b->fleet_id!=r.fleet_id)return common_failure("NavalGunneryInitialBindingMismatch",r.unit_id);}return common_success();
}
// 冻结技艺不因捕获换船而改写；当前binding的真实变迁必须由原Naval历史先验。
Status validate_naval_gunnery_frame_source(const NavalGunneryPreparation& p,const NavalTacticalFrame& f){NavalGunneryState shape;shape.preparations.push_back(p);auto checked=validate_naval_gunnery_shape(shape);if(!checked.ok)return checked;if(p.battle_id!=f.id||p.contact_source_id!=f.source_id)return common_failure("NavalGunneryBattleSourceMismatch",p.id);for(const auto& u:f.units){const NavalGunneryCaptain* k=nullptr;for(const auto& r:p.captains)if(r.unit_id==u.unit_id)k=&r;auto b=binding(f,u.unit_id);if(!k||!b||k->fleet_id!=b->fleet_id||k->army_id!=u.army_id||k->captain_actor_id!=u.commander_actor_id)return common_failure("NavalGunneryBattleSourceMismatch",u.unit_id);}return common_success();}
// 原阶段显式核定to，不能接受任意未来tick；当前技能读失败时不分配准备事实ID。
Outcome<StableId> record_naval_gunnery_preparation(CommonStore& c,const MilitaryState& m,const NavalAssetState& n,NavalGunneryState& s,const NavalContactFact& contact,const NavalTacticalFrame& frame,const NavalGunnerySkillRead& skills,const DynamicStepInput& phase){
 if(phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1||contact.tick!=phase.to_tick||contact.battle_id!=frame.id||contact.id!=frame.source_id||!source(c,contact.id,"naval_contact:","naval_contact")||!source(c,frame.id,"naval_battle:","naval_battle")||!before(c,contact.id,frame.id)||c.state().facts.order().empty()||c.state().facts.order().back()!=frame.id)return fail<StableId>("NavalGunneryCreationPhaseRequired",frame.id);
 for(const auto& old:s.preparations)if(old.battle_id==frame.id||old.contact_source_id==contact.id)return fail<StableId>("NavalGunneryAlreadyPrepared",frame.id);
 NavalGunneryPreparation out;out.battle_id=frame.id;out.contact_source_id=contact.id;out.tick=phase.to_tick;
 for(const auto& b:frame.bindings){auto u=unit(frame,b.unit_id);auto actual=find(m.military_units,b.unit_id);auto vessel=find(n.vessels,b.vessel_id);auto fleet=find(n.fleets,b.fleet_id);
  if(!u||!actual||!vessel||!fleet||fleet->status!="active"||actual->army_id!=u->army_id||actual->commander_actor_id!=u->commander_actor_id||vessel->fleet_id!=fleet->id||fleet->army_id!=actual->army_id||(fleet->id!=contact.first_fleet_id&&fleet->id!=contact.second_fleet_id)||u->side!=(fleet->id==contact.first_fleet_id?0:1)||!c.actor(actual->commander_actor_id))return fail<StableId>("NavalGunneryActualBindingRequired",b.unit_id);
  bool assigned=false;for(const auto& a:fleet->assignments)if(a.unit_id==b.unit_id&&a.vessel_id==b.vessel_id)assigned=true;if(!assigned)return fail<StableId>("NavalGunneryActualBindingRequired",b.unit_id);
  auto read=skills.current_naval_skill(u->commander_actor_id);if(!read.ok)return {false,{},read.error};auto checked=observation(c,read.value,u->commander_actor_id,phase.to_tick,{});if(!checked.ok)return {false,{},checked.error};const auto& k=read.value;
  out.captains.push_back({u->unit_id,b.vessel_id,b.fleet_id,u->army_id,u->commander_actor_id,k.authored_actor_id,k.experience,k.level,k.causal_fact_ids,{}});
 }
 std::sort(out.captains.begin(),out.captains.end(),[](const auto& a,const auto& b){return a.unit_id<b.unit_id;});auto checked=validate_naval_gunnery_initial_bindings(out,frame);if(!checked.ok)return {false,{},checked.error};
 auto id=c.next_id("naval_skill");if(!id.ok)return id;out.id=id.value;NavalGunneryState candidate;candidate.preparations.push_back(out);checked=validate_naval_gunnery_shape(candidate);if(!checked.ok)return {false,{},checked.error};checked=c.append_fact({out.id,"naval_gunnery_preparation","military_naval"});if(!checked.ok)return {false,{},checked.error};s.preparations.push_back(std::move(out));return id;
}
// 完整Common来源序和exact技能史先验；原建场重放仍要交叉核一次真实初始绑定。
Status validate_naval_gunnery_sources(const CommonStore& c,const NavalGunneryState& s,const NavalBattleState& battles){
 auto checked=validate_naval_gunnery_shape(s);if(!checked.ok)return checked;std::set<StableId> ids,seen;StableId previous;Integer last_tick=-1;
 for(const auto& p:s.preparations){const NavalBattleRecord* battle=nullptr;for(const auto& b:battles.battles)if(b.frame.id==p.battle_id){if(battle)return common_failure("NavalGunneryBattleAmbiguous",p.id);battle=&b;}auto contact=find(battles.contacts,p.contact_source_id);
  if(!battle||!contact||!source(c,p.id,"naval_skill:","naval_gunnery_preparation")||!source(c,p.battle_id,"naval_battle:","naval_battle")||!source(c,p.contact_source_id,"naval_contact:","naval_contact")||contact->battle_id!=p.battle_id||battle->contact_source_id!=p.contact_source_id||p.tick!=contact->tick||p.tick!=battle->created_tick||p.tick>c.tick()||p.tick<last_tick||(!previous.empty()&&!before(c,previous,p.id))||!before(c,p.contact_source_id,p.battle_id)||!before(c,p.battle_id,p.id))return common_failure("NavalGunneryHistorySourceMismatch",p.id);
  const auto& order=c.state().facts.order();auto at=std::find(order.begin(),order.end(),p.id);if(at==order.begin()||*(at-1)!=p.battle_id)return common_failure("NavalGunneryCreationBoundaryMismatch",p.id);
  for(const auto& r:p.captains)for(const auto& cause:r.causal_fact_ids)if(!c.state().facts.find(cause)||!before(c,cause,p.id))return common_failure("NavalGunnerySkillCauseMismatch",cause);
  for(const auto& plan:battles.plans)if(plan.battle_id==p.battle_id&&!before(c,p.id,plan.id))return common_failure("NavalGunneryPlanBeforePreparation",plan.id);for(const auto& round:battles.rounds)if(round.battle_id==p.battle_id&&!before(c,p.id,round.id))return common_failure("NavalGunneryRoundBeforePreparation",round.id);
  ids.insert(p.id);seen.insert(p.battle_id);previous=p.id;last_tick=p.tick;
 }
 for(const auto& b:battles.battles)if(!seen.count(b.frame.id))return common_failure("NavalGunneryPreparationRequired",b.frame.id);for(const auto& row:c.state().facts.rows())if((row.second.kind=="naval_gunnery_preparation"||row.first.rfind("naval_skill:",0)==0)&&!ids.count(row.first))return common_failure("NavalGunneryOrphanSource",row.first);return common_success();
}
// 晚门再次绑定同一来源，再核完整Growth史；没有布尔跳过，调用方只能拿完整Status。
Status validate_naval_gunnery_history(const CommonStore& c,const NavalGunneryState& s,const NavalBattleState& battles,const NavalGunnerySkillRead& skills){auto checked=validate_naval_gunnery_sources(c,s,battles);if(!checked.ok)return checked;for(const auto& p:s.preparations){
  for(const auto& r:p.captains){auto read=skills.naval_skill_before(r.captain_actor_id,p.id);if(!read.ok)return {false,{},read.error};checked=observation(c,read.value,r.captain_actor_id,p.tick,p.id);if(!checked.ok)return checked;const auto& k=read.value;if(k.authored_actor_id!=r.authored_actor_id||k.experience!=r.experience||k.level!=r.level||k.causal_fact_ids!=r.causal_fact_ids)return common_failure("NavalGunnerySkillHistoryMismatch",p.id);}
 }return common_success();}
const NavalGunneryPreparation* naval_gunnery_preparation(const NavalGunneryState& s,const StableId& battle){for(const auto& p:s.preparations)if(p.battle_id==battle)return &p;return nullptr;}
}
