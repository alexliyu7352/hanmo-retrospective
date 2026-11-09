#include "care_requirement_retention.hpp"
namespace hanmo::resident {namespace {
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SourcePin& a,const SourcePin& b){return a.consumer.domain==b.consumer.domain&&a.consumer.kind==b.consumer.kind&&a.consumer.id==b.consumer.id&&a.consumer.slot==b.consumer.slot&&same(a.source,b.source)&&a.reason==b.reason;}
}
RequiredSourceConsumerSlot care_goal_consumer_slot(CareGoalConsumerSlot slot){switch(slot){
 case CareGoalConsumerSlot::goal_basis:return {"care_requirements","care_requirement","care_basis"};
 case CareGoalConsumerSlot::current_transition:return {"care_requirements","care_requirement","current_transition_ref"};
 case CareGoalConsumerSlot::transition_predecessor:return {"care_requirements","care_requirement_transition","previous_transition_ref"};
 case CareGoalConsumerSlot::transition_cause:return {"care_requirements","care_requirement_transition","cause_ref"};
 case CareGoalConsumerSlot::created_schedule:return {"care_requirements","care_temporal_event","birth_due_origin"};
 }return {};
}
Status visit_care_goal_source_pins(const CareGoalSourceContext& x,CareGoalConsumerSlot slot,CareGoalPinSink& sink){
 auto valid=validate_care_requirement_goal_sources(x);if(!valid.ok)return valid;const auto spec=care_goal_consumer_slot(slot);if(spec.domain.empty())return common_failure("UnknownCareGoalConsumerSlot",{},ErrorKind::unsupported_capability);
 auto emit=[&](const StableId& id,const SourceRef& source,SourcePinReason reason){return sink.actual_pin({{spec.domain,spec.kind,id,spec.slot},source,reason});};
 if(slot==CareGoalConsumerSlot::goal_basis||slot==CareGoalConsumerSlot::current_transition){for(const auto& row:x.goals.requirements.rows()){
  SourceRef source;if(slot==CareGoalConsumerSlot::current_transition)source=row.second.current_transition_ref;else {const auto* birth=std::get_if<BirthFactRef>(&row.second.care_basis);if(!birth)return common_failure("DependentCareApplicabilitySourceRequired",row.first,ErrorKind::unsupported_capability);source={"family_birth","family_birth_recorded",birth->birth_fact_id};}
  valid=emit(row.first,source,SourcePinReason::dependent_state);if(!valid.ok)return valid;
 }}else if(slot==CareGoalConsumerSlot::transition_predecessor||slot==CareGoalConsumerSlot::transition_cause){for(const auto& row:x.goals.transitions.rows()){
  if(slot==CareGoalConsumerSlot::transition_predecessor&&!row.second.previous_transition_ref)continue;valid=emit(row.first,slot==CareGoalConsumerSlot::transition_cause?row.second.cause_ref:*row.second.previous_transition_ref,SourcePinReason::unique_outcome);if(!valid.ok)return valid;
 }}else if(slot==CareGoalConsumerSlot::created_schedule){for(const auto& row:x.goals.temporal_events.rows())if(const auto* created=std::get_if<CareRequirementCreatedEvent>(&row.second.body)){
  const auto* birth=std::get_if<BirthDueCareOrigin>(&created->origin);if(!birth)return common_failure("DependentCareApplicabilitySourceRequired",row.first,ErrorKind::unsupported_capability);valid=emit(row.first,birth->scheduled_source_ref,SourcePinReason::unique_outcome);if(!valid.ok)return valid;
 }}return common_success();
}
Status validate_care_goal_source_pin(const CareGoalSourceContext& x,CareGoalConsumerSlot slot,const SourcePin& pin){struct Find final:CareGoalPinSink{const SourcePin& pin;std::size_t count=0;explicit Find(const SourcePin& p):pin(p){}Status actual_pin(const SourcePin& candidate)override{if(same(pin,candidate))++count;return common_success();}}find(pin);auto status=visit_care_goal_source_pins(x,slot,find);if(!status.ok)return status;return find.count==1?common_success():common_failure("CareGoalPinNotActual",pin.consumer.id,ErrorKind::internal);}
Outcome<bool> care_goal_transition_owner_may_release(const CareGoalSourceContext& x,const SourceRef& source){
 auto actual=read_care_goal_transition_source(x,source);if(!actual.ok)return {false,false,actual.error};
 // Actual current-head and predecessor references, not a writable refcount.
 // While its original requirement retains this chain, each row is pinned.
 for(const auto& row:x.goals.requirements.rows())if(same(row.second.current_transition_ref,source))return {true,false,{}};
 for(const auto& row:x.goals.transitions.rows())if(row.second.previous_transition_ref&&same(*row.second.previous_transition_ref,source))return {true,false,{}};
 return {true,true,{}};
}
}
