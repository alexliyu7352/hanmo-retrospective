#include "naval_repair_native_original.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "naval_repair_exact_detail.hpp"
namespace hanmo::resident {using namespace naval_repair_exact_detail;namespace {
const ExactNavalRepairHistoryContext& history(const NativeExactNavalRepairOriginalContext& x){return x.participation.history;}
void binding(const NativeExactNavalRepairOriginalContext& x){
 const auto& h=history(x);const auto& p=x.participation;
 if(!x.occupations.bound_to(h.common,h.military,h.naval,h.military_exact,h.actions)||
  &p.bodies.common!=&h.common||&p.spatial.common!=&h.common||
  !p.social.bound_to(h.common)||p.bodies.military!=&h.military||
  p.bodies.military_exact!=&h.military_exact||p.bodies.elapsed!=&h.elapsed||
  x.naval_history.population!=h.population||x.naval_history.population_catalog!=h.population_catalog||
  (h.navigation_geometry&&h.navigation_geometry!=&x.naval_history))
  fail("CrossCandidateExactNavalRepairSources");
 if(!same_moment(p.bodies.actual_now,take(h.clock.read_current_moment()),h.budget))
  fail("ExactNavalRepairFreshBodyReadRequired");
}
void source(const NativeExactNavalRepairOriginalContext& x,const NewNavalRepairTimeSource2& f){
 binding(x);const auto& h=history(x);
 if(h.actions.naval_repair_time_sources.find(f.timing.action_ref)!=&f)
  fail("CrossCandidateExactNavalRepairSource",f.timing.action_ref);
 validate_source_shape(h.common,h.naval,h.actions,
  {h.military_catalog,h.access,h.naval_catalog,h.scale,h.clock,h.budget},f);
}
std::size_t actual_position(const CommonStore& c,const SourceRef& ref){
 return take(read_actual_common_fact_position(c,ref));
}
std::optional<ExactNavalRepairInterruption> first_cause(const NativeExactNavalRepairOriginalContext& x,const NewNavalRepairTimeSource2& f){
 source(x,f);const auto& h=history(x);const auto now=take(h.clock.read_current_moment());
 const auto begin=actual_position(h.common,f.timing.start_fact_ref);
 const auto end=f.timing.result_ref?actual_position(h.common,*f.timing.result_ref):h.common.state().facts.order().size();
 std::optional<ExactNavalRepairInterruption> first;std::optional<std::size_t> cut;
 auto consider=[&](std::optional<ExactNavalRepairInterruption> next){
  if(!next)return;
  if(next->actual_cause.source_schema<1)fail("ExactNavalRepairActualCauseSchemaRequired",next->actual_cause.source_ref.id);
  const auto at=actual_position(h.common,next->actual_cause.source_ref);
  if(at<=begin||at>=end||take(compare_world_moments(next->occurred_at,f.timing.started_at,h.budget))<0||
   take(compare_world_moments(next->occurred_at,now,h.budget))>0||
   (f.timing.completion_or_interruption_at&&take(compare_world_moments(next->occurred_at,*f.timing.completion_or_interruption_at,h.budget))>0))
   fail("ExactNavalRepairActualCauseOrderMismatch",next->actual_cause.source_ref.id);
  if(first){const auto time=take(compare_world_moments(next->occurred_at,first->occurred_at,h.budget));
   if((at<*cut&&time>0)||(at>*cut&&time<0)||(at==*cut&&(time||next->actual_cause.source_schema!=first->actual_cause.source_schema)))
    fail("ExactNavalRepairActualCauseOrderMismatch",next->actual_cause.source_ref.id);
  }
  // Original step checked crew before service. If one genuine source changes
  // both, retain that same original order; distinct sources use Common order.
  if(!cut||at<*cut){cut=at;first=std::move(next);}
 };
 consider(take(read_first_exact_naval_repair_crew_change(h,f)));
 consider(take(read_first_exact_naval_repair_service_change(h,f)));
 consider(take(read_first_exact_naval_repair_legacy_contact(h,x.naval_history,f)));
 auto other=take(x.occupations.read_first_nonlegacy_repair_contact(f));
 if(other&&(other->kind!=ExactNavalRepairInterruptionKind::contact||
  (other->actual_cause.source_ref.domain=="military_naval"&&other->actual_cause.source_ref.kind=="naval_contact")))
  fail("ExactNavalRepairContactOwnerMismatch",other->actual_cause.source_ref.id);
 consider(std::move(other));return first;
}
}
bool NativeExactNavalRepairOriginalRead::bound_to(const CommonStore& c,const MilitaryState& m,const NavalState& n,const MilitaryActionTimeState& s)const noexcept{
 const auto& h=history(context_);return &h.common==&c&&&h.military==&m&&&h.naval==&n&&&h.actions==&s&&context_.occupations.bound_to(c,m,n,h.military_exact,s);
}
Outcome<NavalRepairQuote> NativeExactNavalRepairOriginalRead::read_naval_repair_current_offer(const StableId& actor,const NavalRepairStart& request)const{return guard<NavalRepairQuote>([&]{
 binding(context_);auto quote=take(read_exact_naval_repair_current_offer(history(context_),actor,request));
 take(context_.occupations.validate_repair_current_occupation(quote.terms,{}));return quote;
});}
Status NativeExactNavalRepairOriginalRead::validate_naval_repair_acceptance(const NewNavalRepairTimeSource2& f)const{return status([&]{
 source(context_,f);const auto& h=history(context_);
 take(validate_exact_naval_repair_asset_crew_acceptance(h,f));
 take(validate_exact_naval_repair_original_charge(h,f));
 take(validate_exact_naval_repair_people_at_acceptance(context_.participation,f));
 take(validate_exact_naval_repair_legacy_battle_acceptance(h,context_.naval_history,f));
 take(context_.occupations.validate_repair_acceptance_occupation(f));
});}
Status NativeExactNavalRepairOriginalRead::validate_naval_repair_actual_service(const NewNavalRepairTimeSource2& f,const MomentSpan& span)const{return status([&]{
 source(context_,f);const auto& h=history(context_);
 take(validate_exact_naval_repair_crew_span(h,f,span));
 take(validate_exact_naval_repair_people_service_span(context_.participation,f,span));
 take(validate_exact_naval_repair_legacy_battle_span(h,context_.naval_history,f,span));
 take(context_.occupations.validate_repair_service_occupation(f,span));
 const auto first=first_cause(context_,f);
 if(first&&take(compare_world_moments(first->occurred_at,span.to,h.budget))<0)
  fail("ExactNavalRepairUnconsumedActualCause",first->actual_cause.source_ref.id);
});}
Status NativeExactNavalRepairOriginalRead::validate_naval_repair_current_completion(const NewNavalRepairTimeSource2& f)const{return status([&]{
 source(context_,f);const auto& h=history(context_);const auto& t=f.original_terms;
 if(f.original_result||!same_moment(take(h.clock.read_current_moment()),due(f,h.budget),h.budget))
  fail("ExactNavalRepairCompletionGateRequired",f.timing.action_ref);
 if(first_cause(context_,f))fail("ExactNavalRepairUnconsumedActualCause",f.timing.action_ref);
 take(validate_exact_naval_repair_current_terms(h.common,h.military,h.military_catalog,h.access,h.naval,h.naval_catalog,h.clock,h.formations,f.timing.action_ref,
  {f.request_key,t.fleet_id,t.vessel_id,t.rule_id,t.inventory_id},t));
 take(validate_exact_naval_repair_people_at_current_completion(context_.participation,f));
 take(context_.occupations.validate_repair_current_occupation(t,f.timing.action_ref));
});}
Outcome<std::optional<ExactNavalRepairInterruption>> NativeExactNavalRepairOriginalRead::read_naval_repair_actual_interruption(const NewNavalRepairTimeSource2& f)const{return guard<std::optional<ExactNavalRepairInterruption>>([&]{
 source(context_,f);if(f.original_result)fail("ExactNavalRepairActiveInterruptionRequired",f.timing.action_ref);
 auto first=first_cause(context_,f);const auto& h=history(context_);
 if(first&&!same_moment(first->occurred_at,take(h.clock.read_current_moment()),h.budget))
  fail("ExactNavalRepairUnconsumedActualCause",first->actual_cause.source_ref.id);
 return first;
});}
Status NativeExactNavalRepairOriginalRead::validate_naval_repair_completed_result(const NewNavalRepairTimeSource2& f)const{return status([&]{
 source(context_,f);if(!f.original_result||!f.timing.result_ref||!f.timing.completion_or_interruption_at)
  fail("ExactNavalRepairResultRequired",f.timing.action_ref);
 const auto& h=history(context_);const auto& result=*f.original_result;
 take(validate_exact_naval_repair_result_asset_predecessor(h,f));
 take(validate_exact_naval_repair_original_charge(h,f));
 take(validate_exact_naval_repair_crew_result(h,f));
 take(validate_exact_naval_repair_legacy_battle_result(h,context_.naval_history,f));
 const auto first=first_cause(context_,f);
 if(result.success||result.reason=="NavalRepairCancelled"){
  if(first)fail("ExactNavalRepairUnconsumedActualCause",first->actual_cause.source_ref.id);
 }else if(!first||result.reason!=stop_reason(first->kind)||!result.actual_interruption_source||
  !same_source(result.actual_interruption_source->source_ref,first->actual_cause.source_ref)||
  result.actual_interruption_source->source_schema!=first->actual_cause.source_schema||
  !same_moment(first->occurred_at,*f.timing.completion_or_interruption_at,h.budget))
  fail("ExactNavalRepairActualInterruptionMismatch",result.id);
 if(result.success)take(validate_exact_naval_repair_people_at_success(context_.participation,f));
 take(context_.occupations.validate_repair_result_occupation(f));
});}
}
