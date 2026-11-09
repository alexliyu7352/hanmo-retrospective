// Compiled two actual local MC1 owners. This adapter does not claim carrier or
// scheduled-role capability; complete policy admission still checks each kind.
// The explicit carrier overload additionally borrows its original TaskTransport owner.
#pragma once
#include "face_to_face_report.hpp"
#include "visit_messages/visit_report_custody.hpp"
#include "resident/domains/tasks/task_report_carriage.hpp"
namespace hanmo::resident {
class NativeLocalCommunicationRead final:public CommunicationSourceRead {
 const CommonStore& common_;const SocialState& social_;
 const FaceToFaceReportCommunicationRead& face_;const VisitReportCustodyCommunicationRead& custody_;
 const TaskReportCarriageCommunicationRead* carrier_=nullptr;
 Outcome<const CommunicationSourceRead*> owner(const SourceRef&)const;
public:
 NativeLocalCommunicationRead(const CommonStore&common,const SocialState&social,const FaceToFaceReportCommunicationRead&f,const VisitReportCustodyCommunicationRead&c):common_(common),social_(social),face_(f),custody_(c){}
 NativeLocalCommunicationRead(const CommonStore&common,const SocialState&social,const FaceToFaceReportCommunicationRead&f,const VisitReportCustodyCommunicationRead&c,const TaskReportCarriageCommunicationRead&carrier):common_(common),social_(social),face_(f),custody_(c),carrier_(&carrier){}
 bool has_carrier_delivery()const noexcept{return carrier_!=nullptr;}
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
