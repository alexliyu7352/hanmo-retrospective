#include "military_battle_lifecycle.hpp"
#include "military_roster_history.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <string_view>
namespace hanmo::resident {using namespace movement_detail;namespace {
using Ids=std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>>;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){const T* result=nullptr;for(const auto& row:rows)if(row.id==id){if(result)fail("MilitaryBattleLifecycleDuplicateOwner",id);result=&row;}return result;}
MilitaryBattleRecord* battle(MilitaryState& s,const StableId& id){MilitaryBattleRecord* out=nullptr;for(auto& row:s.battles)if(row.id==id){if(out)fail("MilitaryBattleLifecycleDuplicateOwner",id);out=&row;}return out;}
void selected(Integer value){if(value!=0&&value!=1)fail("MilitaryBattleLifecycleSelectedSchemaRequired");}
void allocated(const CommonStore& common,const StableId& id,std::string_view kind){
 const auto colon=id.find(':');Integer serial=0;
 if(colon==StableId::npos||std::string_view(id).substr(0,colon)!=kind)fail("MilitaryBattleLifecycleAllocatedSourceRequired",id);
 const auto tail=std::string_view(id).substr(colon+1);const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),serial);
 if(tail.empty()||(tail.size()>1&&tail.front()=='0')||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||serial<1||serial>=common.state().next_instance_id)fail("MilitaryBattleLifecycleAllocatedSourceRequired",id);
}
void moment(const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget){take(encode_world_moment(at,budget));if(take(compare_world_moments(at,now,budget))>0)fail("MilitaryBattleLifecycleFutureSource");}
void same_event(const MilitaryBattleLifecycleEvent1& a,const MilitaryBattleLifecycleEvent1& b){if(a.source_id!=b.source_id||take(compare_world_moments(a.occurred_at,b.occurred_at))!=0)fail("MilitaryBattleLifecycleResolverChangedSource");}
void identity(const CommonStore& common,const MilitaryState& state,const MilitaryBattleRecord& b){
 if(b.id.empty()||find(state.battles,b.id)!=&b||b.source_id.empty()||b.army_ids.empty())fail("MilitaryBattleLifecycleOwnerRequired",b.id);
 Ids armies,people;
 for(const auto& id:b.army_ids)if(id.empty()||!armies.insert(id).second||!find(state.armies,id))fail("MilitaryBattleLifecycleArmyRequired",b.id);
 for(const auto& id:b.participant_actor_ids)if(id.empty()||!people.insert(id).second||!common.actor(id))fail("MilitaryBattleLifecycleParticipantRequired",b.id);
 const auto* contact=find(state.contact_facts,b.source_id);
 if(!contact||!contact->battle_id||*contact->battle_id!=b.id||contact->side_by_army.size()!=b.army_ids.size())fail("MilitaryBattleLifecycleContactRequired",b.id);
 Ids contact_armies;for(const auto& row:contact->side_by_army)if(!armies.count(row.first)||!contact_armies.insert(row.first).second)fail("MilitaryBattleLifecycleContactMismatch",b.id);
}
std::size_t event_position(const CommonStore& common,const MilitaryBattleLifecycleEvent1& e,std::string_view kind,const WorldMoment& now,ExactArithmeticBudget budget){
 allocated(common,e.source_id,kind);moment(e.occurred_at,now,budget);
 return take(read_actual_common_fact_position(common,SourceRefBuildView{"military",kind,e.source_id}));
}
// Original round/result payloads remain with the original Battle validator.
// This finite association preserves their real heads and order; it never
// fabricates a result occurrence M from their legacy tick fields.
std::size_t original_sources(const CommonStore& common,const MilitaryState& state,const MilitaryBattleRecord& b,std::size_t start){
 const auto contact=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_contact",b.source_id}));
 if(contact>=start)fail("MilitaryBattleLifecycleSourceOrder",b.id);
 std::size_t previous=start;Ids receipts;
 for(std::size_t i=0;i<b.round_receipt_ids.size();++i){
  const auto& id=b.round_receipt_ids[i];const auto* casualty=find(state.casualty_facts,id);const auto* fact=casualty?find(state.battle_facts,casualty->battle_id):nullptr;
  const bool final=i+1==b.round_receipt_ids.size()&&b.phase!="planning";
  if(!receipts.insert(id).second||!casualty||!fact||fact->phase!="completed"||fact->session_id!=b.id||fact->contact_source_id!=b.source_id||fact->army_ids!=b.army_ids||fact->final!=final||fact->round!=Integer(i)+1||fact->source_id!=(final?b.source_id:fact->id)||(final&&fact->id!=b.id))fail("MilitaryBattleLifecycleResultSourceRequired",b.id);
  const auto result=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_battle",fact->id}));
  const auto applied=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_casualties",casualty->id}));
  if(previous>=result||result>=applied)fail("MilitaryBattleLifecycleSourceOrder",b.id);previous=applied;
 }
 if(b.phase=="planning"){if(b.applied||!b.settlement_receipt_id.empty())fail("MilitaryBattleLifecyclePrematureResult",b.id);}
 else if(!b.applied||!b.result||b.round_receipt_ids.empty()||b.settlement_receipt_id!=b.round_receipt_ids.back())fail("MilitaryBattleLifecycleResultSourceRequired",b.id);
 return previous;
}
WorldMoment current(const CommonWrite& c,const MilitaryState& s,const MilitaryAccess& access){
 take(validate_military_battle_lifecycle_write_binding(c,s,access));if(access.military_battle_lifecycle_schema!=1)fail("MilitaryBattleLifecycleCapabilityRequired");
 const auto& x=*access.battle_lifecycle;auto at=take(x.clock.read_current_moment());take(encode_world_moment(at,x.arithmetic));
 // This lower bound is only a stale-current guard, not a conversion of the
 // old producer tick into an event moment. The real clock supplies all M.
 if(at.half_day_tick<x.common.tick())fail("MilitaryBattleLifecycleStaleClock");return at;
}
}
Status validate_military_battle_lifecycle_layout(const MilitaryState& state,Integer version){return status([&]{
 selected(version);Ids owners;
 for(const auto& b:state.battles){
  if(!version){if(b.lifecycle)fail("UnexpectedMilitaryBattleLifecycle",b.id);continue;}
  if(b.id.empty()||!owners.insert(b.id).second||!b.lifecycle||b.lifecycle->started.source_id.empty()||(b.phase!="planning"&&b.phase!="result"&&b.phase!="closed"))fail("MilitaryBattleLifecycleRequired",b.id);
  if(bool(b.lifecycle->closed)!=(b.phase=="closed")||(b.lifecycle->closed&&b.lifecycle->closed->source_id.empty()))fail("MilitaryBattleLifecyclePhaseMismatch",b.id);
 }
});}
Status validate_military_battle_lifecycle_sources(const CommonStore& common,const MilitaryState& state,Integer version,const WorldMoment& now,ExactArithmeticBudget budget){return status([&]{
 take(validate_military_battle_lifecycle_layout(state,version));if(version)take(encode_world_moment(now,budget));Ids starts,closes;
 struct Event {std::size_t position;const WorldMoment* moment;};CheckpointVector<Event> events;
 if(version)for(const auto& b:state.battles){identity(common,state,b);const auto& life=*b.lifecycle;
  if(!starts.insert(life.started.source_id).second)fail("MilitaryBattleLifecycleRepeatedSource",b.id);
  const auto start=event_position(common,life.started,"military_battle_started",now,budget);events.push_back({start,&life.started.occurred_at});
  const auto last=original_sources(common,state,b,start);
  if(life.closed){if(!closes.insert(life.closed->source_id).second||life.closed->source_id==life.started.source_id)fail("MilitaryBattleLifecycleRepeatedSource",b.id);
   const auto close=event_position(common,*life.closed,"military_battle_closed",now,budget);
   if(last>=close||take(compare_world_moments(life.started.occurred_at,life.closed->occurred_at,budget))>0)fail("MilitaryBattleLifecycleSourceOrder",b.id);
   events.push_back({close,&life.closed->occurred_at});
  }
 }
 for(const auto& row:common.state().facts.rows()){const auto& h=row.second;if(h.domain=="military"&&(h.kind=="military_battle_started"||h.kind=="military_battle_closed"))
  if(!version||row.first!=h.id||(h.kind=="military_battle_started"?!starts.count(h.id):!closes.count(h.id)))fail("MilitaryBattleLifecycleOrphanSource",row.first);
 }
 std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.position<b.position;});
 for(std::size_t i=1;i<events.size();++i)if(events[i-1].position==events[i].position||take(compare_world_moments(*events[i-1].moment,*events[i].moment,budget))>0)fail("MilitaryBattleLifecycleSourceOrder");
});}
template<class Members> static Outcome<MilitaryBattleOccupancyRead1> read_battle_occupancy_impl(const CommonStore& common,const MilitaryState& state,Integer version,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,Members members){return guard<MilitaryBattleOccupancyRead1>([&]{
 if(version!=1)fail("MilitaryBattleLifecycleCapabilityRequired");if(!common.actor(actor))fail("MilitaryBattleLifecycleActorRequired",actor);
 take(validate_military_battle_lifecycle_sources(common,state,version,now,budget));moment(at,now,budget);const auto cut=take(read_actual_common_fact_prefix(common,prefix));
 const auto included=[&](const MilitaryBattleLifecycleEvent1& e,std::string_view kind){const auto pos=take(read_actual_common_fact_position(common,SourceRefBuildView{"military",kind,e.source_id}));const bool prior=pos<cut;const auto order=take(compare_world_moments(e.occurred_at,at,budget));if((prior&&order>0)||(!prior&&order<0))fail("MilitaryBattleLifecycleHistoryBoundary",actor);return prior;};
 // Validate the complete paired cut/M before the original positive OR may
 // short-circuit. This never reads an unrelated Army member source.
 for(const auto& b:state.battles){const auto& life=*b.lifecycle;included(life.started,"military_battle_started");if(life.closed)included(*life.closed,"military_battle_closed");}
 MilitaryBattleOccupancyRead1 out;
 for(const auto& b:state.battles){const auto& life=*b.lifecycle;const bool started=included(life.started,"military_battle_started");const bool closed=life.closed&&included(*life.closed,"military_battle_closed");if(!started||closed)continue;
  bool held=std::find(b.participant_actor_ids.begin(),b.participant_actor_ids.end(),actor)!=b.participant_actor_ids.end();
  // Preserve the original OR and its positive short circuit. If the fixed
  // participant set does not prove it, a missing Army source is never false.
  if(!held)for(const auto& army:b.army_ids){auto people=take(members(army));for(const auto* id:people)if(*id==actor){held=true;break;}if(held)break;}
  if(held)return MilitaryBattleOccupancyRead1{true,&b,&life.started};
 }
 return out;
});}
Outcome<MilitaryBattleOccupancyRead1> read_military_battle_occupied_before(const CommonStore& common,const MilitaryState& state,const MilitaryExactState& exact,Integer version,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,ExactArithmeticBudget budget){return read_battle_occupancy_impl(common,state,version,actor,prefix,at,now,budget,[&](const StableId& army){return read_military_army_member_refs_at_actual_prefix(common,state,exact,army,prefix,at,now,budget,formations,returns);});}
Outcome<MilitaryBattleOccupancyRead1> read_selected_military_battle_occupied_before(const CommonStore& common,const MilitaryState& state,const CatalogIdentity& identity,const MilitaryExactState* exact,Integer version,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,ExactArithmeticBudget budget,const MilitaryFieldHistoryRead* field){
 auto selected=validate_selected_military_roster_root(common,identity,exact);if(!selected.ok)return {false,{},selected.error};selected=validate_selected_military_field_history(common,state,identity,field);if(!selected.ok)return {false,{},selected.error};
 return read_battle_occupancy_impl(common,state,version,actor,prefix,at,now,budget,[&](const StableId& army){return read_military_army_member_refs_at_selected_actual_prefix(common,state,identity,exact,army,prefix,at,now,budget,formations,returns,field);});
}

Status validate_military_battle_lifecycle_write_binding(const CommonWrite& common,const MilitaryState& state,const MilitaryAccess& access){return status([&]{
 selected(access.military_battle_lifecycle_schema);
 if(!access.military_battle_lifecycle_schema){if(access.battle_lifecycle)fail("UnexpectedMilitaryBattleLifecycleContext");for(const auto& b:state.battles)if(b.lifecycle)fail("MilitaryBattleLifecycleCapabilityRequired",b.id);return;}
 if(!access.battle_lifecycle||static_cast<const CommonWrite*>(&access.battle_lifecycle->common)!=&common||&access.battle_lifecycle->state!=&state)fail("CrossCandidateMilitaryBattleLifecycleContext");
});}
Status publish_military_battle_lifecycle_start_in_candidate(CommonWrite& common,MilitaryState& state,const MilitaryAccess& access,const StableId& id){return status([&]{
 take(validate_military_battle_lifecycle_write_binding(common,state,access));if(!access.military_battle_lifecycle_schema)return;
 auto* b=battle(state,id);if(!b||b->phase!="planning"||b->lifecycle)fail("MilitaryBattleLifecycleStartRequired",id);const auto& c=access.battle_lifecycle->common;identity(c,state,*b);
 take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_contact",b->source_id}));
 auto at=current(common,state,access);auto source=take(common.next_id("military_battle_started"));
 b->lifecycle.emplace(MilitaryBattleLifecycle1{{std::move(source),std::move(at)},std::nullopt});
 take(common.append_fact({b->lifecycle->started.source_id,"military_battle_started","military"}));
});}
Status validate_military_battle_resolved_lifecycle(const MilitaryBattleRecord& before,const MilitaryBattleRecord& after,Integer version){return status([&]{
 selected(version);if(!version){if(before.lifecycle||after.lifecycle)fail("UnexpectedMilitaryBattleLifecycle",before.id);return;}
 if(before.id!=after.id||before.source_id!=after.source_id||before.army_ids!=after.army_ids||before.participant_actor_ids!=after.participant_actor_ids||!before.lifecycle||!after.lifecycle)fail("MilitaryBattleLifecycleResolverChangedIdentity",before.id);
 same_event(before.lifecycle->started,after.lifecycle->started);if(bool(before.lifecycle->closed)!=bool(after.lifecycle->closed))fail("MilitaryBattleLifecycleResolverChangedSource",before.id);if(before.lifecycle->closed)same_event(*before.lifecycle->closed,*after.lifecycle->closed);
});}
Status publish_military_battle_lifecycle_close_in_candidate(CommonWrite& common,MilitaryState& state,const MilitaryAccess& access,MilitaryBattleRecord& b){return status([&]{
 take(validate_military_battle_lifecycle_write_binding(common,state,access));if(!access.military_battle_lifecycle_schema)return;
 if(battle(state,b.id)!=&b||b.phase!="closed"||!b.lifecycle||b.lifecycle->closed)fail("MilitaryBattleLifecycleCloseRequired",b.id);
 const auto& c=access.battle_lifecycle->common;const auto budget=access.battle_lifecycle->arithmetic;identity(c,state,b);auto at=current(common,state,access);
 const auto start=event_position(c,b.lifecycle->started,"military_battle_started",at,budget);original_sources(c,state,b,start);
 if(take(compare_world_moments(b.lifecycle->started.occurred_at,at,budget))>0)fail("MilitaryBattleLifecycleSourceOrder",b.id);
 auto source=take(common.next_id("military_battle_closed"));b.lifecycle->closed.emplace(MilitaryBattleLifecycleEvent1{std::move(source),std::move(at)});
 take(common.append_fact({b.lifecycle->closed->source_id,"military_battle_closed","military"}));
});}
}
