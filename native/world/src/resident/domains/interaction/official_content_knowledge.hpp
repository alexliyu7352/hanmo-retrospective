#pragma once
#include "communication_links.hpp"
namespace hanmo::resident {
// Borrowed from one actual owner read lease. The original official-content
// owner resolves its immutable complete terms; this view does not synthesize a
// Report payload, consent, ratification, new knowledge or an institutional read.
struct ActorOfficialContentReceiptView {
 const MessageBusinessLink* business=nullptr;
 const CommunicationDeliverySource* delivery=nullptr;
 const Report* report=nullptr;
 KnowledgeReceiptViewV2 knowledge;
};
Outcome<ActorOfficialContentReceiptView> read_actor_official_content_receipt(
 const CommonStore&,const SocialState&,const CommunicationLinksState&,
 const CommunicationSourceRead&,const ExactResultTimeRead&,const ExactResultReadStamp&,
 const StableId&actor,const OfficialContentRef&expected,const MomentOwnerSource&actual_delivery);
// Historical S6 acceptance/presentation must additionally follow the actual
// delivery writer. The consumer owner first validates its own source payload/M;
// this function checks its real Common cut against this channel's atomic source.
Outcome<ActorOfficialContentReceiptView> read_actor_official_content_receipt_before_source(
 const CommonStore&,const SocialState&,const CommunicationLinksState&,
 const CommunicationSourceRead&,const ExactResultTimeRead&,const ExactResultReadStamp&,
 const StableId&actor,const OfficialContentRef&expected,const MomentOwnerSource&actual_delivery,
 const SourceRef&actual_consumer_source);
}
