#include "treaty_aid_proposal_official.hpp"
namespace hanmo::resident {
Status validate_treaty_signature2_resource_knowledge(const TreatyResourceSourceLinks2&x,const SocialState&social,const CommunicationLinksState&links,const CommunicationSourceRead&communications,const ExactResultTimeRead&times,const ExactResultReadStamp&stamp,const TreatyResourceSignatureSource2&row){
 const auto source=treaty_signature_source(row.signature);
 auto original=read_treaty_resource_signature2_links(x,{source,2});if(!original.ok)return {false,{},original.error};if(original.value!=&row)return common_failure("TreatyResourceSignatureForeignRow",source.id);
 if(!row.actual_aid_resource_proposal)return common_success();
 auto proposal=read_treaty_aid_resource_proposal2_links(x,*row.actual_aid_resource_proposal);if(!proposal.ok)return {false,{},proposal.error};
 if(!row.resource_knowledge)return common_failure("TreatyResourceKnowledgeCoverageMismatch",source.id);
 if(std::holds_alternative<TreatyResourceOwnChoice>(*row.resource_knowledge)){
  const auto*signer=std::get_if<TreatyDirectOfficeSigner>(&row.signature.signer);const auto&proposer=proposal.value->proposer;
  if(!signer||signer->authority.actor_id!=proposer.actor_id||signer->authority.organization_id!=proposer.organization_id||signer->authority.office_id!=proposer.office_id||signer->authority.authority_epoch!=proposer.authority_epoch)return common_failure("TreatyResourceOwnChoiceOriginalActorMismatch",source.id);
  auto at=compare_world_moments(proposal.value->proposed_at,row.signature.signed_at);if(!at.ok)return {false,{},at.error};if(at.value)return common_failure("TreatyResourceOwnChoiceNotSameCandidateMoment",source.id);
  // Actual original-source links already proved proposal fact precedes this
  // signature. This is personal knowledge of one's own choice, not a grant.
  return common_success();
 }
 const auto*delivered=std::get_if<TreatyResourceDeliveredChoice>(&*row.resource_knowledge);if(!delivered)return common_failure("InvalidTreatyResourceKnowledgeKind",source.id);
 auto content=read_treaty_aid_proposal_official_content(x,*row.actual_aid_resource_proposal);if(!content.ok)return {false,{},content.error};
 auto known=read_actor_known_treaty_aid_proposal_before_source(x,social,links,communications,times,stamp,treaty_signature_actor(row.signature),content.value,delivered->actual_receipt,source);if(!known.ok)return {false,{},known.error};
 if(known.value.original!=proposal.value||!known.value.actual_receipt.delivery)return common_failure("TreatyResourceKnowledgeOriginalMismatch",source.id);
 auto at=compare_world_moments(known.value.actual_receipt.delivery->delivered_at,row.signature.signed_at);if(!at.ok)return {false,{},at.error};if(at.value>0)return common_failure("TreatyResourceSignatureBeforeActualDisclosure",source.id);return common_success();
}
}
