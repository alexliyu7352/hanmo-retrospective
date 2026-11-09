#include "treaty_resource_signature2.hpp"
#include <algorithm>
namespace hanmo::resident {
SourceRef treaty_aid_resource_proposal_source(const TreatyAidResourceProposalSource&r){return {"politics","treaty_aid_resources_proposed",r.id};}
Status validate_treaty_aid_resource_proposal_shape(const TreatyAidResourceProposalSource&r){
 if(r.id.empty()||r.treaty_id.empty()||r.terms_revision<1||!valid_source_ref(r.terms_source)||r.terms_source.domain!="politics"||(r.terms_source.kind!="treaty_terms_offered"&&r.terms_source.kind!="treaty_terms_counteroffered")||r.proposer.actor_id.empty()||r.proposer.organization_id.empty()||r.proposer.office_id.empty()||r.proposer.authority_epoch<0||r.selections.empty())return common_failure("InvalidTreatyAidResourceProposal",r.id);
 auto at=encode_world_moment(r.proposed_at);if(!at.ok)return {false,{},at.error};
 for(std::size_t n=0;n<r.selections.size();++n){auto valid=validate_treaty_aid_resource_selection_shape(r.selections[n]);if(!valid.ok)return valid;for(std::size_t j=0;j<n;++j)if(r.selections[j].term_id==r.selections[n].term_id)return common_failure("DuplicateTreatyAidResourceProposalTerm",r.selections[n].term_id);}
 return common_success();
}
Status validate_treaty_resource_signature2_shape(const TreatyResourceSignatureSource2&r){
 auto valid=validate_treaty_signature_source_shape(r.signature);if(!valid.ok)return valid;
 if(bool(r.actual_aid_resource_proposal)!=bool(r.resource_knowledge))return common_failure("TreatyResourceKnowledgeCoverageMismatch",r.signature.id);
 if(r.resource_knowledge&&r.resource_knowledge->index()>1)return common_failure("InvalidTreatyResourceKnowledgeKind",r.signature.id);
 if(r.resource_knowledge)if(const auto*known=std::get_if<TreatyResourceDeliveredChoice>(&*r.resource_knowledge))if(!valid_source_ref(known->actual_receipt.source_ref)||known->actual_receipt.source_schema<1)return common_failure("InvalidTreatyResourceKnowledgeReceipt",r.signature.id);
 if(r.actual_aid_resource_proposal){const auto&a=*r.actual_aid_resource_proposal;if((a.source_schema!=1&&a.source_schema!=2)||!valid_source_ref(a.source_ref)||a.source_ref.domain!="politics"||a.source_ref.kind!="treaty_aid_resources_proposed")return common_failure("InvalidTreatyResourceSignatureProposalRef",r.signature.id);}
 return common_success();
}
Status validate_treaty_aid_resource_proposal_terms(const TreatyAidResourceProposalSource&r,const TreatyRevisionView&v){
 auto valid=validate_treaty_aid_resource_proposal_shape(r);if(!valid.ok)return valid;
 if(!v.definition||v.treaty_id!=r.treaty_id||v.terms_revision!=r.terms_revision||!same_source_ref(v.source,r.terms_source))return common_failure("TreatyAidResourceProposalRevisionMismatch",r.id);
 valid=validate_treaty_terms_shape(v.definition->terms);if(!valid.ok)return valid;
 const auto&t=v.definition->terms;if(std::find(t.parties.begin(),t.parties.end(),r.proposer.organization_id)==t.parties.end())return common_failure("TreatyAidResourceProposalPartyMismatch",r.id);
 auto at=compare_world_moments(v.issued_at,r.proposed_at);if(!at.ok)return {false,{},at.error};if(at.value>0)return common_failure("TreatyAidResourceProposalBeforeRevision",r.id);
 std::size_t count=0;for(const auto&term:t.terms)if(std::holds_alternative<TreatyMaterialAidTerm>(term.terms)){
  ++count;auto i=std::find_if(r.selections.begin(),r.selections.end(),[&](const auto&s){return s.term_id==term.term_id;});if(i==r.selections.end())return common_failure("TreatyAidResourceProposalCoverageMismatch",term.term_id);valid=validate_treaty_aid_resource_selection_for_term(*i,term);if(!valid.ok)return valid;
 }
 if(count!=r.selections.size())return common_failure("TreatyAidResourceProposalExtraTerm",r.id);return common_success();
}
Status validate_treaty_resource_signature2_selection(const TreatyResourceSignatureSource2&r,const TreatyRevisionView&v,const TreatyAidResourceProposalSource*p){
 auto valid=validate_treaty_resource_signature2_shape(r);if(!valid.ok)return valid;
 const auto&s=r.signature;if(!v.definition||v.treaty_id!=s.treaty_id||v.terms_revision!=s.terms_revision||!same_source_ref(v.source,s.terms_source))return common_failure("TreatyResourceSignatureRevisionMismatch",s.id);
 bool aid=false;for(const auto&t:v.definition->terms.terms)aid=aid||std::holds_alternative<TreatyMaterialAidTerm>(t.terms);
 if(aid!=bool(r.actual_aid_resource_proposal)||aid!=bool(p))return common_failure("TreatyResourceSignatureExplicitSelectionRequired",s.id);
 if(!aid)return common_success();if(r.actual_aid_resource_proposal->source_schema!=1)return common_failure("TreatyAidResourceProposalSchemaUnsupported",s.id);valid=validate_treaty_aid_resource_proposal_terms(*p,v);if(!valid.ok)return valid;
 if(!same_source_ref(r.actual_aid_resource_proposal->source_ref,treaty_aid_resource_proposal_source(*p)))return common_failure("TreatyResourceSignatureProposalMismatch",s.id);
 auto at=compare_world_moments(p->proposed_at,s.signed_at);if(!at.ok)return {false,{},at.error};if(at.value>0)return common_failure("TreatyResourceSignatureBeforeProposal",s.id);return common_success();
}
}
