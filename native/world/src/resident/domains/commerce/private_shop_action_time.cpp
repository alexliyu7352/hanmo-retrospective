#include "private_shop_action_time.hpp"
#include "private_shop_capability.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool rule(const ActorFoundationRuleRef&a,const ActorFoundationRuleRef&b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
void registered(const ActionTimeRegistration&r){const auto&actual=private_shop_operation_action_time_registration();
 if(r.action_kind!=actual.action_kind||r.owner_domain!=actual.owner_domain||r.profile_id!=actual.profile_id||r.profile_version!=actual.profile_version||
  r.source_schema!=actual.source_schema||r.mode!=actual.mode||r.remaining_meaning!=actual.remaining_meaning||r.shared_gate_kind||!r.retained_wait_phases.empty())fail("PrivateShopOriginalActionTimeRegistrationRequired");
}
ActorFoundationRuleRef purpose(const PrivateShopOperationSourceContext&x){return {"private_shop_operation",1,x.body.catalog.identity.content_digest};}
std::vector<FixedServiceInputRef> inputs(const PrivateShopOperationSourceContext&x,const StableId&operation,const MomentOwnerSource&body,const MomentOwnerSource&presence,const CharacterCapabilityView&capability){
 if(!rule(capability.purpose_ref,purpose(x))||!x.body.catalog.normal_work_efficiency_rule||!rule(capability.rule_ref,x.body.catalog.normal_work_efficiency_rule->rule_ref))fail("PrivateShopOriginalCapabilityRuleRequired",operation);
 return {FixedAppliedInput{private_shop_operation_started_source(operation),2},FixedAppliedInput{body.source_ref,body.source_schema},
  FixedAppliedInput{presence.source_ref,presence.source_schema},FixedLockedRuleInput{x.body.catalog.identity,capability.purpose_ref.id},FixedLockedRuleInput{x.body.catalog.identity,capability.rule_ref.id}};
}
void same_inputs(const std::vector<FixedServiceInputRef>&actual,const std::vector<FixedServiceInputRef>&expected,const StableId&id){
 if(actual.size()!=expected.size())fail("PrivateShopOriginalInputCoverageMismatch",id);
 for(std::size_t n=0;n<actual.size();++n)if(!same_fixed_input_ref(actual[n],expected[n]))fail("PrivateShopOriginalInputMismatch",id);
}
CharacterCapabilityView transition_inputs(const PrivateShopOperationSourceContext&x,const PrivateShopOperation&o,const FixedServiceTransition&t){
 if(t.input_basis_refs.size()!=5)fail("PrivateShopOriginalInputCoverageMismatch",t.transition_id);
 const auto*body=std::get_if<FixedAppliedInput>(&t.input_basis_refs[1]);const auto*presence=std::get_if<FixedAppliedInput>(&t.input_basis_refs[2]);
 if(!body||!presence||!is_actor_spatial_head_source_kind(presence->source_ref))fail("PrivateShopOriginalInputKindsRequired",t.transition_id);
 const auto condition=take(read_character_condition_at_source(x.body,body->source_ref,body->source_schema,t.occurred_at));
 if(condition.head.actor_id!=o.actor_id)fail("PrivateShopTransitionConditionActorMismatch",t.transition_id);
 auto capability=take(read_character_capability_at_head(x.body,condition.head,purpose(x),std::nullopt,t.occurred_at));
 same_inputs(t.input_basis_refs,inputs(x,o.id,{body->source_ref,body->source_schema},{presence->source_ref,presence->source_schema},capability),t.transition_id);
 const auto location=take(read_actor_spatial_location_from_head_pool(x.spatial,o.actor_id,{{presence->source_ref,presence->source_schema}},t.occurred_at,x.military_handoffs));
 const auto*shop=x.shops.state.shops.find(o.shop_id);if(!shop||!take(same_spatial_anchor(*take(require_actor_ordinary_movement_anchor(location)),take(x.spatial.spatial.original_site_anchor(shop->terms.place_id)))))fail("PrivateShopTransitionActualPresenceRequired",t.transition_id);
 return capability;
}
ActionTimeOwnerProof owner(const PrivateShopOperationSourceContext&x,const StableId&id){
 const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(id),2}));
 take(read_original_private_shop_operation_profile(x,id));const auto*o=x.shops.state.operations.find(id);if(!o)fail("PrivateShopOriginalOperationRequired",id);
 CheckpointVector<std::string_view> participants,gates;participants.reserve(1);participants.push_back(o->actor_id);ActionTimeOwnerPhase current_phase;std::optional<ElapsedTimeSourceBuildView> ended;std::optional<std::string_view> terminal_result;
 if(!o->result_fact_id.empty()){
  const auto*r=x.shops.state.operating_results.find(o->result_fact_id);if(!r)fail("PrivateShopOriginalTerminalRequired",id);
  MomentOwnerSource source{{private_shop_module,r->completed?"private_shop_operating_completed":"private_shop_operating_interrupted",r->id},2};
  const auto&terminal=*take(read_original_private_shop_operation_terminal(x,source));if(terminal.operation_ref!=id)fail("PrivateShopTerminalOperationMismatch",id);
  current_phase=ActionTimeOwnerPhase::terminal;ended=MomentOwnerSourceBuildView{{private_shop_module,r->completed?"private_shop_operating_completed":"private_shop_operating_interrupted",r->id},2};terminal_result=r->id;
 }else{
  const auto*action=x.shops.common.action(id);const auto*actor=x.shops.common.actor(o->actor_id);
  if(!action||!actor||action->kind!="private_shop_operate"||action->actor_id!=o->actor_id||(action->participants.size()!=1||action->participants.front()!=o->actor_id)||action->start_tick!=a.accepted_at.half_day_tick||actor->action_id!=id)fail("PrivateShopActualExclusiveActionRequired",id);
  const auto body=take(read_current_character_condition(x.body,o->actor_id));
  const auto capability=take(read_character_capability_at_head(x.body,body.head,purpose(x),std::nullopt,take(x.clock.read_current_moment())));
  current_phase=capability.allowed?ActionTimeOwnerPhase::working:ActionTimeOwnerPhase::suspended;
 }
 const SourceRefBuildView execution{private_shop_module,"private_shop_operating_started",id};return ActionTimeOwnerProof{ActionTimeOwnerProofBuildView{id,"private_shop_operate",o->actor_id,{participants},execution,2,MomentOwnerSourceBuildView{execution,2},current_phase,std::nullopt,ended,terminal_result,{gates},std::nullopt}};
}
}
const ActionTimeRegistration& private_shop_operation_action_time_registration(){
 static const ActionTimeRegistration value{"private_shop_operate","private_shops","private_shop_operation_sources",1,2,ActionTimeMode::fixed_duration,ActionRemainingMeaning::service_estimate,{},{}};return value;
}
Outcome<std::vector<FixedServiceInputRef>> read_private_shop_current_service_inputs(const PrivateShopOperationSourceContext&x,const StableId&id){
 try{
  take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(id),2}));const auto*o=x.shops.state.operations.find(id);const auto*shop=o?x.shops.state.shops.find(o->shop_id):nullptr;if(!o||!shop||!o->result_fact_id.empty())fail("PrivateShopActiveOriginalOperationRequired",id);
  const auto at=take(x.clock.read_current_moment());const auto body=take(read_current_character_condition(x.body,o->actor_id));
  const auto capability=take(read_character_capability_at_head(x.body,body.head,purpose(x),std::nullopt,at));
  const auto condition=take(read_character_condition_head_source(x.body,body.head,at));
  const auto original_prefix=take(read_current_common_fact_prefix(x.shops.common));ActorSpatialActualFactPrefix prefix;prefix.common_fact_count=original_prefix.common_fact_count;
  if(original_prefix.last_common_fact)prefix.last_common_fact={original_prefix.last_common_fact->domain,original_prefix.last_common_fact->kind,original_prefix.last_common_fact->id};
  const auto&s=x.spatial;const auto location=take(read_actor_spatial_location_at_actual_fact_prefix(s.common,s.common_catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.spatial,o->actor_id,at,prefix,x.military_handoffs,s.budget,s.family_catalog,s.births,s.military_actions));
  if(location.actual_location_sources.size()!=1||!take(same_spatial_anchor(*take(require_actor_ordinary_movement_anchor(location)),take(s.spatial.original_site_anchor(shop->terms.place_id)))))fail("PrivateShopCurrentActualPresenceRequired",id);
  const auto&presence=location.actual_location_sources.front();return {true,inputs(x,id,{condition.source,condition.source_schema},{presence.source,presence.schema},capability),{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status NativePrivateShopActionTimeRead::enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink& sink)const{
 try{registered(r);for(std::size_t i=0;i<x_.sources.accepted_operations.size();++i){const auto& a=x_.sources.accepted_operations[i];take(read_original_private_shop_operation_acceptance(x_,{private_shop_operation_started_source(a.operation_ref),2}));for(std::size_t prior=0;prior<i;++prior)if(x_.sources.accepted_operations[prior].operation_ref==a.operation_ref)fail("DuplicatePrivateShopActionOwner",a.operation_ref);auto appended=sink.append(a.operation_ref);if(!appended.ok)return appended;}return common_success();}
 catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<ActionTimeOwnerProof> NativePrivateShopActionTimeRead::read_action_time_owner(const ActionTimeRegistration&r,const StableId&id)const{
 try{registered(r);return {true,owner(x_,id),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<WorldMoment> NativePrivateShopActionTimeRead::read_action_time_source(const ElapsedTimeSource&source)const{
 try{const auto*ref=std::get_if<MomentOwnerSource>(&source);if(!ref)fail("PrivateShopExactMomentSourceRequired");
  if(ref->source_ref.kind=="private_shop_operating_started")return {true,take(read_original_private_shop_operation_acceptance(x_,*ref))->accepted_at,{}};
  return {true,take(read_original_private_shop_operation_terminal(x_,*ref))->occurred_at,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status NativePrivateShopActionTimeRead::validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&entry,const std::vector<FixedServiceTransition>&transitions)const{
 return validate_fixed_action_sources_in_candidate(r,p,entry,transitions,x_.elapsed);
}
Status NativePrivateShopActionTimeRead::validate_fixed_action_sources_in_candidate(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&entry,const std::vector<FixedServiceTransition>&transitions,const ActionElapsedStore&candidate)const{
 try{
  registered(r);const auto actual=owner(x_,entry.action_ref);const auto&o=*x_.shops.state.operations.find(entry.action_ref);
  const auto&a=*take(read_original_private_shop_operation_acceptance(x_,{private_shop_operation_started_source(o.id),2}));
  const auto&profile=*take(read_original_private_shop_operation_profile(x_,o.id));
  const auto required=take(fixed_duration_work_units(2,x_.scale.units_per_half_day));
  if(p.action_ref!=actual.action_ref||p.action_kind!=actual.action_kind||p.actor_id!=actual.actor_id||p.participant_actor_ids!=actual.participant_actor_ids||
   !same_source_ref(p.original_execution_source,actual.original_execution_source)||p.source_schema!=2||p.phase!=actual.phase||
   entry.execution_source_ref!=o.id||entry.duration_definition_ref!=private_shop_operation_duration_rule||entry.owner_cost_state_ref!=o.id||entry.owner_work_remainder||
   cmp(entry.started_at,a.accepted_at)||take(compare_fractions(required.value,entry.required_units.value))||transitions.empty())fail("PrivateShopOriginalElapsedMismatch",o.id);
  const auto*retained=find_fixed_elapsed(candidate,o.id);if(!retained||cmp(retained->started_at,entry.started_at)||cmp(retained->last_settled_at,entry.last_settled_at)||
   take(compare_fractions(retained->credited_units.value,entry.credited_units.value))||retained->timing.index()!=entry.timing.index())fail("PrivateShopActualElapsedCandidateRequired",o.id);
  bool running=true;std::vector<StableId>condition_changes;
  for(std::size_t n=0;n<transitions.size();++n){const auto&t=transitions[n];
   const auto*saved=take(read_fixed_service_transition_source(candidate,{t.transition_id,t.source_schema}));if(!take(same_fixed_service_transition(t,*saved)))fail("PrivateShopActualTransitionCandidateMismatch",t.transition_id);
   if(t.action_ref!=o.id||t.execution_source_schema!=2||!same_source_ref(t.execution_source_ref,actual.original_execution_source))fail("PrivateShopTransitionExecutionMismatch",t.transition_id);
   if(t.kind==FixedTransitionKind::terminal){
    const auto*cause=std::get_if<FixedOwnerTerminated>(&t.cause);if(!cause||n+1!=transitions.size()||!t.input_basis_refs.empty())fail("PrivateShopOriginalTerminalTransitionRequired",t.transition_id);
    const auto&terminal=*take(read_original_private_shop_operation_terminal(x_,{cause->owner_result_ref,cause->owner_result_schema}));
    const auto*result=x_.shops.state.operating_results.find(terminal.original_result_ref.id);
    const auto disposition=result->completed?FixedTerminalDisposition::completed:(result->reason=="cancelled"?FixedTerminalDisposition::cancelled:FixedTerminalDisposition::interrupted);
    if(terminal.operation_ref!=o.id||t.transition_id!=result->id||cmp(t.occurred_at,terminal.occurred_at)||t.terminal_disposition!=std::optional<FixedTerminalDisposition>{disposition})fail("PrivateShopTerminalTransitionMismatch",t.transition_id);
    running=false;continue;
   }
   const auto capability=transition_inputs(x_,o,t);
   if(n==0){const auto*cause=std::get_if<FixedOwnerAdopted>(&t.cause);
    if(t.kind!=FixedTransitionKind::started||t.transition_id!=o.id||!cause||cause->owner_source_schema!=2||!same_source_ref(cause->owner_source_ref,actual.original_execution_source)||
     cmp(t.occurred_at,a.accepted_at)||!capability.allowed||capability.efficiency_bp<=0)fail("PrivateShopOriginalStartTransitionMismatch",t.transition_id);
    const auto*body=std::get_if<FixedAppliedInput>(&t.input_basis_refs[1]);const auto*presence=std::get_if<FixedAppliedInput>(&t.input_basis_refs[2]);const auto&accepted_body=profile.acceptance_condition_source_refs.front();
    if(!body||body->source_schema!=accepted_body.source_schema||!same_source_ref(body->source_ref,accepted_body.source_ref)||!presence||!same_source_ref(presence->source_ref,a.actual_presence_source_ref))fail("PrivateShopStartInputAdoptionMismatch",o.id);
   }else{const auto*cause=std::get_if<FixedEligibilityChanged>(&t.cause);const auto*body=std::get_if<FixedAppliedInput>(&t.input_basis_refs[1]);
    if(!cause||cause->original_owner_execution_ref!=o.id||cause->original_cause_schema!=2||cause->original_cause_ref.domain!="actor_foundation"||cause->original_cause_ref.kind!="condition_receipt"||
     cause->transition_rule_ref!=capability.rule_ref.id||t.transition_id!=cause->original_cause_ref.id||!body||body->source_schema!=2||!same_source_ref(body->source_ref,cause->original_cause_ref))fail("PrivateShopActualConditionChangeRequired",t.transition_id);
    const auto changed=take(read_character_condition_source(x_.body,cause->original_cause_ref,2));if(changed.actor_id!=o.actor_id||cmp(changed.occurred_at,t.occurred_at))fail("PrivateShopConditionChangeMomentMismatch",t.transition_id);
    if(t.kind==FixedTransitionKind::suspended){if(!running||capability.allowed||capability.efficiency_bp!=0)fail("PrivateShopActualParticipationStopRequired",t.transition_id);running=false;}
    else if(t.kind==FixedTransitionKind::resumed){if(running||!capability.allowed||capability.efficiency_bp<=0)fail("PrivateShopActualParticipationResumeRequired",t.transition_id);running=true;}
    else if(t.kind!=FixedTransitionKind::input_rebound||capability.allowed!=running)fail("PrivateShopConditionInputTransitionMismatch",t.transition_id);
    condition_changes.push_back(cause->original_cause_ref.id);
   }
  }
  if((actual.phase==ActionTimeOwnerPhase::working)!=std::holds_alternative<ElapsedRunning>(entry.timing)||
   (actual.phase==ActionTimeOwnerPhase::suspended)!=std::holds_alternative<ElapsedSuspended>(entry.timing)||
   (actual.phase==ActionTimeOwnerPhase::terminal)!=std::holds_alternative<ElapsedTerminal>(entry.timing))fail("PrivateShopOriginalPhaseMismatch",o.id);
  const auto&accepted_body=profile.acceptance_condition_source_refs.front();const auto initial=take(read_character_condition_at_source(x_.body,accepted_body.source_ref,accepted_body.source_schema,a.accepted_at));
  for(const auto&row:x_.body.state.character_condition_extensions.applied_receipts.rows()){
   const auto&change=row.second;if(change.actor_id!=o.actor_id||change.condition_revision_after<=initial.head.condition_revision)continue;
   if(change.effective_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("PrivateShopExactConditionChangeRequired",row.first);
   const auto relation=cmp(change.effective_at.exact_moment,entry.last_settled_at);if(relation>0||(relation==0&&actual.phase==ActionTimeOwnerPhase::terminal))continue;
   if(std::find(condition_changes.begin(),condition_changes.end(),change.result_id)==condition_changes.end())fail("PrivateShopConditionTransitionCoverageMismatch",change.result_id);
  }
  const auto now=take(x_.clock.read_current_moment());const bool due=std::holds_alternative<ElapsedRunning>(entry.timing)&&take(compare_fractions(entry.credited_units.value,entry.required_units.value))==0;
  auto provided=take(due?read_fixed_service_intervals_at_completion_due(candidate,o.id,entry.started_at,entry.last_settled_at,now,x_.scale,x_.spatial.budget):read_fixed_service_intervals(candidate,o.id,entry.started_at,entry.last_settled_at,now,x_.scale,x_.spatial.budget));
  take(validate_private_shop_original_provided_intervals(x_,o.id,provided,transitions));return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status NativePrivateShopActionTimeRead::validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const{return common_failure("PrivateShopSharedWorkNotSupported",{},ErrorKind::unsupported_capability);}
Outcome<RunStop> NativePrivateShopActionTimeRead::read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const{
 try{registered(r);if(cmp(at,take(x_.clock.read_current_moment())))fail("PrivateShopActualContinuationMomentRequired",p.action_ref);auto actual=owner(x_,p.action_ref);
  if(actual.phase==ActionTimeOwnerPhase::terminal){const auto*o=x_.shops.state.operations.find(p.action_ref);return {true,x_.shops.state.operating_results.find(o->result_fact_id)->completed?RunStop::completed:RunStop::cancelled,{}};}
  return {true,actual.phase==ActionTimeOwnerPhase::suspended?RunStop::paused:RunStop::awaiting_permit,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<OriginalActionRemaining> NativePrivateShopActionTimeRead::read_original_action_remaining(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const{
 try{registered(r);if(cmp(at,take(x_.clock.read_current_moment())))fail("PrivateShopActualRemainingMomentRequired",p.action_ref);const auto actual=owner(x_,p.action_ref);
  const auto*entry=find_fixed_elapsed(x_.elapsed,p.action_ref);if(!entry)fail("PrivateShopOriginalElapsedRequired",p.action_ref);
  auto remaining=take(scale_units_to_duration(take(subtract_work_units(entry->required_units,entry->credited_units)),x_.scale.units_per_half_day));return {true,ActualServiceRemaining{ActualServiceRemainingBuildView{remaining,{actual.original_execution_source.domain,actual.original_execution_source.kind,actual.original_execution_source.id},2}},{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<StableId> NativePrivateShopActionTimeRead::read_gate_result_id(const SharedWorkGateResult&)const{return {false,{},common_failure("PrivateShopSharedGateNotSupported",{},ErrorKind::unsupported_capability).error};}
Outcome<SharedWorkGateResult> NativePrivateShopActionTimeRead::read_original_gate_result(const StableId&id)const{return {false,{},common_failure("PrivateShopSharedGateNotSupported",id,ErrorKind::unsupported_capability).error};}
Status NativePrivateShopActionTimeRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("PrivateShopSharedGateNotSupported",{},ErrorKind::unsupported_capability);}
}
