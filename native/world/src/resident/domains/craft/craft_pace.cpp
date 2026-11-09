#include "craft_pace.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};[[noreturn]]void fail(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void ordered(const CommonStore&c,const StableId&first,const StableId&second){bool seen=false;for(const auto&id:c.state().facts.order()){if(id==second){if(!seen)fail("CraftPaceSourceOrderMismatch",second);return;}if(id==first)seen=true;}fail("CraftPaceHeaderRequired",second);}
enum class PaceSourceScope {complete,original_gate_input};
CraftPaceSelection history(const CraftJobReadContext&x,const CraftJob&job,Integer gate,PaceSourceScope scope=PaceSourceScope::complete){
 if(gate<0)fail("InvalidCraftPaceGate",job.id);CraftPaceSelection out{job.terms.pace,job.id,0};Integer revision=0,last_gate=0;CraftPace prior=job.terms.pace;WorldMoment prior_at=job.terms.accepted_at;StableId prior_source=job.id;const auto current=take(x.clock.read_current_moment());
 for(const auto&id:job.pace_change_refs){const auto*change=x.materials.jobs.pace_changes.find(id);const auto*h=x.materials.common.state().facts.find(id);if(!change||change->id!=id||change->job_id!=job.id||!h||h->domain!="crafting"||h->kind!="craft_pace_changed"||revision==std::numeric_limits<Integer>::max()||change->pace_revision!=revision+1||change->actor_id!=job.terms.maker.id||change->gate_ordinal<last_gate||change->gate_ordinal<0||static_cast<std::uint64_t>(change->gate_ordinal)>job.gate_result_refs.size()||change->previous_pace!=prior||(change->pace!=CraftPace::normal&&change->pace!=CraftPace::rush)||take(compare_world_moments(prior_at,change->occurred_at))>0||take(compare_world_moments(change->occurred_at,current))>0)fail("CraftPaceOriginalHistoryMismatch",id);
  const auto last=change->gate_ordinal?std::optional<StableId>{job.gate_result_refs[static_cast<std::size_t>(change->gate_ordinal-1)]}:std::optional<StableId>{};if(change->last_closed_gate_ref!=last)fail("CraftPacePredecessorMismatch",id);ordered(x.materials.common,prior_source,id);
  if(last){const auto*closed=x.materials.jobs.gates.find(*last);if(!closed||closed->job_id!=job.id||take(compare_world_moments(closed->occurred_at,change->occurred_at))>0)fail("CraftPaceBeforeOriginalGate",id);ordered(x.materials.common,*last,id);}
  for(const auto&service_id:job.actual_service_refs){const auto*service=x.materials.jobs.service_spans.find(service_id);if(!service)fail("CraftPaceServiceSourceRequired",service_id);if(service->gate_ordinal==change->gate_ordinal){ordered(x.materials.common,id,service_id);if(take(compare_world_moments(change->occurred_at,service->actual.actual_span.from))>0)fail("CraftPaceAfterServiceStarted",id);}}
  if(scope==PaceSourceScope::complete)take(x.original.validate_craft_pace_change(job,*change,CraftPaceAdoptionCheck::existing_source));revision=change->pace_revision;last_gate=change->gate_ordinal;prior=change->pace;prior_at=change->occurred_at;prior_source=id;if(change->gate_ordinal<=gate)out={change->pace,id,change->pace_revision};
 }
 return out;
}
}
Outcome<CraftPaceSelection> read_craft_pace_for_gate(const CraftJobReadContext&x,const CraftJob&j,Integer gate){try{return {true,history(x,j,gate),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<CraftPaceSelection> read_craft_pace_for_original_effect(const CraftJobReadContext&x,const CraftJob&j,Integer gate){try{return {true,history(x,j,gate,PaceSourceScope::original_gate_input),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<const CraftPaceChange*> read_craft_pace_change(const CraftJobReadContext&x,const StableId&id){try{const auto*row=x.materials.jobs.pace_changes.find(id);const auto*j=row?x.materials.jobs.jobs.find(row->job_id):nullptr;if(!row||row->id!=id||!j||std::count(j->pace_change_refs.begin(),j->pace_change_refs.end(),id)!=1)fail("CraftPaceSourceRequired",id);history(x,*j,row->gate_ordinal);return {true,row,{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<StableId> change_craft_pace_in_candidate(CommonStore&c,CraftJobsState&s,ActionElapsedStore&e,const CraftJobWriteContext&x,const ChangeCraftPaceRequest&q){try{
 if(&c!=&x.read.materials.common||&s!=&x.read.materials.jobs||&e!=&x.read.elapsed||!x.effects.bound_to(c,s)||s.schema_version!=1)fail("CraftCandidateContextMismatch",q.job_id);
 const auto*found=s.jobs.find(q.job_id);if(!found||found->id!=q.job_id)fail("CraftJobRequired",q.job_id);auto&job=*s.jobs.find(q.job_id);
 if(q.actor_id!=job.terms.maker.id||q.expected_pace_revision<0||q.expected_pace_revision==std::numeric_limits<Integer>::max()||q.expected_gate_ordinal<0||(q.pace!=CraftPace::normal&&q.pace!=CraftPace::rush))fail("InvalidCraftPaceRequest",q.job_id);
 auto same_contract=[](const auto&a,const auto&b){return a.has_value()==b.has_value()&&(!a||(a->source_schema==b->source_schema&&same_source_ref(a->source_ref,b->source_ref)));};
 // Exact accepted source replay precedes today's phase/credit/version checks.
 for(const auto&id:job.pace_change_refs){const auto*old=s.pace_changes.find(id);if(!old)fail("CraftPaceSourceRequired",id);if(old->pace_revision!=q.expected_pace_revision+1)continue;
  if(old->actor_id!=q.actor_id||old->gate_ordinal!=q.expected_gate_ordinal||old->last_closed_gate_ref!=q.expected_last_closed_gate_ref||old->pace!=q.pace||!same_contract(old->original_contract_change,q.original_contract_change))fail("ConflictingCraftPaceReplay",id);
  take(read_craft_pace_change(x.read,id));return {true,id,{}};
 }
 if(job.phase!=CraftJobPhase::working)fail("CraftPaceRequiresWorking",job.id);
 const auto before=*take(read_craft_actual_work(x.read,job));const auto*gate=std::get_if<CraftWorkGate>(&before.gate);if(!gate)fail("CraftGateRequired",job.id);
 const auto selected=take(read_craft_pace_for_gate(x.read,job,gate->gate_ordinal));
 if(gate->gate_ordinal!=q.expected_gate_ordinal||gate->last_closed_gate_result_ref!=q.expected_last_closed_gate_ref||selected.pace_revision!=q.expected_pace_revision||gate->pace_for_gate!=selected.pace||gate->accepted_pace_source_ref!=selected.actual_acceptance_id)fail("StaleCraftPaceRequest",job.id);
 const auto at=take(x.read.clock.read_current_moment());if(at.half_day_tick<c.tick()||take(compare_world_moments(gate->last_accounted_at,at,x.read.arithmetic_budget))!=0||take(compare_fractions(gate->earned_service_units.value,CanonicalFraction{},x.read.arithmetic_budget))!=0)fail("CraftPaceRequiresEmptyCurrentGate",job.id);
 CraftPaceChange row;row.job_id=job.id;row.actor_id=q.actor_id;row.pace_revision=q.expected_pace_revision+1;row.gate_ordinal=q.expected_gate_ordinal;row.last_closed_gate_ref=q.expected_last_closed_gate_ref;row.previous_pace=selected.pace;row.pace=q.pace;row.occurred_at=at;row.original_contract_change=q.original_contract_change;
 take(x.read.original.validate_craft_pace_change(job,row,CraftPaceAdoptionCheck::new_adoption));
 if(selected.pace==q.pace)return {true,selected.actual_acceptance_id,{}};
 const auto id=take(c.next_id("craft_pace"));row.id=id;take(c.append_fact({id,"craft_pace_changed","crafting"}));if(!s.pace_changes.insert(id,std::move(row)))fail("DuplicateCraftPaceSource",id);job.pace_change_refs.push_back(id);
 take(read_craft_pace_change(x.read,id));auto prepared=take(prepare_shared_craft_pace_change(before,q.pace,{craft_pace_change_source(id),1},q.expected_gate_ordinal,q.expected_last_closed_gate_ref,craft_action_time_registration(),x.read.scale,x.action_sources,at,x.read.arithmetic_budget));
 take(apply_shared_craft_pace_change_in_candidate(e,std::move(prepared),craft_action_time_registration(),x.read.scale,x.action_sources,x.read.arithmetic_budget));
 const auto*after=take(read_craft_actual_work(x.read,job));ActionMutation projection;projection.action.id=job.execution_binding.action_id;projection.remaining_ticks=take(craft_remaining_half_days(after->execution,std::get<CraftWorkGate>(after->gate),x.read.scale.units_per_half_day,x.read.arithmetic_budget));take(c.apply_action(projection));return {true,id,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}

}
