#include "treaty_aid_first_signature2.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
[[noreturn]]void fail(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
}
Outcome<RatifyAndActivateTreatyResult> ratify_first_treaty_aid_selection_in_candidate2(
 CommonStore&common,OrderedTable<TreatyAidResourceProposalSource2>&proposals,
 OrderedTable<TreatyResourceSignatureSource2>&signatures,AuthorityBasisRegistry&bases,
 OrderedTable<TreatyActivationSource>&activations,const TreatyResourceActivationContext&x,
 TreatyActivationOriginalWrite&original,const RatifyTreatyTermsCommand&cmd,
 const std::vector<TreatyAidResourceSelection2>&selections){try{
 const auto&links=x.signatures.links;const auto&r=links.revisions;
 if(&common!=&r.common||&proposals!=&links.proposals||&signatures!=&links.signatures||&bases!=&r.authority_bases||&activations!=&x.activations)fail("TreatyAidFirstSignatureCandidateMismatch");
 auto current=take(read_current_treaty_revision(r,cmd.treaty_id));
 if(current.terms_revision!=cmd.terms_revision||!same_source_ref(current.source,cmd.terms_source))fail("TreatyTermsChanged",cmd.treaty_id);
 // A retry must use the actual frozen proposal, never create a second choice.
 for(const auto&p:proposals.rows())if(p.second.treaty_id==cmd.treaty_id&&p.second.terms_revision==cmd.terms_revision)fail("TreatyAidSelectionAlreadyFrozen",p.first);
 for(const auto&s:signatures.rows())if(s.second.signature.treaty_id==cmd.treaty_id&&s.second.signature.terms_revision==cmd.terms_revision)fail("TreatyAidFirstSignatureAlreadyExists",s.first);
 TreatyAidResourceProposalSource2 row;row.treaty_id=cmd.treaty_id;row.terms_revision=cmd.terms_revision;row.terms_source=cmd.terms_source;row.proposer=cmd.authority;row.proposed_at=take(r.clock.read_organization_authority_now());row.selections=selections;row.id=take(common.next_id("treaty_aid_resource_proposal"));
 take(validate_treaty_aid_resource_proposal2_terms(row,current));const auto ref=treaty_aid_resource_proposal2_source(row);
 take(common.append_fact({ref.id,ref.kind,ref.domain}));if(!proposals.insert(ref.id,std::move(row)))fail("DuplicateTreatyAidResourceProposal",ref.id);
 RatifyTreatyResourceTermsCommand2 signing{cmd,MomentOwnerSource{ref,2},TreatyResourceKnowledge{TreatyResourceOwnChoice{}}};
 // This is the permission boundary: original primary and ALL authored term
 // requirements, original knowledge, same Grant/Basis and activation/finance.
 // No standalone proposal command or provisional success is returned.
 auto out=take(ratify_and_activate_treaty_resource_in_candidate(common,signatures,bases,activations,x,original,signing));
 take(validate_treaty_resource_source_links(links));
 take(read_treaty_resource_signature2(x.signatures,{out.signature,2}));
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}
 catch(const std::bad_alloc&){return {false,{},common_failure("TreatyAidFirstSignatureResourceExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("TreatyAidFirstSignatureResourceExhausted").error};}}
}
