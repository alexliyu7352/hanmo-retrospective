#include "official_content_knowledge.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T>fail(const char*code,const StableId&id={}){return {false,{},common_failure(code,id).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool content(const OfficialContentRef&a,const OfficialContentRef&b){return a.kind==b.kind&&a.id==b.id&&same_terms_revision(a.terms_revision,b.terms_revision);}
}
Outcome<ActorOfficialContentReceiptView> read_actor_official_content_receipt(const CommonStore&common,const SocialState&social,const CommunicationLinksState&links,const CommunicationSourceRead&original,const ExactResultTimeRead&results,const ExactResultReadStamp&stamp,const StableId&actor,const OfficialContentRef&expected,const MomentOwnerSource&source){
 if(actor.empty()||!common.actor(actor)||source.source_schema!=1||source.source_ref.domain!="interaction"||source.source_ref.kind!="communication_delivery")return fail<ActorOfficialContentReceiptView>("OfficialPersonalDeliverySourceRequired",source.source_ref.id);
 auto valid=validate_official_content_ref(expected);if(!valid.ok)return {false,{},valid.error};
 if(!social.dynamic||!social.dynamic->time2)return fail<ActorOfficialContentReceiptView>("OfficialPersonalKnowledgeCapabilityRequired");
 const auto*delivery=links.deliveries.find(source.source_ref.id);const auto*business=delivery?links.links.find(delivery->business_link_id):nullptr;
 if(!delivery||delivery->id!=source.source_ref.id||!business||!business->official_content_ref||!content(*business->official_content_ref,expected)||!delivery->contents_disclosed||delivery->custody_receiver||delivery->actual_receiver.kind!=SubjectKind::actor||delivery->actual_receiver.id!=actor)return fail<ActorOfficialContentReceiptView>("OfficialContentNotPersonallyDisclosed",source.source_ref.id);
 const auto&state=*social.dynamic;const auto*message=state.pending_messages.find(business->original_public_message_ref);const auto*execution=message?state.time2->message_execution_bindings.find(message->id):nullptr;const auto*report=message?state.reports.find(message->report_id):nullptr;
 if(!message||!report||message->status!=MessageStatus::delivered||!message->address_binding||!same(message->address_binding->original_business_fact_ref,{"interaction","communication_link",business->id})||!execution||!execution->actual_delivery_ref||!same(*execution->actual_delivery_ref,source.source_ref))return fail<ActorOfficialContentReceiptView>("OfficialPersonalMessageReceiptMissing",source.source_ref.id);
 const auto*binding=std::get_if<SocialS3BusinessLink>(&execution->source_binding);if(!binding||!same(binding->original_business_fact_ref,{"interaction","communication_link",business->id}))return fail<ActorOfficialContentReceiptView>("OfficialPersonalMessageBindingMismatch",source.source_ref.id);
 // This original reader revalidates immutable origin/content/address, actual
 // arrival and the historical authorized recipient, including the office epoch.
 auto proof=read_verified_official_delivery_source(common,links,source.source_ref.id,original,&report->id);if(!proof.ok)return {false,{},proof.error};
 if(proof.value.business!=business||proof.value.delivery!=delivery)return fail<ActorOfficialContentReceiptView>("OfficialPersonalContentMismatch",source.source_ref.id);
 auto at=results.source_moment_v2(source,stamp);if(!at.ok)return {false,{},at.error};auto same_at=compare_world_moments(at.value,delivery->delivered_at);if(!same_at.ok)return {false,{},same_at.error};if(same_at.value)return fail<ActorOfficialContentReceiptView>("OfficialPersonalDeliveryMomentMismatch",source.source_ref.id);
 auto known=read_knowledge_receipt_view_v2(common,social,results,{SubjectKind::actor,actor},report->id,stamp);if(!known.ok)return {false,{},known.error};if(!known.value||!known.value->original)return fail<ActorOfficialContentReceiptView>("OfficialPersonalKnowledgeReceiptMissing",source.source_ref.id);
 auto learned=compare_world_moments(known.value->learned_at,delivery->delivered_at);if(!learned.ok)return {false,{},learned.error};if(learned.value>0)return fail<ActorOfficialContentReceiptView>("OfficialPersonalKnowledgeAfterDisclosure",source.source_ref.id);
 return {true,{business,delivery,report,std::move(*known.value)},{}};
}
Outcome<ActorOfficialContentReceiptView> read_actor_official_content_receipt_before_source(const CommonStore&common,const SocialState&social,const CommunicationLinksState&links,const CommunicationSourceRead&original,const ExactResultTimeRead&results,const ExactResultReadStamp&stamp,const StableId&actor,const OfficialContentRef&expected,const MomentOwnerSource&delivery,const SourceRef&consumer){
 auto receipt=read_actor_official_content_receipt(common,social,links,original,results,stamp,actor,expected,delivery);if(!receipt.ok)return receipt;
 auto valid=original.validate_delivery_before_common_source(*receipt.value.delivery,consumer);return valid.ok?std::move(receipt):Outcome<ActorOfficialContentReceiptView>{false,{},valid.error};
}
}
