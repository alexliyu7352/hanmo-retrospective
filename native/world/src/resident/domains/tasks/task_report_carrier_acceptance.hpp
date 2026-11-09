#pragma once
#include "task_report_carrier_terms.hpp"
namespace hanmo::resident {
// One original transport acceptance. Sender is supplied by the authenticated
// command route; carrier, destination and content derive from the actual sources.
struct AcceptKnownReportCarrier {
 SourceRef authored_offer;StableId recipient_actor_id;
 MomentOwnerSource actual_carrier_disclosure;
 StableId funding_wallet_id;Integer funding_gross_cap=0;WorldMoment funding_until;
};
struct TaskReportCarrierAcceptanceContext {
 const TaskReportCarrierScopeContext& scope;const SelfTransportFundingContext& funding;
};
struct TaskReportCarrierAcceptanceQuote {
 TaskReportCarrierContentSelection selection;SelfTransportFundingQuote funding;
 TaskCarrierAcceptanceQuote acceptance;
};
Outcome<AcceptKnownReportCarrier> decode_accept_known_report_carrier(const BoundaryValue&);
Status emit_accept_known_report_carrier_fields(ProjectionWriter&,const AcceptKnownReportCarrier&);
Outcome<TaskReportCarrierAcceptanceQuote> preview_known_report_carrier_acceptance(const TaskReportCarrierAcceptanceContext&,const StableId&authenticated_sender,const AcceptKnownReportCarrier&);
// No intermediate accepted-but-unfunded plan is published. Outer Common commit
// owns replay and discards every S3/S7 write if any final source check fails.
Outcome<SourceRef> accept_known_report_carrier_in_candidate(CommonStore&,TaskState&,PrivateTransportFundingState&,const TaskReportCarrierAcceptanceContext&,const StableId&authenticated_sender,const TaskReportCarrierAcceptanceQuote&);
}
