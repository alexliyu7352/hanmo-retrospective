#include "care_period_retention.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
namespace hanmo::resident {namespace {
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SourcePin& a,const SourcePin& b){return a.consumer.domain==b.consumer.domain&&a.consumer.kind==b.consumer.kind&&a.consumer.id==b.consumer.id&&a.consumer.slot==b.consumer.slot&&same(a.source,b.source)&&a.reason==b.reason;}
Status enumerate(const CarePeriodSources& x,CarePeriodPinSlot slot,CarePeriodPinSink& sink){
 const auto spec=care_period_consumer_slot(slot);if(spec.domain.empty())return common_failure("UnknownCarePeriodConsumerSlot",{},ErrorKind::unsupported_capability);
 auto emit=[&](const StableId& id,const SourceRef& ref,SourcePinReason reason=SourcePinReason::dependent_state){return sink.actual_pin({{spec.domain,spec.kind,id,spec.slot},ref,reason});};
 for(const auto& pair:x.tables.periods.rows()){
  const auto& p=pair.second;const auto* s=x.tables.sources.find(pair.first);
  if(!s)return common_failure("CarePeriodOriginalPayloadMissing",pair.first,ErrorKind::internal);
  Status status=common_success();
  switch(slot){
   case CarePeriodPinSlot::period_payload:status=emit(pair.first,care_period_source(pair.first),SourcePinReason::unique_outcome);break;
   case CarePeriodPinSlot::place:status=emit(pair.first,p.place.source_ref);break;
   case CarePeriodPinSlot::activity:status=emit(pair.first,p.activity_ref.source_ref);break;
   case CarePeriodPinSlot::food:status=emit(pair.first,p.food_support_ref.source_ref);break;
   case CarePeriodPinSlot::binding_creation:{
    auto b=read_care_binding_coverage_v2(x.coverage,s->binding_ref);if(!b.ok)return {false,{},b.error};
    // The binding has no separate Common head. Its true satisfaction/Completed
    // source owns the retained binding; do not synthesize a binding fact.
    status=emit(pair.first,care_coverage_satisfied_source(b.value.satisfaction->fact_id));break;
   }
   case CarePeriodPinSlot::calendar:status=emit(pair.first,s->calendar_period_ref.source_ref);break;
   case CarePeriodPinSlot::participation_action:
   case CarePeriodPinSlot::participation_period:{
    if(const auto* a=std::get_if<OriginalCareActionParticipation>(&s->participation))status=emit(pair.first,(slot==CarePeriodPinSlot::participation_action?a->original_action_ref:a->provided_period_ref).source_ref);break;
   }
   case CarePeriodPinSlot::participation_activity:
   case CarePeriodPinSlot::participation_actual:{
    if(const auto* a=std::get_if<CompatibleFamilyActivityParticipation>(&s->participation))status=emit(pair.first,(slot==CarePeriodPinSlot::participation_activity?a->family_activity_ref:a->actual_participation_ref).source_ref);break;
   }
   case CarePeriodPinSlot::original_completion:status=emit(pair.first,s->actual_complete_source_ref.source_ref,SourcePinReason::unique_outcome);break;
   case CarePeriodPinSlot::provision_results:for(const auto& r:s->provision_result_refs){status=emit(pair.first,r.source_ref,SourcePinReason::unique_outcome);if(!status.ok)return status;}break;
   case CarePeriodPinSlot::condition_results:for(const auto& r:s->condition_result_refs){status=emit(pair.first,character_condition_receipt_source(r),SourcePinReason::unique_outcome);if(!status.ok)return status;}break;
  }
  if(!status.ok)return status;
 }
 return common_success();
}
}
RequiredSourceConsumerSlot care_period_consumer_slot(CarePeriodPinSlot slot){switch(slot){
#define CARE_PERIOD_SLOT(tag,kind,field) case CarePeriodPinSlot::tag:return {"care_requirements",kind,field};
 CARE_PERIOD_SLOT(period_payload,"care_period_source","care_period_ref")
 CARE_PERIOD_SLOT(place,"care_period","place")
 CARE_PERIOD_SLOT(activity,"care_period","activity_ref")
 CARE_PERIOD_SLOT(food,"care_period","food_support_ref")
 CARE_PERIOD_SLOT(binding_creation,"care_period_source","binding_ref")
 CARE_PERIOD_SLOT(calendar,"care_period_source","calendar_period_ref")
 CARE_PERIOD_SLOT(participation_action,"care_period_source","original_action_ref")
 CARE_PERIOD_SLOT(participation_period,"care_period_source","provided_period_ref")
 CARE_PERIOD_SLOT(participation_activity,"care_period_source","family_activity_ref")
 CARE_PERIOD_SLOT(participation_actual,"care_period_source","actual_participation_ref")
 CARE_PERIOD_SLOT(original_completion,"care_period_source","actual_complete_source_ref")
 CARE_PERIOD_SLOT(provision_results,"care_period_source","provision_result_refs")
 CARE_PERIOD_SLOT(condition_results,"care_period_source","condition_result_refs")
#undef CARE_PERIOD_SLOT
 }return {};}
Status visit_care_period_source_pins(const CarePeriodSources& x,CarePeriodPinSlot slot,CarePeriodPinSink& sink){auto checked=validate_care_period_sources(x);return checked.ok?enumerate(x,slot,sink):checked;}
Status validate_care_period_source_pin(const CarePeriodSources& x,CarePeriodPinSlot slot,const SourcePin& pin){
 struct Find final:CarePeriodPinSink {const SourcePin& expected;std::size_t count=0;explicit Find(const SourcePin& p):expected(p){}Status actual_pin(const SourcePin& p)override{if(same(expected,p))++count;return common_success();}} found(pin);
 auto checked=visit_care_period_source_pins(x,slot,found);if(!checked.ok)return checked;return found.count==1?common_success():common_failure("CarePeriodPinNotActual",pin.consumer.id,ErrorKind::internal);
}
Status visit_care_period_rule_references(const CarePeriodSources& x,CarePeriodRulePinSink& sink){
 auto checked=validate_care_period_sources(x);if(!checked.ok)return checked;
 for(const auto& pair:x.tables.sources.rows())if(const auto* p=std::get_if<CompatibleFamilyActivityParticipation>(&pair.second.participation)){
  checked=sink.actual_rule({"care_requirements","care_period_source",pair.first,"compatibility_rule_ref"},p->compatibility_rule_ref);if(!checked.ok)return checked;
 }
 return common_success();
}
Outcome<bool> care_period_source_owner_may_release(const CarePeriodSources& x,const SourceRef& ref){
 auto schema=care_period_source_schema(ref);if(!schema.ok)return {false,false,schema.error};auto actual=read_care_period(x,ref,schema.value);if(!actual.ok)return {false,false,actual.error};
 auto checked=validate_care_period_sources(x);if(!checked.ok)return {false,false,checked.error};
 struct Find final:CarePeriodPinSink {const SourceRef& source;bool retained=false;explicit Find(const SourceRef& s):source(s){}Status actual_pin(const SourcePin& p)override{if(same(source,p.source))retained=true;return common_success();}} found(ref);
 for(Integer n=0;n<=static_cast<Integer>(CarePeriodPinSlot::condition_results);++n){checked=enumerate(x,static_cast<CarePeriodPinSlot>(n),found);if(!checked.ok)return {false,false,checked.error};}
 // A retained source row pins its real paired period, not a writable refcount.
 // External consumers are checked by the shared retention coordinator as well.
 return {true,!found.retained,{}};
}
}
