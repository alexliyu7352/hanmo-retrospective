#include "treaty_signature_sources.hpp"
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
std::size_t position(const CommonStore& c,const SourceRef& s){const auto* head=c.state().facts.find(s.id);const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),s.id);if(!head||head->domain!=s.domain||head->kind!=s.kind||i==order.end())fail("TreatySignatureActualSourceMissing",s.id);return static_cast<std::size_t>(i-order.begin());}
const TreatySignatureSource& raw(const TreatySignatureContext& x,const SourceRef& ref){
 if(ref.domain!="politics"||ref.kind!="treaty_terms_ratified"||!valid_source_ref(ref))fail("TreatySignatureSourceUnsupported",ref.id);
 const auto* r=x.signatures.find(ref.id);if(!r||r->id!=ref.id)fail("TreatySignatureSourceMissing",ref.id);take(validate_treaty_signature_source_shape(*r));
 auto revision=take(read_treaty_revision_source(x.revisions,r->terms_source,1));
 if(revision.treaty_id!=r->treaty_id||revision.terms_revision!=r->terms_revision||!revision.definition||
    std::find(revision.definition->terms.parties.begin(),revision.definition->terms.parties.end(),r->party_id)==revision.definition->terms.parties.end()||
    position(x.revisions.common,revision.source)>=position(x.revisions.common,ref)||compare(revision.issued_at,r->signed_at)>0||
    compare(r->signed_at,WorldMoment{revision.definition->terms.effective_window.until_tick,{}})>=0||
    compare(r->signed_at,take(x.revisions.clock.read_organization_authority_now()))>0)fail("TreatySignatureOriginalRevisionMismatch",ref.id);
 // A later counteroffer preserves this signature as history but cannot inherit
 // it. At this actual signature cut, there must not already be a newer revision.
 for(const auto& other:x.revisions.counteroffers.rows())if(other.second.treaty_id==r->treaty_id&&other.second.terms_revision>r->terms_revision&&position(x.revisions.common,treaty_counteroffer_source(other.second))<position(x.revisions.common,ref))fail("TreatySignatureOnSupersededRevision",ref.id);
 return *r;
}
TreatyRatificationAuthorityContext authority(const TreatySignatureContext& x,const NativeTreatyRatificationOriginalRead& sources){const auto& r=x.revisions;return {r.common,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,r.authority_bases,sources,r.budgets};}
void knowledge(const TreatySignatureContext& x,const TreatySignatureSource& r,const SourceRef* actual_boundary){
 auto content=take(read_treaty_revision_official_content(x.revisions,r.terms_source,1));
 auto known=take(actual_boundary?read_actor_known_treaty_revision_before_source(x.revisions,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(r),content,r.actual_content_receipt,*actual_boundary):read_actor_known_treaty_revision(x.revisions,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,treaty_signature_actor(r),content,r.actual_content_receipt));
 if(!known.actual_receipt.delivery||compare(known.actual_receipt.delivery->delivered_at,r.signed_at)>0)fail("TreatySignatureBeforeActualKnowledge",r.id);
}
}
SourceRef treaty_signature_source(const TreatySignatureSource& r){return {"politics","treaty_terms_ratified",r.id};}
const StableId& treaty_signature_actor(const TreatySignatureSource& r){return std::visit([](const auto& signer)->const StableId&{using T=std::decay_t<decltype(signer)>;if constexpr(std::is_same_v<T,TreatyDirectOfficeSigner>)return signer.authority.actor_id;else return signer.actor_id;},r.signer);}
Status validate_treaty_signature_source_shape(const TreatySignatureSource& r){
 if(r.id.empty()||r.treaty_id.empty()||r.terms_revision<1||r.party_id.empty()||treaty_signature_actor(r).empty()||!valid_source_ref(r.terms_source)||r.terms_source.domain!="politics"||(r.terms_source.kind!="treaty_terms_offered"&&r.terms_source.kind!="treaty_terms_counteroffered")||!valid_source_ref(r.actual_content_receipt.source_ref)||r.actual_content_receipt.source_schema!=1)return common_failure("InvalidTreatySignatureSource",r.id);
 if(const auto* direct=std::get_if<TreatyDirectOfficeSigner>(&r.signer)){
  if(direct->authority.organization_id!=r.party_id||direct->authority.office_id.empty()||direct->authority.authority_epoch<0)return common_failure("InvalidTreatyDirectOfficeSigner",r.id);
 }else{
  const auto& mandated=std::get<TreatyMandatedSigner>(r.signer);
  if(!valid_source_ref(mandated.actual_mandate_ref.source_ref)||mandated.actual_mandate_ref.source_schema<1||
     !valid_source_ref(mandated.actual_task_acceptance_ref.source_ref)||mandated.actual_task_acceptance_ref.source_schema<1||
     same_source_ref(mandated.actual_mandate_ref.source_ref,mandated.actual_task_acceptance_ref.source_ref))return common_failure("InvalidTreatyMandatedSigner",r.id);
 }
 auto at=encode_world_moment(r.signed_at);return at.ok?common_success():Status{false,{},at.error};
}
Outcome<WorldMoment> NativeTreatyRatificationOriginalRead::read_organization_authority_now()const{return context_.revisions.clock.read_organization_authority_now();}
Outcome<TreatyRatificationAuthorityRequirements> NativeTreatyRatificationOriginalRead::read_original_ratification_requirements(const SourceRef& source,Integer revision,const StableId& party,const ActingAuthority& acting,const SourceRef* boundary)const{try{
 auto actual=take(read_treaty_revision_source(context_.revisions,source,1));if(actual.terms_revision!=revision||!actual.definition)fail("TreatyTermsChanged",source.id);
 if(boundary){const auto& row=raw(context_,*boundary);if(!same_source_ref(row.terms_source,source)||row.terms_revision!=revision||row.party_id!=party||!actor(direct_signer(row),acting))fail("TreatyRatificationActualBoundaryMismatch",boundary->id);}
 auto result=take(context_.term_authorities.read_original_term_authority_requirements(actual,party,acting,boundary));
 if(result.original_terms!=&actual.definition->terms)fail("TreatyRatificationOriginalBodyMismatch",source.id);return {true,std::move(result),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyRatificationOriginalView> NativeTreatyRatificationOriginalRead::read_actual_treaty_ratification(const SourceRef& ref)const{try{const auto& r=raw(context_,ref);return {true,{ref,r.terms_source,r.terms_revision,r.party_id,direct_signer(r),r.signed_at,{ref,1}},{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<AuthorityIssueSourceView> NativeTreatyRatificationOriginalRead::read_authority_issue_source(const SourceRef& ref)const{try{
 const auto& row=raw(context_,ref);auto required=take(read_original_ratification_requirements(row.terms_source,row.terms_revision,row.party_id,direct_signer(row),&ref));ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=row.signed_at;
 return {true,{ref,direct_signer(row).actor_id,"ratify_treaty",std::move(at),std::move(required.primary.amount_components)},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<const TreatySignatureSource*> read_treaty_signature_source(const TreatySignatureContext& x,const SourceRef& ref,Integer schema){try{
 if(schema!=1)fail("TreatySignatureSchemaUnsupported",ref.id);const auto& r=raw(x,ref);direct_signer(r);knowledge(x,r,&ref);
 NativeTreatyRatificationOriginalRead sources(x);take(read_treaty_ratification_authority(authority(x,sources),ref));
 for(const auto& other:x.signatures.rows())if(other.first!=r.id&&other.second.treaty_id==r.treaty_id&&other.second.terms_revision==r.terms_revision&&other.second.party_id==r.party_id)fail("DuplicateTreatyPartySignature",r.id);
 return {true,&r,{}};
 }catch(const Failure& f){return {false,nullptr,f.error};}}
Status validate_treaty_signature_sources(const TreatySignatureContext& x){try{
 for(const auto& row:x.signatures.rows()){if(row.first!=row.second.id)fail("TreatySignatureIdentityMismatch",row.first);take(read_treaty_signature_source(x,treaty_signature_source(row.second),1));}
 for(const auto& fact:x.revisions.common.state().facts.rows())if(fact.second.domain=="politics"&&fact.second.kind=="treaty_terms_ratified"&&!x.signatures.find(fact.first))fail("TreatySignatureOrphanFact",fact.first);return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> ratify_treaty_terms_in_candidate(CommonStore& c,OrderedTable<TreatySignatureSource>& rows,AuthorityBasisRegistry& bases,const TreatySignatureContext& x,const RatifyTreatyTermsCommand& command){try{
 const auto& r=x.revisions;if(&c!=&r.common||&rows!=&x.signatures||&bases!=&r.authority_bases||!r.organization_catalog.exact_fact_clock_enabled||!r.organization_catalog.authority_lifecycle_enabled)fail("TreatyRatificationCandidateMismatch");
 auto current=take(read_current_treaty_revision(r,command.treaty_id));if(current.terms_revision!=command.terms_revision||!same_source_ref(current.source,command.terms_source))fail("TreatyTermsChanged",command.treaty_id);
 for(const auto& old:rows.rows())if(old.second.treaty_id==command.treaty_id&&old.second.terms_revision==command.terms_revision&&old.second.party_id==command.authority.organization_id){
  const auto ref=treaty_signature_source(old.second);take(read_treaty_signature_source(x,ref,1));
  if(!actor(direct_signer(old.second),command.authority)||old.second.actual_content_receipt.source_schema!=command.actual_content_receipt.source_schema||!same_source_ref(old.second.actual_content_receipt.source_ref,command.actual_content_receipt.source_ref))fail("TreatyPartyAlreadyRatified",old.first);
  return {true,ref,{}};
 }
 TreatySignatureSource row;row.treaty_id=command.treaty_id;row.terms_revision=command.terms_revision;row.terms_source=command.terms_source;row.party_id=command.authority.organization_id;row.signer=TreatyDirectOfficeSigner{command.authority};row.actual_content_receipt=command.actual_content_receipt;row.signed_at=take(r.clock.read_organization_authority_now());knowledge(x,row,nullptr);
 NativeTreatyRatificationOriginalRead sources(x);auto auth=authority(x,sources);auto captured=take(capture_treaty_ratification_authority(auth,row.terms_source,row.terms_revision,row.party_id,direct_signer(row)));
 take(validate_treaty_ratification_authority_capture(auth,row.terms_source,row.terms_revision,row.party_id,direct_signer(row),captured));
 row.id=take(c.next_id("treaty_signature"));take(validate_treaty_signature_source_shape(row));const auto ref=treaty_signature_source(row);take(c.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatySignatureSource",ref.id);
 take(register_authority_basis_for_fact(c,bases,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,captured.primary,captured.authority,ref,sources,{},r.budgets));
 take(read_treaty_signature_source(x,ref,1));return {true,ref,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
