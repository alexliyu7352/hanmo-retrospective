#include "treaty_resource_signature_read.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
// 现已闭合的原Office签署路径。使者必须等原mandate＋Mission接受完整读写源，
// 不能由存在一个tag或两个ref绕过同Grant/同版/本人实际接受验证。
const ActingAuthority& direct_signer(const TreatySignatureSource& row){
 const auto* signer=std::get_if<TreatyDirectOfficeSigner>(&row.signer);
 if(!signer)fail("TreatyMandatedSignatureOriginalSourceRequired",row.id);
 return signer->authority;
}
bool actor(const ActingAuthority& a,const ActingAuthority& b){return a.actor_id==b.actor_id&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch;}
TreatyRatificationAuthorityContext authority(const TreatyResourceSignatureReadContext& x,const NativeTreatyRatificationOriginalRead2& sources){const auto& r=x.links.revisions;return {r.common,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,r.authority_bases,sources,r.budgets};}
void knowledge(const TreatyResourceSignatureReadContext& x,const TreatySignatureSource& r,const SourceRef* actual_boundary){
 auto content=take(read_treaty_revision_official_content(x.links.revisions,r.terms_source,1));
 auto known=take(actual_boundary?read_actor_known_treaty_revision_before_source(x.links.revisions,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(r),content,r.actual_content_receipt,*actual_boundary):read_actor_known_treaty_revision(x.links.revisions,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(r),content,r.actual_content_receipt));
 if(!known.actual_receipt.delivery||compare(known.actual_receipt.delivery->delivered_at,r.signed_at)>0)fail("TreatySignatureBeforeActualKnowledge",r.id);
}
// An AuthorityBasis consumer may enter the original reader without the full
// signature consumer. Keep actual personal knowledge here too, but never call
// the Basis reader: both knowledge paths only read original disclosures/links.
const TreatySignatureSource& original_identity(const TreatyResourceSignatureReadContext& x,const SourceRef& ref){
 const auto* row=take(read_treaty_resource_signature2_links(x.links,MomentOwnerSource{ref,2}));
 direct_signer(row->signature);knowledge(x,row->signature,&ref);
 take(validate_treaty_signature2_resource_knowledge(x.links,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,*row));
 return row->signature;
}
}
Outcome<WorldMoment> NativeTreatyRatificationOriginalRead2::read_organization_authority_now()const{return context_.links.revisions.clock.read_organization_authority_now();}
Outcome<TreatyRatificationAuthorityRequirements> NativeTreatyRatificationOriginalRead2::read_original_ratification_requirements(const SourceRef& source,Integer revision,const StableId& party,const ActingAuthority& acting,const SourceRef* boundary)const{try{
 auto actual=take(read_treaty_revision_source(context_.links.revisions,source,1));if(actual.terms_revision!=revision||!actual.definition)fail("TreatyTermsChanged",source.id);
 if(boundary){const auto& row=original_identity(context_,*boundary);if(!same_source_ref(row.terms_source,source)||row.terms_revision!=revision||row.party_id!=party||!actor(direct_signer(row),acting))fail("TreatyRatificationActualBoundaryMismatch",boundary->id);}
 auto result=take(context_.term_authorities.read_original_term_authority_requirements(actual,party,acting,boundary));
 if(result.original_terms!=&actual.definition->terms)fail("TreatyRatificationOriginalBodyMismatch",source.id);return {true,std::move(result),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyRatificationOriginalView> NativeTreatyRatificationOriginalRead2::read_actual_treaty_ratification(const SourceRef& ref)const{try{const auto& r=original_identity(context_,ref);return {true,{ref,r.terms_source,r.terms_revision,r.party_id,direct_signer(r),r.signed_at,{ref,1}},{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<AuthorityIssueSourceView> NativeTreatyRatificationOriginalRead2::read_authority_issue_source(const SourceRef& ref)const{try{
 const auto& row=original_identity(context_,ref);auto required=take(read_original_ratification_requirements(row.terms_source,row.terms_revision,row.party_id,direct_signer(row),&ref));ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=row.signed_at;
 return {true,{ref,direct_signer(row).actor_id,"ratify_treaty",std::move(at),std::move(required.primary.amount_components)},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<const TreatyResourceSignatureSource2*> read_treaty_resource_signature2(const TreatyResourceSignatureReadContext& x,const MomentOwnerSource& source){try{
 if(source.source_schema!=2)fail("TreatySignatureSchemaUnsupported",source.source_ref.id);
 const auto* r=take(read_treaty_resource_signature2_links(x.links,source));
 direct_signer(r->signature);knowledge(x,r->signature,&source.source_ref);
 take(validate_treaty_signature2_resource_knowledge(x.links,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,*r));
 NativeTreatyRatificationOriginalRead2 originals(x);
 take(read_treaty_ratification_authority(authority(x,originals),source.source_ref));
 for(const auto& other:x.links.signatures.rows())if(other.first!=r->signature.id&&other.second.signature.treaty_id==r->signature.treaty_id&&other.second.signature.terms_revision==r->signature.terms_revision&&other.second.signature.party_id==r->signature.party_id)fail("DuplicateTreatyPartySignature",r->signature.id);
 return {true,r,{}};
 }catch(const Failure& f){return {false,nullptr,f.error};}}
Status validate_treaty_resource_signature2_sources(const TreatyResourceSignatureReadContext& x){try{
 take(validate_treaty_resource_source_links(x.links));
 for(const auto& row:x.links.signatures.rows())take(read_treaty_resource_signature2(x,MomentOwnerSource{treaty_signature_source(row.second.signature),2}));
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
}
