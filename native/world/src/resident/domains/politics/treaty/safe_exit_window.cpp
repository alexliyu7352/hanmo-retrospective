#include "safe_exit_window.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* c,const StableId& id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{std::move(x.error)};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{std::move(x.error)};}
int compare(const SafeExitEntryContext& x,const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b,x.arithmetic));}
WorldMoment add(const SafeExitEntryContext& x,const WorldMoment& at,Integer duration){if(duration<=0)fail("SafeExitPositiveDurationRequired");auto n=take(normalize_fraction(std::to_string(duration),"1",x.arithmetic));return take(add_world_duration(at,HalfDayDuration{std::move(n)},x.arithmetic));}
std::size_t position(const CommonStore& c,const SourceRef& s){const auto* h=c.state().facts.find(s.id);const auto& order=c.state().facts.order();auto it=std::find(order.begin(),order.end(),s.id);if(!h||h->domain!=s.domain||h->kind!=s.kind||it==order.end())fail("SafeExitActualSourceMissing",s.id);return static_cast<std::size_t>(it-order.begin());}
SafeExitBindingState aggregate(const SafeExitBinding& b,const OrderedTable<SafeExitLine>& lines){bool active=false,all_outside=true,progress=false,expired=false;for(const auto& id:b.exit_line_refs){const auto* l=lines.find(id);if(!l||l->binding_id!=b.id)fail("SafeExitLineBindingMismatch",id);if(l->state==SafeExitLineState::superseded)continue;active=true;all_outside&=l->state==SafeExitLineState::outside;progress|=l->state==SafeExitLineState::outside||l->state==SafeExitLineState::exiting;expired|=l->state==SafeExitLineState::expired;}if(!active)fail("SafeExitOriginalTerminalDispositionRequired",b.id);if(all_outside)return SafeExitBindingState::outside;if(expired)return SafeExitBindingState::expired;return progress?SafeExitBindingState::exiting:SafeExitBindingState::inside;}
std::optional<SafeExitWindowView> window(const SafeExitEntryContext& x,const SourceRef& ref){
 auto origin=take(read_safe_exit_original_entry(x,ref,1));const auto& b=*origin.binding;
 const auto now=take(x.clock.read_organization_authority_now());const auto& term=*origin.term;
 auto base=take(read_safe_exit_original_termination_window(x.treaties,treaty_activation_source(*origin.activation.source),b.term_id,x.clock,x.arithmetic));
 if(!base){for(const auto& id:b.exit_line_refs){const auto* l=x.state.lines.find(id);if(!l->actual_crossing_refs.empty()||l->chosen_path_id||l->last_exit_boundary_ordinal)fail("SafeExitExitBeforeTermination",id);}return std::nullopt;}
 SafeExitWindowView out=*base;const auto& at=out.termination_at;
 // Inherited crossing prefixes can be repeated across successor lines. Their
 // immutable source Moment is read, never counted as another service or toll.
 for(const auto& id:b.exit_line_refs){const auto* l=x.state.lines.find(id);for(const auto& cross:l->actual_crossing_refs){
  if(cross.domain!="travel"||cross.kind!="boundary_crossing")fail("SafeExitExitCrossingRequired",cross.id);const auto* actual=x.carriers.crossings.find(cross.id);
  if(!actual||actual->receipt_id!=cross.id||position(x.common,ref)>=position(x.common,cross)||compare(x,actual->entry_at,at)<0||compare(x,actual->entry_at,now)>0)fail("SafeExitExitCrossingTimeMismatch",cross.id);
  take(validate_group_boundary_crossing_source(x.common,x.group_execution,x.carriers,x.movement,x.travel_catalog,*actual,x.arithmetic));
  if(!out.first_actual_exit_entry||compare(x,actual->entry_at,*out.first_actual_exit_entry)<0)out.first_actual_exit_entry=actual->entry_at;
 }}
 if(out.first_actual_exit_entry){auto service_end=add(x,*out.first_actual_exit_entry,term.safe_exit.max_exit_half_days);if(compare(x,service_end,out.until)<0)out.until=std::move(service_end);}
 for(const auto& id:b.exit_line_refs)for(const auto& cross:x.state.lines.find(id)->actual_crossing_refs){const auto* actual=x.carriers.crossings.find(cross.id);if(compare(x,actual->entry_at,out.until)>=0)fail("SafeExitCrossingOutsideOriginalWindow",cross.id);}
 return out;
}
}
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_original_termination_window(const TreatyTerminationContext& treaties,const SourceRef& activation_ref,const StableId& term_id,const OrganizationAuthorityClock& clock,ExactArithmeticBudget arithmetic){try{
 auto activation=take(read_original_treaty_activation(treaties.activations,activation_ref,1));const TreatyPassageTerm* term=nullptr;for(const auto& row:activation.revision.definition->terms.terms)if(row.term_id==term_id)term=std::get_if<TreatyPassageTerm>(&row.terms);if(!term)fail("SafeExitOriginalPassageTermRequired",term_id);
 WorldMoment at{std::min(term->window.until_tick,activation.revision.definition->terms.effective_window.until_tick),{}};SourceRef source=activation_ref;
 for(const auto& row:treaties.terminations.rows())if(same_source_ref(row.second.activation_ref,activation_ref)){auto actual=take(read_original_treaty_termination(treaties,treaty_termination_source(row.second),1));if(take(compare_world_moments(actual.source->terminated_at,at,arithmetic))<0){at=actual.source->terminated_at;source=treaty_termination_source(*actual.source);}}
 if(take(compare_world_moments(take(clock.read_organization_authority_now()),at,arithmetic))<0)return {true,std::nullopt,{}};
 if(term->safe_exit.grace_ticks<=0)fail("SafeExitPositiveDurationRequired",term_id);auto duration=take(normalize_fraction(std::to_string(term->safe_exit.grace_ticks),"1",arithmetic));auto until=take(add_world_duration(at,HalfDayDuration{std::move(duration)},arithmetic));return {true,SafeExitWindowView{source,at,at,std::move(until),{}},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_window_sources(const SafeExitEntryContext& x,const SourceRef& ref){try{return {true,window(x,ref),{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_window(const SafeExitEntryContext& x,const SourceRef& ref){try{auto original=take(read_safe_exit_original_entry(x,ref,1));take(x.originals.validate_safe_exit_retained_progress(*original.binding,x.state.lines));return {true,window(x,ref),{}};}catch(const Failure& f){return {false,{},f.error};}}
Status validate_safe_exit_window_projection(const SafeExitEntryContext& x,const SourceRef& ref){try{
 const auto* b=x.state.bindings.find(ref.id);if(!b)fail("SafeExitBindingSourceMissing",ref.id);take(x.originals.validate_safe_exit_retained_progress(*b,x.state.lines));auto expected=window(x,ref);
 if(!expected){if(b->termination_source||b->termination_at||b->exit_from||b->exit_until)fail("SafeExitPrematureTerminationProjection",b->id);}
 else if(!b->termination_source||!b->termination_at||!b->exit_from||!b->exit_until||!same_source_ref(*b->termination_source,expected->termination_source)||compare(x,*b->termination_at,expected->termination_at)||compare(x,*b->exit_from,expected->from)||compare(x,*b->exit_until,expected->until))fail("SafeExitWindowProjectionMismatch",b->id);
 if(!expected||compare(x,take(x.clock.read_organization_authority_now()),expected->until)<0)for(const auto& id:b->exit_line_refs){const auto* l=x.state.lines.find(id);if(l&&l->state==SafeExitLineState::expired)fail("SafeExitPrematureExpiryProjection",id);}
 if(expected&&compare(x,take(x.clock.read_organization_authority_now()),expected->until)>=0)for(const auto& id:b->exit_line_refs){const auto* l=x.state.lines.find(id);if(l&&(l->state==SafeExitLineState::inside||l->state==SafeExitLineState::exiting))fail("SafeExitExpiredWindowStillActive",id);}
 if(b->state!=aggregate(*b,x.state.lines))fail("SafeExitParentStateNotDerived",b->id);return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Status project_safe_exit_window_sources_in_candidate(CommonStore& common,SafeExitState& state,const SafeExitEntryContext& x,const SourceRef& source){try{
 if(&common!=&x.common||&state!=&x.state)fail("SafeExitWindowCandidateMismatch");const auto now=take(x.clock.read_organization_authority_now());auto projected=window(x,source);auto* b=state.bindings.find(source.id);if(!b)fail("SafeExitBindingSourceMissing",source.id);
 if(projected){b->termination_source=projected->termination_source;b->termination_at=projected->termination_at;b->exit_from=projected->from;b->exit_until=projected->until;
  if(compare(x,now,projected->until)>=0)for(const auto& line_id:b->exit_line_refs){auto* line=state.lines.find(line_id);if(!line)fail("SafeExitLineBindingMismatch",line_id);if(line->state==SafeExitLineState::inside||line->state==SafeExitLineState::exiting)line->state=SafeExitLineState::expired;}
 }else if(b->termination_source||b->termination_at||b->exit_from||b->exit_until)fail("SafeExitPrematureTerminationProjection",source.id);
 b->state=aggregate(*b,state.lines);return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Status adopt_safe_exit_original_deadlines_in_candidate(CommonStore& common,SafeExitState& state,const SafeExitEntryContext& x){try{
 if(&common!=&x.common||&state!=&x.state)fail("SafeExitWindowCandidateMismatch");
 for(const auto& id:state.bindings.order()){const auto source=SourceRef{"politics","safe_exit_bound",id};const auto* b=state.bindings.find(id);if(!b)fail("SafeExitBindingSourceMissing",id);take(x.originals.validate_safe_exit_retained_progress(*b,state.lines));take(project_safe_exit_window_sources_in_candidate(common,state,x,source));take(validate_safe_exit_window_projection(x,source));}
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
}
