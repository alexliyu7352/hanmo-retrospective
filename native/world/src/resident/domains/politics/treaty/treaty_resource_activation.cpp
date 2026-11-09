#include "treaty_resource_activation.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){const auto* head=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),ref.id);if(!head||head->domain!=ref.domain||head->kind!=ref.kind||i==order.end())fail("TreatyActivationActualSourceMissing",ref.id);return static_cast<std::size_t>(i-order.begin());}
std::optional<TreatyActivationReadyView> ready(const TreatyResourceActivationContext& x,const TreatyRevisionView& revision,const SourceRef* boundary){
 if(!revision.definition)fail("TreatyActivationOriginalTermsRequired",revision.treaty_id);
 TreatyActivationReadyView out;out.revision=revision;out.activation_at={revision.definition->terms.effective_window.from_tick,{}};
 const auto& common=x.signatures.links.revisions.common;
 for(const auto& party:revision.definition->terms.parties){const TreatySignatureSource* found=nullptr;
  for(const auto& row:x.signatures.links.signatures.rows())if(row.second.signature.treaty_id==revision.treaty_id&&row.second.signature.terms_revision==revision.terms_revision&&row.second.signature.party_id==party){if(found)fail("DuplicateTreatyPartySignature",party);found=&take(read_treaty_resource_signature2(x.signatures,MomentOwnerSource{treaty_signature_source(row.second.signature),2}))->signature;}
  if(!found)return std::nullopt;
  const auto ref=treaty_signature_source(*found);if(!same_source_ref(found->terms_source,revision.source))fail("TreatyActivationSignatureRevisionMismatch",found->id);
  if(boundary&&position(common,ref)>=position(common,*boundary))fail("TreatyActivationBeforeSignature",found->id);
  if(compare(found->signed_at,out.activation_at)>0)out.activation_at=found->signed_at;
  out.signature_refs.push_back(ref);
 }
 if(!take(treaty_window_contains(revision.definition->terms.effective_window,out.activation_at)))fail("TreatyActivationOutsideWindow",revision.treaty_id);
 return out;
}
const TreatyActivationSource* existing(const TreatyResourceActivationContext& x,const StableId& treaty,Integer revision){const TreatyActivationSource* found=nullptr;for(const auto& row:x.activations.rows())if(row.second.treaty_id==treaty&&row.second.terms_revision==revision){if(found)fail("DuplicateTreatyActivation",treaty);found=&row.second;}return found;}
void same_refs(const std::vector<SourceRef>& a,const std::vector<SourceRef>& b,const StableId& id){if(a.size()!=b.size())fail("TreatyActivationRecognitionIncomplete",id);for(std::size_t i=0;i<a.size();++i)if(!same_source_ref(a[i],b[i]))fail("TreatyActivationRecognitionOrderMismatch",id);}
}
Outcome<std::optional<TreatyActivationReadyView>> read_current_treaty_resource_activation_ready(const TreatyResourceActivationContext& x,const StableId& treaty){try{
 auto revision=take(read_current_treaty_revision(x.signatures.links.revisions,treaty));auto result=ready(x,revision,nullptr);return {true,std::move(result),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyActivationView> read_original_treaty_resource_activation(const TreatyResourceActivationContext& x,const SourceRef& ref,Integer schema){try{
 if(schema!=2||ref.domain!="politics"||ref.kind!="treaty_terms_activated"||!valid_source_ref(ref))fail("TreatyActivationSourceUnsupported",ref.id);
 const auto* r=x.activations.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyActivationSourceMissing",ref.id);take(validate_treaty_activation_shape(*r));const auto& common=x.signatures.links.revisions.common;position(common,ref);
 auto revision=take(read_treaty_revision_source(x.signatures.links.revisions,r->terms_source,1));if(revision.treaty_id!=r->treaty_id||revision.terms_revision!=r->terms_revision||position(common,revision.source)>=position(common,ref))fail("TreatyActivationRevisionMismatch",ref.id);
 auto recognition=ready(x,revision,&ref);if(!recognition)fail("TreatyActivationRecognitionIncomplete",ref.id);same_refs(r->signature_refs,recognition->signature_refs,ref.id);
 if(compare(r->activated_at,recognition->activation_at)||compare(r->activated_at,take(x.signatures.links.revisions.clock.read_organization_authority_now()))>0)fail("TreatyActivationMomentMismatch",ref.id);
 for(const auto& next:x.signatures.links.revisions.counteroffers.rows())if(next.second.treaty_id==r->treaty_id&&next.second.terms_revision>r->terms_revision&&position(common,treaty_counteroffer_source(next.second))<position(common,ref))fail("TreatyActivationSupersededRevision",ref.id);
 if(existing(x,r->treaty_id,r->terms_revision)!=r)fail("TreatyActivationIdentityMismatch",ref.id);
 // No downstream owner callback here: its original result reader may safely
 // borrow this fully proved source while validating actual term adoption.
 return {true,{r,std::move(revision)},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyActivationView> read_verified_treaty_resource_activation(const TreatyResourceActivationContext& x,const SourceRef& ref,Integer schema){try{
 auto r=take(read_original_treaty_resource_activation(x,ref,schema));auto recognition=ready(x,r.revision,&ref);if(!recognition)fail("TreatyActivationRecognitionIncomplete",ref.id);
 take(x.originals.validate_original_activation_inputs(*recognition,&ref));take(x.originals.validate_applied_treaty_activation(r));return {true,std::move(r),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_resource_activation_sources(const TreatyResourceActivationContext& x){try{
 take(validate_treaty_resource_signature2_sources(x.signatures));const auto now=take(x.signatures.links.revisions.clock.read_organization_authority_now());
 for(const auto& row:x.activations.rows()){if(row.first!=row.second.id)fail("TreatyActivationIdentityMismatch",row.first);take(read_verified_treaty_resource_activation(x,treaty_activation_source(row.second),2));}
 for(const auto& row:x.signatures.links.revisions.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="treaty_terms_activated"&&!x.activations.find(row.first))fail("TreatyActivationOrphanFact",row.first);
 // The stable-current revision cannot lose its activation merely by deleting
 // the source row and all downstream results together.
 for(const auto& offer:x.signatures.links.revisions.offers.rows()){auto r=take(read_current_treaty_resource_activation_ready(x,offer.first));if(r&&compare(r->activation_at,now)<=0&&!existing(x,offer.first,r->revision.terms_revision))fail("TreatyActivationMissingAtActualDue",offer.first);}
 // A later counteroffer cannot conceal an activation already due in the old
 // revision. Its actual append boundary also proves all prior signatures.
 for(const auto& next:x.signatures.links.revisions.counteroffers.rows()){
  const auto cut=treaty_counteroffer_source(next.second);auto previous=take(read_treaty_revision_source(x.signatures.links.revisions,next.second.previous_terms_source,1));auto r=ready(x,previous,&cut);
  if(r&&compare(r->activation_at,next.second.issued_at)<=0&&!existing(x,r->revision.treaty_id,r->revision.terms_revision))fail("TreatyActivationMissingBeforeCounteroffer",next.first);
 }
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> activate_treaty_resource_terms_in_candidate(CommonStore& common,OrderedTable<TreatyActivationSource>& rows,const TreatyResourceActivationContext& x,TreatyActivationOriginalWrite& original,const StableId& treaty){try{
 if(&common!=&x.signatures.links.revisions.common||&rows!=&x.activations)fail("TreatyActivationCandidateMismatch",treaty);
 auto current=take(read_current_treaty_revision(x.signatures.links.revisions,treaty));if(const auto* old=existing(x,treaty,current.terms_revision)){auto ref=treaty_activation_source(*old);take(read_verified_treaty_resource_activation(x,ref,2));return {true,ref,{}};}
 auto r=take(read_current_treaty_resource_activation_ready(x,treaty));if(!r)fail("TreatyRecognitionIncomplete",treaty);const auto now=take(x.signatures.links.revisions.clock.read_organization_authority_now());if(compare(r->activation_at,now))fail("TreatyActivationNotAtActualDue",treaty);take(x.originals.validate_original_activation_inputs(*r,nullptr));
 TreatyActivationSource row;row.id=take(common.next_id("treaty_activation"));row.treaty_id=treaty;row.terms_revision=r->revision.terms_revision;row.terms_source=r->revision.source;row.signature_refs=r->signature_refs;row.activated_at=now;take(validate_treaty_activation_shape(row));const auto ref=treaty_activation_source(row);
 take(common.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatyActivation",ref.id);
 auto actual=take(read_original_treaty_resource_activation(x,ref,2));take(original.apply_treaty_activation_in_candidate(common,actual));take(read_verified_treaty_resource_activation(x,ref,2));return {true,ref,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
