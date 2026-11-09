#include "repair_acceptance.hpp"
#include "repair_codec.hpp"
#include "repair_detail.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
namespace hanmo::resident {using namespace repair_detail;
SourceRef repair_accepted_source(const StableId&id){return {"crafting","repair_accepted",id};}
SourceRef repair_unstarted_interrupted_source(const StableId&id){return {"crafting","repair_unstarted_interrupted",id};}
namespace {
bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
void accepted_shape(const RepairReadContext&x,const RepairAcceptedTerms&t,const WorldMoment&at,const RepairAcceptedSource*actual){
 const auto*p=take(read_repair_policy(x.policies,t.policy_ref));take(compare_world_moments(at,at));
 if(t.instance_id.empty()||t.worker_id.empty()||t.original_item.inventory_id.empty()||t.original_item.lot_id.empty()||t.original_item.quantity!=1||!x.common.valid_resource_owner(t.legal_owner)||t.condition_head.instance_id!=t.instance_id||t.terms_revision<1||t.workstation.slot_kind!=p->workstation_slot_kind||compare(at,now(x))>0)fail("RepairAcceptedTermsShapeMismatch",t.instance_id);
 for(const auto*r:{&t.actual_contact_ref,&t.owner_authority_ref,&t.fee_terms_ref,&t.return_terms_ref})if(r->source_schema<1||!valid_source_ref(r->source_ref))fail("RepairAcceptedOriginalSourceRequired",t.instance_id);
 if(t.delivered_custody_source_ref&&(t.delivered_custody_source_ref->source_schema<1||!valid_source_ref(t.delivered_custody_source_ref->source_ref)))fail("RepairAcceptedCustodySourceRequired",t.instance_id);
 const auto*i=x.items.instances.find(t.instance_id);if(!i||i->repair_policy_id!=p->policy_id||std::find(p->applicable_definitions.begin(),p->applicable_definitions.end(),i->item_definition_id)==p->applicable_definitions.end())fail("RepairAcceptedItemDefinitionMismatch",t.instance_id);
 if(actual)take(read_item_condition_before_common_source(x.conditions,t.condition_head,repair_accepted_source(actual->id),at));else take(read_item_condition_at_head(x.conditions,t.condition_head));
 if(!x.commercial_sources)fail("RepairOriginalCommercialReaderRequired",t.instance_id,ErrorKind::unsupported_capability);if(&x.commercial_sources->common!=&x.common||&x.commercial_sources->items!=&x.items||&x.commercial_sources->policies!=&x.policies||&x.commercial_sources->clock!=&x.clock)fail("RepairCommercialContextMismatch",t.instance_id);take(validate_repair_commercial_accepted_terms(*x.commercial_sources,t,at,actual));
 take(x.original.validate_repair_acceptance(t,at,actual));
}
const RepairJob* started(const RepairReadContext&x,const MomentOwnerSource&r){const RepairJob*out=nullptr;for(const auto&row:x.items.repair_jobs.jobs.rows())if(source(row.second.terms.accepted_service_ref,r)){if(out)fail("DuplicateRepairAcceptedStart",r.source_ref.id);out=&row.second;}return out;}
const RepairUnstartedInterruptionSource* stopped(const RepairReadContext&x,const MomentOwnerSource&r){const RepairUnstartedInterruptionSource*out=nullptr;for(const auto&row:x.items.repair_jobs.unstarted_interruptions.rows())if(source(row.second.accepted_service_ref,r)){if(out)fail("DuplicateRepairUnstartedTerminal",r.source_ref.id);if(row.first!=row.second.id)fail("RepairUnstartedTerminalIdentityMismatch",row.first);out=&row.second;}return out;}
void stop_source(const RepairReadContext&x,const RepairAcceptedSource&a,const RepairUnstartedInterruptionSource&s){
 if(s.id.empty()||!source(s.accepted_service_ref,{repair_accepted_source(a.id),1})||s.actual_cause.source_schema<1||!valid_source_ref(s.actual_cause.source_ref)||compare(s.occurred_at,a.accepted_at)<0||compare(s.occurred_at,now(x))>0||header(x.common,repair_unstarted_interrupted_source(s.id))<=header(x.common,repair_accepted_source(a.id)))fail("RepairUnstartedTerminalSourceMismatch",s.id);
 take(x.original.validate_repair_unstarted_interruption(a,s));
}
void unbound_candidate(CommonStore&c,ItemInstancesState&i,const RepairReadContext&x){context(x);if(&c!=&x.common||&i!=&x.items)fail("RepairAcceptanceCandidateMismatch");}
}
Outcome<RepairAcceptedServiceView> read_repair_accepted_service(const RepairReadContext&x,const MomentOwnerSource&r){try{
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_accepted_source(r.source_ref.id)))fail("UnsupportedRepairAcceptanceSource",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*a=x.items.repair_jobs.acceptances.find(r.source_ref.id);if(!a||a->id!=r.source_ref.id)fail("RepairAcceptedSourceRequired",r.source_ref.id);header(x.common,r.source_ref);accepted_shape(x,a->terms,a->accepted_at,a);
 for(const auto&row:x.items.repair_jobs.acceptances.rows())if(row.first!=a->id){
  const auto&prior=row.second;if(source(prior.terms.fee_terms_ref,a->terms.fee_terms_ref))fail("DuplicateRepairCanonicalFeeScope",a->id);
  if(prior.terms.instance_id!=a->terms.instance_id||header(x.common,repair_accepted_source(prior.id))>=header(x.common,r.source_ref))continue;
  const MomentOwnerSource earlier{repair_accepted_source(prior.id),1};const auto*old_job=started(x,earlier);const auto*old_stop=stopped(x,earlier);
  if(old_job&&old_stop)fail("RepairStartedAfterUnstartedTerminal",prior.id);
  if(old_stop){stop_source(x,prior,*old_stop);if(header(x.common,repair_unstarted_interrupted_source(old_stop->id))>=header(x.common,r.source_ref))fail("OverlappingRepairAcceptedServices",a->id);take(x.original.validate_repair_reacceptance_after_unstarted_terminal(prior,*old_stop,a->terms,a->accepted_at,a));}
  else{if(!old_job||!old_job->returned_ref)fail("OverlappingRepairAcceptedServices",a->id);const auto*returned=x.common.state().facts.find(*old_job->returned_ref);if(!returned||header(x.common,{returned->domain,returned->kind,returned->id})>=header(x.common,r.source_ref))fail("RepairOriginalReturnBeforeAcceptanceRequired",a->id);take(x.original.validate_repair_return(*old_job));}
 }
 const auto*j=started(x,r);const auto*stop=stopped(x,r);if(j&&stop)fail("RepairStartedAfterUnstartedTerminal",a->id);
 if(j&&(j->id.empty()||x.items.repair_jobs.jobs.find(j->id)!=j||header(x.common,repair_job_source(j->id))<=header(x.common,r.source_ref)||compare(j->started_at,a->accepted_at)<0))fail("RepairAcceptedStartSourceMismatch",a->id);
 if(stop)stop_source(x,*a,*stop);return {true,{a,j,stop},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<const RepairUnstartedInterruptionSource*> read_repair_unstarted_interruption(const RepairReadContext&x,const MomentOwnerSource&r){try{
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_unstarted_interrupted_source(r.source_ref.id)))fail("UnsupportedRepairUnstartedTerminalSource",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*stop=x.items.repair_jobs.unstarted_interruptions.find(r.source_ref.id);if(!stop||stop->id!=r.source_ref.id)fail("RepairUnstartedTerminalSourceRequired",r.source_ref.id);
 const auto accepted=take(read_repair_accepted_service(x,stop->accepted_service_ref));if(accepted.unstarted_interruption!=stop||accepted.started_job)fail("RepairUnstartedTerminalBindingMismatch",stop->id);return {true,stop,{}};
}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<WorldMoment> read_repair_acceptance_source_moment(const RepairReadContext&x,const SourceRef&r,Integer schema){try{
 if(r.domain=="crafting"&&r.kind=="repair_accepted")return {true,take(read_repair_accepted_service(x,{r,schema})).acceptance->accepted_at,{}};
 if(r.domain=="crafting"&&r.kind=="repair_unstarted_interrupted")return {true,take(read_repair_unstarted_interruption(x,{r,schema}))->occurred_at,{}};
 fail("UnsupportedRepairAcceptanceSource",r.id,ErrorKind::unsupported_capability);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<RepairAcceptanceQuote> preview_repair_acceptance(const RepairReadContext&x,const RepairAcceptedTerms&t){try{
 context(x);take(validate_repair_policy_catalog(x.policies,x.common_catalog));const auto at=now(x);accepted_shape(x,t,at,nullptr);
 const auto current_head=take(read_current_item_condition_head(x.conditions,t.instance_id));
 if(!same_head(t.condition_head,current_head.value()))fail("StaleRepairInspection",t.instance_id,ErrorKind::rule);
 const auto&item=lot(x.common,t.original_item);if(item.item_instance_id!=std::optional<StableId>{t.instance_id}||item.quantity!=1||!owner(item.owner,t.legal_owner))fail("RepairInspectionActualItemRequired",t.instance_id,ErrorKind::rule);
 for(const auto&row:x.items.repair_jobs.acceptances.rows()){
  const auto&r=row.second;if(source(r.terms.fee_terms_ref,t.fee_terms_ref))fail("RepairServiceAlreadyAccepted",r.id,ErrorKind::rule);
  if(r.terms.instance_id!=t.instance_id)continue;auto existing=take(read_repair_accepted_service(x,{repair_accepted_source(r.id),1}));if(existing.unstarted_interruption){take(x.original.validate_repair_reacceptance_after_unstarted_terminal(r,*existing.unstarted_interruption,t,at,nullptr));continue;}if(!existing.started_job||!existing.started_job->returned_ref)fail("RepairItemAlreadyInAcceptedService",t.instance_id,ErrorKind::rule);take(x.original.validate_repair_return(*existing.started_job));
 }
 return {true,{t,x.common.state().identity,at,x.common.read_version({ResourceKey::Kind::inventory,t.original_item.inventory_id})},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<SourceRef> accept_repair_service_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairReadContext&x,const RepairAcceptanceQuote&q){try{
 unbound_candidate(c,i,x);
 for(const auto&row:i.repair_jobs.acceptances.rows())if(source(row.second.terms.fee_terms_ref,q.terms.fee_terms_ref)){if(!same_repair_accepted_terms(row.second.terms,q.terms))fail("ConflictingRepairAcceptanceReplay",row.first);take(read_repair_accepted_service(x,{repair_accepted_source(row.first),1}));return {true,repair_accepted_source(row.first),{}};}
 const auto&identity=c.state().identity;if(identity.world_id!=q.identity.world_id||identity.execution_epoch!=q.identity.execution_epoch||identity.revision!=q.identity.revision||identity.tick!=q.identity.tick||compare(now(x),q.quoted_at)||q.actual_inventory_version.resource.kind!=ResourceKey::Kind::inventory||q.actual_inventory_version.resource.id!=q.terms.original_item.inventory_id||c.read_version(q.actual_inventory_version.resource).version!=q.actual_inventory_version.version)fail("StaleRepairAcceptanceQuote",q.terms.instance_id,ErrorKind::rule);
 take(preview_repair_acceptance(x,q.terms));RepairAcceptedSource accepted{take(c.next_id("repair_accepted")),q.terms,now(x)};const auto ref=repair_accepted_source(accepted.id);take(c.append_fact({accepted.id,"repair_accepted","crafting"}));if(!i.repair_jobs.acceptances.insert(ref.id,std::move(accepted)))fail("DuplicateRepairAcceptance",ref.id);take(read_repair_accepted_service(x,{ref,1}));return {true,ref,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_repair_start_acceptance(const RepairReadContext&x,const RepairTerms&t,const RepairJob*j){try{
 const auto accepted=take(read_repair_accepted_service(x,t.accepted_service_ref));const auto&a=*accepted.acceptance;const auto&v=a.terms;
 if(accepted.unstarted_interruption||(accepted.started_job&&accepted.started_job!=j)||v.policy_ref.policy_id!=t.policy_ref.policy_id||v.policy_ref.version!=t.policy_ref.version||v.instance_id!=t.instance_id||v.worker_id!=t.worker_id||!owner(v.legal_owner,t.legal_owner)||!same_head(v.condition_head,t.condition_head)||v.workstation.provider_domain!=t.workstation.provider_domain||v.workstation.facility_id!=t.workstation.facility_id||v.workstation.slot_kind!=t.workstation.slot_kind||!source(v.fee_terms_ref,t.fee_terms_ref)||!source(v.return_terms_ref,t.return_terms_ref)||v.terms_revision!=t.terms_revision||compare(a.accepted_at,t.accepted_at))fail("RepairStartAcceptedScopeMismatch",t.instance_id);
 // The original delivered custody reader proves any actual lot movement since
 // inspection and continuation of the accepted optional early custody source.
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<SourceRef> interrupt_unstarted_repair_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairReadContext&x,const MomentOwnerSource&r,const MomentOwnerSource&cause){try{
 unbound_candidate(c,i,x);auto accepted=take(read_repair_accepted_service(x,r));if(accepted.unstarted_interruption){if(!source(accepted.unstarted_interruption->actual_cause,cause))fail("ConflictingRepairUnstartedInterruptionReplay",r.source_ref.id);return {true,repair_unstarted_interrupted_source(accepted.unstarted_interruption->id),{}};}if(accepted.started_job)fail("RepairActualExecutionStopRequired",r.source_ref.id);
 if(cause.source_schema<1||!valid_source_ref(cause.source_ref))fail("RepairActualUnavailabilityRequired",r.source_ref.id);
 RepairUnstartedInterruptionSource stop{take(c.next_id("repair_unstarted_interrupted")),r,now(x),cause};const auto ref=repair_unstarted_interrupted_source(stop.id);take(c.append_fact({stop.id,"repair_unstarted_interrupted","crafting"}));if(!i.repair_jobs.unstarted_interruptions.insert(ref.id,std::move(stop)))fail("DuplicateRepairUnstartedTerminal",ref.id);take(read_repair_accepted_service(x,r));return {true,ref,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_repair_acceptances(const RepairReadContext&x){try{
 context(x);for(const auto&row:x.items.repair_jobs.acceptances.rows()){if(row.first!=row.second.id)fail("RepairAcceptanceIdentityMismatch",row.first);take(read_repair_accepted_service(x,{repair_accepted_source(row.first),1}));}
 for(const auto&row:x.items.repair_jobs.unstarted_interruptions.rows()){const auto v=take(read_repair_accepted_service(x,row.second.accepted_service_ref));if(v.unstarted_interruption!=&row.second)fail("OrphanRepairUnstartedTerminal",row.first);}
 for(const auto&row:x.common.state().facts.rows())if(row.second.domain=="crafting"&&((row.second.kind=="repair_accepted"&&!x.items.repair_jobs.acceptances.find(row.first))||(row.second.kind=="repair_unstarted_interrupted"&&!x.items.repair_jobs.unstarted_interruptions.find(row.first))))fail("RepairAcceptanceOriginalCoverageMismatch",row.first);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_repair_acceptance_pins(const RepairReadContext&x,const MomentOwnerSource&r,RepairAcceptancePinSink&sink){try{
 const auto v=take(read_repair_accepted_service(x,r));const auto&t=v.acceptance->terms;for(const auto*s:{&t.actual_contact_ref,&t.owner_authority_ref,&t.fee_terms_ref,&t.return_terms_ref})take(sink.source(*s));if(t.delivered_custody_source_ref)take(sink.source(*t.delivered_custody_source_ref));take(sink.item_condition(t.condition_head));
 const auto cut=header(x.common,r.source_ref);for(const auto&row:x.items.repair_jobs.acceptances.rows())if(row.second.terms.instance_id==t.instance_id&&header(x.common,repair_accepted_source(row.first))<cut){
  const MomentOwnerSource prior{repair_accepted_source(row.first),1};take(sink.source(prior));
  if(const auto*stop=stopped(x,prior))take(sink.source({repair_unstarted_interrupted_source(stop->id),1}));
  else if(const auto*job=started(x,prior)){take(sink.source({repair_job_source(job->id),1}));}
 }return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_repair_unstarted_interruption_pins(const RepairReadContext&x,const MomentOwnerSource&r,RepairAcceptancePinSink&sink){try{
 const auto*stop=take(read_repair_unstarted_interruption(x,r));take(sink.source(stop->accepted_service_ref));take(sink.source(stop->actual_cause));return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}

}
