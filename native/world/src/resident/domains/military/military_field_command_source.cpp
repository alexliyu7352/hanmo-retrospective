// 真实形成与交接共用原还原器；actual-prefix 按真实Common次序，不凭当前名册补过去。
#include "military_field_command_source.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "military_integration_assets.hpp"
#include "military_formation_source.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
Integer sequence(const StableId& id){auto at=id.rfind(':');if(at==StableId::npos)return -1;const std::string_view s{id.data()+at+1,id.size()-at-1};Integer n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n);return !s.empty()&&s.front()!='0'&&r.ec==std::errc{}&&r.ptr==s.data()+s.size()&&n>0?n:-1;}
bool same(const MilitaryFieldCommandSource& a,const MilitaryFieldCommandSource& b){return a.kind==b.kind&&a.id==b.id;}
// 军团形成的来源不随现任统帅改变；army分配身份在实际编军完成以后。
Outcome<MilitaryArmyCommandBinding> army_binding(const CommonStore& c,const MilitaryState& s,const StableId& id,Integer limit,Integer observed_tick=-1){
 using B=MilitaryArmyCommandBinding;if(observed_tick<0)observed_tick=c.tick();const auto* army=find(s.armies,id);const auto* f=army?find(s.action_facts,army->formation_source):nullptr;const auto* t=f?std::get_if<ArmyFormationTerms>(&f->terms):nullptr;const auto* head=f?c.state().facts.find(f->id):nullptr;
 if(!army||!t||!head||head->kind!="military_action"||head->domain!="military"||f->command!="army_form"||f->phase!="completed"||!f->success||!f->result||f->result->army_id!=id||f->actor_id!=t->actor_id||!c.actor(t->actor_id)||t->order_id!=army->order_id||t->organization_id!=army->organization_id||f->completed_tick<0||f->completed_tick>observed_tick||sequence(id)<=sequence(f->id)||sequence(f->id)<1||sequence(id)>=limit)return fail<B>("MilitaryArmyCommandFormationRequired",id);
 B out{t->actor_id,{MilitaryFieldCommandSourceKind::army_formation,f->id}};Integer last=sequence(id),tick=f->completed_tick;
 for(const auto& e:s.field_commands)if(e.target.kind==MilitaryFieldCommandKind::army&&e.target.army_id==id){const auto seq=sequence(e.id);const auto* h=c.state().facts.find(e.id);if(seq<=last||seq>=c.state().next_instance_id||!h||h->kind!="military_field_command_assigned"||h->domain!="military"||e.id.rfind("military_field_command:",0)!=0||e.tick<tick||e.tick>observed_tick||e.target.organization_id!=army->organization_id||e.target.campaign_id!=army->order_id||e.target.formation_source_id!=army->formation_source||e.target.next_actor_id!=e.actor_id||!c.actor(e.actor_id))return fail<B>("MilitaryFieldCommandHistoryMismatch",e.id);last=seq;tick=e.tick;if(seq>=limit)continue;if(e.target.previous_actor_id!=out.commander_actor_id||!same(e.target.previous_source,out.command_source))return fail<B>("MilitaryFieldCommandPredecessorMismatch",e.id);out={e.actor_id,{MilitaryFieldCommandSourceKind::field_assignment,e.id}};}
 return {true,std::move(out),{}};
}
// 舟上历史只逆转本舰真实capture名册；release前后由原事实边界判断，不读当前active冒历史。
Outcome<std::pair<const NavalFleetRecord*,std::vector<NavalVesselAssignment>>> fleet_at(const CommonStore& c,const NavalState* n,const StableId& army,Integer limit){
 using R=std::pair<const NavalFleetRecord*,std::vector<NavalVesselAssignment>>;R out;
 if(!n)return {true,std::move(out),{}};
 for(const auto& f:n->fleets)if(f.army_id==army){const auto created=sequence(f.source_id);const auto* head=c.state().facts.find(f.source_id);if(created<1||!head||head->kind!="naval_form"||head->domain!="military_naval")return fail<R>("MilitaryFieldFleetSourceRequired",f.id);if(created>=limit)continue;if(!f.release_source_id.empty()){const auto released=sequence(f.release_source_id);if(released<1||!c.state().facts.find(f.release_source_id))return fail<R>("MilitaryFieldFleetSourceRequired",f.id);if(released<limit)continue;}if(out.first)return fail<R>("MilitaryFieldFleetAmbiguous",army);out.first=&f;out.second=f.assignments;
  for(auto i=n->battle.captures.rbegin();i!=n->battle.captures.rend();++i)if(sequence(i->id)>=limit){if(i->to_fleet_id==f.id)out.second=i->before_assignments;else if(i->from_fleet_id==f.id)out.second=i->from_before_assignments;}
 }
 return {true,std::move(out),{}};
}
Outcome<MilitaryFieldCommandObservation> observe(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,const MilitaryAccess* access,MilitaryFieldCommandKind kind,const StableId& army_id,const StableId& unit_id,Integer tick,const StableId& before){
 using O=MilitaryFieldCommandObservation;const bool historical=!before.empty();const auto limit=historical?sequence(before):c.state().next_instance_id;
 if(!cat.field_command_policy||!cat.officer_assignment_policy||cat.field_command_policy->profile!="casualty_appointment_v1"||tick<0||tick>c.tick()||limit<1||(historical&&!c.state().facts.find(before)))return fail<O>("MilitaryFieldCommandBoundaryRequired",before);
 const auto* army=find(s.armies,army_id);const auto* formation=army?find(s.action_facts,army->formation_source):nullptr;const auto* terms=formation?std::get_if<ArmyFormationTerms>(&formation->terms):nullptr;
 if(!army||!terms||sequence(army_id)>=limit||formation->completed_tick>tick)return fail<O>("MilitaryFieldArmySourceRequired",army_id);
 auto army_source=historical?army_binding(c,s,army_id,limit):query_military_army_command_source(c,s,army_id);if(!army_source.ok)return {false,{},army_source.error};O out;out.source_identity=c.state().identity;out.kind=kind;out.organization_id=army->organization_id;out.army_id=army_id;out.campaign_id=army->order_id;out.formation_source_id=army->formation_source;out.origin_garrison_id=terms->garrison_id;out.unit_id=unit_id;out.army_active=true;
 for(const auto& f:s.return_facts)if(f.army_id==army_id&&sequence(f.id)<limit)out.army_active=false;
 for(const auto& f:s.integration_facts)if(sequence(f.id)<limit)for(const auto& a:f.source_armies)if(a.id==army_id)out.army_active=false;
 if(n)for(const auto& f:n->battle.captures)if(sequence(f.id)<limit&&f.from_after_assignments.empty()){const auto* fleet=find(n->fleets,f.from_fleet_id);if(fleet&&fleet->army_id==army_id)out.army_active=false;}
 if(!historical&&out.army_active!=(army->status=="active"))return fail<O>("MilitaryFieldArmyStatusMismatch",army_id);
 MilitaryUnitRecord unit;if(!unit_id.empty()){
  if(historical){if(!access||!access->rosters_before)return fail<O>("MilitaryFieldRosterReadRequired");auto r=access->rosters_before({unit_id},tick,before);if(!r.ok)return {false,{},r.error};if(r.value.size()!=1)return fail<O>("MilitaryFieldUnitSourceRequired",unit_id);unit=r.value.front();}
  else {const auto* u=find(s.military_units,unit_id);if(!u)return fail<O>("MilitaryFieldUnitSourceRequired",unit_id);unit=*u;}
  if(unit.army_id!=army_id||unit.organization_id!=army->organization_id)return fail<O>("MilitaryFieldUnitSourceRequired",unit_id);
 }
 if(kind==MilitaryFieldCommandKind::unit){if(unit_id.empty())return fail<O>("MilitaryFieldUnitSourceRequired");auto source=historical?query_military_unit_command_source_before(c,s,cat,unit_id,before):Outcome<MilitaryUnitCommandBinding>{};if(!historical){auto current=query_military_unit_command_source(c,s,cat,unit_id);if(!current.ok)return {false,{},current.error};source={true,{current.value.commander_actor_id,current.value.command_source},{}};}if(!source.ok)return {false,{},source.error};if(source.value.commander_actor_id!=unit.commander_actor_id)return fail<O>("MilitaryFieldUnitSourceMismatch",unit_id);out.commander_actor_id=source.value.commander_actor_id;out.command_source=military_field_source(source.value.command_source);}
 else {out.commander_actor_id=army_source.value.commander_actor_id;out.command_source=army_source.value.command_source;}
 auto death=query_military_actor_death(c,s,out.commander_actor_id);if(!death.ok)return {false,{},death.error};if(death.value&&sequence(death.value->casualty_fact_id)<limit&&death.value->tick<=tick){const auto* casualty=find(s.casualty_facts,death.value->casualty_fact_id);unsigned matches=0;if(casualty)for(const auto& change:casualty->actor_changes)if(change.actor_id==out.commander_actor_id&&change.status=="dead"&&change.army_id==army_id)++matches;if(matches!=1)return fail<O>("MilitaryFieldDeathArmyMismatch",army_id);out.commander_death=MilitaryFieldCommandDeath{death.value->actor_id,death.value->casualty_fact_id,death.value->battle_id,death.value->tick};}
 auto fleet=fleet_at(c,n,army_id,limit);if(!fleet.ok)return {false,{},fleet.error};if(fleet.value.first){out.fleet_id=fleet.value.first->id;if(kind==MilitaryFieldCommandKind::unit){for(const auto& a:fleet.value.second)if(a.unit_id==unit_id){if(!out.vessel_id.empty())return fail<O>("MilitaryFieldBindingAmbiguous",unit_id);out.vessel_id=a.vessel_id;}}
  else if(out.commander_death){const auto* round=find(n->battle.rounds,out.commander_death->battle_id);if(round)for(const auto& u:round->before.units)if(u.army_id==army_id&&(u.commander_actor_id==out.commander_actor_id||has(u.member_ids,out.commander_actor_id)))for(const auto& b:round->before.bindings)if(b.unit_id==u.unit_id)for(const auto& a:fleet.value.second)if(a.vessel_id==b.vessel_id&&a.unit_id==b.unit_id){if(!out.vessel_id.empty()&&out.vessel_id!=b.vessel_id)return fail<O>("MilitaryFieldBindingAmbiguous",army_id);out.vessel_id=b.vessel_id;}}
  else for(const auto& a:fleet.value.second){auto binding=historical?query_military_unit_command_source_before(c,s,cat,a.unit_id,before):Outcome<MilitaryUnitCommandBinding>{};if(!historical){auto q=query_military_unit_command_source(c,s,cat,a.unit_id);if(!q.ok)return {false,{},q.error};binding={true,{q.value.commander_actor_id,q.value.command_source},{}};}if(!binding.ok)return {false,{},binding.error};if(binding.value.commander_actor_id==out.commander_actor_id)out.vessel_id=a.vessel_id;}
  if(out.vessel_id.empty())return fail<O>("MilitaryFieldBindingRequired",army_id);
  if(out.commander_death){const auto* round=find(n->battle.rounds,out.commander_death->battle_id);StableId death_vessel;if(round)for(const auto& u:round->before.units)if(u.army_id==army_id&&(u.commander_actor_id==out.commander_actor_id||has(u.member_ids,out.commander_actor_id)))for(const auto& b:round->before.bindings)if(b.unit_id==u.unit_id){if(!death_vessel.empty()&&death_vessel!=b.vessel_id)return fail<O>("MilitaryFieldBindingAmbiguous",army_id);death_vessel=b.vessel_id;}if(death_vessel!=out.vessel_id)return fail<O>("MilitaryFieldDeathBindingMismatch",army_id);}

 }
 return {true,std::move(out),{}};
}
// 同军现场是原Army成员绑定；舟队进一步核逐船人名，不把舰队中心或水面坐标当会面。
Outcome<MilitaryFieldCommandContact> contact(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,const MilitaryAccess* a,const MilitaryFieldCommandTarget& t,const StableId& actor,Integer tick,const StableId& before){
 using C=MilitaryFieldCommandContact;const bool historical=!before.empty();auto observed=observe(c,s,cat,n,a,t.kind,t.army_id,t.unit_id,tick,before);if(!observed.ok)return {false,{},observed.error};const auto& o=observed.value;
 if(!o.army_active||o.organization_id!=t.organization_id||o.campaign_id!=t.campaign_id||o.formation_source_id!=t.formation_source_id||o.commander_actor_id!=t.previous_actor_id||!same(o.command_source,t.previous_source)||o.fleet_id!=t.fleet_id||!o.commander_death||o.commander_death->actor_id!=t.predecessor_death.actor_id||o.commander_death->casualty_fact_id!=t.predecessor_death.casualty_fact_id||o.commander_death->battle_id!=t.predecessor_death.battle_id||o.commander_death->tick!=t.predecessor_death.tick)return fail<C>("MilitaryFieldContactTargetChanged",t.army_id);
 const auto* army=find(s.armies,t.army_id);const auto* person=c.actor(actor);if(!army||!person)return fail<C>("MilitaryFieldContactActorRequired",actor);std::vector<StableId> members;
 if(historical){auto read=military_army_actor_members_before(c,s,t.army_id,tick,before);if(!read.ok)return {false,{},read.error};members=std::move(read.value);}else members=army->member_actor_ids;
 if(!has(members,actor))return fail<C>("MilitaryFieldContactMemberRequired",actor);
 StableId actor_unit;auto unit_ids=historical?military_army_units_at(s,t.army_id,tick,before):army->unit_ids;
 for(const auto& id:unit_ids){MilitaryUnitRecord u;if(historical){if(!a||!a->rosters_before)return fail<C>("MilitaryFieldRosterReadRequired");auto rows=a->rosters_before({id},tick,before);if(!rows.ok)return {false,{},rows.error};if(rows.value.size()!=1)return fail<C>("MilitaryFieldContactUnitRequired",actor);u=rows.value.front();}else{const auto* row=find(s.military_units,id);if(!row)return fail<C>("MilitaryFieldContactUnitRequired",id);u=*row;}if(u.army_id==t.army_id&&(u.commander_actor_id==actor||has(u.fit,actor)||has(u.wounded,actor))){if(!actor_unit.empty())return fail<C>("MilitaryFieldContactUnitAmbiguous",actor);actor_unit=u.id;}}
 if(actor_unit.empty())return fail<C>("MilitaryFieldContactUnitRequired",actor);

 const auto limit=historical?sequence(before):c.state().next_instance_id;auto fleet=fleet_at(c,n,t.army_id,limit);if(!fleet.ok)return {false,{},fleet.error};StableId vessel;
 if(fleet.value.first){for(const auto& b:fleet.value.second){MilitaryUnitRecord unit;if(historical){if(!a||!a->rosters_before)return fail<C>("MilitaryFieldRosterReadRequired");auto r=a->rosters_before({b.unit_id},tick,before);if(!r.ok)return {false,{},r.error};if(r.value.size()!=1)return fail<C>("MilitaryFieldContactBindingRequired",b.unit_id);unit=r.value.front();}else{const auto* u=find(s.military_units,b.unit_id);if(!u)return fail<C>("MilitaryFieldContactBindingRequired",b.unit_id);unit=*u;}if(unit.army_id!=t.army_id)return fail<C>("MilitaryFieldContactBindingRequired",b.unit_id);if(unit.commander_actor_id==actor||has(unit.fit,actor)||has(unit.wounded,actor)||has(b.passenger_ids,actor)){if(!vessel.empty())return fail<C>("MilitaryFieldContactBindingAmbiguous",actor);vessel=b.vessel_id;}}
  if(vessel.empty()||vessel!=o.vessel_id)return fail<C>("MilitaryFieldContactSameVesselRequired",actor);
 }
 C out;out.source_identity=c.state().identity;out.army_id=t.army_id;out.unit_id=actor_unit;out.accepting_actor_id=actor;out.previous_actor_id=t.previous_actor_id;out.fleet_id=o.fleet_id;out.vessel_id=vessel;out.existing_army_member=true;out.observed_tick=tick;out.boundary_fact_id=before;
 if(!historical){const auto* in=std::get_if<InArmyLocation>(&person->location);if(actor!=army->commander_actor_id&&(!in||in->army_id!=army->id))return fail<C>("MilitaryFieldContactLocationMismatch",actor);const auto* commander=c.actor(army->commander_actor_id);if(!commander||(!std::holds_alternative<SettlementLocation>(commander->location)&&!std::holds_alternative<WildernessLocation>(commander->location)))return fail<C>("MilitaryFieldContactLocationMismatch",actor);out.actor_location=person->location;out.army_anchor=commander->location;}
 return {true,std::move(out),{}};
}

}
const MilitaryFieldCommandFact* current_military_field_army_assignment(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& army_id,const StableId& actor){if(!cat.field_command_policy||!cat.officer_assignment_policy)return nullptr;const auto* army=find(s.armies,army_id);if(!army||army->status!="active"||army->commander_actor_id!=actor)return nullptr;for(auto i=s.field_commands.rbegin();i!=s.field_commands.rend();++i)if(i->target.kind==MilitaryFieldCommandKind::army&&i->target.army_id==army_id)return i->actor_id==actor&&i->target.next_actor_id==actor&&i->target.formation_source_id==army->formation_source&&i->target.campaign_id==army->order_id&&i->target.organization_id==army->organization_id?&*i:nullptr;return nullptr;}
// 新接任时原战场必须已退出；后来接敌只核当前真实统帅，不反复用亡者令其解战。
StableId military_campaign_battle_stop_reason(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const CampaignRecord& p){
 const auto* army=find(s.armies,p.army_id);const auto* field=army?current_military_field_army_assignment(s,cat,army->id,army->commander_actor_id):nullptr;
 const auto* person=c.actor(field?army->commander_actor_id:p.actor_id);
 if(!person||!person->alive||!person->available)return "responsible_actor_lost";
 if(field&&field->target.campaign_id==p.id)return {};
 const auto* office=find(s.offices,p.office_id);if(!office||!office->enabled||office->holder_actor_id!=person->id)return "authority_lost";
 return p.objective_result&&(p.objective_result->kind=="expired"||p.objective_result->kind=="cancelled"||p.objective_result->kind=="withdrawn")?"cancelled":StableId{};
}
MilitaryFieldCommandSource military_field_source(const MilitaryUnitCommandSource& s){switch(s.kind){case MilitaryUnitCommandSourceKind::authored:return {MilitaryFieldCommandSourceKind::unit_authored,s.id};case MilitaryUnitCommandSourceKind::army_formation:return {MilitaryFieldCommandSourceKind::army_formation,s.id};case MilitaryUnitCommandSourceKind::officer_assignment:return {MilitaryFieldCommandSourceKind::garrison_assignment,s.id};case MilitaryUnitCommandSourceKind::field_assignment:return {MilitaryFieldCommandSourceKind::field_assignment,s.id};}return {};}
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source(const CommonStore& c,const MilitaryState& s,const StableId& id){auto b=army_binding(c,s,id,c.state().next_instance_id);if(!b.ok)return b;const auto* a=find(s.armies,id);return a->commander_actor_id==b.value.commander_actor_id?b:fail<MilitaryArmyCommandBinding>("MilitaryArmyCommandCurrentMismatch",id);}
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_before(const CommonStore& c,const MilitaryState& s,const StableId& id,const StableId& before){auto limit=sequence(before);if(limit<1||limit>=c.state().next_instance_id||!c.state().facts.find(before))return fail<MilitaryArmyCommandBinding>("MilitaryFieldCommandBoundaryRequired",before);return army_binding(c,s,id,limit);}
// The exact legacy entry and actual-prefix entry share one original commander
// fold. Only the explicit prefix mode changes the occurrence-order capability;
// integer-only field command ticks never acquire a synthetic exact moment.
namespace {
Outcome<MilitaryArmyCommandBinding> army_binding_exact(const CommonStore& c,const MilitaryState& s,const StableId& id,const WorldMoment& now,const std::optional<SourceRef>& before,const MilitaryFormationSourceRead* formations,ExactArithmeticBudget budget,const CommonActualFactPrefix* prefix=nullptr,const WorldMoment* endpoint=nullptr,bool ordinal_only=false,const MilitaryActualSourceWindow* window=nullptr){
 using B=MilitaryArmyCommandBinding;const auto* army=find(s.armies,id);if(!army)return fail<B>("MilitaryArmyCommandFormationRequired",id);
 auto valid=encode_world_moment(now,budget);if(!valid.ok)return {false,{},valid.error};if(now.half_day_tick<c.tick()||now.half_day_tick-c.tick()>1)return fail<B>("MilitaryFieldCommandExactClockRequired",id);
 std::size_t actual_limit=0;
 if(prefix){
  if(!ordinal_only){if(!endpoint)return fail<B>("MilitaryFieldCommandExactBoundaryRequired",id);
   valid=encode_world_moment(*endpoint,budget);if(!valid.ok)return {false,{},valid.error};auto upper=compare_world_moments(*endpoint,now,budget);if(!upper.ok)return {false,{},upper.error};if(upper.value>0)return fail<B>("MilitaryFieldCommandExactBoundaryRequired",id);}
  auto cut=read_actual_common_fact_prefix(c,*prefix);if(!cut.ok)return {false,{},cut.error};actual_limit=cut.value;if(window){if(!ordinal_only)return fail<B>("MilitaryFieldCommandWindowModeMismatch",id);auto bound=validate_military_actual_source_window(c,*prefix,now,window,budget);if(!bound.ok)return {false,{},bound.error};}
 }
 Integer limit=c.state().next_instance_id;if(!prefix&&before){const auto* head=c.state().facts.find(before->id);limit=sequence(before->id);if(!head||head->kind!=before->kind||head->domain!=before->domain||limit<1||limit>=c.state().next_instance_id)return fail<B>("MilitaryFieldCommandExactBoundaryRequired",before->id);}
 const auto formed=read_original_military_formation(c.state(),s,*army,now,formations,budget);if(!formed.ok)return {false,{},formed.error};const auto& f=formed.value;const auto* terms=f.original_terms;
 if(!terms)return fail<B>("MilitaryArmyCommandFormationRequired",id);
 std::size_t actual_last=0;
 if(prefix){
  // Schema 1 carries only a start head. Its completion tick/allocation serial
  // cannot prove that completion occurred inside this actual fact prefix.
  if(f.completed_source.source_schema!=2)return fail<B>("MilitaryActualFormationCompletionRequired",id);
  auto completed=read_actual_common_fact_position(c,f.completed_source.source_ref);if(!completed.ok)return {false,{},completed.error};
  if(window){auto checked=validate_military_source_in_actual_window(c,f.completed_source.source_ref,f.completed_at,now,window,budget);if(!checked.ok)return {false,{},checked.error};}
  if(completed.value>=actual_limit)return fail<B>("MilitaryArmyCommandFormationRequired",id);
  if(!ordinal_only){auto upper=compare_world_moments(f.completed_at,*endpoint,budget);if(!upper.ok)return {false,{},upper.error};if(upper.value>0)return fail<B>("MilitaryArmyCommandFormationRequired",id);}
  actual_last=completed.value;
 }else if(f.original_completion_serial>=limit)return fail<B>("MilitaryArmyCommandFormationRequired",id);
 B out{terms->actor_id,{MilitaryFieldCommandSourceKind::army_formation,f.original_action_source.id}};Integer last=f.original_completion_serial;WorldMoment prior=f.completed_at;
 struct Event {const MilitaryFieldCommandFact* original=nullptr;std::size_t position=0;};
 CheckpointVector<Event> events;
 for(const auto& e:s.field_commands)if(e.target.kind==MilitaryFieldCommandKind::army&&e.target.army_id==id){
  std::size_t position=0;if(prefix){const auto* head=c.state().facts.find(e.id);if(!head)return fail<B>("MilitaryFieldCommandHistoryMismatch",e.id);auto actual=read_actual_common_fact_position(c,*head);if(!actual.ok)return {false,{},actual.error};position=actual.value;}
  events.push_back({&e,position});
 }
 if(prefix)std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.position<b.position;});
 for(const auto& event:events){const auto& e=*event.original;
  const auto seq=sequence(e.id);const auto* head=c.state().facts.find(e.id);
  if(seq<1||seq>=c.state().next_instance_id||!head||head->kind!="military_field_command_assigned"||head->domain!="military"||e.id.rfind("military_field_command:",0)!=0||e.target.organization_id!=army->organization_id||e.target.campaign_id!=army->order_id||e.target.formation_source_id!=army->formation_source||e.target.next_actor_id!=e.actor_id||!c.actor(e.actor_id))return fail<B>("MilitaryFieldCommandHistoryMismatch",e.id);
  bool included=false;
  if(prefix){
   // The retained integer tick is only a legal/nonfuture source bound. It
   // neither compares to endpoint M nor establishes cross-halfday order.
   if(e.tick<0||e.tick>now.half_day_tick||event.position<=actual_last)return fail<B>("MilitaryFieldCommandHistoryMismatch",e.id);
   actual_last=event.position;included=event.position<actual_limit;
  }else{
   auto at=normalize_world_moment(e.tick,{},budget);if(!at.ok)return {false,{},at.error};auto order=compare_world_moments(prior,at.value,budget),upper=compare_world_moments(at.value,now,budget);if(!order.ok)return {false,{},order.error};if(!upper.ok)return {false,{},upper.error};
   if(seq<=last||order.value>0||upper.value>0)return fail<B>("MilitaryFieldCommandHistoryMismatch",e.id);
   last=seq;prior=at.value;included=seq<limit;
  }
  if(!included)continue;if(e.target.previous_actor_id!=out.commander_actor_id||!same(e.target.previous_source,out.command_source))return fail<B>("MilitaryFieldCommandPredecessorMismatch",e.id);out={e.actor_id,{MilitaryFieldCommandSourceKind::field_assignment,e.id}};
 }
 const bool current=prefix?actual_limit==c.state().facts.order().size():!before;
 if(current&&out.commander_actor_id!=army->commander_actor_id)return fail<B>("MilitaryArmyCommandCurrentMismatch",id);return {true,std::move(out),{}};
}
}
// Explicit Time2 observation never changes the legacy caller's clock bound.
// The writer supplies this value from CurrentReadStampRead, not a request tick.
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_exact(const CommonStore& c,const MilitaryState& s,const StableId& id,const WorldMoment& now,const std::optional<SourceRef>& before,const MilitaryFormationSourceRead* formations,ExactArithmeticBudget budget){
 return army_binding_exact(c,s,id,now,before,formations,budget);
}
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_at_actual_prefix(const CommonStore& c,const MilitaryState& s,const StableId& id,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,const MilitaryFormationSourceRead* formations,ExactArithmeticBudget budget){
 return army_binding_exact(c,s,id,now,{},formations,budget,&prefix,&at);
}
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_at_ordinal_prefix(const CommonStore& c,const MilitaryState& s,const StableId& id,const CommonActualFactPrefix& prefix,const WorldMoment& now,const MilitaryFormationSourceRead* formations,ExactArithmeticBudget budget,const MilitaryActualSourceWindow* window){
 return army_binding_exact(c,s,id,now,{},formations,budget,&prefix,nullptr,true,window);
}
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_at(const CommonStore& c,const MilitaryState& s,const StableId& id,const CurrentMomentRead& clock,const MilitaryFormationSourceRead* formations){auto now=clock.read_current_moment();if(!now.ok)return {false,{},now.error};return read_military_army_command_source_exact(c,s,id,now.value,{},formations);}
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_before_at(const CommonStore& c,const MilitaryState& s,const StableId& id,const StableId& before,const WorldMoment& actual,const MilitaryFormationSourceRead* formations){const auto* head=c.state().facts.find(before);if(!head)return fail<MilitaryArmyCommandBinding>("MilitaryFieldCommandExactBoundaryRequired",before);return read_military_army_command_source_exact(c,s,id,actual,SourceRef{head->domain,head->kind,head->id},formations);}
// 当前Fleet字段只随其实际活动期间的军团交接变化，原naval_form作者仍从形成事实读取。
Outcome<MilitaryArmyCommandBinding> query_military_fleet_command_source(const CommonStore& c,const MilitaryState& s,const NavalFleetRecord& fleet){using B=MilitaryArmyCommandBinding;const auto* f=find(s.action_facts,fleet.formation_source);const auto* terms=f?std::get_if<ArmyFormationTerms>(&f->terms):nullptr;const auto* head=c.state().facts.find(fleet.source_id);if(!f||!terms||!head||head->kind!="naval_form"||head->domain!="military_naval"||f->command!="army_form"||f->phase!="completed"||!f->success||!f->result||f->result->army_id!=fleet.army_id||f->actor_id!=terms->actor_id||f->completed_tick>fleet.created_tick)return fail<B>("MilitaryFieldFleetFormationRequired",fleet.id);B out{terms->actor_id,{MilitaryFieldCommandSourceKind::army_formation,f->id}};const auto limit=fleet.release_source_id.empty()?c.state().next_instance_id:sequence(fleet.release_source_id);Integer last=sequence(fleet.source_id);
 for(const auto& field:s.field_commands)if(field.target.kind==MilitaryFieldCommandKind::army&&field.target.fleet_id==fleet.id){auto seq=sequence(field.id);const auto* h=c.state().facts.find(field.id);if(seq<=last||seq>=limit||!h||h->kind!="military_field_command_assigned"||h->domain!="military"||field.target.army_id!=fleet.army_id||field.target.formation_source_id!=fleet.formation_source||field.target.previous_actor_id!=out.commander_actor_id||!same(field.target.previous_source,out.command_source)||field.actor_id!=field.target.next_actor_id)return fail<B>("MilitaryFieldFleetCommandMismatch",field.id);out={field.actor_id,{MilitaryFieldCommandSourceKind::field_assignment,field.id}};last=seq;}
 if(out.commander_actor_id!=fleet.commander_actor_id)return fail<B>("MilitaryFieldFleetCommandMismatch",fleet.id);return {true,std::move(out),{}};
}
Outcome<MilitaryFieldCommandObservation> query_military_field_command_source(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,MilitaryFieldCommandKind kind,const StableId& army,const StableId& unit){return observe(c,s,cat,n,nullptr,kind,army,unit,c.tick(),{});}
Outcome<MilitaryFieldCommandObservation> query_military_field_command_source_before(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,const MilitaryAccess& a,MilitaryFieldCommandKind kind,const StableId& army,const StableId& unit,Integer tick,const StableId& before){return observe(c,s,cat,n,&a,kind,army,unit,tick,before);}
Outcome<MilitaryFieldCommandContact> query_military_field_contact(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,const MilitaryFieldCommandTarget& t,const StableId& actor){return contact(c,s,cat,n,nullptr,t,actor,c.tick(),{});}
Outcome<MilitaryFieldCommandContact> query_military_field_contact_before(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const NavalState* n,const MilitaryAccess& a,const MilitaryFieldCommandTarget& t,const StableId& actor,Integer tick,const StableId& before){return contact(c,s,cat,n,&a,t,actor,tick,before);}

}
