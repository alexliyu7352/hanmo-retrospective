// MC1 custodian_pickup: original witnessed reception owner, physical /3.
#pragma once
#include "visit_message.hpp"
#include "resident/domains/interaction/report_disclosure.hpp"
#include "resident/domains/interaction/face_to_face_contact_source.hpp"
namespace hanmo::resident {
enum class VisitReportCustodyAction {deposit,pickup,withdraw};
struct VisitReportCustodyRequest {VisitReportCustodyAction action=VisitReportCustodyAction::deposit;StableId policy_id,report_id,deposit_ref;};
struct VisitReportCustodyContext {
 const CommonStore& common;const VisitMessageState& state;const VisitMessageCatalog& policies;
 const InteractionState& interaction;const InteractionCatalog& catalog;const InteractionAccess& access;
 const CatalogIdentity& identity;const ReportDisclosureReadContext& disclosure;
 const VisitMessageLifeContext& life;
};
struct VisitReportCustodyQuote {
 CheckpointMemoryCharge memory;
 VisitReportCustodyQuote()=default;
 VisitReportCustodyQuote(const VisitReportCustodyQuote&)=delete;
 VisitReportCustodyQuote& operator=(const VisitReportCustodyQuote&)=delete;
 VisitReportCustodyQuote(VisitReportCustodyQuote&&)=default;
 VisitReportCustodyQuote& operator=(VisitReportCustodyQuote&&)=delete;
 StableId actor_id;VisitReportCustodyRequest request;VisitMessageContact contact;
 MomentOwnerSource contact_source;std::optional<ReportDisclosurePermission> permission;
 std::optional<SourceRef> sender_knowledge_source;WorldMoment quoted_at;
 std::shared_ptr<const ReadStamp> stamp;
};
Outcome<VisitReportCustodyRequest> decode_visit_report_custody_request(const StableId&command,const BoundaryValue&);
Status emit_visit_report_custody_request_fields(ProjectionWriter&,const VisitReportCustodyRequest&);
Outcome<VisitReportCustodyQuote> preview_visit_report_custody(const VisitReportCustodyContext&,const StableId& authenticated_actor,const VisitReportCustodyRequest&);
// All writes participate in the existing outer World candidate. Returns the
// actual original deposit/pickup/withdrawal source, not a synthetic delivered flag.
Outcome<SourceRef> apply_visit_report_custody_in_candidate(CommonStore&,VisitMessageState&,SocialState&,CommunicationLinksState&,const VisitReportCustodyContext&,const StableId& authenticated_actor,const VisitReportCustodyQuote&);
Outcome<WorldMoment> read_original_visit_report_custody_moment(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionState&,const InteractionCatalog&,const SourceRef&,Integer schema);
Outcome<bool> emit_visit_report_custody_fact_payload_fields(ProjectionWriter&,const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionState&,const InteractionCatalog&,const FactRecord&);
Status validate_visit_report_custody_sources(const VisitReportCustodyContext&);
class VisitReportCustodyCommunicationRead final:public CommunicationSourceRead {
 const VisitReportCustodyContext& context_;
public:explicit VisitReportCustodyCommunicationRead(const VisitReportCustodyContext&x):context_(x){}
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
