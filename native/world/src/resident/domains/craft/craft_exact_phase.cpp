#include "craft_exact_phase.hpp"
#include "craft_control.hpp"
#include "craft_cancellation.hpp"
#include "craft_condition_effects.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <iterator>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};[[noreturn]]void fail(const char*c,const StableId&id={},ErrorKind kind=ErrorKind::internal){auto error=common_failure(c,id).error;error.kind=kind;throw Failure{std::move(error)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b,ExactArithmeticBudget budget={}){return take(compare_world_moments(a,b,budget));}
void actual(const CraftJobReadContext&x,const WorldMoment&at){if(x.materials.jobs.schema_version!=1||cmp(at,take(x.clock.read_current_moment()),x.arithmetic_budget)||at.half_day_tick<x.materials.common.tick())fail("CraftExactReadBindingMismatch");}
void bound(const CraftExactWriteBinding&b,const WorldMoment&at){const auto&x=b.craft.read;actual(x,at);if(&b.common!=&x.materials.common||&b.jobs!=&x.materials.jobs||&b.elapsed!=&x.elapsed||&b.instances!=&x.materials.instances||&b.properties!=&x.materials.properties||!b.craft.effects.bound_to(b.common,b.jobs))fail("CraftExactWriteBindingMismatch");}
std::shared_ptr<const CraftExactReadLease> read(CraftExactWorldFactory f,const WorldBundle&w,const WorldMoment&at){if(!f.read||!f.write)fail("CraftExactWorldFactoryRequired",{},ErrorKind::unsupported_capability);auto lease=take(f.read(w,at));if(!lease)fail("CraftExactReadLeaseRequired",{},ErrorKind::unsupported_capability);actual(lease->bindings().craft,at);return lease;}
std::shared_ptr<CraftExactWriteLease> write(CraftExactWorldFactory f,WorldBundle&w,const WorldMoment&at,ExactPhaseWriteContext&writes){if(!f.read||!f.write)fail("CraftExactWorldFactoryRequired",{},ErrorKind::unsupported_capability);auto lease=take(f.write(w,at,writes));if(!lease)fail("CraftExactWriteLeaseRequired",{},ErrorKind::unsupported_capability);const auto b=lease->bindings();bound(b,at);if(!b.craft.condition_effects)fail("CraftConditionWriterRequired",{},ErrorKind::unsupported_capability);if(&b.craft.condition_effects->condition_writes!=&writes.condition_writes)fail("CraftConditionCaptureCollectorMismatch");return lease;}
WorldMoment gate_due(const CraftJobReadContext&x,const CraftJob&job,const WorldMoment&from){
 const auto*work=take(read_craft_actual_work(x,job));const auto*gate=std::get_if<CraftWorkGate>(&work->gate);if(!gate||work->execution.phase!=WorkExecutionPhase::working||!same_work_binding(gate->execution_binding,job.execution_binding))fail("CraftExactWorkingGateRequired",job.id);
 const auto full=take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.arithmetic_budget));const auto credit=take(compare_fractions(gate->earned_service_units.value,full.value,x.arithmetic_budget));if(credit>0)fail("CraftGateServiceOverflow",job.id);
 if(credit==0){
  // N has just been earned in this unpublished positive slice. Stable S0
  // validation intentionally rejects it until the actual gate writer consumes it.
  const auto history=take(read_craft_service_chronology(x,job));if(history.phase!=WorkExecutionPhase::working||history.gate_ordinal!=gate->gate_ordinal||cmp(history.gate_opened_at,gate->gate_opened_at,x.arithmetic_budget)||cmp(history.last_accounted_at,gate->last_accounted_at,x.arithmetic_budget)||cmp(gate->last_accounted_at,from,x.arithmetic_budget)||take(compare_fractions(history.earned_service_units.value,full.value,x.arithmetic_budget)))fail("CraftDueServiceHistoryMismatch",job.id);
  NativeCraftActionTimeRead original(x);take(original.validate_pending_craft_gate_sources(*work,from));return gate->last_accounted_at;
 }
 NativeCraftActionTimeRead original(x);auto owner=take(original.read_action_time_owner(craft_action_time_registration(),job.execution_binding.action_id));take(original.validate_shared_action_sources(craft_action_time_registration(),owner,*work));const auto due=take(craft_next_gate_due(work->execution,*gate,x.scale.units_per_half_day,x.arithmetic_budget));if(cmp(due,from,x.arithmetic_budget)<0)fail("UnconsumedCraftGate",job.id);return due;
}
}
Outcome<std::vector<ExactEventPrediction>> predict_craft_exact_events(const CraftJobReadContext&x,const WorldMoment&from,const WorldMoment&until){return guard<std::vector<ExactEventPrediction>>([&]{
 actual(x,from);if(cmp(from,until,x.arithmetic_budget)>0)fail("InvalidCraftPredictionWindow");std::vector<ExactEventPrediction> out;
 for(const auto&row:x.materials.jobs.jobs.rows()){const auto&job=row.second;
  if(job.phase==CraftJobPhase::working){const auto due=gate_due(x,job,from);if(cmp(due,until,x.arithmetic_budget)<=0){const auto*gate=std::get_if<CraftWorkGate>(&take(read_craft_actual_work(x,job))->gate);out.push_back({due,ExactStage::action_completions,craft_job_source(job.id),1,gate->gate_ordinal});}}
  else if(job.phase==CraftJobPhase::awaiting_output_space&&take(read_craft_output_ready(x,job.id)))out.push_back({from,ExactStage::action_completions,craft_quality_source(*job.quality_result_ref),1,0});
  else if(job.phase==CraftJobPhase::settling&&take(read_craft_cancellation_ready(x,job.id)))out.push_back({from,ExactStage::action_completions,craft_cancellation_source(*job.cancellation_ref),1,0});
 }
 return out;
});}
namespace {
bool craft_exact_phase_requires_craft(const CraftExactWorldFactory& factory,const RepairExactWorldFactory& repair)noexcept{
 // No Repair selection retains the original mandatory Craft factory check.
 // A partially supplied Craft factory is still selected and must be rejected
 // by the original read/write helpers, never silently treated as absent.
 return factory.read||factory.write||!repair_exact_selected(repair);
}
}
ExactPhaseRegistration make_craft_exact_phase_registration(CraftExactWorldFactory factory){return make_craft_exact_phase_registration(factory,{});}
ExactPhaseRegistration make_craft_exact_phase_registration(CraftExactWorldFactory factory,RepairExactWorldFactory repair){
 ExactPhaseRegistration r;r.provider=ExactProviderId::crafting;r.profile_id="craft_original_service_and_gates";r.version=1;r.stages={ExactStage::action_completions};r.interval_writer=true;
 if(craft_exact_phase_requires_craft(factory,repair))r.effect_writer_keys={"crafting/service_credit","crafting/work_gate","crafting/output_materialization","crafting/cancellation_materialization"};
 if(repair_exact_selected(repair)){r.effect_writer_keys.push_back("crafting/repair_service");r.effect_writer_keys.push_back("crafting/repair_completed");}
 r.validate_author_selection=[factory,repair](const WorldBundle&w){return guard<Done>([&]{auto at=take(w.actual_phase_moment());if(craft_exact_phase_requires_craft(factory,repair)){auto lease=read(factory,w,at);const auto&x=lease->bindings().craft;take(validate_time_scale(x.scale));take(validate_craft_recipe_catalog(x.materials.recipes,x.materials.common_catalog,x.materials.items));}if(repair_exact_selected(repair))take(validate_repair_exact_author_selection(repair,w));return Done{};});};
 r.validate_world_sources=[factory,repair](const WorldBundle&w,const WorldMoment&at){return guard<Done>([&]{if(craft_exact_phase_requires_craft(factory,repair)){auto lease=read(factory,w,at);take(validate_craft_jobs(lease->bindings().craft));}if(repair_exact_selected(repair))take(validate_repair_exact_world(repair,w,at));return Done{};});};
 r.predict=[factory,repair](const WorldBundle&w,const WorldMoment&from,const WorldMoment&until){return guard<std::vector<ExactEventPrediction>>([&]{if(!craft_exact_phase_requires_craft(factory,repair))return take(predict_repair_exact_events(repair,w,from,until));auto lease=read(factory,w,from);auto out=take(predict_craft_exact_events(lease->bindings().craft,from,until));if(repair_exact_selected(repair)){auto extra=take(predict_repair_exact_events(repair,w,from,until));out.insert(out.end(),std::make_move_iterator(extra.begin()),std::make_move_iterator(extra.end()));}return out;});};
 if(craft_exact_phase_requires_craft(factory,repair))r.adopt_interval_from=[factory,repair](WorldBundle&w,const MomentSpan&span,ExactPhaseWriteContext&writes){return guard<Done>([&]{if(writes.craft_interval)fail("DuplicateCraftIntervalCandidate");auto lease=read(factory,w,span.from);writes.craft_interval=take(begin_craft_interval_candidate(lease->bindings().craft,span));return Done{};});};
 r.provide_interval=[factory,repair](WorldBundle&w,const MomentSpan&span,ExactPhaseWriteContext&writes){return guard<Done>([&]{if(craft_exact_phase_requires_craft(factory,repair)){if(!writes.craft_interval)fail("CraftIntervalCandidateRequired");auto lease=write(factory,w,span.to,writes);auto b=lease->bindings();take(consume_craft_interval_candidate_in_candidate(b.common,b.jobs,b.elapsed,b.craft,*writes.craft_interval,span));}if(repair_exact_selected(repair))take(provide_repair_exact_interval(repair,w,span,writes));return Done{};});};
 r.apply_due=[factory,repair](WorldBundle&w,const ExactEventPrediction&p,ExactPhaseWriteContext&writes){return guard<ExactEventApplied>([&]{
  if(p.actual_source_basis.domain=="crafting"&&p.actual_source_basis.kind=="repair_job")return take(apply_repair_exact_due(repair,w,p,writes));
  if(p.stage!=ExactStage::action_completions||p.source_schema!=1||p.actual_source_basis.domain!="crafting")fail("CraftDueSourceMismatch",p.actual_source_basis.id);
  if(!craft_exact_phase_requires_craft(factory,repair))fail("UnsupportedCraftDueSource",p.actual_source_basis.id);
  auto lease=write(factory,w,p.at,writes);auto b=lease->bindings();const auto pending=take(predict_craft_exact_events(b.craft.read,p.at,p.at));bool found=false;for(const auto&item:pending)if(same_source_ref(item.actual_source_basis,p.actual_source_basis)&&item.source_schema==p.source_schema&&item.dependency_revision==p.dependency_revision)found=true;if(!found)fail("StaleCraftDuePrediction",p.actual_source_basis.id);
  if(p.actual_source_basis.kind=="craft_job")return ExactEventApplied{craft_gate_source(take(apply_craft_work_gate_in_candidate(b.common,b.jobs,b.elapsed,b.craft,p.actual_source_basis.id))),1};
  if(p.actual_source_basis.kind=="craft_quality_result"){const auto*q=take(read_craft_actual_quality(b.craft.read,p.actual_source_basis.id));auto completed=take(materialize_craft_output_in_candidate(b.common,b.jobs,b.elapsed,b.instances,b.properties,b.craft,b.item_outputs,q->job_id));if(!completed)fail("CraftDueOutputChanged",q->job_id);return ExactEventApplied{craft_completion_source(*completed),1};}
  if(p.actual_source_basis.kind=="craft_cancellation_requested"){const auto*q=take(read_craft_cancellation_source(b.craft.read,p.actual_source_basis.id));auto ended=take(settle_craft_cancellation_in_candidate(b.common,b.jobs,b.elapsed,b.instances,b.properties,b.craft,b.item_outputs,q->job_id));if(!ended)fail("CraftDueRecoveryChanged",q->job_id);return ExactEventApplied{craft_cancelled_source(*ended),1};}
  fail("UnsupportedCraftDueSource",p.actual_source_basis.id);return ExactEventApplied{};
 });};
 r.validate_applied_source=[factory,repair](const WorldBundle&w,const ExactEventApplied&event){return guard<Done>([&]{if(event.applied_result.domain=="crafting"&&event.applied_result.kind=="repair_completed"){take(validate_repair_exact_applied(repair,w,event));return Done{};}if(!craft_exact_phase_requires_craft(factory,repair))fail("UnsupportedCraftAppliedSource",event.applied_result.id);auto at=take(w.actual_phase_moment());auto lease=read(factory,w,at);const auto&x=lease->bindings().craft;if(event.source_schema!=1||event.applied_result.domain!="crafting")fail("CraftAppliedSourceMismatch",event.applied_result.id);
  if(event.applied_result.kind=="craft_work_gate"){const auto*gate=take(read_craft_actual_gate(x,event.applied_result.id));if(cmp(gate->occurred_at,at,x.arithmetic_budget)>0)fail("FutureCraftAppliedSource",gate->id);}
  else if(event.applied_result.kind=="craft_completed"){const auto*c=x.materials.jobs.completions.find(event.applied_result.id);const auto*j=c?x.materials.jobs.jobs.find(c->job_id):nullptr;if(!c||!j||j->completion_fact_id!=std::optional<StableId>{c->id}||cmp(c->occurred_at,at,x.arithmetic_budget)>0)fail("CraftAppliedCompletionRequired",event.applied_result.id);take(x.original.validate_actual_craft_completion(*j,*c));}
  else if(event.applied_result.kind=="craft_cancelled"){const auto*r=take(read_craft_cancelled_for_output(x,event.applied_result.id));take(validate_craft_cancellation_materialization(x,*r));const auto*j=x.materials.jobs.jobs.find(r->job_id);take(x.cancellation->validate_craft_cancellation_settlement(*j,*r));}
  else fail("UnsupportedCraftAppliedSource",event.applied_result.id);return Done{};
 });};return r;
}
}
