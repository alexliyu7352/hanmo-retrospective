#include "treaty_resource_source_links.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
std::size_t position(const CommonStore&c,const SourceRef&ref){const auto*f=c.state().facts.find(ref.id);const auto&order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),ref.id);if(!f||f->id!=ref.id||f->domain!=ref.domain||f->kind!=ref.kind||i==order.end())fail("TreatyResourceActualFactMissing",ref.id);return static_cast<std::size_t>(i-order.begin());}
void before(const CommonStore&c,const SourceRef&a,const SourceRef&b){if(position(c,a)>=position(c,b))fail("TreatyResourceActualFactOrderMismatch",b.id);}
void time(const WorldMoment&a,const WorldMoment&b,const StableId&id){if(take(compare_world_moments(a,b))>0)fail("TreatyResourceActualMomentOrderMismatch",id);}
void revision_cut(const TreatyResourceSourceLinks&x,const TreatyRevisionView&v,const SourceRef&boundary,const WorldMoment&at){
 before(x.revisions.common,v.source,boundary);time(v.issued_at,at,boundary.id);time(at,take(x.revisions.clock.read_organization_authority_now()),boundary.id);
 if(!v.definition||take(compare_world_moments(at,WorldMoment{v.definition->terms.effective_window.until_tick,{}}))>=0)fail("TreatyResourceSourceAfterExpiry",boundary.id);
 for(const auto&other:x.revisions.counteroffers.rows())if(other.second.treaty_id==v.treaty_id&&other.second.terms_revision>v.terms_revision&&position(x.revisions.common,treaty_counteroffer_source(other.second))<position(x.revisions.common,boundary))fail("TreatyResourceSourceOnSupersededRevision",boundary.id);
}
const TreatyAidResourceProposalSource& proposal(const TreatyResourceSourceLinks&x,const MomentOwnerSource&ref){
 if(ref.source_schema!=1||ref.source_ref.domain!="politics"||ref.source_ref.kind!="treaty_aid_resources_proposed")fail("TreatyAidResourceProposalSchemaUnsupported",ref.source_ref.id);
 const auto*r=x.proposals.find(ref.source_ref.id);if(!r||r->id!=ref.source_ref.id)fail("TreatyAidResourceProposalSourceMissing",ref.source_ref.id);
 take(validate_treaty_aid_resource_proposal_shape(*r));auto v=take(read_treaty_revision_source(x.revisions,r->terms_source,1));take(validate_treaty_aid_resource_proposal_terms(*r,v));revision_cut(x,v,ref.source_ref,r->proposed_at);return *r;
}
const TreatyResourceSignatureSource2& signature(const TreatyResourceSourceLinks&x,const MomentOwnerSource&ref){
 if(ref.source_schema!=2||ref.source_ref.domain!="politics"||ref.source_ref.kind!="treaty_terms_ratified")fail("TreatyResourceSignatureSchemaUnsupported",ref.source_ref.id);
 const auto*r=x.signatures.find(ref.source_ref.id);if(!r||r->signature.id!=ref.source_ref.id)fail("TreatyResourceSignatureSourceMissing",ref.source_ref.id);
 take(validate_treaty_resource_signature2_shape(*r));const auto&s=r->signature;auto v=take(read_treaty_revision_source(x.revisions,s.terms_source,1));const TreatyAidResourceProposalSource*p=nullptr;
 if(r->actual_aid_resource_proposal){p=&proposal(x,*r->actual_aid_resource_proposal);before(x.revisions.common,r->actual_aid_resource_proposal->source_ref,ref.source_ref);}
 take(validate_treaty_resource_signature2_selection(*r,v,p));revision_cut(x,v,ref.source_ref,s.signed_at);
 if(std::find(v.definition->terms.parties.begin(),v.definition->terms.parties.end(),s.party_id)==v.definition->terms.parties.end())fail("TreatyResourceSignaturePartyMismatch",s.id);
 for(const auto&other:x.signatures.rows())if(other.first!=s.id){const auto&o=other.second.signature;if(o.treaty_id==s.treaty_id&&o.terms_revision==s.terms_revision){
  if(o.party_id==s.party_id)fail("DuplicateTreatyPartySignature",s.id);
  // Parties must sign the same immutable selected proposal, not two equal-ID
  // terms with different payer/recipient containers. No later substitution.
  const auto&left=r->actual_aid_resource_proposal;const auto&right=other.second.actual_aid_resource_proposal;
  if(bool(left)!=bool(right)||(left&&(left->source_schema!=right->source_schema||!same_source_ref(left->source_ref,right->source_ref))))fail("TreatyPartyResourceSelectionMismatch",s.id);
 }}
 return *r;
}
}
Outcome<const TreatyAidResourceProposalSource*> read_treaty_aid_resource_proposal_links(const TreatyResourceSourceLinks&x,const MomentOwnerSource&ref){try{return {true,&proposal(x,ref),{}};}catch(const Failure&f){return {false,nullptr,f.error};}}
Outcome<const TreatyResourceSignatureSource2*> read_treaty_resource_signature2_links(const TreatyResourceSourceLinks&x,const MomentOwnerSource&ref){try{return {true,&signature(x,ref),{}};}catch(const Failure&f){return {false,nullptr,f.error};}}
Status validate_treaty_resource_source_links(const TreatyResourceSourceLinks&x){try{
 for(const auto&r:x.proposals.rows()){if(r.first!=r.second.id)fail("TreatyAidResourceProposalIdentityMismatch",r.first);proposal(x,{treaty_aid_resource_proposal_source(r.second),1});}
 for(const auto&r:x.signatures.rows()){if(r.first!=r.second.signature.id)fail("TreatyResourceSignatureIdentityMismatch",r.first);signature(x,{treaty_signature_source(r.second.signature),2});}
 for(const auto&f:x.revisions.common.state().facts.rows())if(f.second.domain=="politics"){
  if(f.second.kind=="treaty_aid_resources_proposed"&&!x.proposals.find(f.first))fail("TreatyAidResourceProposalOrphanFact",f.first);
  if(f.second.kind=="treaty_terms_ratified"&&!x.signatures.find(f.first))fail("TreatyResourceSignatureOrphanFact",f.first);
 }return common_success();
 }catch(const Failure&f){return {false,{},f.error};}}
}
