#include "treaty_resource_termination.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind||i==order.end())fail("TreatyTerminationActualSourceMissing",ref.id);return static_cast<std::size_t>(i-order.begin());}
void refs(const std::vector<SourceRef>& list){if(list.empty())fail("TreatyTerminationOriginalCauseRequired");for(std::size_t i=0;i<list.size();++i){if(!valid_source_ref(list[i]))fail("TreatyTerminationInvalidCauseSource",list[i].id);for(std::size_t j=0;j<i;++j)if(same_source_ref(list[i],list[j]))fail("TreatyTerminationDuplicateCauseSource",list[i].id);}}
const TreatyTerminationSource* existing(const TreatyResourceTerminationContext& x,const SourceRef& activation){const TreatyTerminationSource* result=nullptr;for(const auto& row:x.terminations.rows())if(same_source_ref(row.second.activation_ref,activation)){if(result)fail("DuplicateTreatyTermination",activation.id);result=&row.second;}return result;}
void cause_shape(const TreatyTerminationCause& cause){std::visit([](const auto& c){using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,TreatyMutualReleaseTermination>)refs(c.release_acceptance_refs);else if constexpr(std::is_same_v<T,TreatyBreachAfterNoticeTermination>){if(!valid_source_ref(c.incident_ref)||!valid_source_ref(c.actual_decision_ref))fail("TreatyTerminationOriginalCauseRequired");refs(c.notice_refs);}},cause);}
void cause(const TreatyResourceTerminationContext& x,const TreatyActivationView& a,const TreatyTerminationCause& c,const WorldMoment& at,const SourceRef* boundary){
 if(!a.source||!a.revision.definition)fail("TreatyTerminationActivationRequired");cause_shape(c);const auto& terms=a.revision.definition->terms;
 if(compare(at,a.source->activated_at)<0||compare(at,WorldMoment{terms.effective_window.until_tick,{}})>0)fail("TreatyTerminationOutsideOriginalWindow",a.source->id);
 const auto method=static_cast<TreatyTerminationMethod>(c.index());
 // The original until endpoint always ends validity. The selected methods
 // restrict optional early exit; omission cannot extend a finite window.
 if(method!=TreatyTerminationMethod::by_expiry&&std::find(terms.termination_policy.begin(),terms.termination_policy.end(),method)==terms.termination_policy.end())fail("TreatyExitNotPermitted",a.source->id);
 if(std::holds_alternative<TreatyExpiryTermination>(c)){
  if(compare(at,WorldMoment{terms.effective_window.until_tick,{}}))fail("TreatyExpiryNotAtOriginalBoundary",a.source->id);
 }else{
  if(compare(at,WorldMoment{terms.effective_window.until_tick,{}})>=0)fail("TreatyManualTerminationAfterExpiry",a.source->id);
  // The real original evidence owner supplies precise source times/receivers,
  // historical authority and same-version notice rules. Common position alone
  // is not an assertion of consent or of a valid material breach decision.
  take(x.originals.validate_original_treaty_termination_cause(a,c,at,boundary));
 }
}
bool same_refs(const std::vector<SourceRef>& a,const std::vector<SourceRef>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_source_ref(a[i],b[i]))return false;return true;}
bool same_cause(const TreatyTerminationCause& a,const TreatyTerminationCause& b){if(a.index()!=b.index())return false;if(std::holds_alternative<TreatyExpiryTermination>(a))return true;if(auto* r=std::get_if<TreatyMutualReleaseTermination>(&a))return same_refs(r->release_acceptance_refs,std::get<TreatyMutualReleaseTermination>(b).release_acceptance_refs);const auto& r=std::get<TreatyBreachAfterNoticeTermination>(a);const auto& s=std::get<TreatyBreachAfterNoticeTermination>(b);return same_source_ref(r.incident_ref,s.incident_ref)&&same_refs(r.notice_refs,s.notice_refs)&&same_source_ref(r.actual_decision_ref,s.actual_decision_ref);}
}
Outcome<TreatyTerminationView> read_original_treaty_resource_termination(const TreatyResourceTerminationContext& x,const SourceRef& ref,Integer schema){try{
 if(schema!=2||ref.domain!="politics"||ref.kind!="treaty_terms_terminated"||!valid_source_ref(ref))fail("TreatyTerminationSourceUnsupported",ref.id);const auto* r=x.terminations.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyTerminationSourceMissing",ref.id);take(validate_treaty_termination_shape(*r));
 auto a=take(read_original_treaty_resource_activation(x.activations,r->activation_ref,2));const auto& common=x.activations.signatures.links.revisions.common;if(r->treaty_id!=a.source->treaty_id||r->terms_revision!=a.source->terms_revision||position(common,r->activation_ref)>=position(common,ref))fail("TreatyTerminationActivationMismatch",ref.id);
 if(compare(r->terminated_at,take(x.activations.signatures.links.revisions.clock.read_organization_authority_now()))>0||existing(x,r->activation_ref)!=r)fail("TreatyTerminationIdentityMismatch",ref.id);cause(x,a,r->cause,r->terminated_at,&ref);
 return {true,{r,std::move(a)},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyTerminationView> read_verified_treaty_resource_termination(const TreatyResourceTerminationContext& x,const SourceRef& ref,Integer schema){try{auto r=take(read_original_treaty_resource_termination(x,ref,schema));take(x.originals.validate_applied_treaty_termination(r));return {true,std::move(r),{}};}catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_resource_termination_sources(const TreatyResourceTerminationContext& x){try{
 take(validate_treaty_resource_activation_sources(x.activations));const auto now=take(x.activations.signatures.links.revisions.clock.read_organization_authority_now());
 for(const auto& row:x.terminations.rows()){if(row.first!=row.second.id)fail("TreatyTerminationIdentityMismatch",row.first);take(read_verified_treaty_resource_termination(x,treaty_termination_source(row.second),2));}
 for(const auto& row:x.activations.signatures.links.revisions.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="treaty_terms_terminated"&&!x.terminations.find(row.first))fail("TreatyTerminationOrphanFact",row.first);
 for(const auto& row:x.activations.activations.rows()){auto r=take(read_original_treaty_resource_activation(x.activations,treaty_activation_source(row.second),2));if(compare(now,WorldMoment{r.revision.definition->terms.effective_window.until_tick,{}})>=0&&!existing(x,treaty_activation_source(row.second)))fail("TreatyExpirySourceMissing",row.first);}
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> terminate_treaty_resource_terms_in_candidate(CommonStore& common,OrderedTable<TreatyTerminationSource>& rows,const TreatyResourceTerminationContext& x,TreatyTerminationOriginalWrite& original,const SourceRef& activation,const TreatyTerminationCause& actual_cause){try{
 if(&common!=&x.activations.signatures.links.revisions.common||&rows!=&x.terminations)fail("TreatyTerminationCandidateMismatch");if(const auto* old=existing(x,activation)){auto ref=treaty_termination_source(*old);take(read_verified_treaty_resource_termination(x,ref,2));if(!same_cause(old->cause,actual_cause))fail("TreatyAlreadyTerminated",old->id);return {true,ref,{}};}
 auto a=take(read_verified_treaty_resource_activation(x.activations,activation,2));const auto now=take(x.activations.signatures.links.revisions.clock.read_organization_authority_now());cause(x,a,actual_cause,now,nullptr);
 TreatyTerminationSource row;row.id=take(common.next_id("treaty_termination"));row.treaty_id=a.source->treaty_id;row.terms_revision=a.source->terms_revision;row.activation_ref=activation;row.cause=actual_cause;row.terminated_at=now;take(validate_treaty_termination_shape(row));const auto ref=treaty_termination_source(row);
 take(common.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatyTermination",ref.id);
 auto actual=take(read_original_treaty_resource_termination(x,ref,2));take(original.apply_treaty_termination_in_candidate(common,actual));take(read_verified_treaty_resource_termination(x,ref,2));return {true,ref,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<ActivatedTreatyAidTermsRead> NativeTreatyResourceAidTermsRead::read_activated_material_aid_terms(const StableId& treaty,Integer revision,const StableId& term)const{try{
 const TreatyActivationSource* source=nullptr;for(const auto& row:context_.activations.activations.rows())if(row.second.treaty_id==treaty&&row.second.terms_revision==revision){if(source)fail("DuplicateTreatyActivation",treaty);source=&row.second;}if(!source)fail("TreatyAidActivationSourceRequired",treaty);
 auto activation=take(read_original_treaty_resource_activation(context_.activations,treaty_activation_source(*source),2));const auto& body=activation.revision.definition->terms;const TreatyMaterialAidTerm* original=nullptr;for(const auto& row:body.terms)if(row.term_id==term)original=std::get_if<TreatyMaterialAidTerm>(&row.terms);if(!original)fail("TreatyMaterialAidTermRequired",term);
 ActivatedTreatyAidTermsRead out{treaty,revision,term,&body,original,activation.revision.source,treaty_activation_source(*source),source->activated_at,{},{}};
 if(const auto* end=existing(context_,out.activation_source)){auto actual=take(read_original_treaty_resource_termination(context_,treaty_termination_source(*end),2));out.termination_source=treaty_termination_source(*actual.source);out.terminated_at=actual.source->terminated_at;}
 out.activation_source_schema=2;out.termination_source_schema=2;out.schedule_source_schema=2;
 take(validate_treaty_aid_activation_terms(out));return {true,std::move(out),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
