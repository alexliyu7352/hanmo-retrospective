// Original S6 immutable terms + actual S3 personal disclosure. A receipt is not
// acceptance, ratification, treaty effectiveness or permission to sign for a party.
#pragma once
#include "treaty_revision_sources.hpp"
#include "resident/domains/interaction/official_content_knowledge.hpp"
namespace hanmo::resident {
struct TreatyKnownRevisionView {
 TreatyRevisionView original_revision;
 ActorOfficialContentReceiptView actual_receipt;
};
Outcome<OfficialContentRef> read_treaty_revision_official_content(
 const TreatyRevisionContext&,const SourceRef& terms_source,Integer source_schema);
// The body is borrowed from the selected locked original definition, and the
// revision is read from the actual offer/counteroffer chain, not Report strings.
Outcome<TreatyRevisionView> read_treaty_revision_official_body(
 const TreatyRevisionContext&,const OfficialContentRef&);
Outcome<TreatyKnownRevisionView> read_actor_known_treaty_revision(
 const TreatyRevisionContext&,const SocialState&,const CommunicationLinksState&,
 const CommunicationSourceRead&,const ExactResultTimeRead&,const ExactResultReadStamp&,
 const StableId& actor,const OfficialContentRef&,const MomentOwnerSource& actual_delivery);
// Historical consumer has already validated its actual payload and Moment.
// The original channel proves delivery precedes this real Common append cut.
Outcome<TreatyKnownRevisionView> read_actor_known_treaty_revision_before_source(
 const TreatyRevisionContext&,const SocialState&,const CommunicationLinksState&,
 const CommunicationSourceRead&,const ExactResultTimeRead&,const ExactResultReadStamp&,
 const StableId& actor,const OfficialContentRef&,const MomentOwnerSource& actual_delivery,
 const SourceRef& actual_consumer_source);
}
