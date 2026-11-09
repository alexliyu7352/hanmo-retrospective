#pragma once
#include "care_requirement_contract.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareGoalConsumerSlot {goal_basis,current_transition,transition_predecessor,transition_cause,created_schedule};
RequiredSourceConsumerSlot care_goal_consumer_slot(CareGoalConsumerSlot);
class CareGoalPinSink {public:virtual ~CareGoalPinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
Status visit_care_goal_source_pins(const CareGoalSourceContext&,CareGoalConsumerSlot,CareGoalPinSink&);
Status validate_care_goal_source_pin(const CareGoalSourceContext&,CareGoalConsumerSlot,const SourcePin&);
Outcome<bool> care_goal_transition_owner_may_release(const CareGoalSourceContext&,const SourceRef&);
}
