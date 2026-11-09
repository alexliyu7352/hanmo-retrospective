#include "repair_service_sources.hpp"
#include "repair_detail.hpp"
#include "resident/domains/services/service_actual_termination_contract.hpp"
namespace hanmo::resident {using namespace repair_detail;
Outcome<const RepairAcceptedSource*> read_repair_service_scope(const RepairReadContext&x,const RepairService&s){try{
 const auto*terms=std::get_if<SourceFactTerms>(&s.repair_terms_ref);
 if(!terms||!same_source_ref(terms->source,s.repair_activity_ref)||!same_source_ref(s.repair_activity_ref,repair_accepted_source(s.repair_activity_ref.id)))fail("RepairServiceAcceptedTermsIdentityRequired",s.repair_activity_ref.id);
 const auto accepted=take(read_repair_accepted_service(x,{s.repair_activity_ref,1}));if(accepted.acceptance->terms.instance_id!=s.instance_id)fail("RepairServiceInstanceMismatch",s.instance_id);return {true,accepted.acceptance,{}};
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<const RepairInterruptedSource*> read_repair_interruption_source(const RepairReadContext&x,const MomentOwnerSource&s){try{
 context(x);if(s.source_schema!=1||!same_source_ref(s.source_ref,repair_interrupted_source(s.source_ref.id)))fail("UnsupportedRepairInterruptionSource",s.source_ref.id,ErrorKind::unsupported_capability);
 const auto*r=x.items.repair_jobs.interruptions.find(s.source_ref.id);if(!r||r->id!=s.source_ref.id)fail("RepairInterruptionSourceRequired",s.source_ref.id);
 const auto*j=take(read_original_repair_job(x,r->job_id));if(j->phase!=RepairJobPhase::interrupted||j->interrupted_ref!=std::optional<StableId>{r->id}||j->completed_ref||compare(r->occurred_at,j->started_at)<0||compare(r->occurred_at,now(x))>0||header(x.common,s.source_ref)<=header(x.common,repair_job_source(j->id)))fail("RepairOriginalInterruptionMismatch",r->id);
 take(x.original.validate_repair_interruption(*j,*r));
 const auto*elapsed=find_fixed_elapsed(x.elapsed,j->action_id);const auto*terminal=elapsed?std::get_if<ElapsedTerminal>(&elapsed->timing):nullptr;
 if(!elapsed||!terminal||terminal->outcome_ref!=r->id||compare(elapsed->last_settled_at,r->occurred_at)||x.common.action(j->action_id))fail("RepairAppliedInterruptionRequired",r->id);
 const auto*worker=x.common.actor(j->terms.worker_id);if(worker&&worker->action_id==j->action_id)fail("RepairInterruptedWorkerStillBound",r->id);
 const ActionElapsedTerminalBinding*binding=nullptr;for(const auto&row:x.elapsed.terminal_bindings)if(row.action_ref==j->action_id){if(binding)fail("DuplicateRepairTerminalBinding",r->id);binding=&row;}
 const auto*began=binding?std::get_if<MomentOwnerSource>(&binding->started_source_ref):nullptr;const auto*ended=binding?std::get_if<MomentOwnerSource>(&binding->ended_source_ref):nullptr;
 if(!binding||binding->action_kind!="repair"||binding->actor_id!=j->terms.worker_id||binding->participant_actor_ids!=std::vector<StableId>{j->terms.worker_id}||binding->timing_kind!=0||binding->terminal_result_ref!=r->id||binding->last_closed_gate_result_ref||!binding->retained_gate_result_refs.empty()||!same_source_ref(binding->original_execution_source_ref,repair_job_source(j->id))||!began||began->source_schema!=1||!same_source_ref(began->source_ref,repair_job_source(j->id))||!ended||ended->source_schema!=1||!same_source_ref(ended->source_ref,s.source_ref))fail("RepairOriginalTerminalBindingMismatch",r->id);
 const auto history=transitions(x,*j);if(history.empty()||history.back().transition_id!=r->id||history.back().kind!=FixedTransitionKind::terminal||history.back().terminal_disposition!=std::optional<FixedTerminalDisposition>{FixedTerminalDisposition::interrupted})fail("RepairInterruptionTransitionRequired",r->id);
 NativeRepairActionTimeRead original(x);const auto proof=take(original.read_action_time_owner(repair_action_time_registration(),j->action_id));take(original.validate_fixed_action_sources(repair_action_time_registration(),proof,*elapsed,history));take(validate_fixed_elapsed_value(*elapsed,now(x),x.scale,x.arithmetic_budget));return {true,r,{}};
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<RepairOriginalServiceTermination> read_repair_service_internal_termination(const RepairReadContext&x,const RepairService&s,const RepairTerminal&t){try{
 const auto*a=take(read_repair_service_scope(x,s));if(t.source_schema!=1||!same_source_ref(t.repair_activity_ref,s.repair_activity_ref)||t.repair_result_id.empty())fail("RepairServiceTerminalIdentityMismatch",t.repair_result_id);
 const auto*head=x.common.state().facts.find(t.repair_result_id);if(!head||head->id!=t.repair_result_id||head->domain!="crafting")fail("RepairServiceTerminalSourceRequired",t.repair_result_id);
 if(head->kind=="repair_unstarted_interrupted"){
  const auto*original=take(read_repair_unstarted_interruption(x,{{head->domain,head->kind,head->id},t.source_schema}));if(!same_source_ref(original->accepted_service_ref.source_ref,s.repair_activity_ref)||original->accepted_service_ref.source_schema!=1)fail("RepairServiceTerminalScopeMismatch",t.repair_result_id);return {true,{a,nullptr,original,nullptr},{}};
 }
 if(head->kind=="repair_interrupted"){
  const auto*original=take(read_repair_interruption_source(x,{{head->domain,head->kind,head->id},t.source_schema}));const auto*job=x.items.repair_jobs.jobs.find(original->job_id);if(!job||!same_source_ref(job->terms.accepted_service_ref.source_ref,s.repair_activity_ref)||job->terms.accepted_service_ref.source_schema!=1)fail("RepairServiceTerminalScopeMismatch",t.repair_result_id);return {true,{a,job,nullptr,original},{}};
 }
 fail("UnsupportedRepairInternalTerminal",t.repair_result_id,ErrorKind::unsupported_capability);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
