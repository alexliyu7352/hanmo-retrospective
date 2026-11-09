#pragma once
#include "care_period.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CarePeriodPinSlot {
 period_payload,place,activity,food,binding_creation,calendar,
 participation_action,participation_period,participation_activity,
 participation_actual,original_completion,provision_results,condition_results
};
RequiredSourceConsumerSlot care_period_consumer_slot(CarePeriodPinSlot);
class CarePeriodPinSink {public:virtual ~CarePeriodPinSink()=default;
 virtual Status actual_pin(const SourcePin&)=0;
};
// Catalog rules retain their original typed catalog identity. They are never
// converted into invented Common facts or source IDs.
class CarePeriodRulePinSink {public:virtual ~CarePeriodRulePinSink()=default;
 virtual Status actual_rule(const ConsumerRef&,const ActorFoundationRuleRef&)=0;
};
Status visit_care_period_source_pins(const CarePeriodSources&,CarePeriodPinSlot,CarePeriodPinSink&);
Status validate_care_period_source_pin(const CarePeriodSources&,CarePeriodPinSlot,const SourcePin&);
Status visit_care_period_rule_references(const CarePeriodSources&,CarePeriodRulePinSink&);
Outcome<bool> care_period_source_owner_may_release(const CarePeriodSources&,const SourceRef&);
}
