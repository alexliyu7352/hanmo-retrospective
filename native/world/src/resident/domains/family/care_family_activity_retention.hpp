#pragma once
#include "care_family_activity_read.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareFamilyActivityPinSlot {
 accepted_terms,actual_handover,actual_work_period,actual_placement,
 adopted_participation,actual_end,end_cause,end_condition_receipt,stop_accepted_terms,stop_player_control,stop_condition_seed,stop_condition_receipt
};
RequiredSourceConsumerSlot care_family_activity_consumer_slot(CareFamilyActivityPinSlot);
class CareFamilyActivityPinSink {
public:virtual ~CareFamilyActivityPinSink()=default;
 virtual Status actual_pin(const SourcePin&)=0;
};
// Full original coverage, goal and activity closure precedes enumeration.
// Rules keep their CatalogIdentity ownership; they are not fabricated sources.
Status visit_care_family_activity_source_pins(const CareFamilyActivitySources&,
 CareFamilyActivityPinSlot,CareFamilyActivityPinSink&);
Status validate_care_family_activity_source_pin(const CareFamilyActivitySources&,
 CareFamilyActivityPinSlot,const SourcePin&);
Outcome<bool> care_family_activity_source_owner_may_release(
 const CareFamilyActivitySources&,const SourceRef&,Integer source_schema);
}
