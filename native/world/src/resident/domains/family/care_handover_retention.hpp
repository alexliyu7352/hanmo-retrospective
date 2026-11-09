// Actual references of the original Care roots. No writable retention counter.
#pragma once
#include "care_coverage.hpp"
#include "care_handover_command_source.hpp"
#include "care_handover_time.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CareHandoverConsumerSlot {
 execution_accepted,execution_boundary,execution_period,execution_placement,execution_outcome,
 boundary_execution,boundary_predecessor,boundary_cause,boundary_authority,boundary_player_control,boundary_seed,
 boundary_receipt,boundary_limits,boundary_participation,boundary_facility,
 period_execution,period_seed,period_receipt,period_limits,period_effect
};
struct CareHandoverRetentionContext {
 const CareCoverageSources& coverage;const CareHandoverCommandSourceContext& commands;
 const CareHandoverBusinessRead& business;
};
RequiredSourceConsumerSlot care_handover_consumer_slot(CareHandoverConsumerSlot);
class CareHandoverPinSink {public:virtual ~CareHandoverPinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
Status visit_care_handover_source_pins(const CareHandoverRetentionContext&,
 CareHandoverConsumerSlot,CareHandoverPinSink&);
Status validate_care_handover_source_pin(const CareHandoverRetentionContext&,
 CareHandoverConsumerSlot,const SourcePin&);
Outcome<bool> care_handover_source_owner_may_release(const CareHandoverRetentionContext&,const SourceRef&);
// WorkPeriodRef2 refers to the sole S0 action chain by its original typed
// binding. The core owner retains that chain; Care does not invent a SourceRef.
class CareHandoverWorkBindingSink {
public:virtual ~CareHandoverWorkBindingSink()=default;
 virtual Status actual_binding(const StableId& original_execution,
  const ActorActionWorkBinding&)=0;
};
Status visit_care_handover_work_binding_references(const CareHandoverRetentionContext&,
 CareHandoverWorkBindingSink&);
}
