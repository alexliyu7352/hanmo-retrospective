#include "treaty_aid_schedule.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
void actual_terms(const ActivatedTreatyAidTermsRead& r){
 if((r.activation_source_schema!=1&&r.activation_source_schema!=2)||
    r.termination_source_schema!=r.activation_source_schema||r.schedule_source_schema!=r.activation_source_schema)
  fail("TreatyAidSelectedSourceSchemaMismatch",r.treaty_id);
 if(r.treaty_id.empty()||r.terms_revision<1||r.term_id.empty()||!r.original_terms||!r.original_term||
    !valid_source_ref(r.original_terms_source)||!valid_source_ref(r.activation_source)||
    bool(r.termination_source)!=bool(r.terminated_at))fail("TreatyAidActivationSourceRequired",r.treaty_id);
 take(validate_treaty_terms_shape(*r.original_terms));take(encode_world_moment(r.activated_at));
 const TreatyMaterialAidTerm* original=nullptr;
 for(const auto& term:r.original_terms->terms)if(term.term_id==r.term_id)original=std::get_if<TreatyMaterialAidTerm>(&term.terms);
 if(original!=r.original_term)fail("TreatyAidOriginalTermMismatch",r.term_id);
 if(!take(treaty_window_contains(r.original_terms->effective_window,r.activated_at)))fail("TreatyAidActivationOutsideWindow",r.treaty_id);
 // A Calendar due equal to floor(fractional activation) is already in the past.
 // Do not invent debt for elapsed time or silently move that original due.
 if(compare(WorldMoment{original->first_due_tick,{}},r.activated_at)<0)
  fail("TreatyAidFirstDueBeforeActivation",r.term_id);
 if(r.termination_source){if(!valid_source_ref(*r.termination_source))fail("TreatyAidTerminationSourceRequired",r.treaty_id);take(encode_world_moment(*r.terminated_at));if(compare(*r.terminated_at,r.activated_at)<0)fail("TreatyAidTerminationBeforeActivation",r.treaty_id);}
}
}
Status validate_treaty_aid_activation_terms(const ActivatedTreatyAidTermsRead& r){try{actual_terms(r);return common_success();}catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyAidDueRead> read_treaty_aid_due(const TreatyAidSchedule& schedule,const TreatyAidTermsOriginalRead& original,const OrganizationAuthorityClock& clock){try{
 const auto now=take(clock.read_organization_authority_now());take(encode_world_moment(now));auto r=take(original.read_activated_material_aid_terms(schedule.treaty_id,schedule.terms_revision,schedule.term_id));actual_terms(r);
 if(schedule.id.empty()||schedule.treaty_id!=r.treaty_id||schedule.terms_revision!=r.terms_revision||schedule.term_id!=r.term_id||
    !same_source_ref(schedule.activation_ref,r.activation_source)||!same_source_ref(schedule.schedule_terms_ref,r.original_terms_source)||
    schedule.next_occurrence_ordinal<0||schedule.next_occurrence_ordinal>r.original_term->occurrences)
  fail("TreatyAidScheduleIdentityMismatch",schedule.id);
 if(compare(r.activated_at,now)>0)fail("TreatyAidActivationInFuture",schedule.id);
 if(schedule.termination_ref&&(!r.termination_source||!same_source_ref(*schedule.termination_ref,*r.termination_source)))fail("TreatyAidScheduleTerminationMismatch",schedule.id);
 if(r.terminated_at&&compare(*r.terminated_at,now)>0)fail("TreatyAidTerminationInFuture",schedule.id);
 TreatyAidDueRead out;out.ordinal=schedule.next_occurrence_ordinal;out.activation_ref=r.activation_source;out.termination_ref=r.termination_source;
 if(out.ordinal==r.original_term->occurrences){out.kind=TreatyAidDueKind::all_established;return {true,std::move(out),{}};}
 const Integer due=take(material_aid_occurrence_due(*r.original_term,out.ordinal));out.due_tick=due;
 if(due>std::numeric_limits<Integer>::max()-r.original_term->delivery_deadline_ticks)fail("TreatyAidDeadlineOverflow",schedule.id);
 out.delivery_deadline_tick=due+r.original_term->delivery_deadline_ticks;
 // Genuine earlier termination closes only still-future occurrences. A missed
 // old due remains a damaged/incomplete history; it is never silently skipped.
 WorldMoment end{r.original_terms->effective_window.until_tick,{}};
 if(r.terminated_at&&compare(*r.terminated_at,end)<0)end=*r.terminated_at;
 const WorldMoment at_due{due,{}};
 if(compare(at_due,end)>=0){out.kind=compare(now,end)>=0?TreatyAidDueKind::term_ended:TreatyAidDueKind::future;return {true,std::move(out),{}};}
 const int order=compare(now,at_due);
 if(order<0)out.kind=TreatyAidDueKind::future;
 else if(order==0)out.kind=TreatyAidDueKind::due;
 else fail("TreatyAidUnestablishedPastDue",schedule.id);
 return {true,std::move(out),{}};
}catch(const Failure& f){return {false,{},f.error};}}
}
