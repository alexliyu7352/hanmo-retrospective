#include "organization_calendar_admission.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{x.error};return std::move(x.value);}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){const auto* f=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();const auto p=std::find(order.begin(),order.end(),ref.id);if(!f||f->domain!=ref.domain||f->kind!=ref.kind||p==order.end())fail("OrganizationCalendarOriginalCutRequired",ref.id);return static_cast<std::size_t>(p-order.begin());}
OrganizationCalendarWindow window(Integer from,Integer duration,std::optional<SourceRef> excluded={}){if(from<0||duration<=0||from>std::numeric_limits<Integer>::max()-duration)fail("OrganizationCalendarWindowOverflow");return {from,from+duration,std::move(excluded)};}
void current(const EconomicCalendarReadContext& x,const WorldTimelineState& timeline,const WorldMoment& now){
 take(validate_timeline_value(x.common.tick(),timeline));take(validate_economic_calendar_sources(x,now));
 const auto published=take(read_world_moment(x.common.tick(),timeline));if(compare(now,published)<0)fail("OrganizationCalendarClockBeforeTimeline");
 if(timeline.boundary_state==PeriodBoundaryState::ready){if(compare(now,published)||compare(now,{now.half_day_tick,{}}))fail("OrganizationCalendarReadyMomentMismatch");return;}
 const auto* p=x.sources.period_openings.find(timeline.period_id);
 if(!p||p->opening_at.half_day_tick!=x.common.tick()||compare(now,p->opening_at)<0||compare(now,{p->through_tick,{}})>0)fail("OrganizationCalendarCurrentOpeningRequired",timeline.period_id);
}
}
Outcome<OrganizationCalendarWindow> NativeOrganizationCalendarAdmissionRead::read_new_calendar_window(Integer duration)const{
 return guard<OrganizationCalendarWindow>([&]{const auto now=take(original_.clock.read_current_moment());current(original_,timeline_,now);
  if(timeline_.boundary_state==PeriodBoundaryState::ready)return window(now.half_day_tick,duration);
  const auto& p=*original_.sources.period_openings.find(timeline_.period_id);
  // 同M已经冻结者只能等下一期；真正through处尚未开启的下一期可从该整界起。
  if(compare(now,{p.through_tick,{}})<0)return window(p.through_tick,duration,economic_period_opening_source(p));
  return window(p.through_tick,duration);
 });
}
Outcome<OrganizationActualCalendarWindow> NativeOrganizationCalendarAdmissionRead::read_original_calendar_window(const SourceRef& approval,Integer duration)const{
 return guard<OrganizationActualCalendarWindow>([&]{
  const auto now=take(original_.clock.read_current_moment());current(original_,timeline_,now);
  const auto& c=original_.common;const auto* f=original_.organization.facts.find(approval.id);const auto cut=position(c,approval);
  if(!f||f->id!=approval.id||approval.domain!="organization"||f->kind!=approval.kind||(f->kind!="organization_activity_approved"&&f->kind!="population_training_approved")||!f->code.empty()||!f->occurred_at||f->tick!=f->occurred_at->half_day_tick||f->activity_id.empty()||compare(*f->occurred_at,now)>0)fail("OrganizationCalendarApprovalSourceRequired",approval.id);
  const auto* activity=original_.organization.activities.find(f->activity_id);if(!activity||activity->principal.kind!=SubjectKind::organization||activity->principal.id!=f->organization_id||activity->responsible_office_id!=f->office_id)fail("OrganizationCalendarApprovalActivityMismatch",approval.id);
  const EconomicPeriodOpening* excluded=nullptr;
  for(const auto& row:original_.sources.period_openings.rows()){
   const auto& p=row.second;const auto source=economic_period_opening_source(p);const auto sequence=position(c,source);
   if(sequence<cut){if(compare(p.opening_at,*f->occurred_at)>0)fail("OrganizationCalendarApprovalOrderMismatch",approval.id);if(compare(*f->occurred_at,{p.through_tick,{}})<0){if(excluded)fail("OrganizationCalendarMultipleCurrentOpenings",approval.id);excluded=&p;}}
   else if(compare(p.opening_at,*f->occurred_at)<0)fail("OrganizationCalendarOpeningMissingAtApproval",approval.id);
  }
  OrganizationActualCalendarWindow out;
  if(excluded)out.approved_window=window(excluded->through_tick,duration,economic_period_opening_source(*excluded));
  else{if(compare(*f->occurred_at,{f->tick,{}}))fail("OrganizationCalendarFractionWithoutOpening",approval.id);out.approved_window=window(f->tick,duration);}
  for(const auto& row:original_.sources.period_openings.rows()){
   const auto& p=row.second;if(p.opening_at.half_day_tick!=out.approved_window.from_tick)continue;
   if(out.actual_first_opening||position(c,economic_period_opening_source(p))<=cut||std::find(p.existing_activity_ids.begin(),p.existing_activity_ids.end(),activity->id)==p.existing_activity_ids.end())fail("OrganizationFirstEligibleOpeningMismatch",activity->id);
   // Borrow the actual validated owner row; it is not a reconstructed attendance receipt.
   out.actual_first_opening=take(read_economic_period_opening(original_,economic_period_opening_source(p),1));
  }
  if(!out.actual_first_opening&&original_.economy.last_step_tick>out.approved_window.from_tick)fail("OrganizationFirstEligibleOpeningMissing",activity->id);
  return out;
 });
}
}

namespace hanmo::resident {
Status NativeOrganizationCalendarAdmissionRead::validate_completed_calendar_window(const SourceRef& approval,Integer duration)const{
 auto admitted=read_original_calendar_window(approval,duration);if(!admitted.ok)return {false,{},admitted.error};
 const auto& window=admitted.value.approved_window;
 const auto* fact=original_.organization.facts.find(approval.id);
 if(!fact||!admitted.value.actual_first_opening||original_.economy.last_step_tick<window.until_tick)return common_failure("OrganizationCalendarServiceWindowIncomplete",approval.id);
 Integer expected=window.from_tick;
 for(const auto& row:original_.sources.period_openings.rows()){
  const auto& p=row.second;if(p.opening_at.half_day_tick<window.from_tick||p.opening_at.half_day_tick>=window.until_tick)continue;
  if(p.opening_at.half_day_tick!=expected||p.through_tick!=expected+1||std::find(p.existing_activity_ids.begin(),p.existing_activity_ids.end(),fact->activity_id)==p.existing_activity_ids.end())return common_failure("OrganizationCalendarServiceOpeningGap",fact->activity_id);
  auto actual=read_economic_period_opening(original_,economic_period_opening_source(p),1);if(!actual.ok)return {false,{},actual.error};
  expected=p.through_tick;
 }
 if(expected!=window.until_tick)return common_failure("OrganizationCalendarServiceOpeningGap",fact->activity_id);
 return common_success();
}
Status validate_organization_calendar_windows(const OrganizationState& state,const OrganizationCatalog& catalog,const OrganizationCalendarAdmissionRead& calendar,const OrganizationTrainingCoverageRead* training){
 if(!catalog.exact_fact_clock_enabled)return common_failure("OrganizationCalendarExactProfileRequired");
 for(const auto& row:state.activities.rows()){
  const auto& a=row.second;if(catalog.initial_activities.find(a.id))continue;
  bool personal=false,market=false;for(const auto& p:a.participation){personal|=p.binding_kind==ParticipationKind::personal_action;market|=p.role=="market_service_staffing";}
  if(personal||market)continue;
  const auto* terms=catalog.activity_terms.find(a.terms_id);if(!terms)return common_failure("UnknownOrganizationActivityTerms",a.id);
  const OrganizationFact* approved=nullptr;
  for(const auto& f:state.facts.rows())if(f.second.activity_id==a.id&&(f.second.kind=="organization_activity_approved"||f.second.kind=="population_training_approved")){
   if(approved)return common_failure("DuplicateOrganizationActivityApproval",a.id);approved=&f.second;
  }
  if(!approved)return common_failure("OrganizationCalendarApprovalSourceRequired",a.id);
  SourceRef source{"organization",approved->kind,approved->id};auto read=calendar.read_original_calendar_window(source,terms->duration_ticks);if(!read.ok)return {false,{},read.error};const auto& window=read.value.approved_window;
  if(a.started_tick!=window.from_tick||a.phase_started_tick!=window.from_tick||a.due_tick!=window.until_tick)return common_failure("OrganizationCalendarActivityWindowMismatch",a.id);
  if(a.status==OrgActivityStatus::completed||a.status==OrgActivityStatus::failed){
   auto valid=calendar.validate_completed_calendar_window(source,terms->duration_ticks);if(!valid.ok)return valid;
   if(a.result_fact_ids.size()!=1)return common_failure("OrganizationCalendarTerminalCoverage",a.id);
   const auto* terminal=state.facts.find(a.result_fact_ids.front());if(!terminal||terminal->activity_id!=a.id||terminal->tick!=window.until_tick||!terminal->occurred_at)return common_failure("OrganizationCalendarTerminalWindowMismatch",a.id);
   auto exact=compare_world_moments(*terminal->occurred_at,{window.until_tick,{}});if(!exact.ok)return {false,{},exact.error};if(exact.value)return common_failure("OrganizationCalendarTerminalWindowMismatch",a.id);
   if(a.kind==OrgActivityKind::population_training){if(!training)return common_failure("OrganizationTrainingOriginalCoverageRequired",a.id,ErrorKind::unsupported_capability);valid=training->validate_original_training_window(a,*terms,source,window.from_tick,window.until_tick);if(!valid.ok)return valid;}
  }
 }
 return common_success();
}
}
