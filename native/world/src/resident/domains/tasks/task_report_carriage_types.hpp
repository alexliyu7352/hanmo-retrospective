// MC1 carrier_delivery's original TaskTransport accepted scope and real results.
#pragma once
#include "resident/domains/interaction/report_disclosure_types.hpp"
namespace hanmo::resident {
// The original offered contract supplies this finite scope before acceptance.
// Time/cost are its original Travel/transport quote; no MC1 tariff is invented.
struct TaskCarrierReportTerms {
 StableId report_id,sender_actor_id,carrier_actor_id,recipient_actor_id;
 MomentOwnerSource actual_carrier_disclosure;
 Integer earliest_delivery_tick=0;
};
struct TaskReportCarriageDispatch {
 StableId id,action_id;MomentOwnerSource original_execution_source;
 SourceRef sender_knowledge_source;ReportDisclosurePermission permission;WorldMoment sent_at;
};
// Key is the original accepted agreement. Accepted scope is written in that
// same transaction; a later actual dispatch fills its one immutable source.
struct TaskReportCarriage {StableId agreement_id;TaskCarrierReportTerms terms;std::optional<TaskReportCarriageDispatch> dispatch;};
struct TaskReportHandoverSource {
 StableId id,carriage_id;MomentOwnerSource actual_arrival,contact_source;
 StableId dialogue_node_id,dialogue_phase;WorldMoment handed_over_at;
};
}
