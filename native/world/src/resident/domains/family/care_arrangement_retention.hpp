#pragma once
#include "care_arrangement.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareAcceptancePinSlot {requirement_transition,institutional_assignment,allowed_dependency,provision,fee_arrangement,guardianship_permission,acceptance_condition_seed,acceptance_condition_receipt};
class CareAcceptancePinSink {public:virtual ~CareAcceptancePinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
RequiredSourceConsumerSlot care_acceptance_consumer_slot(CareAcceptancePinSlot);
Status visit_care_acceptance_source_pins(const CareAcceptanceContext&,CareAcceptancePinSlot,CareAcceptancePinSink&);
Status validate_care_acceptance_source_pin(const CareAcceptanceContext&,CareAcceptancePinSlot,const SourcePin&);
}
