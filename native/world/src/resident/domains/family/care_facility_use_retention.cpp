#include "care_facility_use_retention.hpp"
#include "resident/facility_claims/facility_claims.hpp"
namespace hanmo::resident {
RequiredSourceConsumerSlot care_facility_use_consumer_slot(CareFacilityUsePinSlot slot){switch(slot){
#define CARE_USE_SLOT(tag,field) case CareFacilityUsePinSlot::tag:return {"care_requirements","care_facility_use_binding",field};
 CARE_USE_SLOT(owner_business,"owner_business_ref")
 CARE_USE_SLOT(accepted_terms,"accepted_terms_ref")
 CARE_USE_SLOT(created_source,"created_source_ref")
 CARE_USE_SLOT(physical_holder,"physical_holder_source_ref")
 CARE_USE_SLOT(authorized_use,"authorized_use_source_ref")
#undef CARE_USE_SLOT
 }return {};}
Status visit_care_facility_use_source_pins(const CareFacilityUseSources& x,CareFacilityUsePinSlot slot,CareFacilityUsePinSink& sink){
 auto checked=validate_care_facility_use_sources(x);if(!checked.ok)return checked;const auto spec=care_facility_use_consumer_slot(slot);if(spec.domain.empty())return common_failure("UnknownCareFacilityUseConsumerSlot",{},ErrorKind::unsupported_capability);
 for(const auto& pair:x.bindings.rows()){const auto& v=pair.second;const auto* borrowed=std::get_if<CareFacilityBorrowedPhysicalHolder>(&v.mode);SourceRef ref;
  switch(slot){case CareFacilityUsePinSlot::owner_business:ref=care_acceptance_source(v.owner_business_ref.accepted_arrangement_id);break;case CareFacilityUsePinSlot::accepted_terms:ref=care_acceptance_source(v.accepted_terms_ref);break;case CareFacilityUsePinSlot::created_source:ref=v.created_source_ref;break;case CareFacilityUsePinSlot::physical_holder:if(!borrowed)continue;ref=borrowed->physical_holder_source_ref;break;case CareFacilityUsePinSlot::authorized_use:if(!borrowed)continue;ref=borrowed->authorized_use_source_ref;break;}
  checked=sink.actual_pin({{spec.domain,spec.kind,pair.first,spec.slot},ref,SourcePinReason::dependent_state});if(!checked.ok)return checked;
 }return common_success();
}
Status validate_care_facility_use_source_pin(const CareFacilityUseSources& x,CareFacilityUsePinSlot slot,const SourcePin& pin){
 struct Find final:CareFacilityUsePinSink{const SourcePin& expected;std::size_t count=0;explicit Find(const SourcePin& p):expected(p){}Status actual_pin(const SourcePin& p)override{if(expected.consumer.domain==p.consumer.domain&&expected.consumer.kind==p.consumer.kind&&expected.consumer.id==p.consumer.id&&expected.consumer.slot==p.consumer.slot&&same_source_ref(expected.source,p.source)&&expected.reason==p.reason)++count;return common_success();}} found(pin);
 auto checked=visit_care_facility_use_source_pins(x,slot,found);if(!checked.ok)return checked;return found.count==1?common_success():common_failure("CareFacilityUsePinNotActual",pin.consumer.id,ErrorKind::internal);
}
Status visit_care_facility_use_typed_references(const CareFacilityUseSources& x,CareFacilityUseTypedReferenceSink& sink){
 auto checked=validate_care_facility_use_sources(x);if(!checked.ok)return checked;auto now=x.acceptance.goals.clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 for(const auto& pair:x.bindings.rows()){const auto& v=pair.second;const auto* own=std::get_if<CareFacilityOwnClaim>(&v.mode);const auto& id=own?own->original_claim_id:std::get<CareFacilityBorrowedPhysicalHolder>(v.mode).placement_claim_id;auto episode=read_facility_claim_episode(x.claims,id,now.value);if(!episode.ok)return {false,{},episode.error};
  checked=sink.original_claim({"care_requirements","care_facility_use_binding",pair.first,own?"original_claim_id":"placement_claim_id"},id,episode.value.admission_receipt_ref);if(!checked.ok)return checked;
  checked=sink.original_rule({"care_requirements","care_facility_use_binding",pair.first,"usage_rule_ref"},v.usage_rule_ref);if(!checked.ok)return checked;
 }return common_success();
}
}
