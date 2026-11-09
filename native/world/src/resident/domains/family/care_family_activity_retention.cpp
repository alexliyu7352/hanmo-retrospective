#include "care_family_activity_retention.hpp"
#include "care_personal_stop.hpp"
#include "profile_control_source.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
namespace hanmo::resident {namespace {
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SourcePin& a,const SourcePin& b){return a.consumer.domain==b.consumer.domain&&a.consumer.kind==b.consumer.kind&&a.consumer.id==b.consumer.id&&a.consumer.slot==b.consumer.slot&&same(a.source,b.source)&&a.reason==b.reason;}
Status closure(const CareFamilyActivitySources& x){
 if(x.coverage.originals.family_activity_sources()!=&x)
  return common_failure("CareActivitySameSourceOwnerRequired",{},ErrorKind::internal);
 auto checked=validate_care_family_activity_sources(x);if(!checked.ok)return checked;
 checked=validate_care_coverage_source_closure(x.coverage);if(!checked.ok)return checked;
 return validate_care_requirement_goal_sources(x.coverage.handovers.acceptance.goals);
}
}
RequiredSourceConsumerSlot care_family_activity_consumer_slot(CareFamilyActivityPinSlot slot){switch(slot){
 case CareFamilyActivityPinSlot::accepted_terms:return {"care_requirements","care_family_activity","accepted_care_terms_ref"};
 case CareFamilyActivityPinSlot::actual_handover:return {"care_requirements","care_family_activity","actual_handover_execution_ref"};
 case CareFamilyActivityPinSlot::actual_work_period:return {"care_requirements","care_family_activity","actual_start_work_period_ref"};
 case CareFamilyActivityPinSlot::actual_placement:return {"care_requirements","care_family_activity","actual_placement_result_ref"};
 case CareFamilyActivityPinSlot::adopted_participation:return {"care_requirements","care_family_activity","adopted_participation_refs"};
 case CareFamilyActivityPinSlot::actual_end:return {"care_requirements","care_family_activity","ended_by"};
 case CareFamilyActivityPinSlot::end_cause:return {"care_requirements","care_family_activity_end","cause"};
 case CareFamilyActivityPinSlot::end_condition_receipt:return {"care_requirements","care_family_activity_end","condition_receipt_ref"};
 case CareFamilyActivityPinSlot::stop_accepted_terms:return {"care_requirements","care_stopped","accepted_terms_ref"};
 case CareFamilyActivityPinSlot::stop_player_control:return {"care_requirements","care_stopped","original_control_source_ref"};
 case CareFamilyActivityPinSlot::stop_condition_seed:return {"care_requirements","care_stopped","actual_condition_seed"};
 case CareFamilyActivityPinSlot::stop_condition_receipt:return {"care_requirements","care_stopped","actual_condition_before"};
 }return {};
}
Status visit_care_family_activity_source_pins(const CareFamilyActivitySources& x,
 CareFamilyActivityPinSlot slot,CareFamilyActivityPinSink& sink){
 const auto spec=care_family_activity_consumer_slot(slot);
 if(spec.domain.empty())return common_failure("UnknownCareFamilyActivityConsumerSlot",{},ErrorKind::unsupported_capability);
 auto checked=closure(x);if(!checked.ok)return checked;
 auto emit=[&](const StableId& id,const SourceRef& source,SourcePinReason reason=SourcePinReason::dependent_state){
  return sink.actual_pin({{spec.domain,spec.kind,id,spec.slot},source,reason});
 };
 for(const auto& pair:x.activities.rows()){
  const auto& a=pair.second;
  switch(slot){
   case CareFamilyActivityPinSlot::accepted_terms:checked=emit(pair.first,care_acceptance_source(a.accepted_care_terms_ref));break;
   case CareFamilyActivityPinSlot::actual_handover:checked=emit(pair.first,a.actual_handover_execution_ref.source_ref);break;
   case CareFamilyActivityPinSlot::actual_work_period:checked=emit(pair.first,a.actual_start_work_period_ref.source_ref);break;
   case CareFamilyActivityPinSlot::actual_placement:checked=emit(pair.first,a.actual_placement_result_ref.source_ref);break;
   case CareFamilyActivityPinSlot::adopted_participation:
    for(const auto& ref:a.adopted_participation_refs){checked=emit(pair.first,ref.source_ref);if(!checked.ok)return checked;}break;
   case CareFamilyActivityPinSlot::actual_end:
    if(a.ended_by)checked=emit(pair.first,care_family_activity_ended_source(a.ended_by->end_id),SourcePinReason::unique_outcome);break;
   case CareFamilyActivityPinSlot::end_condition_receipt:
    if(a.ended_by)if(const auto* loss=std::get_if<CareLossCauseV2>(&a.ended_by->cause))
     if(const auto* capacity=std::get_if<CareRequiredCapacityLost>(loss))
      checked=emit(a.ended_by->end_id,character_condition_receipt_source(capacity->condition_receipt_ref),SourcePinReason::unique_outcome);
    break;
   case CareFamilyActivityPinSlot::stop_accepted_terms:
    if(a.ended_by)if(const auto* stop=std::get_if<CarePersonalStop>(&a.ended_by->cause))checked=emit(stop->actual_stop_result.source_ref.id,care_acceptance_source(stop->actual_command.accepted_terms_ref));
    break;
   case CareFamilyActivityPinSlot::stop_player_control:
    if(a.ended_by)if(const auto* stop=std::get_if<CarePersonalStop>(&a.ended_by->cause))if(const auto* player=std::get_if<LifePlayerActorRoute>(&stop->actual_command.route_basis))if(const auto* ref=std::get_if<SourceRef>(&player->original_control_source_ref)){
     const auto& accepted=x.coverage.handovers.acceptance;auto actual=read_profile_controller_actual_source(accepted.controls,accepted.body.catalog.identity,*ref);if(!actual.ok)return {false,{},actual.error};checked=emit(stop->actual_stop_result.source_ref.id,*ref);
    }
    break;
   case CareFamilyActivityPinSlot::stop_condition_seed:
   case CareFamilyActivityPinSlot::stop_condition_receipt:
    if(a.ended_by)if(const auto* stop=std::get_if<CarePersonalStop>(&a.ended_by->cause)){
     auto actual=read_care_personal_stop_source(x,stop->actual_stop_result);if(!actual.ok)return {false,{},actual.error};
     if(slot==CareFamilyActivityPinSlot::stop_condition_seed){if(!actual.value.actual_condition_seed)return common_failure("CarePersonalStopConditionSeedRequired",pair.first,ErrorKind::internal);checked=emit(stop->actual_stop_result.source_ref.id,initial_condition_seed_source({actual.value.actual_condition_seed->seed_key}));}
     else if(actual.value.actual_condition_before)checked=emit(stop->actual_stop_result.source_ref.id,character_condition_receipt_source({actual.value.actual_condition_before->result_id}));
    }
    break;
   case CareFamilyActivityPinSlot::end_cause:
    if(a.ended_by)checked=std::visit([&](const auto& cause)->Status{
     using T=std::decay_t<decltype(cause)>;
     auto actual=[&](const SourceRef& ref){return emit(a.ended_by->end_id,ref,SourcePinReason::unique_outcome);};
     if constexpr(std::is_same_v<T,CareSubjectDeath>)return actual({cause.death_ref.domain,cause.death_ref.kind,cause.death_ref.id});
     else if constexpr(std::is_same_v<T,CarePersonalStop>)return actual(cause.actual_stop_result.source_ref);
     else return std::visit([&](const auto& loss)->Status{using L=std::decay_t<decltype(loss)>;
      if constexpr(std::is_same_v<L,CareHolderDeath>)return actual({loss.death_ref.domain,loss.death_ref.kind,loss.death_ref.id});
      else if constexpr(std::is_same_v<L,CareRequiredCapacityLost>)return actual(loss.capacity_transition_ref);
      else if constexpr(std::is_same_v<L,CareActualSeparation>)return actual(loss.actual_location_or_binding_transition.source_ref);
      else if constexpr(std::is_same_v<L,CareAcceptedWindowEnded>)return actual(care_acceptance_source(loss.accepted_care_terms_ref));
      else if constexpr(std::is_same_v<L,CareParticipationConflict>)return actual(loss.actual_occupation_change.source_ref);
      else return actual(loss.actual_ended_result.source_ref);
     },cause);
    },a.ended_by->cause);break;
  }
  if(!checked.ok)return checked;
 }
 return common_success();
}
Status validate_care_family_activity_source_pin(const CareFamilyActivitySources& x,
 CareFamilyActivityPinSlot slot,const SourcePin& pin){
 struct Find final:CareFamilyActivityPinSink {
  const SourcePin& expected;std::size_t count=0;
  explicit Find(const SourcePin& p):expected(p){}
  Status actual_pin(const SourcePin& p)override{if(same(expected,p))++count;return common_success();}
 } found(pin);
 auto checked=visit_care_family_activity_source_pins(x,slot,found);if(!checked.ok)return checked;
 return found.count==1?common_success():common_failure("CareFamilyActivityPinNotActual",pin.consumer.id,ErrorKind::internal);
}
Outcome<bool> care_family_activity_source_owner_may_release(const CareFamilyActivitySources& x,
 const SourceRef& source,Integer schema){
 auto checked=closure(x);if(!checked.ok)return {false,false,checked.error};
 if(source.domain=="care_requirements"&&source.kind=="care_stopped"){
  auto actual=read_care_personal_stop_source(x,{source,schema});if(!actual.ok)return {false,false,actual.error};
  return {true,false,{}}; // the sole embedded retained Stop command owns it
 }
 auto actual=read_care_family_activity_source(x,source,schema);if(!actual.ok)return {false,false,actual.error};
 // The retained original binding owns its actual ongoing-activity source.
 // Ended bindings still need this historical proof until their real owner
 // releases them. No mutable refcount or terminal-state exception is used.
 for(const auto& pair:x.coverage.tables.bindings.rows())
  if(same(pair.second.care_activity_ref.source_ref,source))return {true,false,{}};
 // An end is embedded in its original retained activity, not a second table.
 for(const auto& pair:x.activities.rows())if(pair.second.ended_by&&
  same(care_family_activity_ended_source(pair.second.ended_by->end_id),source))return {true,false,{}};
 return {true,true,{}};
}
}
