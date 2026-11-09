#pragma once
#include "task_report_carrier_catalog.hpp"
#include "task_transport_agreement.hpp"
#include "resident/domains/interaction/face_to_face_report.hpp"
namespace hanmo::resident {
// The command selects actual content already disclosed to the named carrier.
// No actor, report, agreement, funding source or knowledge is created by this read.
struct TaskReportCarrierContentSelection {
 SourceRef authored_offer;StableId sender_actor_id,recipient_actor_id;
 MomentOwnerSource actual_carrier_disclosure;
};
struct TaskReportCarrierScopeContext {
 const CommonCatalog& common_catalog;const TaskState& tasks;const TaskReportCarrierOfferCatalog& offers;
 const FaceToFaceReportContext& face;
};
struct TaskReportCarrierScopeView {
 const TaskReportCarrierOfferDefinition* offer=nullptr;
 const FaceToFaceReportDisclosure* disclosure=nullptr;
 const FaceToFaceContactResult* contact=nullptr;
 const Report* report=nullptr;
};
// The explicit cut is the actual current acceptance attempt, or a persisted
// original acceptance supplied to the history overload. Never an arbitrary tick.
Outcome<TaskReportCarrierScopeView> read_current_task_report_carrier_scope(const TaskReportCarrierScopeContext&,const TaskReportCarrierContentSelection&);
Status validate_task_report_carrier_prospective_scope(const TaskReportCarrierScopeContext&,const TaskReportCarrierContentSelection&,const TaskCarrierAgreement&,const TaskCarrierReportTerms&);
Outcome<TaskReportCarrierScopeView> read_task_report_carrier_accepted_scope(const TaskReportCarrierScopeContext&,const TaskCarrierAgreement&,const TaskCarrierReportTerms&);
}
