#pragma once
#include "care_coverage.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareCoveragePinSlot {
 binding_terms,binding_acceptance,binding_activity,binding_placement,
 satisfaction_result,loss_cause,completion_period,completion_participation,
 completion_placement,completion_fees
};
RequiredSourceConsumerSlot care_coverage_consumer_slot(CareCoveragePinSlot);
class CareCoveragePinSink {public:virtual ~CareCoveragePinSink()=default;
 virtual Status actual_pin(const SourcePin&)=0;
};
Status visit_care_coverage_source_pins(const CareCoverageSources&,CareCoveragePinSlot,CareCoveragePinSink&);
Status validate_care_coverage_source_pin(const CareCoverageSources&,CareCoveragePinSlot,const SourcePin&);
Outcome<bool> care_coverage_source_owner_may_release(const CareCoverageSources&,const SourceRef&);
}
