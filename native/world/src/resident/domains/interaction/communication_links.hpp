// OC1: S3 owns addressing and business provenance; Social owns the one pending/delivered/known lifecycle.
#pragma once
#include <string_view>
#include "resident/domains/social/social_time2.hpp"
#include "communication_delivery_read.hpp"
namespace hanmo::resident {
using CommunicationAddress=RegisteredAddress;
struct MessageBusinessLink {StableId id,original_public_message_ref;SourceRef original_business_fact_ref;CommunicationAddress addressed_to;std::vector<SourceRef> route_source_refs,dossier_source_refs;Integer original_business_source_schema=0;std::optional<MomentOwnerSource> cancellation_source;std::optional<OfficialContentRef> official_content_ref;};
struct CommunicationOrigin {SourceRef source;Integer source_schema=0;StableId report_id,channel;SubjectRef sender;CommunicationAddress addressed_to;WorldMoment sent_at;Integer due_tick=0;FixedLockedRuleInput channel_rule_ref;std::optional<StableId> local_place_id;std::optional<OfficialContentRef> official_content_ref;};
struct CommunicationArrival {SourceRef source;Integer source_schema=0;StableId public_message_id;WorldMoment arrived_at;StableId site_id;};
struct CommunicationCancellation {SourceRef source;Integer source_schema=0;StableId public_message_id;WorldMoment cancelled_at;};
struct CommunicationResolvedRecipient {SubjectRef receiver;SourceRef authority_source;Integer office_epoch=0;bool disclose_contents=false;std::optional<CustodyReceiver> custody_receiver;};
struct CommunicationDeliverySource {StableId id,business_link_id;SourceRef arrival_ref;Integer arrival_schema=0;SubjectRef actual_receiver;SourceRef authority_source;Integer office_epoch=0;WorldMoment delivered_at;bool contents_disclosed=false;std::optional<CustodyReceiver> custody_receiver;};
struct CommunicationDossierAccess {StableId id,business_link_id;SourceRef original_access_ref;Integer source_schema=0;SubjectRef reader;SourceRef authority_source;WorldMoment accessed_at;};
struct CommunicationLinksState {OrderedTable<MessageBusinessLink> links;OrderedTable<CommunicationDeliverySource> deliveries;OrderedTable<CommunicationDossierAccess> dossier_accesses;};
// All reads return real already-applied business/channel/access results. Due alone is not arrival.
class CommunicationSourceRead {public:virtual ~CommunicationSourceRead()=default;
 virtual Outcome<CommunicationOrigin> origin(const SourceRef&,Integer schema)const=0;
 virtual Outcome<CommunicationCancellation> cancellation(const SourceRef&,Integer schema)const=0;
 virtual Outcome<CommunicationArrival> arrival(const SourceRef&,Integer schema)const=0;
 virtual Outcome<CommunicationResolvedRecipient> resolve_arrived_address(const CommunicationAddress&,const CommunicationArrival&)const=0;
 virtual Status validate_historical_recipient(const CommunicationAddress&,const CommunicationArrival&,const CommunicationResolvedRecipient&)const=0;
 virtual Outcome<CommunicationDossierAccess> dossier_access(const SourceRef&,Integer schema)const=0;
 virtual Status may_disclose(const CommunicationOrigin&,const Report&)const=0;
 // Only an original channel whose actual arrival and delivery are one atomic
 // writer may establish this cut. Equal Moment or message-derived IDs cannot.
 virtual Status validate_delivery_before_common_source(const CommunicationDeliverySource&,const SourceRef&)const{return common_failure("CommunicationDeliveryCausalSourceRequired");}
};
// Internal proof shared by the finite atomic local writers. Call only after
// that owner has re-read its genuine arrival through the original source reader.
Status validate_atomic_communication_arrival_before_common_source(const CommonStore&,const CommunicationLinksState&,const CommunicationDeliverySource&,const CommunicationArrival&,const SourceRef&actual_later_source);
Outcome<StableId> post_communication_in_candidate(CommonStore&,SocialState&,CommunicationLinksState&,const SourceRef& business_source,Integer schema,const CommunicationSourceRead&,const ExactResultTimeRead&,const WorldMoment& now);
Status deliver_communication_in_candidate(const CommonStore&,SocialState&,CommunicationLinksState&,const StableId& message_id,const SourceRef& arrival_source,Integer schema,const CommunicationSourceRead&,const WorldMoment& now);
Status cancel_communication_in_candidate(const CommonStore&,SocialState&,CommunicationLinksState&,const StableId& message_id,const SourceRef& cancellation_source,Integer schema,const CommunicationSourceRead&,const WorldMoment& now);
Status collect_communication_in_candidate(const CommonStore&,SocialState&,CommunicationLinksState&,const SourceRef& access_source,Integer schema,const CommunicationSourceRead&,const WorldMoment& now);
Status validate_communication_links(const CommonStore&,const SocialState&,const CommunicationLinksState&,const CommunicationSourceRead&,const ExactResultTimeRead&);
// Registered internal reader; user-facing disclosure remains the original KQ1/official known-source route.
struct VerifiedCommunicationDeliveryView {
 const MessageBusinessLink* business=nullptr;
 const CommunicationDeliverySource* delivery=nullptr;
};
using VerifiedOfficialDeliveryView=VerifiedCommunicationDeliveryView;
// Same original arrival/address/permission proof for a real Report, including
// reports without an OfficialContentRef. The caller must validate the Report
// against its original result owner; absence of an official body never waives
// actual delivery or the original disclosure permission.
Outcome<VerifiedCommunicationDeliveryView> read_verified_communication_delivery_source(const CommonStore&,const CommunicationLinksState&,const StableId&,const CommunicationSourceRead&,const Report&);
// Borrowed form runs exactly the canonical official-delivery proof below. The
// caller retains the same candidate lease; this does not create knowledge.
Outcome<VerifiedOfficialDeliveryView> read_verified_official_delivery_source(const CommonStore&,const CommunicationLinksState&,const StableId&,const CommunicationSourceRead&,const StableId* expected_report_id=nullptr);
Outcome<DeliverySourceView> read_official_delivery_source(const CommonStore&,const CommunicationLinksState&,const StableId&,const CommunicationSourceRead&);
Outcome<WorldMoment> read_communication_source_moment(const CommunicationLinksState&,const SourceRef&,Integer schema);
Outcome<CommunicationLinksState> decode_communication_links(const BoundaryValue&);
Outcome<BoundaryValue> encode_communication_links(const CommunicationLinksState&);
Status visit_communication_links_checkpoint(CodecVisitor&,CommunicationLinksState&);
class ProjectionWriter;
Status emit_communication_links_projection(ProjectionWriter&,std::string_view,const CommunicationLinksState&);
}
