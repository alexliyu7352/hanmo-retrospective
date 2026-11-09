#include "treaty_resource_signature_write.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool acting(const ActingAuthority&a,const ActingAuthority&b){return a.actor_id==b.actor_id&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch;}
bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
bool knowledge_equal(const std::optional<TreatyResourceKnowledge>&a,const std::optional<TreatyResourceKnowledge>&b){
 if(bool(a)!=bool(b))return false;if(!a)return true;if(a->index()!=b->index())return false;
 if(const auto*d=std::get_if<TreatyResourceDeliveredChoice>(&*a))return source(d->actual_receipt,std::get<TreatyResourceDeliveredChoice>(*b).actual_receipt);
 return std::holds_alternative<TreatyResourceOwnChoice>(*a);
}
TreatyRatificationAuthorityContext authority(const TreatyResourceSignatureReadContext&x,const NativeTreatyRatificationOriginalRead2&original){const auto&r=x.links.revisions;return {r.common,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,r.authority_bases,original,r.budgets};}
void current_knowledge(const TreatyResourceSignatureReadContext&x,const TreatyResourceSignatureSource2&row,const TreatyRevisionView&revision){
 const auto&s=row.signature;
 auto content=take(read_treaty_revision_official_content(x.links.revisions,s.terms_source,1));
 auto known=take(read_actor_known_treaty_revision(x.links.revisions,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(s),content,s.actual_content_receipt));
 if(!known.actual_receipt.delivery||compare(known.actual_receipt.delivery->delivered_at,s.signed_at)>0)fail("TreatySignatureBeforeActualKnowledge",s.treaty_id);
 const TreatyAidResourceProposalSource2*p=nullptr;if(row.actual_aid_resource_proposal)p=take(read_treaty_aid_resource_proposal2_links(x.links,*row.actual_aid_resource_proposal));
 take(validate_treaty_resource_signature2_selection(row,revision,p));if(!p)return;
 if(!row.resource_knowledge)fail("TreatyResourceKnowledgeCoverageMismatch",s.treaty_id);
 if(std::holds_alternative<TreatyResourceOwnChoice>(*row.resource_knowledge)){
  const auto*signer=std::get_if<TreatyDirectOfficeSigner>(&s.signer);
  if(!signer||!acting(signer->authority,p->proposer)||compare(p->proposed_at,s.signed_at))fail("TreatyResourceOwnChoiceOriginalActorMismatch",s.treaty_id);
 }else{
  const auto official=take(read_treaty_aid_proposal_official_content(x.links,*row.actual_aid_resource_proposal));
  const auto*receipt=std::get_if<TreatyResourceDeliveredChoice>(&*row.resource_knowledge);if(!receipt)fail("InvalidTreatyResourceKnowledgeKind",s.treaty_id);
  auto resource_known=take(read_actor_known_treaty_aid_proposal(x.links,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(s),official,receipt->actual_receipt));
  if(resource_known.original!=p||!resource_known.actual_receipt.delivery||compare(resource_known.actual_receipt.delivery->delivered_at,s.signed_at)>0)fail("TreatyResourceSignatureBeforeActualDisclosure",s.treaty_id);
 }
}
}
Outcome<SourceRef> ratify_treaty_resource_terms_in_candidate(CommonStore&c,
 OrderedTable<TreatyResourceSignatureSource2>&rows,AuthorityBasisRegistry&bases,
 const TreatyResourceSignatureReadContext&x,const RatifyTreatyResourceTermsCommand2&command){try{
 auto shape=validate_ratify_treaty_resource_command2_shape(command);if(!shape.ok)throw Failure{shape.error};
 const auto&r=x.links.revisions;const auto&cmd=command.ratification;
 if(&c!=&r.common||&rows!=&x.links.signatures||&bases!=&r.authority_bases||!r.organization_catalog.exact_fact_clock_enabled||!r.organization_catalog.authority_lifecycle_enabled)fail("TreatyRatificationCandidateMismatch");
 auto current=take(read_current_treaty_revision(r,cmd.treaty_id));
 if(current.terms_revision!=cmd.terms_revision||!same_source_ref(current.source,cmd.terms_source))fail("TreatyTermsChanged",cmd.treaty_id);
 for(const auto&old:rows.rows())if(old.second.signature.treaty_id==cmd.treaty_id&&old.second.signature.terms_revision==cmd.terms_revision&&old.second.signature.party_id==cmd.authority.organization_id){
  const auto ref=treaty_signature_source(old.second.signature);take(read_treaty_resource_signature2(x,{ref,2}));
  const auto*signer=std::get_if<TreatyDirectOfficeSigner>(&old.second.signature.signer);
  if(!signer||!acting(signer->authority,cmd.authority)||!source(old.second.signature.actual_content_receipt,cmd.actual_content_receipt)||
     bool(old.second.actual_aid_resource_proposal)!=bool(command.actual_aid_resource_proposal)||
     (command.actual_aid_resource_proposal&&!source(*old.second.actual_aid_resource_proposal,*command.actual_aid_resource_proposal))||
     !knowledge_equal(old.second.resource_knowledge,command.resource_knowledge))fail("TreatyPartyAlreadyRatified",old.first);
  return {true,ref,{}};
 }
 TreatyResourceSignatureSource2 row;auto&s=row.signature;
 s.treaty_id=cmd.treaty_id;s.terms_revision=cmd.terms_revision;s.terms_source=cmd.terms_source;s.party_id=cmd.authority.organization_id;
 s.signer=TreatyDirectOfficeSigner{cmd.authority};s.actual_content_receipt=cmd.actual_content_receipt;s.signed_at=take(r.clock.read_organization_authority_now());
 row.actual_aid_resource_proposal=command.actual_aid_resource_proposal;row.resource_knowledge=command.resource_knowledge;
 // A real allocated identity is required by the original selected-source shape;
 // allocation, knowledge, Basis and adoption all remain in the same candidate.
 s.id=take(c.next_id("treaty_signature"));current_knowledge(x,row,current);
 NativeTreatyRatificationOriginalRead2 originals{x};auto auth=authority(x,originals);
 auto captured=take(capture_treaty_ratification_authority(auth,s.terms_source,s.terms_revision,s.party_id,cmd.authority));
 take(validate_treaty_ratification_authority_capture(auth,s.terms_source,s.terms_revision,s.party_id,cmd.authority,captured));
 take(validate_treaty_resource_signature2_shape(row));const auto ref=treaty_signature_source(s);
 take(c.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatySignatureSource",ref.id);
 // Original Basis remains /1. Signature/2 is a different source schema, not a
 // reason to relabel or bypass the original registry and same-Grant proof.
 take(register_authority_basis_for_fact(c,bases,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,captured.primary,captured.authority,ref,originals,{},r.budgets));
 take(read_treaty_resource_signature2(x,{ref,2}));return {true,ref,{}};
 }catch(const Failure&e){return {false,{},e.error};}
 catch(const std::bad_alloc&){return {false,{},common_failure("TreatyResourceSignatureResourceExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("TreatyResourceSignatureResourceExhausted").error};}}
Outcome<RatifyAndActivateTreatyResult> ratify_and_activate_treaty_resource_in_candidate(
 CommonStore&common,OrderedTable<TreatyResourceSignatureSource2>&signatures,AuthorityBasisRegistry&bases,
 OrderedTable<TreatyActivationSource>&activations,const TreatyResourceActivationContext&x,
 TreatyActivationOriginalWrite&original,const RatifyTreatyResourceTermsCommand2&command){try{
 RatifyAndActivateTreatyResult out;out.signature=take(ratify_treaty_resource_terms_in_candidate(common,signatures,bases,x.signatures,command));
 auto ready=take(read_current_treaty_resource_activation_ready(x,command.ratification.treaty_id));if(!ready)return {true,std::move(out),{}};
 const auto now=take(x.signatures.links.revisions.clock.read_organization_authority_now());
 if(compare(ready->activation_at,now)<=0)out.activation=take(activate_treaty_resource_terms_in_candidate(common,activations,x,original,command.ratification.treaty_id));
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
}
