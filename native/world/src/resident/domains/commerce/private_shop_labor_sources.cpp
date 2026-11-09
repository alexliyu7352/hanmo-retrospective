// S7 actual labor application. S0 alone measures fixed service; S4 alone owns
// body/capability and S1 alone owns historical presence. No attendance synthesis.
#include "private_shop_operation_sources.hpp"
#include "private_shop_capability.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
ActorActionWorkBinding binding(const StableId&id){return {id,"private_shop_operate",private_shop_operation_started_source(id)};}
const FixedServiceTransition& transition(const PrivateShopOperationSourceContext&x,const StableId&id,const StableId&operation,const std::vector<FixedServiceTransition>*candidate=nullptr){
 const FixedServiceTransition*found=nullptr;
 for(const auto&t:candidate?*candidate:x.elapsed.fixed_service_transitions)if(t.transition_id==id){if(found)fail("DuplicatePrivateShopServiceTransition",id);found=&t;}
 if(!found||found->action_ref!=operation||found->source_schema!=1||found->execution_source_schema!=2||
  !same_source_ref(found->execution_source_ref,private_shop_operation_started_source(operation))||
  (found->kind!=FixedTransitionKind::started&&found->kind!=FixedTransitionKind::resumed&&found->kind!=FixedTransitionKind::input_rebound))
  fail("PrivateShopActualServiceOpeningRequired",id);
 return *found;
}
class AllowedSpans final:public CharacterCapabilitySpanVisitor {
 const PrivateShopOperationSourceContext&x_;
 const ActorStationaryLocationSpan&location_;
 PrivateShopLaborSpanVisitor*visitor_;
 PrivateShopOperationSourcePinSink*sink_;
public:
 AllowedSpans(const PrivateShopOperationSourceContext&x,const ActorStationaryLocationSpan&location,
  PrivateShopLaborSpanVisitor*visitor,PrivateShopOperationSourcePinSink*sink):x_(x),location_(location),visitor_(visitor),sink_(sink){}
 Status visit_capability_span(const MomentSpan&span,const CharacterCapabilityView&v)override{
  if(!v.allowed||v.efficiency_bp<=0||v.efficiency_bp>10000)return common_failure("PrivateShopUnqualifiedLaborSpan",v.actor_id);
  if(sink_){
   const auto source=read_character_condition_head_source(x_.body,v.condition_head,span.from);
   if(!source.ok)return {false,{},source.error};
   auto kept=sink_->source({source.value.source,source.value.source_schema});if(!kept.ok)return kept;
   kept=sink_->locked_rule(v.rule_ref);if(!kept.ok)return kept;
  }
  return visitor_?visitor_->applied_span(span,v,location_):common_success();
 }
};
// Re-read the actual immutable inputs adopted by the sole Fixed transition.
// This read does not infer a historical head from today's body or position.
void actual_intervals(const PrivateShopOperationSourceContext&x,const PrivateShopOperation&o,
 const PrivateShopOperationAcceptance&a,const ActualServiceIntervalView&provided,
 PrivateShopOperationSourcePinSink*sink=nullptr,PrivateShopLaborSpanVisitor*visitor=nullptr,const std::vector<FixedServiceTransition>*candidate=nullptr){
 const auto&s=x.spatial;const auto*shop=x.shops.state.shops.find(o.shop_id);
 if(!shop)fail("PrivateShopOriginalShopRequired",o.id);
 for(const auto&span:provided.intervals){
  const auto&t=transition(x,span.opening_transition_ref,o.id,candidate);
  if(t.input_basis_refs.size()!=span.input_basis_refs.size())fail("PrivateShopServiceInputCoverageMismatch",o.id);
  for(std::size_t n=0;n<t.input_basis_refs.size();++n)if(!same_fixed_input_ref(t.input_basis_refs[n],span.input_basis_refs[n]))fail("PrivateShopServiceInputMismatch",o.id);
  const FixedAppliedInput*body=nullptr;const FixedAppliedInput*presence=nullptr;
  for(const auto&input:span.input_basis_refs)if(const auto*source=std::get_if<FixedAppliedInput>(&input)){
   if(source->source_ref.domain=="actor_foundation"&&(source->source_ref.kind=="initial_condition_seed"||source->source_ref.kind=="condition_receipt")){
    if(body)fail("PrivateShopUniqueConditionInputRequired",o.id);body=source;
   }else if(is_actor_spatial_head_source_kind(source->source_ref)){
    if(presence)fail("PrivateShopUniquePresenceInputRequired",o.id);presence=source;
   }
  }
  if(!body||!presence||cmp(t.occurred_at,span.from)>0||cmp(span.from,a.accepted_at)<0)fail("PrivateShopOriginalServiceInputsRequired",o.id);
  // The immutable advertised market window bounds future customer capacity,
  // not original labor already owed after a lawful pause. Actual staff duty
  // remains bounded by its own accepted terms and genuine termination source.
  if(!o.staff_contract_id.empty()){
   const auto*staff=take(read_original_private_shop_staff_contract(x.shops,{private_shop_module,"private_shop_staff_accepted",o.staff_contract_id}));
   if(staff->terms.worker_actor_id!=o.actor_id||staff->terms.shop_id!=o.shop_id||staff->terms.service_item_id!=o.item_id||
    cmp(span.from,{staff->accepted_tick,{}})<0||cmp(span.until,{staff->through_tick,{}})>0)fail("PrivateShopActualStaffDutySpanRequired",o.id);
   if(!staff->terminal_fact_id.empty()){
    const auto*decision=x.shops.state.staff.decisions.find(staff->terminal_fact_id);if(!decision)fail("PrivateShopOriginalStaffTerminalRequired",o.id);
    if(decision->decision=="left"||decision->decision=="dismissed"){
     // This old source has only its actual calendar tick. A strictly prior
     // labor interval is provable; do not promote the old decision to exact M.
     if(cmp(span.until,{decision->tick,{}})>0)fail("PrivateShopOriginalExactStaffDecisionRequired",o.id);
    }else{const auto end=take(read_private_shop_exact_staff_terminal(x,{private_shop_module,"private_shop_staff_ended",staff->terminal_fact_id}));if(cmp(span.until,end.occurred_at)>0)fail("PrivateShopLaborAfterStaffDutyEnd",o.id);}
   }
  }
  const auto condition=take(read_character_condition_at_source(x.body,body->source_ref,body->source_schema,span.from));
  if(condition.head.actor_id!=o.actor_id)fail("PrivateShopConditionActorMismatch",o.id);
  if(take(actor_spatial_history_source_schema(presence->source_ref))!=presence->source_schema)fail("PrivateShopPresenceSchemaMismatch",o.id);
  const auto location=take(read_actor_stationary_location_span(s.common,s.common_catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.spatial,
   o.actor_id,presence->source_ref,{span.from,span.until},x.military_handoffs,s.budget,s.family_catalog,s.births,s.military_actions));
  if(!take(same_spatial_anchor(location.actual_anchor,take(s.spatial.original_site_anchor(shop->terms.place_id)))))fail("PrivateShopLaborAwayFromShop",o.id);
  AllowedSpans allowed(x,location,visitor,sink);take(visit_character_capability_span(x.body,condition.head,
   {"private_shop_operation",1,x.body.catalog.identity.content_digest},std::nullopt,{span.from,span.until},allowed));
  if(sink){
   take(sink->fixed_transition(span.opening_transition_ref));
   if(span.closing_transition_ref)take(sink->fixed_transition(*span.closing_transition_ref));
   take(sink->source({body->source_ref,body->source_schema}));
   // Each consumer slot may have several distinct dependencies. Do not emit
   // an identical original edge twice inside this row's direct callback.
   for(const auto&original:location.original_sources)take(sink->source(original));
  }
 }
}
const PrivateShopLaborAppliedSource& original(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref){
 if(ref.source_schema!=1||!same_source_ref(ref.source_ref,private_shop_labor_applied_source(ref.source_ref.id)))fail("PrivateShopOriginalLaborSourceRequired",ref.source_ref.id);
 const PrivateShopLaborAppliedSource*found=nullptr;
 for(const auto&v:x.sources.labor_applied_sources)if(v.id==ref.source_ref.id){if(found)fail("DuplicatePrivateShopLaborSource",v.id);found=&v;}
 if(!found)fail("PrivateShopOriginalLaborSourceMissing",ref.source_ref.id);
 const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(found->operation_ref),2}));
 const auto*o=x.shops.state.operations.find(found->operation_ref);
 const auto serial=organization_instance_sequence(found->id,"private_shop_labor");
 if(!o||!serial||*serial>=x.shops.common.state().next_instance_id||!o->attendance_ticks.empty())fail("PrivateShopExactLaborIdentityMismatch",found->id);
 const auto position=take(read_actual_common_fact_position(x.shops.common,ref.source_ref));
 if(take(read_actual_common_fact_position(x.shops.common,private_shop_operation_started_source(o->id)))>=position)fail("PrivateShopLaborBeforeStart",found->id);
 if(!o->result_fact_id.empty()){
  const auto*r=x.shops.state.operating_results.find(o->result_fact_id);if(!r)fail("PrivateShopOriginalTerminalRequired",o->id);
  if(take(read_actual_common_fact_position(x.shops.common,{private_shop_module,r->completed?"private_shop_operating_completed":"private_shop_operating_interrupted",r->id}))<=position)fail("PrivateShopLaborAfterTerminal",found->id);
 }
 const auto period=take(read_fixed_service_work_period2(x.elapsed,binding(o->id),found->work_period_ref.period_ordinal,2,
  take(x.clock.read_current_moment()),x.scale,x.spatial.budget));
 if(!take(same_work_period_ref2(found->work_period_ref,period.ref,x.spatial.budget))||cmp(found->occurred_at,period.ref.until))fail("PrivateShopLaborPeriodMismatch",found->id);
 Integer earlier=0;
 for(const auto&v:x.sources.labor_applied_sources)if(v.operation_ref==o->id){
  if(v.work_period_ref.period_ordinal==found->work_period_ref.period_ordinal&&v.id!=found->id)fail("PrivateShopDuplicateLaborPeriod",found->id);
  if(v.work_period_ref.period_ordinal<found->work_period_ref.period_ordinal){
   if(v.work_period_ref.period_ordinal<0||take(read_actual_common_fact_position(x.shops.common,private_shop_labor_applied_source(v.id)))>=position)fail("PrivateShopLaborOrderMismatch",found->id);
   ++earlier;
  }
 }
 if(earlier!=found->work_period_ref.period_ordinal)fail("PrivateShopLaborPeriodGap",found->id);
 actual_intervals(x,*o,a,period.actual_service);
 return *found;
}
}
Status validate_private_shop_original_provided_intervals(const PrivateShopOperationSourceContext&x,const StableId&operation,
 const ActualServiceIntervalView&provided,const std::vector<FixedServiceTransition>&actual_transitions){
 try{
  const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(operation),2}));
  const auto*o=x.shops.state.operations.find(operation);if(!o||provided.action_ref!=operation||!provided.complete)fail("PrivateShopActualProvidedIntervalRequired",operation);
  actual_intervals(x,*o,a,provided,nullptr,nullptr,&actual_transitions);return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
SourceRef private_shop_labor_applied_source(const StableId&id){return {private_shop_module,"private_shop_labor_applied",id};}
Outcome<const PrivateShopLaborAppliedSource*> read_original_private_shop_labor_for_wage(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref){
 try{return {true,&original(x,ref),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<const PrivateShopLaborAppliedSource*> read_original_private_shop_labor(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref){
 try{
  const auto&row=original(x,ref);const auto&o=*x.shops.state.operations.find(row.operation_ref);
  if(o.staff_contract_id.empty()){if(row.original_staff_service_ref)fail("PrivateShopOwnerHasStaffWage",row.id);}
  else{
   if(!row.original_staff_service_ref||row.original_staff_service_ref->domain!=private_shop_module||row.original_staff_service_ref->kind!="private_shop_staff_served")fail("PrivateShopOriginalStaffServiceRequired",row.id);
   const auto*service=x.shops.state.staff.services.find(row.original_staff_service_ref->id);
   if(!service||service->operation_id!=o.id||service->contract_id!=o.staff_contract_id||service->closing_fraction||
    service->attendance_index!=row.work_period_ref.period_ordinal+1||service->tick!=row.occurred_at.half_day_tick||
    take(read_actual_common_fact_position(x.shops.common,*row.original_staff_service_ref))<=take(read_actual_common_fact_position(x.shops.common,ref.source_ref)))fail("PrivateShopLaborWageSourceMismatch",row.id);
   take(validate_private_shop_staff_services(x.shops));
  }
  return {true,&row,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status visit_private_shop_labor_pins(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref,PrivateShopOperationSourcePinSink&sink){
 try{
  class UniquePins final:public PrivateShopOperationSourcePinSink {
   PrivateShopOperationSourcePinSink&out_;std::vector<MomentOwnerSource>sources_;std::vector<ActorFoundationRuleRef>rules_;std::vector<StableId>transitions_;
  public:explicit UniquePins(PrivateShopOperationSourcePinSink&out):out_(out){}
   Status source(const MomentOwnerSource&s)override{for(const auto&old:sources_)if(old.source_schema==s.source_schema&&same_source_ref(old.source_ref,s.source_ref))return common_success();sources_.push_back(s);return out_.source(s);}
   Status locked_rule(const ActorFoundationRuleRef&r)override{for(const auto&old:rules_)if(old.id==r.id&&old.version==r.version&&old.catalog_digest==r.catalog_digest)return common_success();rules_.push_back(r);return out_.locked_rule(r);}
   Status fixed_transition(const StableId&id)override{for(const auto&old:transitions_)if(old==id)return common_success();transitions_.push_back(id);return out_.fixed_transition(id);}
  }unique(sink);
  const auto&row=*take(read_original_private_shop_labor(x,ref));
  take(unique.source({private_shop_operation_started_source(row.operation_ref),2}));
  if(row.original_staff_service_ref)take(unique.source({*row.original_staff_service_ref,1}));
  const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(row.operation_ref),2}));
  const auto period=take(read_fixed_service_work_period2(x.elapsed,binding(row.operation_ref),row.work_period_ref.period_ordinal,2,take(x.clock.read_current_moment()),x.scale,x.spatial.budget));
  actual_intervals(x,*x.shops.state.operations.find(row.operation_ref),a,period.actual_service,&unique);
  return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status visit_private_shop_original_provided_spans(const PrivateShopOperationSourceContext&x,const StableId&id,const ActualServiceIntervalView&provided,PrivateShopLaborSpanVisitor&visitor){try{
 const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(id),2}));const auto*o=x.shops.state.operations.find(id);if(!o||provided.action_ref!=id||!provided.complete)fail("PrivateShopActualProvidedIntervalRequired",id);actual_intervals(x,*o,a,provided,nullptr,&visitor);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_original_private_shop_labor_spans(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref,PrivateShopLaborSpanVisitor&visitor){
 try{
  const auto&row=*take(read_original_private_shop_labor(x,ref));
  const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(row.operation_ref),2}));
  const auto period=take(read_fixed_service_work_period2(x.elapsed,binding(row.operation_ref),row.work_period_ref.period_ordinal,2,take(x.clock.read_current_moment()),x.scale,x.spatial.budget));
  actual_intervals(x,*x.shops.state.operations.find(row.operation_ref),a,period.actual_service,nullptr,&visitor);return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<StableId> apply_private_shop_labor_period_in_candidate(CommonStore&c,LocalEconomyState&economy,
 PrivateShopState&shops,PrivateShopOperationSourcesState&state,PrivateShopOperationCapabilitiesState&capabilities,const PrivateShopOperationSourceContext&x,const StableId&operation,Integer ordinal){
 try{
  if(&c!=&x.shops.common||&economy!=&x.shops.economy||&shops!=&x.shops.state||&state!=&x.sources||&capabilities!=&x.capabilities)fail("PrivateShopLaborCandidateMismatch",operation);
  const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(operation),2}));
  const auto*o=shops.operations.find(operation);const auto*action=c.action(operation);const auto at=take(x.clock.read_current_moment());
  if(!o||!action||action->kind!="private_shop_operate"||action->actor_id!=o->actor_id||
   !o->result_fact_id.empty()||!o->attendance_ticks.empty())fail("PrivateShopActiveExactOperationRequired",operation);
  for(const auto&old:state.labor_applied_sources)if(old.operation_ref==operation&&old.work_period_ref.period_ordinal==ordinal)fail("PrivateShopLaborAlreadyApplied",operation);
  const auto period=take(read_fixed_service_work_period2(x.elapsed,binding(operation),ordinal,2,at,x.scale,x.spatial.budget));
  if(cmp(at,period.ref.until))fail("PrivateShopActualLaborBoundaryRequired",operation);
  actual_intervals(x,*o,a,period.actual_service);
  const auto id=take(c.next_id("private_shop_labor"));const auto source=private_shop_labor_applied_source(id);
  take(c.append_fact({id,source.kind,source.domain}));
  state.labor_applied_sources.push_back({id,operation,period.ref,at,std::nullopt});
  auto service=take(record_private_shop_staff_exact_labor(c,economy,shops,x,source));
  // The same candidate owns this new row. No other operation may publish an
  // incomplete wage edge; any late failure discards the entire World candidate.
  state.labor_applied_sources.back().original_staff_service_ref=std::move(service);
  take(read_original_private_shop_labor(x,{source,1}));
  take(adopt_private_shop_labor_capabilities_in_candidate(capabilities,x,{source,1}));
  return {true,id,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
}
