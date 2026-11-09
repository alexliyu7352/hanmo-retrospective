#include "private_shop_capability.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool rule(const ActorFoundationRuleRef&a,const ActorFoundationRuleRef&b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
bool sources(const std::vector<MomentOwnerSource>&a,const std::vector<MomentOwnerSource>&b){
 if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(a[n].source_schema!=b[n].source_schema||!same_source_ref(a[n].source_ref,b[n].source_ref))return false;return true;
}
ActorFoundationRuleRef purpose(const PrivateShopOperationSourceContext&x){return {"private_shop_operation",1,x.body.catalog.identity.content_digest};}
ShopOperationCapabilityBinding frozen_base(const PrivateShopOperationSourceContext&x,const StableId&operation,
 const MomentSpan&span,const CharacterCapabilityView&capability){
 const auto*o=x.shops.state.operations.find(operation);if(!o)fail("PrivateShopOriginalOperationRequired",operation);
 ShopOperationCapabilityBinding out;out.operation_ref=o->id;
 out.actor_ref=o->actor_id;out.role=o->staff_contract_id.empty()?PrivateShopOperatorRole::owner_operator:PrivateShopOperatorRole::accepted_staff;
 if(!o->staff_contract_id.empty())out.staff_contract_ref=SourceRef{private_shop_module,"private_shop_staff_accepted",o->staff_contract_id};
 out.purpose_ref=capability.purpose_ref;out.actual_span=span;out.condition_source_refs=take(private_shop_capability_condition_sources(x.body,capability));
 out.capability_rule_id=capability.rule_ref.id;out.capability_rule_version=capability.rule_ref.version;
 out.allowed=capability.allowed;out.efficiency_bp=capability.efficiency_bp;
 return out;
}
ShopOperationCapabilityBinding frozen(const PrivateShopOperationSourceContext&x,const PrivateShopLaborAppliedSource&labor,
 const MomentSpan&span,const CharacterCapabilityView&capability){
 auto out=frozen_base(x,labor.operation_ref,span,capability);
 out.segment_source_ref=PrivateShopActualLaborSourceRef{private_shop_labor_applied_source(labor.id)};
 out.activity_evidence=PrivateShopServiceApplied{labor.work_period_ref,private_shop_labor_applied_source(labor.id)};return out;
}
bool labor_address(const PrivateShopCapabilitySegmentSource&address,const SourceRef&source){
 const auto*labor=std::get_if<PrivateShopActualLaborSourceRef>(&address);return labor&&same_source_ref(labor->source_ref,source);
}
bool segment_address(const PrivateShopCapabilitySegmentSource&a,const PrivateShopCapabilitySegmentSource&b){
 if(a.index()!=b.index())return false;
 if(const auto*labor=std::get_if<PrivateShopActualLaborSourceRef>(&a))return labor_address(b,labor->source_ref);
 const auto&left=std::get<FixedServiceTransitionRef>(a);const auto&right=std::get<FixedServiceTransitionRef>(b);return left.transition_id==right.transition_id&&left.source_schema==right.source_schema;
}
bool same(const ShopOperationCapabilityBinding&a,const ShopOperationCapabilityBinding&b){
 if(a.operation_ref!=b.operation_ref||!segment_address(a.segment_source_ref,b.segment_source_ref)||a.actor_ref!=b.actor_ref||a.role!=b.role||
  bool(a.staff_contract_ref)!=bool(b.staff_contract_ref)||(a.staff_contract_ref&&!same_source_ref(*a.staff_contract_ref,*b.staff_contract_ref))||
  !rule(a.purpose_ref,b.purpose_ref)||cmp(a.actual_span.from,b.actual_span.from)||cmp(a.actual_span.to,b.actual_span.to)||
  !sources(a.condition_source_refs,b.condition_source_refs)||a.capability_rule_id!=b.capability_rule_id||
  a.capability_rule_version!=b.capability_rule_version||a.allowed!=b.allowed||a.efficiency_bp!=b.efficiency_bp||
  a.activity_evidence.index()!=b.activity_evidence.index())return false;
 const auto*left=std::get_if<PrivateShopServiceApplied>(&a.activity_evidence);const auto*right=std::get_if<PrivateShopServiceApplied>(&b.activity_evidence);
 if(left||right)return left&&right&&take(same_work_period_ref2(left->work_period_ref,right->work_period_ref))&&same_source_ref(left->original_labor_receipt_ref,right->original_labor_receipt_ref);
 const auto&ls=std::get<PrivateShopParticipationStopped>(a.activity_evidence).original_operation_pause_or_ineligibility_result_ref;
 const auto&rs=std::get<PrivateShopParticipationStopped>(b.activity_evidence).original_operation_pause_or_ineligibility_result_ref;
 return ls.transition_id==rs.transition_id&&ls.source_schema==rs.source_schema;
}
class ExpectedSpans final:public PrivateShopLaborSpanVisitor {
 const PrivateShopOperationSourceContext&x_;const PrivateShopLaborAppliedSource&labor_;
public:std::vector<ShopOperationCapabilityBinding>values;
 ExpectedSpans(const PrivateShopOperationSourceContext&x,const PrivateShopLaborAppliedSource&labor):x_(x),labor_(labor){}
 Status applied_span(const MomentSpan&span,const CharacterCapabilityView&capability,const ActorStationaryLocationSpan&)override{
  try{values.push_back(frozen(x_,labor_,span,capability));return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}
 }
};
std::vector<ShopOperationCapabilityBinding> expected(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&source){
 const auto&labor=*take(read_original_private_shop_labor(x,source));take(read_original_private_shop_operation_profile(x,labor.operation_ref));
 ExpectedSpans out(x,labor);take(visit_original_private_shop_labor_spans(x,source,out));if(out.values.empty())fail("PrivateShopActualCapabilitySpansRequired",labor.id);
 return std::move(out.values);
}
// The actual transition owns a stopped boundary. Only an actual later resume
// or terminal source can close its positive denied span; current time alone is
// never a closing event and no future period proof is manufactured.
const FixedServiceTransition* stopped_closing(const PrivateShopOperationSourceContext&x,const FixedServiceTransition&t){
 const FixedServiceTransition*closed=nullptr;
 for(const auto&row:x.elapsed.fixed_service_transitions)if(row.action_ref==t.action_ref&&row.transition_ordinal>t.transition_ordinal&&
   (row.kind==FixedTransitionKind::resumed||row.kind==FixedTransitionKind::terminal)){
  if(!closed||row.transition_ordinal<closed->transition_ordinal)closed=&row;
 }
 return closed;
}
CharacterCapabilityView eligibility(const PrivateShopOperationSourceContext&x,const FixedServiceTransition&t,const StableId&actor){
 const auto*cause=std::get_if<FixedEligibilityChanged>(&t.cause);
 if(!cause||cause->original_owner_execution_ref!=t.action_ref||cause->original_cause_schema!=2||
  cause->original_cause_ref.domain!="actor_foundation"||cause->original_cause_ref.kind!="condition_receipt")fail("PrivateShopOriginalConditionTransitionRequired",t.transition_id);
 const auto source=take(read_character_condition_source(x.body,cause->original_cause_ref,cause->original_cause_schema));
 if(source.actor_id!=actor||cmp(source.occurred_at,t.occurred_at))fail("PrivateShopConditionTransitionMomentMismatch",t.transition_id);
 const auto condition=take(read_character_condition_at_source(x.body,cause->original_cause_ref,cause->original_cause_schema,t.occurred_at));
 auto capability=take(read_character_capability_at_head(x.body,condition.head,purpose(x),std::nullopt,t.occurred_at));
 if(capability.actor_id!=actor||!x.body.catalog.normal_work_efficiency_rule||
  !rule(capability.rule_ref,x.body.catalog.normal_work_efficiency_rule->rule_ref)||cause->transition_rule_ref!=capability.rule_ref.id)
  fail("PrivateShopOriginalEligibilityRuleMismatch",t.transition_id);
 return capability;
}
struct ExpectedStopped {const FixedServiceTransition*closing=nullptr;std::vector<ShopOperationCapabilityBinding>rows;};
ExpectedStopped expected_stopped(const PrivateShopOperationSourceContext&x,const FixedServiceTransitionRef&ref){
 const auto&t=*take(read_fixed_service_transition_source(x.elapsed,ref));
 const auto*entry=find_fixed_elapsed(x.elapsed,t.action_ref);if(!entry)fail("PrivateShopOriginalElapsedRequired",t.action_ref);
 // The original selected-action reader validates this actual chain, without
 // imposing stable-world phase restrictions on unrelated candidate actions.
 take(read_fixed_service_intervals(x.elapsed,t.action_ref,entry->started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.spatial.budget));
 if(t.kind!=FixedTransitionKind::suspended||t.execution_source_schema!=2||
  !same_source_ref(t.execution_source_ref,private_shop_operation_started_source(t.action_ref)))fail("PrivateShopOriginalSuspendedTransitionRequired",ref.transition_id);
 const auto&a=*take(read_original_private_shop_operation_acceptance(x,{t.execution_source_ref,2}));
 const auto*o=x.shops.state.operations.find(t.action_ref);if(!o||cmp(t.occurred_at,a.accepted_at)<0)fail("PrivateShopStoppedOperationMismatch",t.action_ref);
 take(read_original_private_shop_operation_profile(x,o->id));
 const auto denied=eligibility(x,t,o->actor_id);if(denied.allowed||denied.efficiency_bp!=0)fail("PrivateShopActualDeniedParticipationRequired",t.transition_id);
 const auto*closed=stopped_closing(x,t);if(!closed)return {};
 if(closed->execution_source_schema!=2||!same_source_ref(closed->execution_source_ref,t.execution_source_ref)||
  cmp(closed->occurred_at,t.occurred_at)<0||cmp(closed->occurred_at,take(x.clock.read_current_moment()))>0)fail("PrivateShopStoppedClosingMismatch",t.transition_id);
 if(closed->kind==FixedTransitionKind::resumed){
  const auto resumed=eligibility(x,*closed,o->actor_id);if(!resumed.allowed||resumed.efficiency_bp<=0)fail("PrivateShopActualParticipationRecoveryRequired",closed->transition_id);
 }else{
  const auto*cause=std::get_if<FixedOwnerTerminated>(&closed->cause);if(!cause)fail("PrivateShopOriginalTerminalTransitionCauseRequired",closed->transition_id);
  const auto&terminal=*take(read_original_private_shop_operation_terminal(x,{cause->owner_result_ref,cause->owner_result_schema}));
  if(terminal.operation_ref!=o->id||cmp(terminal.occurred_at,closed->occurred_at))fail("PrivateShopStoppedTerminalMismatch",closed->transition_id);
 }
 ExpectedStopped result;result.closing=closed;if(cmp(t.occurred_at,closed->occurred_at)==0)return result;
 class DeniedSpans final:public CharacterCapabilitySpanVisitor {
  const PrivateShopOperationSourceContext&x_;const FixedServiceTransition&t_;std::vector<ShopOperationCapabilityBinding>&out_;
 public:DeniedSpans(const PrivateShopOperationSourceContext&x,const FixedServiceTransition&t,std::vector<ShopOperationCapabilityBinding>&out):x_(x),t_(t),out_(out){}
  Status visit_capability_span(const MomentSpan&span,const CharacterCapabilityView&capability)override{try{
   if(capability.allowed||capability.efficiency_bp!=0)fail("PrivateShopStoppedSpanContainsParticipation",t_.transition_id);
   auto row=frozen_base(x_,t_.action_ref,span,capability);row.segment_source_ref=FixedServiceTransitionRef{t_.transition_id,t_.source_schema};
   row.activity_evidence=PrivateShopParticipationStopped{{t_.transition_id,t_.source_schema}};out_.push_back(std::move(row));return common_success();
  }catch(Failure&e){return {false,{},std::move(e.error)};}}
 }visitor(x,t,result.rows);
 take(visit_character_capability_span(x.body,denied.condition_head,purpose(x),std::nullopt,{t.occurred_at,closed->occurred_at},visitor));
 return result;
}
}
Outcome<std::vector<MomentOwnerSource>> private_shop_capability_condition_sources(const CharacterConditionSources&body,const CharacterCapabilityView&capability){
 try{
  std::vector<MomentOwnerSource>out;
  auto append=[&](MomentOwnerSource source){for(const auto&old:out)if(old.source_schema==source.source_schema&&same_source_ref(old.source_ref,source.source_ref))return;out.push_back(std::move(source));};
  const auto head=take(read_character_condition_head_source(body,capability.condition_head,capability.observed_at));append({head.source,head.source_schema});
  for(const auto&ref:capability.limiting_condition_refs){
   if(const auto*seed=std::get_if<InitialConditionSeedRef>(&ref))append({initial_condition_seed_source(*seed),take(read_initial_condition_seed_schema(body.catalog))});
   else append({character_condition_receipt_source(std::get<ConditionReceiptRefV2>(ref)),2});
  }
  for(const auto&ref:out){const auto source=take(read_character_condition_source(body,ref.source_ref,ref.source_schema));if(source.actor_id!=capability.actor_id||cmp(source.occurred_at,capability.observed_at)>0)fail("PrivateShopCapabilityConditionSourceMismatch",capability.actor_id);}
  return {true,std::move(out),{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Outcome<const PrivateShopOperationProfile*> read_original_private_shop_operation_profile(const PrivateShopOperationSourceContext&x,const StableId&operation){
 try{
  if(x.capabilities.schema_version!=1||x.shops.exact_operations!=&x)fail("PrivateShopCapabilitySelectionRequired",operation);
  const PrivateShopOperationProfile*found=nullptr;for(const auto&row:x.capabilities.operation_profiles)if(row.operation_ref==operation){if(found)fail("DuplicatePrivateShopOperationProfile",operation);found=&row;}
  if(!found)fail("PrivateShopActualOperationProfileRequired",operation);
  if(found->operation_profile==PrivateShopOperationProfileKind::legacy_attendance){
   if(found->accepted_purpose_ref||!found->acceptance_condition_source_refs.empty()||found->migration_source||!x.shops.state.operations.find(operation))fail("PrivateShopLegacyProfileProofMismatch",operation);
   for(const auto&accepted:x.sources.accepted_operations)if(accepted.operation_ref==operation)fail("PrivateShopLegacyOperationRetagged",operation);
   return {true,found,{}};
  }
  if(found->operation_profile!=PrivateShopOperationProfileKind::purpose_capability1||!found->accepted_purpose_ref||
   !rule(*found->accepted_purpose_ref,purpose(x))||found->acceptance_condition_source_refs.empty())fail("PrivateShopPurposeProfileRequired",operation);
  // No historical integer attendance is silently converted to a new proof.
  // A selected migration needs its actual original migration producer.
  if(found->migration_source)fail("PrivateShopOriginalMigrationSourceRequired",operation);
  const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(operation),2}));
  const auto&o=*x.shops.state.operations.find(operation);const auto&head=found->acceptance_condition_source_refs.front();
  const auto condition=take(read_character_condition_at_source(x.body,head.source_ref,head.source_schema,a.accepted_at));
  if(condition.head.actor_id!=o.actor_id)fail("PrivateShopAcceptanceConditionActorMismatch",operation);
  const auto capability=take(read_character_capability_at_head(x.body,condition.head,*found->accepted_purpose_ref,std::nullopt,a.accepted_at));
  if(!capability.allowed||!sources(found->acceptance_condition_source_refs,take(private_shop_capability_condition_sources(x.body,capability))))fail("PrivateShopAcceptanceCapabilityMismatch",operation);
  return {true,found,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status adopt_private_shop_labor_capabilities_in_candidate(PrivateShopOperationCapabilitiesState&state,const PrivateShopOperationSourceContext&x,const MomentOwnerSource&source){
 try{
  if(&state!=&x.capabilities)fail("PrivateShopCapabilityCandidateMismatch",source.source_ref.id);
  const auto&labor=*take(read_original_private_shop_labor(x,source));
  if(cmp(labor.occurred_at,take(x.clock.read_current_moment()))||
   !x.shops.common.action(labor.operation_ref))fail("PrivateShopActualCapabilityAdoptionBoundaryRequired",labor.id);
  for(const auto&row:state.period_capability_bindings)if(labor_address(row.segment_source_ref,source.source_ref))fail("PrivateShopLaborCapabilityAlreadyAdopted",labor.id);
  auto rows=expected(x,source);
  for(const auto&row:rows)for(const auto&old:state.period_capability_bindings)if(old.operation_ref==row.operation_ref&&cmp(old.actual_span.from,row.actual_span.to)<0&&cmp(row.actual_span.from,old.actual_span.to)<0)fail("OverlappingPrivateShopCapabilityBindings",row.operation_ref);
  for(auto&row:rows)state.period_capability_bindings.push_back(std::move(row));
  return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status adopt_private_shop_stopped_capabilities_in_candidate(PrivateShopOperationCapabilitiesState&state,const PrivateShopOperationSourceContext&x,const FixedServiceTransitionRef&ref){
 try{
  if(&state!=&x.capabilities)fail("PrivateShopCapabilityCandidateMismatch",ref.transition_id);
  auto original=expected_stopped(x,ref);
  if(!original.closing||cmp(original.closing->occurred_at,take(x.clock.read_current_moment())))fail("PrivateShopActualStoppedClosingBoundaryRequired",ref.transition_id);
  for(const auto&row:state.period_capability_bindings)if(const auto*address=std::get_if<FixedServiceTransitionRef>(&row.segment_source_ref))if(address->transition_id==ref.transition_id)fail("PrivateShopStoppedCapabilityAlreadyAdopted",ref.transition_id);
  for(const auto&row:original.rows)for(const auto&old:state.period_capability_bindings)if(old.operation_ref==row.operation_ref&&cmp(old.actual_span.from,row.actual_span.to)<0&&cmp(row.actual_span.from,old.actual_span.to)<0)fail("OverlappingPrivateShopCapabilityBindings",row.operation_ref);
  for(auto&row:original.rows)state.period_capability_bindings.push_back(std::move(row));return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status validate_private_shop_operation_capabilities(const PrivateShopOperationSourceContext&x){
 try{
  if(x.capabilities.operation_profiles.size()!=x.shops.state.operations.size())fail("PrivateShopProfileCoverageMismatch");
  for(const auto&row:x.capabilities.operation_profiles)take(read_original_private_shop_operation_profile(x,row.operation_ref));
  std::size_t matched=0;
  for(const auto&labor:x.sources.labor_applied_sources){
   const auto original=expected(x,{private_shop_labor_applied_source(labor.id),1});
   std::size_t index=0;for(const auto&row:x.capabilities.period_capability_bindings)if(labor_address(row.segment_source_ref,private_shop_labor_applied_source(labor.id))){
    if(index>=original.size()||!same(row,original[index++]))fail("PrivateShopAppliedCapabilitySpanMismatch",labor.id);++matched;
   }
   if(index!=original.size())fail("PrivateShopAppliedCapabilityCoverageMismatch",labor.id);
  }
  for(const auto&t:x.elapsed.fixed_service_transitions)if(t.kind==FixedTransitionKind::suspended&&t.execution_source_schema==2&&
   same_source_ref(t.execution_source_ref,private_shop_operation_started_source(t.action_ref))){
   const auto original=expected_stopped(x,{t.transition_id,t.source_schema});std::size_t index=0;
   for(const auto&row:x.capabilities.period_capability_bindings)if(const auto*address=std::get_if<FixedServiceTransitionRef>(&row.segment_source_ref))if(address->transition_id==t.transition_id){
    if(index>=original.rows.size()||!same(row,original.rows[index++]))fail("PrivateShopStoppedCapabilitySpanMismatch",t.transition_id);++matched;
   }
   if(index!=original.rows.size())fail("PrivateShopStoppedCapabilityCoverageMismatch",t.transition_id);
  }
  if(matched!=x.capabilities.period_capability_bindings.size())fail("PrivateShopCapabilityOriginalSourceCoverageMismatch");
  for(std::size_t n=0;n<x.capabilities.period_capability_bindings.size();++n)for(std::size_t k=0;k<n;++k){
   const auto&a=x.capabilities.period_capability_bindings[n];const auto&b=x.capabilities.period_capability_bindings[k];
   if(a.operation_ref==b.operation_ref&&cmp(a.actual_span.from,b.actual_span.to)<0&&cmp(b.actual_span.from,a.actual_span.to)<0)fail("OverlappingPrivateShopCapabilityBindings",a.operation_ref);
  }
  return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status visit_private_shop_capability_binding_pins(const PrivateShopOperationSourceContext&x,const ShopOperationCapabilityBinding&binding,PrivateShopOperationSourcePinSink&sink){
 try{
  take(validate_private_shop_operation_capabilities(x));
  bool owned=false;for(const auto&row:x.capabilities.period_capability_bindings)if(&row==&binding)owned=true;
  if(!owned)fail("PrivateShopOwnedCapabilityBindingRequired",binding.operation_ref);
  if(const auto*service=std::get_if<PrivateShopServiceApplied>(&binding.activity_evidence))take(sink.source({service->original_labor_receipt_ref,1}));
  else{
   const auto&ref=std::get<PrivateShopParticipationStopped>(binding.activity_evidence).original_operation_pause_or_ineligibility_result_ref;
   const auto original=expected_stopped(x,ref);if(!original.closing)fail("PrivateShopActualStoppedClosingRequired",ref.transition_id);
   take(sink.fixed_transition(ref.transition_id));take(sink.fixed_transition(original.closing->transition_id));
  }
  take(sink.source({private_shop_operation_started_source(binding.operation_ref),2}));
  if(binding.staff_contract_ref)take(sink.source({*binding.staff_contract_ref,1}));
  for(const auto&source:binding.condition_source_refs)take(sink.source(source));
  take(sink.locked_rule(binding.purpose_ref));
  take(sink.locked_rule({binding.capability_rule_id,binding.capability_rule_version,binding.purpose_ref.catalog_digest}));
  return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}
}
}
