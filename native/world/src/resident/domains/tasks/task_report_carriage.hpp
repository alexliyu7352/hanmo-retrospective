#pragma once
#include "task_transport_agreement.hpp"
#include "resident/domains/interaction/face_to_face_report.hpp"
#include "resident/domains/travel/movement_action_time.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
namespace hanmo::resident {
struct TaskReportCarriageContext {
 const CommonStore&common;const CommonCatalog&common_catalog;const TaskState&tasks;const TaskTransportTermsRead&original_terms;
 const FaceToFaceReportContext&face;const NativeMovementActionTimeRead&movement_actions;
 const MovementExactState&movement;const TravelCatalog&travel_catalog;
};
struct TaskReportTransportBindingView {
 const TaskCarrierAgreement* agreement=nullptr;
 const TaskReportCarriage* carriage=nullptr;
 const TaskReportCarriageDispatch* dispatch=nullptr;
};
// Source-local economic input: actual S3 acceptance/dispatch only, with no fee
// callback. S1 separately verifies the returned real execution source and its
// original opening cut; a typed reference is not a reconstructed start proof.
Outcome<std::optional<TaskReportTransportBindingView>> read_original_task_report_transport_binding(const CommonStore&,const TaskState&,const StableId&actual_action_id);
struct PostTaskReportCarriage {StableId agreement_id,actual_action_id;};
struct HandOverTaskReportCarriage {StableId carriage_id;MomentOwnerSource actual_arrival;};
// Pure current admission for a real already-accepted carriage and live original
// carrier execution. Does not allocate an ID, post a message or advance travel.
Status validate_task_report_carriage_dispatch_request(const TaskReportCarriageContext&,const StableId&authenticated_sender,const PostTaskReportCarriage&,const ReadStamp&);
// Both actual commands remain inside the one original World candidate. Sending
// requires the real accepted report scope and already-adopted carrier execution;
// delivery additionally requires a real arrival and the actual handover step.
Outcome<StableId> post_task_report_carriage_in_candidate(CommonStore&,TaskState&,SocialState&,CommunicationLinksState&,const TaskReportCarriageContext&,const StableId&authenticated_sender,const PostTaskReportCarriage&,const ReadStamp&);
Outcome<StableId> hand_over_task_report_carriage_in_candidate(CommonStore&,TaskState&,SocialState&,CommunicationLinksState&,const TaskReportCarriageContext&,const StableId&authenticated_carrier,const HandOverTaskReportCarriage&,const ReadStamp&);
Status validate_native_task_report_delivery_acceptance(const TaskReportCarriageContext&,const TaskCarrierAgreement&,const TaskCarrierReportTerms&);
Outcome<WorldMoment> read_original_task_report_carriage_moment(const CommonStore&,const TaskState&,const SourceRef&,Integer schema);
Status validate_task_report_carriages(const TaskReportCarriageContext&);
class TaskReportCarriageCommunicationRead final:public CommunicationSourceRead {
 const TaskReportCarriageContext&context_;
public:explicit TaskReportCarriageCommunicationRead(const TaskReportCarriageContext&x):context_(x){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &context_.common==&c&&&context_.face.disclosure.social==&s;}
 Outcome<CommunicationOrigin> origin(const SourceRef&,Integer)const override;
 Outcome<CommunicationCancellation> cancellation(const SourceRef&,Integer)const override;
 Outcome<CommunicationArrival> arrival(const SourceRef&,Integer)const override;
 Outcome<CommunicationResolvedRecipient> resolve_arrived_address(const CommunicationAddress&,const CommunicationArrival&)const override;
 Status validate_historical_recipient(const CommunicationAddress&,const CommunicationArrival&,const CommunicationResolvedRecipient&)const override;
 Outcome<CommunicationDossierAccess> dossier_access(const SourceRef&,Integer)const override;
 Status may_disclose(const CommunicationOrigin&,const Report&)const override;
 Status validate_delivery_before_common_source(const CommunicationDeliverySource&,const SourceRef&)const override;
};
}
