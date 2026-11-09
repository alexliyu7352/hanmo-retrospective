#include "military_exact_sources.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <charconv>
#include <algorithm>
#include <set>
namespace hanmo::resident { using namespace movement_detail;
namespace {
// Borrowed identity scratch uses the actual shared allocator for tree nodes;
// string payloads stay with their immutable source rows.
using BorrowedMilitaryIds=std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>>;
using BorrowedMilitaryPurpose=std::pair<std::string_view,std::string_view>;
using BorrowedMilitaryPurposes=std::set<BorrowedMilitaryPurpose,std::less<>,CheckpointAllocator<BorrowedMilitaryPurpose>>;
template<class T> const T* find(const std::vector<T>& values,const StableId& id){auto i=std::find_if(values.begin(),values.end(),[&](const auto& x){return x.id==id;});return i==values.end()?nullptr:&*i;}
bool has(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
struct CasualtySourceView { const SourceRef& source_ref; Integer schema; const StableId& session_ref;const StableId& battle_id; WorldMoment occurred_at; const std::vector<MilitaryRosterReceipt>& roster_receipts; const std::vector<MilitaryActorChange>& actor_changes; const std::vector<MilitaryMemberEffectLink>& member_effect_links; };
CheckpointMemoryCharge reserve_actor_consequence(const MilitaryActorConsequenceBuildView& input){
 return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge reserve_fatal_consequence(const MilitaryFatalConsequenceBuildView& input){
 return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge reserve_casualty_source(const MilitaryCasualtySourceBuildView& input){
 return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge reserve_commander_loss(const StableId& actor){
 const Integer initial_loss=0;const CheckpointVector<MomentOwnerSource> no_sources;
 return CheckpointMemoryCharge(0);
}
// The low-level named consequence reader has no current-roster shortcut.
// Its officer identity is reconstructed from retained real formation/assignment/
// integration/return records. Complete group mathematics is additionally checked
// by the original land-consequence validator before stable publication.
// Legacy formation completion is the actual Army allocation serial, not a
// Common completion fact. Preserve that original source-local identity proof;
// real Common fact ordering always uses read_actual_common_fact_position.
Integer original_serial(const StableId& id){
 const auto colon=id.rfind(':');if(colon==StableId::npos)fail("MilitaryOriginalSourceSerialRequired",id);
 const std::string_view tail{id.data()+colon+1,id.size()-colon-1};Integer n=0;
 const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),n);
 if(tail.empty()||tail.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||n<=0)
  fail("MilitaryOriginalSourceSerialRequired",id);
 return n;
}
std::pair<std::string_view,std::string_view> commander_before(const MilitaryState& state,const StableId& unit,const StableId& army,const SourceRef& cut,const WorldMoment& occurred,const MilitaryCasualtyHistoryContext* history,ExactArithmeticBudget budget){
 if(!history)throw Failure{common_failure("MilitaryCommanderOriginalHistoryRequired",cut.id,ErrorKind::unsupported_capability).error};
 (void)army;
 const auto& c=history->common;
 const bool actual_order=history->ordering==MilitaryCasualtyHistoryOrder::actual_common_prefix;
 if(!actual_order&&history->ordering!=MilitaryCasualtyHistoryOrder::legacy_serial)fail("MilitaryCommanderHistoryOrderRequired",cut.id);
 auto position=[&](const SourceRef& ref){return take(read_actual_common_fact_position(c,ref));};
 const auto limit=original_serial(cut.id);const auto cutoff=position(cut);if(limit>=c.state().next_instance_id||take(compare_world_moments(occurred,history->actual_now,budget))>0)fail("MilitaryCommanderActualCutRequired",cut.id);
 struct Event {Integer serial;int kind;std::string_view army;const ArmyFormationTerms* terms=nullptr;const MilitaryFieldCommandFact* command=nullptr;const MilitaryIntegrationFact* integration=nullptr;std::optional<WorldMoment> at;std::size_t actual_position=0;};
 CheckpointVector<Event> events;BorrowedMilitaryIds formation_heads,return_heads,return_applications;
 auto add=[&](Event value,const SourceRef& source,bool actual_common_cut=true,const std::optional<Integer>& integer_tick=std::nullopt){
  const auto p=position(source);
  if(actual_order){
   // Allocation serial remains canonical identity only. Occurrence is the
   // real Common head, including completion application heads where required.
   if(!actual_common_cut)fail("MilitaryActualFormationCompletionRequired",source.id);
   if(value.serial<1||value.serial>=c.state().next_instance_id)fail("MilitaryCommanderHistoryCutMismatch",source.id);
   value.actual_position=p;const bool before=p<cutoff;
   if(integer_tick){
    if(value.at||*integer_tick<0||*integer_tick>history->actual_now.half_day_tick)fail("MilitaryCommanderHistoryCutMismatch",source.id);
   }else{
    if(!value.at)fail("MilitaryCommanderHistoryMomentRequired",source.id);const auto relative=take(compare_world_moments(*value.at,occurred,budget));
    if(take(compare_world_moments(*value.at,history->actual_now,budget))>0||(before&&relative>0)||(!before&&relative<0))fail("MilitaryCommanderHistoryCutMismatch",source.id);
   }
   if(before)events.push_back(std::move(value));return;
  }
  if(!value.at)fail("MilitaryCommanderHistoryMomentRequired",source.id);const auto relative=take(compare_world_moments(*value.at,occurred,budget));const bool before=value.serial<limit;if(value.serial<1||value.serial>=c.state().next_instance_id||take(compare_world_moments(*value.at,history->actual_now,budget))>0||(actual_common_cut&&before!=(p<cutoff))||(before&&relative>0)||(!before&&relative<0))fail("MilitaryCommanderHistoryCutMismatch",source.id);if(before)events.push_back(std::move(value));
 };
 for(const auto& a:state.armies){const auto source=take(read_original_military_formation(c.state(),state,a,history->actual_now,history->formations,budget));if(source.completed_source.source_schema==2){const auto* actual=c.state().facts.find(source.completed_source.source_ref.id);if(!actual||!formation_heads.insert(actual->id).second)fail("MilitaryCommanderFormationSourceRepeated",a.id);}if(has(source.original_terms->unit_ids,unit)){if(actual_order&&source.completed_source.source_schema!=2)fail("MilitaryActualFormationCompletionRequired",a.id);add({source.original_completion_serial,0,a.id,source.original_terms,nullptr,nullptr,source.completed_at},source.completed_source.source_ref,source.completed_source.source_schema==2);}}
 for(const auto& f:state.field_commands)if(f.target.kind==MilitaryFieldCommandKind::unit&&f.target.unit_id==unit)add({original_serial(f.id),1,{},nullptr,&f,nullptr,actual_order?std::optional<WorldMoment>{}:std::optional<WorldMoment>{take(normalize_world_moment(f.tick,{},budget))}},{"military","military_field_command_assigned",f.id},true,actual_order?std::optional<Integer>{f.tick}:std::nullopt);
 for(const auto& f:state.return_facts){if(!return_heads.insert(f.id).second)fail("MilitaryCommanderReturnSourceRepeated",f.id);if(has(f.unit_ids,unit))add({original_serial(f.id),2,f.army_id,nullptr,nullptr,nullptr,actual_order?std::optional<WorldMoment>{}:std::optional<WorldMoment>{take(normalize_world_moment(f.tick,{},budget))}},{"military","military_return",f.id},true,actual_order?std::optional<Integer>{f.tick}:std::nullopt);}
 if(history->returns&&!history->returns->bound_to(c,state))fail("CrossCandidateMilitaryCommanderReturnRead");
 if(history->returns)for(const auto& a:state.armies){const auto source=take(history->returns->read_exact_army_return(a.id));if(!source)continue;const auto* row=source->original;const auto& ref=source->actual_return;const auto& application=source->actual_application;
  if(!row||row->army_id!=a.id||ref.source_schema!=2||ref.source_ref.domain!="military"||ref.source_ref.kind!="military_return"||ref.source_ref.id!=row->id||application.source_schema!=2||application.source_ref.domain!="military"||application.source_ref.kind!="military_logistics_completion_applied"||!same_source(application.source_ref,row->completion_source_ref)||!return_heads.insert(row->id).second||!return_applications.insert(row->completion_source_ref.id).second||position(ref.source_ref)>=position(application.source_ref)||(position(ref.source_ref)<cutoff)!=(position(application.source_ref)<cutoff))fail("MilitaryCommanderReturnSourceMismatch",a.id);
  // Completion occurs at the real application, not its earlier receipt.
  // The original two-head same-side gate above still forbids an intermediate cut.
  if(has(row->unit_ids,unit))add({original_serial(row->id),2,row->army_id,nullptr,nullptr,nullptr,row->completed_at},actual_order?application.source_ref:ref.source_ref);
 }
 for(const auto& f:state.integration_facts)for(const auto& a:f.source_armies)if(has(a.unit_ids,unit)){add({original_serial(f.id),3,{},nullptr,nullptr,&f,actual_order?std::optional<WorldMoment>{}:std::optional<WorldMoment>{take(normalize_world_moment(f.tick,{},budget))}},{"military","military_integration",f.id},true,actual_order?std::optional<Integer>{f.tick}:std::nullopt);break;}
 for(const auto& entry:c.state().facts.rows())if(entry.second.domain=="military"){
  if(entry.second.kind=="military_action_completion_applied"&&!formation_heads.count(entry.first))fail("MilitaryCommanderFormationSourceCoverage",entry.first);
  if(entry.second.kind=="military_return"&&!return_heads.count(entry.first))fail("MilitaryCommanderReturnSourceCoverage",entry.first);
  if(entry.second.kind=="military_logistics_completion_applied"&&!return_applications.count(entry.first))fail("MilitaryCommanderReturnApplicationCoverage",entry.first);
 }
 std::sort(events.begin(),events.end(),[&](const auto& a,const auto& z){return actual_order?a.actual_position<z.actual_position:a.serial<z.serial;});std::string_view current_army,current_actor;Integer prior=0;std::optional<WorldMoment> prior_at;std::optional<std::size_t> prior_position;
 for(const auto& event:events){
  if(actual_order){
   if(prior_position&&event.actual_position<=*prior_position)fail("MilitaryCommanderHistoryOrderMismatch",unit);prior_position=event.actual_position;
   // Only genuine Formation/Return moments participate in temporal checks.
   // Integer-only assignments/returns/integrations cannot order exact M.
   if(event.at){if(prior_at&&take(compare_world_moments(*prior_at,*event.at,budget))>0)fail("MilitaryCommanderHistoryOrderMismatch",unit);prior_at=event.at;}
  }else{if(event.serial<=prior||(prior_at&&take(compare_world_moments(*prior_at,*event.at,budget))>0))fail("MilitaryCommanderHistoryOrderMismatch",unit);prior=event.serial;prior_at=event.at;}
  if(event.kind==0){if(!current_army.empty()||!event.terms)fail("MilitaryCommanderFormationPredecessorMismatch",unit);current_actor={};for(const auto& binding:event.terms->commanders)if(binding.first==unit){if(!current_actor.empty()||binding.second.empty())fail("MilitaryCommanderFormationSourceRequired",unit);current_actor=binding.second;}if(current_actor.empty())fail("MilitaryCommanderFormationSourceRequired",unit);current_army=event.army;}
  else if(event.kind==1){const auto& f=*event.command;if(current_army!=f.target.army_id||current_actor!=f.target.previous_actor_id||f.actor_id!=f.target.next_actor_id||f.actor_id.empty())fail("MilitaryCommanderAssignmentPredecessorMismatch",unit);current_actor=f.actor_id;}
  else if(event.kind==2){if(current_army!=event.army)fail("MilitaryCommanderReturnPredecessorMismatch",unit);current_army={};current_actor={};}
  else{const auto& f=*event.integration;bool found=false;for(const auto& source:f.source_armies)if(has(source.unit_ids,unit)){if(found||source.id!=current_army)fail("MilitaryCommanderIntegrationPredecessorMismatch",unit);found=true;}if(!found||f.recipient_army_id.empty())fail("MilitaryCommanderIntegrationSourceRequired",unit);current_army=f.recipient_army_id;}
 }
 if(current_army.empty()||current_actor.empty())fail("MilitaryCommanderHistoricalArmyRequired",unit);return {current_army,current_actor};
}
bool actual_named_commander_consequence(const MilitaryState& state,const MilitaryExactState& exact,const CasualtySourceView& source,const MilitaryActorChange& change,const MilitaryCasualtyHistoryContext* history,ExactArithmeticBudget budget){
 if(change.status!="wounded"&&change.status!="dead")return false;const auto* casualty=exact.dynamic_casualty_facts.find(source.source_ref.id);if(!casualty)return false;const auto* group=exact.effect_groups.find(casualty->effect_group_ref);if(!group||group->session_ref!=casualty->session_ref)return false;
 const auto effect=[&](const SourceRef& ref){return ref.domain=="military"&&ref.kind=="dynamic_effect_group"&&ref.id==group->id;};bool linked=false;for(const auto& link:source.member_effect_links)if(link.actor_id==change.actor_id)linked=effect(link.effect_source)&&link.purpose==change.status;
 if(!linked||std::none_of(group->rng_slots.begin(),group->rng_slots.end(),[&](const auto& slot){return slot.stage==DynamicMilitaryRandomStage::commander_exposure&&effect(slot.effect_ref);}))return false;
 bool actual=false;for(const auto& roster:source.roster_receipts){if(has(roster.member_ids,change.actor_id))return false;const auto commander=commander_before(state,roster.unit_id,change.army_id,source.source_ref,source.occurred_at,history,budget);if(commander.first==change.army_id&&commander.second==change.actor_id){if(actual)fail("MilitaryCommanderHistoricalUnitAmbiguous",change.actor_id);actual=true;}}
 return actual;
}
// A complete new group can wound and subsequently capture the same actor.
// Only the retained E allocation proves an intermediate wound; final captured
// membership alone never manufactures a body consequence.
bool actual_intermediate_wound(const MilitaryExactState& exact,const CasualtySourceView& source,std::size_t ordinal){const auto& change=source.actor_changes[ordinal];if(change.status!="wounded"||ordinal>=source.member_effect_links.size())return false;const auto* casualty=exact.dynamic_casualty_facts.find(source.source_ref.id);const auto* group=casualty?exact.effect_groups.find(casualty->effect_group_ref):nullptr;if(!group||group->session_ref!=source.session_ref)return false;const auto& link=source.member_effect_links[ordinal];if(link.actor_id!=change.actor_id||link.purpose!="wounded"||link.effect_index!=Integer(ordinal))return false;bool final_captured=false;for(const auto& roster:source.roster_receipts)if(has(roster.captured,change.actor_id))final_captured=true;if(!final_captured)return false;for(const auto& selected:group->member_allocations)if(selected.member_id==change.actor_id&&selected.resulting_status=="wounded"&&same_source(selected.effect_ref,link.effect_source))return true;return false;}
void validate_rosters(const CasualtySourceView& source,const MilitaryState* legacy=nullptr,const MilitaryExactState* exact=nullptr,const MilitaryCasualtyHistoryContext* history=nullptr,ExactArithmeticBudget budget={}){
 BorrowedMilitaryIds units,people;for(const auto& r:source.roster_receipts){if(r.unit_id.empty()||!units.insert(r.unit_id).second||r.morale<0||r.morale>100)fail("InvalidMilitaryCasualtyRoster",source.source_ref.id);BorrowedMilitaryIds partition;for(const auto* group:{&r.fit,&r.wounded,&r.dead,&r.captured,&r.deserted})for(const auto& id:*group)if(id.empty()||!partition.insert(id).second||!people.insert(id).second)fail("MilitaryCasualtyDuplicateMember",id);BorrowedMilitaryIds members(r.member_ids.begin(),r.member_ids.end());if(members.size()!=r.member_ids.size()||members!=partition)fail("MilitaryCasualtyPartitionMismatch",r.unit_id);}
 BorrowedMilitaryIds actors;BorrowedMilitaryPurposes purposes;
 for(std::size_t n=0;n<source.actor_changes.size();++n){const auto& change=source.actor_changes[n];if(change.actor_id.empty()||!purposes.emplace(change.actor_id,change.status).second||(source.schema==1&&!actors.insert(change.actor_id).second))fail("MilitaryCasualtyDuplicateActor",change.actor_id);if(source.schema==2&&!actors.insert(change.actor_id).second){bool actual_pair=false;for(std::size_t previous=0;previous<n;++previous)if(source.actor_changes[previous].actor_id==change.actor_id)actual_pair=change.status=="captured"&&source.actor_changes[previous].status=="wounded"&&exact&&actual_intermediate_wound(*exact,source,previous);if(!actual_pair)fail("MilitaryCasualtyDuplicateActor",change.actor_id);}
  bool matching=false;for(const auto& r:source.roster_receipts){const std::vector<StableId>* group=change.status=="dead"?&r.dead:change.status=="wounded"?&r.wounded:change.status=="captured"?&r.captured:change.status=="deserted"?&r.deserted:change.status=="fit"?&r.fit:nullptr;if(group&&has(*group,change.actor_id))matching=true;}
  if(!matching&&!(legacy&&exact&&source.schema==2&&(actual_intermediate_wound(*exact,source,n)||actual_named_commander_consequence(*legacy,*exact,source,change,history,budget))))fail("MilitaryCasualtyActorRosterMismatch",change.actor_id);
 }
}
CasualtySourceView casualty_source(const MilitaryState& legacy,const MilitaryExactState* exact,const SourceRef& ref,Integer schema,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){
    if(ref.domain!="military"||ref.kind!="military_casualties"||(schema!=1&&schema!=2))fail("UnsupportedMilitaryCasualtySource",ref.id);
    if(history){const auto* head=history->common.state().facts.find(ref.id);if(!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind||(history->formations&&!history->formations->bound_to(history->common.state(),legacy))||(history->returns&&!history->returns->bound_to(history->common,legacy)))fail("MilitaryCasualtyOriginalContextMismatch",ref.id);}
    const auto* old=find(legacy.casualty_facts,ref.id);const auto* dynamic=exact?exact->dynamic_casualty_facts.find(ref.id):nullptr;
    if(old&&dynamic)fail("MilitarySourcePayloadDuplicated",ref.id);
    if(schema==1){
        if(!old)fail("MilitaryCasualtySourceRequired",ref.id);const auto* battle=find(legacy.battle_facts,old->battle_id);
        if(!battle||battle->tick!=old->tick)fail("MilitaryCasualtyBattleSourceRequired",ref.id);
        static const std::vector<MilitaryMemberEffectLink> no_links;
        CasualtySourceView out{ref,schema,battle->session_id,old->battle_id,take(normalize_world_moment(old->tick,{},b)),old->units,old->actor_changes,no_links};validate_rosters(out);return out;
    }
    if(!dynamic)fail("DynamicMilitaryCasualtySourceRequired",ref.id);const DynamicMilitaryBattleFact2* battle=nullptr;
    for(const auto& row:exact->dynamic_battle_facts.rows())if(row.second.session_ref==dynamic->session_ref&&row.second.effect_group_ref==dynamic->effect_group_ref){if(battle)fail("MilitaryCasualtyAmbiguousEffectGroup",ref.id);battle=&row.second;}
    if(!battle||!same_moment(battle->effective_at,dynamic->effective_at,b)||dynamic->original_casualty_rule_ref.empty())fail("MilitaryCasualtyBattleSourceRequired",ref.id);
    CasualtySourceView out{ref,schema,dynamic->session_ref,battle->id,dynamic->effective_at,dynamic->roster_receipts,dynamic->actor_changes,dynamic->member_effect_links};
    if(out.member_effect_links.size()!=out.actor_changes.size())fail("MilitaryCasualtyEffectCoverage",ref.id);
    for(std::size_t i=0;i<out.actor_changes.size();++i){const auto& link=out.member_effect_links[i];const auto& change=out.actor_changes[i];if(link.actor_id!=change.actor_id||link.effect_index!=Integer(i)||link.purpose!=change.status||!source_valid(link.effect_source))fail("MilitaryCasualtyEffectMismatch",ref.id);}
    if(history&&take(compare_world_moments(out.occurred_at,history->actual_now,b))>0)fail("MilitaryCasualtyActualFutureSource",ref.id);
    validate_rosters(out,&legacy,exact,history,b);return out;
}
MilitaryFatalConsequence fatal(const CasualtySourceView& source,Integer ordinal){if(ordinal<0||std::size_t(ordinal)>=source.actor_changes.size()||source.actor_changes[std::size_t(ordinal)].status!="dead")fail("MilitaryFatalConsequenceRequired",source.source_ref.id);return MilitaryFatalConsequence{MilitaryFatalConsequenceBuildView{source.actor_changes[std::size_t(ordinal)].actor_id,source.source_ref.id,source.battle_id,source.occurred_at,ordinal}};}
}
Outcome<DynamicMilitaryCommanderLossRead> read_dynamic_military_commander_loss_before_group(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const StableId& session,const StableId& actor,const SourceRef& ref,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<DynamicMilitaryCommanderLossRead>([&]{
 if(history&&(&history->common!=&common||!same_moment(history->actual_now,now,b)))fail("CrossCandidateMilitaryCasualtyHistory");
 const auto* head=common.state().facts.find(ref.id);const auto* group=exact.effect_groups.find(ref.id);if(ref.domain!="military"||ref.kind!="dynamic_effect_group"||!head||head->domain!=ref.domain||head->kind!=ref.kind||!group||group->id!=ref.id||group->session_ref!=session||!same_moment(group->effective_at,at,b)||take(compare_world_moments(at,now,b))>0||!common.actor(actor))fail("DynamicMilitaryCommanderHistoryBoundaryRequired",ref.id);
 const auto limit=take(read_actual_common_fact_position(common,ref));
 struct HistoryRow {const DynamicMilitaryCasualtyFact2* row;std::size_t position;};
 CheckpointVector<HistoryRow> rows;
 for(const auto& entry:exact.dynamic_casualty_facts.rows()){
  const auto& row=entry.second;if(row.session_ref!=session)continue;
  const auto position=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_casualties",row.id}));
  const auto temporal=take(compare_world_moments(row.effective_at,at,b));
  if(position<limit){if(temporal>0)fail("DynamicMilitaryCommanderHistoryTimeReversed",row.id);rows.push_back({&row,position});}
  else if(temporal<0)fail("DynamicMilitaryCommanderHistoryIncompleteCut",row.id);
 }
 std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& z){return a.position<z.position;});
 DynamicMilitaryCommanderLossRead out{actor};out.actual_sources.reserve(2);std::optional<WorldMoment> prior;
 for(const auto& selected:rows){
  const auto* row=selected.row;const auto* h=common.state().facts.find(row->id);
  if(!h||h->id!=row->id||h->domain!="military"||h->kind!="military_casualties"||
     (prior&&take(compare_world_moments(*prior,row->effective_at,b))>0))
   fail("DynamicMilitaryCommanderHistorySourceRequired",row->id);
  prior=row->effective_at;
  const SourceRefBuildView source_view{h->domain,h->kind,h->id};
  auto local_fields=CheckpointMemoryCharge(0);
  const SourceRef source{StableId(source_view.domain),StableId(source_view.kind),StableId(source_view.id)};
  const auto original=casualty_source(military,&exact,source,2,b,history);
  for(const auto& change:original.actor_changes)if(change.actor_id==actor&&(change.status=="wounded"||change.status=="dead")){
   const Integer loss=change.status=="dead"?2:1;
   if(loss>out.loss){
    (out.copied_fields).grow(0);
    out.actual_sources.push_back({source,2});out.loss=loss;
   }
  }
 }
 return out;
});}
MilitaryFatalConsequence::MilitaryFatalConsequence(const MilitaryFatalConsequenceBuildView& input)
 :copied_fields(reserve_fatal_consequence(input)),actor_id(checkpoint_fresh_string_copy(input.actor_id)),
 casualty_id(checkpoint_fresh_string_copy(input.casualty_id)),battle_id(checkpoint_fresh_string_copy(input.battle_id)),
 occurred_at(input.occurred_at),source_ordinal(input.source_ordinal){}
MilitaryCasualtySourceRead::MilitaryCasualtySourceRead(const MilitaryCasualtySourceBuildView& input)
 :copied_fields(reserve_casualty_source(input)),source_ref(input.source_ref),schema(input.schema),
 session_ref(checkpoint_fresh_string_copy(input.session_ref)),battle_id(checkpoint_fresh_string_copy(input.battle_id)),
 occurred_at(input.occurred_at),roster_receipts(input.roster_receipts),actor_changes(input.actor_changes),
 member_effect_links(input.member_effect_links){}
DynamicMilitaryCommanderLossRead::DynamicMilitaryCommanderLossRead(const StableId& actor)
 :copied_fields(reserve_commander_loss(actor)),actor_id(checkpoint_fresh_string_copy(actor)){
 static_assert(std::is_nothrow_move_constructible_v<MomentOwnerSource>);
 static_assert(std::is_nothrow_move_constructible_v<CheckpointVector<MomentOwnerSource>>);
}
MilitaryActorConsequence::MilitaryActorConsequence(const MilitaryActorConsequenceBuildView& input)
 :copied_fields(reserve_actor_consequence(input)),
  actor_id(checkpoint_fresh_string_copy(input.actor_id)),status(checkpoint_fresh_string_copy(input.status)),
  casualty_id(checkpoint_fresh_string_copy(input.casualty_id)),battle_id(checkpoint_fresh_string_copy(input.battle_id)),
  original_casualty_rule_ref(checkpoint_fresh_string_copy(input.original_casualty_rule_ref)),
  occurred_at(input.occurred_at),source_ordinal(input.source_ordinal),
  effect_source{checkpoint_fresh_string_copy(input.effect_source.domain),
   checkpoint_fresh_string_copy(input.effect_source.kind),checkpoint_fresh_string_copy(input.effect_source.id)}{}
Outcome<MilitaryActorConsequence> read_military_actor_consequence(const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,Integer ordinal,ExactArithmeticBudget budget,const MilitaryCasualtyHistoryContext* history){return guard<MilitaryActorConsequence>([&]{
    if(schema!=2)fail("MilitaryActorConsequenceRequiresExactSource",ref.id);
    const auto source=casualty_source(legacy,&exact,ref,schema,budget,history);
    if(ordinal<0||std::size_t(ordinal)>=source.actor_changes.size())fail("MilitaryActorConsequenceOrdinalMismatch",ref.id);
    const auto& change=source.actor_changes[std::size_t(ordinal)];
    const auto& link=source.member_effect_links[std::size_t(ordinal)];
    const auto* original=exact.dynamic_casualty_facts.find(ref.id);
    if(!original)fail("DynamicMilitaryCasualtySourceRequired",ref.id);
    return MilitaryActorConsequence{MilitaryActorConsequenceBuildView{change.actor_id,change.status,ref.id,source.battle_id,original->original_casualty_rule_ref,source.occurred_at,ordinal,link.effect_source}};
});}
Outcome<MilitaryCasualtySourceRead> read_military_casualty_source(const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<MilitaryCasualtySourceRead>([&]{auto source=casualty_source(legacy,&exact,ref,schema,b,history);return MilitaryCasualtySourceRead{MilitaryCasualtySourceBuildView{ref,schema,source.session_ref,source.battle_id,source.occurred_at,source.roster_receipts,source.actor_changes,source.member_effect_links}};});}
Outcome<MilitaryFatalConsequence> read_military_fatal_consequence(const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,Integer ordinal,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<MilitaryFatalConsequence>([&]{return fatal(casualty_source(legacy,&exact,ref,schema,b,history),ordinal);});}
Outcome<MilitaryFatalConsequence> read_military_fatal_consequence_for_actor(const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,const StableId& actor,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<MilitaryFatalConsequence>([&]{auto source=casualty_source(legacy,&exact,ref,schema,b,history);for(std::size_t i=0;i<source.actor_changes.size();++i)if(source.actor_changes[i].actor_id==actor)return fatal(source,Integer(i));fail("MilitaryFatalConsequenceRequired",actor);});}
Outcome<WorldMoment> read_legacy_military_casualty_source_moment(const MilitaryState& legacy,const SourceRef& ref,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<WorldMoment>([&]{return casualty_source(legacy,nullptr,ref,1,b,history).occurred_at;});}
Outcome<WorldMoment> read_military_source_moment(const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return guard<WorldMoment>([&]{if(ref.domain!="military")fail("UnsupportedMilitarySource",ref.id);if(ref.kind=="military_casualties")return casualty_source(legacy,&exact,ref,schema,b,history).occurred_at;if(ref.kind=="military_battle"){const auto* old=find(legacy.battle_facts,ref.id);const auto* dynamic=exact.dynamic_battle_facts.find(ref.id);if(old&&dynamic)fail("MilitarySourcePayloadDuplicated",ref.id);if(schema==1&&old)return take(normalize_world_moment(old->tick,{},b));if(schema==2&&dynamic)return dynamic->effective_at;fail("MilitaryBattleSourceRequired",ref.id);}if(ref.kind=="military_occupation"&&schema==2){const auto* f=find(legacy.occupation_facts,ref.id);if(!f||!f->exact_time||!f->assets_applied.value_or(false)||f->exact_time->actual_occupation_at.half_day_tick!=f->tick)fail("MilitaryOccupationActualMomentRequired",ref.id);take(encode_world_moment(f->exact_time->actual_occupation_at,b));return f->exact_time->actual_occupation_at;}if(schema!=1)fail("UnsupportedMilitarySourceSchema",ref.id);if(ref.kind=="military_return"){const auto* f=find(legacy.return_facts,ref.id);if(f)return take(normalize_world_moment(f->tick,{},b));}if(ref.kind=="military_occupation"){const auto* f=find(legacy.occupation_facts,ref.id);if(f&&!f->exact_time&&f->assets_applied.value_or(false))return take(normalize_world_moment(f->tick,{},b));}if(ref.kind=="military_delivery"){const auto* f=find(legacy.delivery_facts,ref.id);if(f)return take(normalize_world_moment(f->tick,{},b));}if(ref.kind=="campaign_arrival"){const auto* f=find(legacy.arrival_facts,ref.id);if(f)return take(normalize_world_moment(f->tick,{},b));}if(ref.kind=="military_supply")fail("MilitarySupplyPeriodIsNotMoment",ref.id);fail("MilitaryActualSourceRequired",ref.id);});}
Status validate_military_exact_sources(const CommonRead& common,const MilitaryState& legacy,const MilitaryExactState& exact,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext* history){return status([&]{if(history&&static_cast<const CommonRead*>(&history->common)!=&common)fail("CrossCandidateMilitaryCasualtyHistory");for(const auto& row:exact.dynamic_battle_facts.rows()){const auto& f=row.second;if(row.first!=f.id||f.id.empty()||f.session_ref.empty()||f.effect_group_ref.empty()||find(legacy.battle_facts,f.id)||!source_valid(f.source_ref)||!source_valid(f.contact_source_ref)||take(compare_world_moments(f.started_at,f.effective_at,b))>0||f.final!=bool(f.result)||bool(f.winner_side)!=bool(f.result)||(f.result&&*f.winner_side!=f.result->winner_side))fail("InvalidDynamicMilitaryBattleSource",row.first);}for(const auto& row:exact.dynamic_casualty_facts.rows()){if(row.first!=row.second.id)fail("MilitaryCasualtyKeyMismatch",row.first);const SourceRef ref{"military","military_casualties",row.first};auto source=casualty_source(legacy,&exact,ref,2,b,history);for(const auto& change:source.actor_changes)if(!common.actor(change.actor_id))fail("MilitaryCasualtyActorMissing",change.actor_id);}for(const auto& row:exact.supply_effect_sources.rows()){const auto& x=row.second;const auto* original=find(legacy.supply_facts,x.original_supply_fact_ref.id);if(row.first!=x.original_supply_fact_ref.id||x.original_supply_fact_ref.domain!="military"||x.original_supply_fact_ref.kind!="military_supply"||!original||x.period_ref.empty()||!source_valid(x.opening_source_ref)||x.shortage_value!=original->shortage||x.original_rule_ref.empty())fail("MilitarySupplyEffectSourceMismatch",row.first);BorrowedMilitaryIds unique;for(const auto& actor:x.selected_named_actor_ids)if(!common.actor(actor)||!has(original->mouth_ids,actor)||!unique.insert(actor).second)fail("MilitarySupplyNamedSelectionMismatch",actor);if(x.shortage_value==0&&!x.selected_named_actor_ids.empty())fail("FullSupplyHasShortageEffects",row.first);take(encode_world_moment(x.occurred_at,b));}});}
}
