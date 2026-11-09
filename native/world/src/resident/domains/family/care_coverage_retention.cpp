#include "care_coverage_retention.hpp"
namespace hanmo::resident {namespace {
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SourcePin& a,const SourcePin& b){return a.consumer.domain==b.consumer.domain&&a.consumer.kind==b.consumer.kind&&a.consumer.id==b.consumer.id&&a.consumer.slot==b.consumer.slot&&same(a.source,b.source)&&a.reason==b.reason;}
}
RequiredSourceConsumerSlot care_coverage_consumer_slot(CareCoveragePinSlot slot){switch(slot){
 case CareCoveragePinSlot::binding_terms:return {"care_requirements","care_binding","terms_ref"};
 case CareCoveragePinSlot::binding_acceptance:return {"care_requirements","care_binding","accepted_ref"};
 case CareCoveragePinSlot::binding_activity:return {"care_requirements","care_binding","care_activity_ref"};
 case CareCoveragePinSlot::binding_placement:return {"care_requirements","care_binding","placement_ref"};
 case CareCoveragePinSlot::satisfaction_result:return {"care_requirements","care_coverage_satisfied","handover_result_ref"};
 case CareCoveragePinSlot::loss_cause:return {"care_requirements","care_coverage_lost","cause_ref"};
 case CareCoveragePinSlot::completion_period:return {"care_requirements","care_temporal_event","completion_work_period_ref"};
 case CareCoveragePinSlot::completion_participation:return {"care_requirements","care_temporal_event","actual_participation_refs"};
 case CareCoveragePinSlot::completion_placement:return {"care_requirements","care_temporal_event","placement_result_ref"};
 case CareCoveragePinSlot::completion_fees:return {"care_requirements","care_temporal_event","fee_result_refs"};
 }return {};
}
Status visit_care_coverage_source_pins(const CareCoverageSources& x,CareCoveragePinSlot slot,CareCoveragePinSink& sink){
 auto checked=validate_care_coverage_source_closure(x);if(!checked.ok)return checked;checked=validate_care_requirement_goal_sources(x.handovers.acceptance.goals);if(!checked.ok)return checked;
 const auto spec=care_coverage_consumer_slot(slot);if(spec.domain.empty())return common_failure("UnknownCareCoverageConsumerSlot",{},ErrorKind::unsupported_capability);
 auto emit=[&](const StableId& id,const SourceRef& source,SourcePinReason reason){return sink.actual_pin({{spec.domain,spec.kind,id,spec.slot},source,reason});};
 if(slot==CareCoveragePinSlot::binding_terms||slot==CareCoveragePinSlot::binding_acceptance||slot==CareCoveragePinSlot::binding_activity||slot==CareCoveragePinSlot::binding_placement){
  for(const auto& pair:x.tables.bindings.rows()){const auto& row=pair.second;
   if(slot==CareCoveragePinSlot::binding_terms||slot==CareCoveragePinSlot::binding_acceptance)checked=emit(pair.first,care_acceptance_source(slot==CareCoveragePinSlot::binding_terms?row.terms_ref:row.accepted_ref),SourcePinReason::dependent_state);
   else checked=emit(pair.first,slot==CareCoveragePinSlot::binding_activity?row.care_activity_ref.source_ref:row.placement_ref.source_ref,SourcePinReason::dependent_state);
   if(!checked.ok)return checked;
  }
 }else if(slot==CareCoveragePinSlot::satisfaction_result){for(const auto& pair:x.tables.satisfied.rows()){checked=emit(pair.first,pair.second.handover_result_ref,SourcePinReason::unique_outcome);if(!checked.ok)return checked;}}
 else if(slot==CareCoveragePinSlot::loss_cause){for(const auto& pair:x.tables.lost.rows()){checked=emit(pair.first,pair.second.cause_ref,SourcePinReason::unique_outcome);if(!checked.ok)return checked;}}
 else for(const auto& pair:x.handovers.acceptance.goals.goals.temporal_events.rows())if(const auto* completed=std::get_if<CareHandoverCompletedEvent>(&pair.second.body)){
  if(slot==CareCoveragePinSlot::completion_period)checked=emit(pair.first,completed->completion_work_period_ref.source_ref,SourcePinReason::unique_outcome);
  else if(slot==CareCoveragePinSlot::completion_placement)checked=emit(pair.first,completed->placement_result_ref.source_ref,SourcePinReason::unique_outcome);
  else {const auto& refs=slot==CareCoveragePinSlot::completion_participation?completed->actual_participation_refs:completed->fee_result_refs;for(const auto& ref:refs){checked=emit(pair.first,ref.source_ref,SourcePinReason::unique_outcome);if(!checked.ok)return checked;}}
  if(!checked.ok)return checked;
 }
 return common_success();
}
Status validate_care_coverage_source_pin(const CareCoverageSources& x,CareCoveragePinSlot slot,const SourcePin& pin){
 struct Find final:CareCoveragePinSink {const SourcePin& expected;std::size_t count=0;explicit Find(const SourcePin& p):expected(p){}Status actual_pin(const SourcePin& p)override{if(same(expected,p))++count;return common_success();}} found(pin);
 auto status=visit_care_coverage_source_pins(x,slot,found);if(!status.ok)return status;
 return found.count==1?common_success():common_failure("CareCoveragePinNotActual",pin.consumer.id,ErrorKind::internal);
}
Outcome<bool> care_coverage_source_owner_may_release(const CareCoverageSources& x,const SourceRef& ref){
 auto schema=care_coverage_source_schema(ref);if(!schema.ok)return {false,false,schema.error};
 auto checked=validate_care_coverage_source_closure(x);if(!checked.ok)return {false,false,checked.error};checked=validate_care_requirement_goal_sources(x.handovers.acceptance.goals);if(!checked.ok)return {false,false,checked.error};
 if(ref.kind=="care_coverage_satisfied"){if(!x.tables.satisfied.find(ref.id))return {false,false,common_failure("CareSatisfactionMissing",ref.id,ErrorKind::internal).error};}
 else if(!x.tables.lost.find(ref.id))return {false,false,common_failure("CareLossMissing",ref.id,ErrorKind::internal).error};
 // The actual retained goal transition owns the outcome. Current and historical
 // chains remain pinned until the real owner can release those dependants.
 for(const auto& pair:x.handovers.acceptance.goals.goals.transitions.rows())if(same(pair.second.cause_ref,ref))return {true,false,{}};
 return {true,true,{}};
}
}
