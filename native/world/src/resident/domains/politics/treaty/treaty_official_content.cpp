#include "treaty_official_content.hpp"
namespace hanmo::resident {
Outcome<OfficialContentRef> read_treaty_revision_official_content(const TreatyRevisionContext& x,const SourceRef& source,Integer schema){
 auto actual=read_treaty_revision_source(x,source,schema);if(!actual.ok)return {false,{},actual.error};
 OfficialContentRef out;out.kind=actual.value.terms_revision==1?OfficialContentKind::treaty_offer:OfficialContentKind::treaty_counteroffer;out.id=source.id;out.terms_revision=Revision{actual.value.terms_revision};
 auto valid=validate_official_content_ref(out);return {valid.ok,std::move(out),valid.error};
}
Outcome<TreatyRevisionView> read_treaty_revision_official_body(const TreatyRevisionContext& x,const OfficialContentRef& content){
 auto valid=validate_official_content_ref(content);if(!valid.ok)return {false,{},valid.error};
 const auto* revision=std::get_if<Revision>(&content.terms_revision);
 if(!revision||(content.kind!=OfficialContentKind::treaty_offer&&content.kind!=OfficialContentKind::treaty_counteroffer))return {false,{},common_failure("TreatyOfficialRevisionSourceRequired",content.id,ErrorKind::unsupported_capability).error};
 SourceRef source{"politics",content.kind==OfficialContentKind::treaty_offer?"treaty_terms_offered":"treaty_terms_counteroffered",content.id};
 auto actual=read_treaty_revision_source(x,source,1);if(!actual.ok)return actual;
 if(actual.value.terms_revision!=revision->revision)return {false,{},common_failure("TreatyOfficialContentRevisionMismatch",content.id).error};
 return actual;
}
Outcome<TreatyKnownRevisionView> read_actor_known_treaty_revision(const TreatyRevisionContext& x,const SocialState& social,const CommunicationLinksState& links,
 const CommunicationSourceRead& communications,const ExactResultTimeRead& times,const ExactResultReadStamp& stamp,
 const StableId& actor,const OfficialContentRef& content,const MomentOwnerSource& delivery){
 auto original=read_treaty_revision_official_body(x,content);if(!original.ok)return {false,{},original.error};
 auto known=read_actor_official_content_receipt(x.common,social,links,communications,times,stamp,actor,content,delivery);if(!known.ok)return {false,{},known.error};
 if(!known.value.business||!known.value.delivery||!known.value.report||!known.value.business->official_content_ref)return {false,{},common_failure("TreatyOfficialDisclosureSourceRequired",content.id).error};
 const auto& addressed=*known.value.business->official_content_ref;
 if(addressed.kind!=content.kind||addressed.id!=content.id||!same_terms_revision(addressed.terms_revision,content.terms_revision))return {false,{},common_failure("TreatyOfficialDisclosureContentMismatch",content.id).error};
 auto order=compare_world_moments(original.value.issued_at,known.value.delivery->delivered_at);if(!order.ok)return {false,{},order.error};
 if(order.value>0)return {false,{},common_failure("TreatyOfficialDisclosureBeforeOriginal",content.id).error};
 return {true,{std::move(original.value),std::move(known.value)},{}};
}
Outcome<TreatyKnownRevisionView> read_actor_known_treaty_revision_before_source(const TreatyRevisionContext& x,const SocialState& social,const CommunicationLinksState& links,
 const CommunicationSourceRead& communications,const ExactResultTimeRead& times,const ExactResultReadStamp& stamp,
 const StableId& actor,const OfficialContentRef& content,const MomentOwnerSource& delivery,const SourceRef& consumer){
 auto original=read_treaty_revision_official_body(x,content);if(!original.ok)return {false,{},original.error};
 auto known=read_actor_official_content_receipt_before_source(x.common,social,links,communications,times,stamp,actor,content,delivery,consumer);if(!known.ok)return {false,{},known.error};
 if(!known.value.business||!known.value.delivery||!known.value.report||!known.value.business->official_content_ref)return {false,{},common_failure("TreatyOfficialDisclosureSourceRequired",content.id).error};
 const auto& addressed=*known.value.business->official_content_ref;
 if(addressed.kind!=content.kind||addressed.id!=content.id||!same_terms_revision(addressed.terms_revision,content.terms_revision))return {false,{},common_failure("TreatyOfficialDisclosureContentMismatch",content.id).error};
 auto order=compare_world_moments(original.value.issued_at,known.value.delivery->delivered_at);if(!order.ok)return {false,{},order.error};
 if(order.value>0)return {false,{},common_failure("TreatyOfficialDisclosureBeforeOriginal",content.id).error};
 return {true,{std::move(original.value),std::move(known.value)},{}};
}

}
