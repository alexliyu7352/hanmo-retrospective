#pragma once
#include "care_facility_use.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareFacilityUsePinSlot {owner_business,accepted_terms,created_source,physical_holder,authorized_use};
RequiredSourceConsumerSlot care_facility_use_consumer_slot(CareFacilityUsePinSlot);
class CareFacilityUsePinSink {public:virtual ~CareFacilityUsePinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
Status visit_care_facility_use_source_pins(const CareFacilityUseSources&,CareFacilityUsePinSlot,CareFacilityUsePinSink&);
Status validate_care_facility_use_source_pin(const CareFacilityUseSources&,CareFacilityUsePinSlot,const SourcePin&);
// Claims and catalog rules keep their original typed owners. No invented
// SourceRef is built from a claim ID, slot, holder or catalog rule name.
class CareFacilityUseTypedReferenceSink {
public:virtual ~CareFacilityUseTypedReferenceSink()=default;
 virtual Status original_claim(const ConsumerRef&,const StableId& claim_id,
  const FacilityClaimReceiptRef& actual_admission)=0;
 virtual Status original_rule(const ConsumerRef&,const ActorFoundationRuleRef&)=0;
};
Status visit_care_facility_use_typed_references(const CareFacilityUseSources&,CareFacilityUseTypedReferenceSink&);
}
