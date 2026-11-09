#include "organization_population_training_source.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{std::move(x.error)};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{std::move(x.error)};}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore&c,const SourceRef& source){
 const auto* h=c.state().facts.find(source.id);const auto& order=c.state().facts.order();
 const auto i=std::find(order.begin(),order.end(),source.id);
 if(!h||h->id!=source.id||h->domain!=source.domain||h->kind!=source.kind||i==order.end())fail("OrganizationTrainingSourceHeaderMismatch",source.id);
 return static_cast<std::size_t>(i-order.begin());
}
void moment(const OrganizationTrainingSourceContext&x,const OrganizationFact& f,const WorldMoment& now){
 if(!f.occurred_at||f.tick<0||f.occurred_at->half_day_tick!=f.tick)fail("OrganizationTrainingExactSourceRequired",f.id);
 take(encode_world_moment(*f.occurred_at));if(compare(*f.occurred_at,now)>0)fail("OrganizationTrainingSourceFromFuture",f.id);
}
OrganizationPopulationTrainingChange read(const OrganizationTrainingSourceContext&x,const MomentOwnerSource& source){
 const auto& ref=source.source_ref;
 if(!x.catalog.enabled||!x.catalog.authority_lifecycle_enabled||!x.catalog.exact_fact_clock_enabled||source.source_schema!=1||ref.domain!="organization"||
    (ref.kind!="population_training_approved"&&ref.kind!="population_training_completed"))fail("OrganizationTrainingSourceKindRequired",ref.id);
 const auto cut=position(x.common,ref);const auto* event=x.organization.facts.find(ref.id);
 if(!event||event->id!=ref.id||event->kind!=ref.kind||!event->code.empty()||!event->cause_fact_ids.empty()||event->office_change_payload||event->grant_change_payload)
  fail("OrganizationTrainingSourcePayloadMismatch",ref.id);
 const auto now=take(read_organization_authority_now(x.common,&x.clock));moment(x,*event,now);
 const auto* activity=x.organization.activities.find(event->activity_id);
 const auto* terms=activity?x.catalog.activity_terms.find(activity->terms_id):nullptr;
 if(!activity||activity->id!=event->activity_id||!terms||terms->id!=activity->terms_id||activity->kind!=OrgActivityKind::population_training||terms->kind!=OrgActivityKind::population_training||
    activity->principal.kind!=SubjectKind::organization||activity->principal.id!=event->organization_id||activity->responsible_office_id.empty()||
    activity->participation.size()!=1||activity->executors.size()!=1||activity->execution.activity_id!=activity->id||activity->execution.mode!=ExecutionMode::background||
    activity->execution.local_session_id||terms->duration_ticks<=0||terms->training_capacity<=0)
  fail("OrganizationTrainingActivitySourceMismatch",ref.id);
 const auto& participant=activity->participation.front();const auto& executor=activity->executors.front();
 if(participant.subject.kind!=SubjectKind::cohort||participant.subject.id.empty()||participant.role!="trainee"||participant.binding_kind!=ParticipationKind::cohort_allocation||participant.binding_id.empty()||
    executor.kind!=SubjectKind::cohort||executor.id!=participant.subject.id)
  fail("OrganizationTrainingParticipantSourceMismatch",activity->id);
 const OrganizationFact* approval=nullptr;const OrganizationFact* terminal=nullptr;
 for(const auto& row:x.organization.facts.rows()){
  const auto& f=row.second;if(f.activity_id!=activity->id)continue;
  if(f.kind=="population_training_approved"){
   if(approval)fail("DuplicateOrganizationTrainingApproval",activity->id);approval=&f;
  }else if(f.kind=="population_training_completed"){
   if(terminal)fail("DuplicateOrganizationTrainingCompletion",activity->id);terminal=&f;
  }else if(f.kind=="organization_activity_approved"||f.kind=="organization_activity_completed"||f.kind=="organization_activity_failed"||f.kind=="organization_activity_cancelled")
   fail("OrganizationTrainingUnexpectedOutcome",f.id);
 }
 if(!approval||approval->id.empty()||approval->organization_id!=activity->principal.id||approval->office_id!=activity->responsible_office_id||approval->actor_id.empty()||
    approval->amount<=0||approval->amount>terms->training_capacity||!approval->code.empty()||!approval->cause_fact_ids.empty()||approval->office_change_payload||approval->grant_change_payload)
  fail("OrganizationTrainingApprovalSourceMismatch",activity->id);
 moment(x,*approval,now);const SourceRef approval_ref{"organization","population_training_approved",approval->id};
 const auto approved_pos=position(x.common,approval_ref);
 const PopulationTransfer* transfer=nullptr;
 for(const auto& row:x.economy.population_transfers.rows())if(row.second.source_fact_id==approval->id){
  if(transfer||row.first!=row.second.id)fail("OrganizationTrainingTransferAmbiguous",approval->id);transfer=&row.second;
 }
 if(!transfer||transfer->reason!="voluntary_training"||transfer->to_cohort_id!=participant.subject.id||transfer->from_cohort_id==transfer->to_cohort_id||transfer->count!=approval->amount||
    transfer->labor_eligible_count<=0||transfer->labor_eligible_count>transfer->count||transfer->effective_tick!=approval->tick||
    std::count(terms->consenting_cohort_ids.begin(),terms->consenting_cohort_ids.end(),transfer->from_cohort_id)!=1)
  fail("OrganizationTrainingOriginalTransferMismatch",approval->id);
 const SourceRef transfer_ref{"local_economy","population_transfer",transfer->id};
 const auto transferred_pos=position(x.common,transfer_ref);
 if(transferred_pos<=approved_pos)fail("OrganizationTrainingTransferBeforeApproval",transfer->id);
 NativeOrganizationOriginalAuthorityRead authority(x.common,x.organization,x.clock);
 const auto* basis=take(read_registered_authority_basis(x.common,x.runtime.authority_bases,x.organization,x.catalog,x.catalog.authority_catalog_identity,{approval_ref,1},authority));
 if(basis->actor_id!=approval->actor_id||basis->organization_id!=approval->organization_id||basis->office_id!=approval->office_id||basis->command_kind!="recruit_population"||
    basis->scope.kind!=ScopeRef::Kind::site||basis->scope.id!=terms->training_site_id||basis->amount!=approval->amount||basis->budget_ref||!basis->amount_components.empty())
  fail("OrganizationTrainingApprovalAuthorityMismatch",approval->id);
 const auto admitted=take(x.calendar.read_original_calendar_window(approval_ref,terms->duration_ticks));const auto& window=admitted.approved_window;
 if(activity->started_tick!=window.from_tick||activity->phase_started_tick!=window.from_tick||activity->due_tick!=window.until_tick||window.until_tick<=window.from_tick)
  fail("OrganizationTrainingOriginalWindowMismatch",activity->id);
 OrganizationPopulationTrainingChange out;out.actual_source=source;out.occurred_at=*event->occurred_at;out.activity_id=activity->id;
 out.cohort_id=participant.subject.id;out.allocation_id=participant.binding_id;out.original_approval=approval_ref;out.original_transfer=transfer_ref;
 if(ref.kind=="population_training_approved"){
  if(event!=approval||cut!=approved_pos)fail("OrganizationTrainingApprovalSourceMismatch",ref.id);
  out.after_assignment=activity->id;
 }else{
  if(event!=terminal||cut<=transferred_pos||activity->status!=OrgActivityStatus::completed||activity->phase!="completed"||activity->execution.phase!=activity->phase||
     activity->outcome_sequence!=1||activity->result_fact_ids!=std::vector<StableId>{event->id}||activity->execution.last_receipt_id!=std::optional<StableId>{event->id}||
     !activity->result_code.empty()||!event->actor_id.empty()||!event->office_id.empty()||event->amount!=transfer->count||
     compare(*event->occurred_at,WorldMoment{window.until_tick,{}})!=0||compare(*approval->occurred_at,*event->occurred_at)>0)
   fail("OrganizationTrainingCompletionSourceMismatch",ref.id);
  take(x.calendar.validate_completed_calendar_window(approval_ref,terms->duration_ticks));
  out.before_assignment=activity->id;out.after_status=PopulationStatus::trained_reserve;
 }
 return out;
}
}
Outcome<OrganizationPopulationTrainingChange> read_organization_population_training_change(const OrganizationTrainingSourceContext&x,const MomentOwnerSource&s){
 try{return {true,read(x,s),{}};}catch(const Failure&e){return {false,{},e.error};}
}
}
