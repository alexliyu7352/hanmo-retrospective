// Required MC1 active telling. Uses the existing real dialogue/contact and the
// one original S3/Social pending-message, delivery and knowledge writers.
#pragma once
#include "face_to_face_contact_source.hpp"
#include "report_disclosure.hpp"
namespace hanmo::resident {
inline constexpr const char* face_to_face_report_source_kind="face_to_face_report_disclosed";
struct TellKnownReport {StableId report_id,recipient_actor_id;};
Outcome<TellKnownReport> decode_tell_known_report(const BoundaryValue&);
Status emit_tell_known_report_fields(ProjectionWriter&,const TellKnownReport&);
struct FaceToFaceReportContext {
 const CommonStore& common;const InteractionState& interaction;const InteractionCatalog& catalog;
 const InteractionAccess& access;const CatalogIdentity& catalog_identity;
 const ReportDisclosureReadContext& disclosure;
};
struct FaceToFaceReportQuote {
 // 首字段先持费、末销毁；quote只移动到Prepared，不能保可写或廉价copy别名。
 CheckpointMemoryCharge memory;
 FaceToFaceReportQuote()=default;
 FaceToFaceReportQuote(const FaceToFaceReportQuote&)=delete;
 FaceToFaceReportQuote& operator=(const FaceToFaceReportQuote&)=delete;
 FaceToFaceReportQuote(FaceToFaceReportQuote&&)noexcept=default;
 FaceToFaceReportQuote& operator=(FaceToFaceReportQuote&&)noexcept;
 StableId sender_actor_id;TellKnownReport request;FaceToFaceContactProof contact;
 ReportDisclosurePermission permission;SourceRef sender_knowledge_source;
 std::shared_ptr<const ReadStamp> stamp;
};
Outcome<FaceToFaceReportQuote> preview_face_to_face_report(const FaceToFaceReportContext&,const StableId&sender,const TellKnownReport&);
// Caller is authenticated by the original command/Director execution route.
// The whole World candidate is discarded if any post/delivery/cold check fails.
Outcome<StableId> tell_known_report_in_candidate(CommonStore&,InteractionState&,SocialState&,const FaceToFaceReportContext&,const StableId&authenticated_sender,const FaceToFaceReportQuote&);
// Original immutable result/header/contact reader for P10 time capture. Full
// authorization and message coverage are checked by the selected cold validator.
Outcome<const FaceToFaceReportDisclosure*> read_original_face_to_face_report_source(const CommonStore&,const InteractionState&,const InteractionCatalog&,const SourceRef&,Integer schema=1);
Outcome<const FaceToFaceReportDisclosure*> read_face_to_face_report_source(const FaceToFaceReportContext&,const SourceRef&,Integer schema=1);
Status validate_face_to_face_report_sources(const FaceToFaceReportContext&);
// Composable source reader: owns this actual face-to-face branch only. Other
// channels must be dispatched to their original accepted service/transport owner.
class FaceToFaceReportCommunicationRead final:public CommunicationSourceRead {
 const FaceToFaceReportContext& context_;
public:explicit FaceToFaceReportCommunicationRead(const FaceToFaceReportContext&x):context_(x){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &context_.common==&c&&&context_.disclosure.social==&s;}
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
