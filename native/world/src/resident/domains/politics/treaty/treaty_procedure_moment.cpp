#include "treaty_procedure_moment.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{std::move(x.error)};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{std::move(x.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);const auto& order=common.state().facts.order();auto it=std::find(order.begin(),order.end(),ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind||it==order.end())fail("TreatyProcedureActualSourceMissing",ref.id);return static_cast<std::size_t>(it-order.begin());}
TreatyRevisionView revision(const TreatyProcedureMomentContext& x,const SourceRef& source,const StableId& treaty,Integer version,const WorldMoment& at,const SourceRef& cut){auto r=take(read_treaty_revision_source(x.revisions,source,1));if(r.treaty_id!=treaty||r.terms_revision!=version||!r.definition||compare(r.issued_at,at)>0||position(x.revisions.common,source)>=position(x.revisions.common,cut))fail("TreatyProcedureRevisionMismatch",cut.id);return r;}
const TreatySignatureSource& signature(const TreatyProcedureMomentContext& x,const SourceRef& ref){if(ref.domain!="politics"||ref.kind!="treaty_terms_ratified")fail("TreatySignatureSourceUnsupported",ref.id);const auto* r=x.signatures.find(ref.id);if(!r||r->id!=ref.id)fail("TreatySignatureSourceMissing",ref.id);take(validate_treaty_signature_source_shape(*r));auto terms=revision(x,r->terms_source,r->treaty_id,r->terms_revision,r->signed_at,ref);if(std::find(terms.definition->terms.parties.begin(),terms.definition->terms.parties.end(),r->party_id)==terms.definition->terms.parties.end()||compare(r->signed_at,WorldMoment{terms.definition->terms.effective_window.until_tick,{}})>=0)fail("TreatySignatureOriginalPartyMismatch",ref.id);return *r;}
const TreatyActivationSource& activation(const TreatyProcedureMomentContext& x,const SourceRef& ref){
 if(ref.domain!="politics"||ref.kind!="treaty_terms_activated")fail("TreatyActivationSourceUnsupported",ref.id);const auto* r=x.activations.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyActivationSourceMissing",ref.id);take(validate_treaty_activation_shape(*r));auto terms=revision(x,r->terms_source,r->treaty_id,r->terms_revision,r->activated_at,ref);const auto& parties=terms.definition->terms.parties;if(r->signature_refs.size()!=parties.size())fail("TreatyActivationRecognitionIncomplete",ref.id);
 WorldMoment at{terms.definition->terms.effective_window.from_tick,{}};for(std::size_t i=0;i<parties.size();++i){const auto& s=signature(x,r->signature_refs[i]);if(s.party_id!=parties[i]||s.treaty_id!=r->treaty_id||s.terms_revision!=r->terms_revision||!same_source_ref(s.terms_source,r->terms_source)||position(x.revisions.common,r->signature_refs[i])>=position(x.revisions.common,ref))fail("TreatyActivationSignatureMismatch",ref.id);if(compare(s.signed_at,at)>0)at=s.signed_at;}
 if(compare(at,r->activated_at)||!take(treaty_window_contains(terms.definition->terms.effective_window,at)))fail("TreatyActivationMomentMismatch",ref.id);return *r;
}
// 只反原行/activation/实际Common切点；完整许可与知情由release owner另核，避免时间读回环。
const TreatyReleaseRequestSource& release_request(const TreatyProcedureMomentContext& x,const SourceRef& ref){
 if(ref.domain!="politics"||ref.kind!="treaty_release_requested")fail("TreatyReleaseRequestSourceUnsupported",ref.id);const auto* record=x.replies.find(ref.id);const auto* r=record?std::get_if<TreatyReleaseRequestSource>(record):nullptr;if(!r||r->id!=ref.id)fail("TreatyReleaseRequestMissing",ref.id);take(validate_treaty_release_request_shape(*r));const auto& active=activation(x,r->activation_source);auto terms=revision(x,r->terms_source,r->treaty_id,r->terms_revision,r->requested_at,ref);
 if(active.treaty_id!=r->treaty_id||active.terms_revision!=r->terms_revision||!same_source_ref(active.terms_source,r->terms_source)||compare(active.activated_at,r->requested_at)>0||position(x.revisions.common,r->activation_source)>=position(x.revisions.common,ref)||!take(treaty_window_contains(terms.definition->terms.effective_window,r->requested_at))||std::find(terms.definition->terms.parties.begin(),terms.definition->terms.parties.end(),r->requester.organization_id)==terms.definition->terms.parties.end())fail("TreatyReleaseRequestActivationMismatch",ref.id);return *r;
}
}
bool native_treaty_procedure_source_kind(const FactRecord& h){if(h.domain!="politics")return false;for(const auto* k:{"treaty_release_requested","treaty_release_answered","treaty_negotiation_mandated","treaty_terms_presented","treaty_terms_replied","treaty_terms_ratified","treaty_terms_activated","treaty_terms_terminated"})if(h.kind==k)return true;return false;}
Outcome<WorldMoment> read_treaty_procedure_source_moment(const TreatyProcedureMomentContext& x,const MomentOwnerSource& source){try{
 const auto& ref=source.source_ref;if(source.source_schema!=1||!valid_source_ref(ref)||!native_treaty_procedure_source_kind({ref.id,ref.kind,ref.domain}))fail("TreatyProcedureSourceUnsupported",ref.id);position(x.revisions.common,ref);WorldMoment at;
 if(ref.kind=="treaty_negotiation_mandated"){const auto* r=x.mandates.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyMandateSourceMissing",ref.id);take(validate_treaty_mandate_source_shape(*r));auto original=revision(x,r->original_terms_source,r->treaty_id,r->original_terms_revision,r->issued_at,ref);if(std::find(original.definition->terms.parties.begin(),original.definition->terms.parties.end(),r->issuer.organization_id)==original.definition->terms.parties.end()||compare(r->issued_at,WorldMoment{r->mandate.window.until_tick,{}})>=0)fail("TreatyMandateOriginalRevisionMismatch",ref.id);at=r->issued_at;}
 else if(ref.kind=="treaty_terms_presented"){const auto* r=x.presentations.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyPresentationSourceMissing",ref.id);take(validate_treaty_presentation_shape(*r));revision(x,r->terms_source,r->treaty_id,r->terms_revision,r->presented_at,ref);at=r->presented_at;}
 else if(ref.kind=="treaty_terms_replied"){const auto* record=x.replies.find(ref.id);const auto* r=record?std::get_if<TreatyReplySource>(record):nullptr;if(!r||r->id!=ref.id)fail("TreatyReplySourceMissing",ref.id);take(validate_treaty_reply_shape(*r));revision(x,r->terms_source,r->treaty_id,r->terms_revision,r->replied_at,ref);const auto* p=x.presentations.find(r->presentation_source.id);if(!p||p->treaty_id!=r->treaty_id||p->terms_revision!=r->terms_revision||!same_source_ref(p->terms_source,r->terms_source)||compare(p->presented_at,r->replied_at)>0||position(x.revisions.common,r->presentation_source)>=position(x.revisions.common,ref))fail("TreatyReplyPresentationMismatch",ref.id);take(validate_treaty_presentation_shape(*p));at=r->replied_at;}
 else if(ref.kind=="treaty_release_requested")at=release_request(x,ref).requested_at;
 else if(ref.kind=="treaty_release_answered"){const auto* record=x.replies.find(ref.id);const auto* r=record?std::get_if<TreatyReleaseAnswerSource>(record):nullptr;if(!r||r->id!=ref.id)fail("TreatyReleaseAnswerMissing",ref.id);take(validate_treaty_release_answer_shape(*r));const auto& request=release_request(x,r->request_source);if(r->treaty_id!=request.treaty_id||r->terms_revision!=request.terms_revision||!same_source_ref(r->activation_source,request.activation_source)||r->respondent.organization_id==request.requester.organization_id||compare(request.requested_at,r->answered_at)>0||position(x.revisions.common,r->request_source)>=position(x.revisions.common,ref))fail("TreatyReleaseAnswerRequestMismatch",ref.id);auto terms=take(read_treaty_revision_source(x.revisions,request.terms_source,1));if(!take(treaty_window_contains(terms.definition->terms.effective_window,r->answered_at))||std::find(terms.definition->terms.parties.begin(),terms.definition->terms.parties.end(),r->respondent.organization_id)==terms.definition->terms.parties.end())fail("TreatyReleaseAnswerPartyMismatch",ref.id);at=r->answered_at;}
 else if(ref.kind=="treaty_terms_ratified")at=signature(x,ref).signed_at;
 else if(ref.kind=="treaty_terms_activated")at=activation(x,ref).activated_at;
 else {const auto* r=x.terminations.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyTerminationSourceMissing",ref.id);take(validate_treaty_termination_shape(*r));const auto& a=activation(x,r->activation_ref);if(a.treaty_id!=r->treaty_id||a.terms_revision!=r->terms_revision||compare(a.activated_at,r->terminated_at)>0||position(x.revisions.common,r->activation_ref)>=position(x.revisions.common,ref))fail("TreatyTerminationActivationMismatch",ref.id);auto terms=take(read_treaty_revision_source(x.revisions,a.terms_source,1));const WorldMoment until{terms.definition->terms.effective_window.until_tick,{}};if(std::holds_alternative<TreatyExpiryTermination>(r->cause)?compare(r->terminated_at,until)!=0:compare(r->terminated_at,until)>=0)fail("TreatyTerminationOriginalTimeMismatch",ref.id);at=r->terminated_at;}
 if(compare(at,take(x.revisions.clock.read_organization_authority_now()))>0)fail("FutureTreatyProcedureSource",ref.id);return {true,std::move(at),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<WorldMoment> NativeTreatyProcedureResultRead::read_selected_source_moment(const MomentOwnerSource& s)const{return read_treaty_procedure_source_moment(context_,s);}
// 原发起/答复/签署只能令实际本人直接观察自己的真实结果；不让其它方同步知情。
// 此capture沿已核原行/time leg，不调用signature→Knowledge→capture；完整冷门另核全部Basis/receipt。
Outcome<ExactOwnerFactRead> NativeTreatyProcedureResultRead::read_selected_fact(const FactRecord& h)const{try{
 const SourceRef ref{h.domain,h.kind,h.id};auto at=take(read_selected_source_moment({ref,1}));
 const auto& x=context_;
 StableId actor_id,treaty_id,outcome;SourceRef terms_source;std::vector<StableId> causes;
 if(h.kind=="treaty_release_requested"){
  const auto* r=&release_request(x,ref);actor_id=r->requester.actor_id;treaty_id=r->treaty_id;
  terms_source=r->terms_source;causes.push_back(r->activation_source.id);outcome="release_requested";
 }else if(h.kind=="treaty_release_answered"){
  const auto* record=x.replies.find(ref.id);const auto* r=record?std::get_if<TreatyReleaseAnswerSource>(record):nullptr;if(!r)fail("TreatyReleaseAnswerMissing",ref.id);const auto* request=&release_request(x,r->request_source);
  actor_id=r->respondent.actor_id;treaty_id=r->treaty_id;terms_source=request->terms_source;causes.push_back(r->request_source.id);
  outcome=r->decision==TreatyReplyDecision::accept?"release_accepted":"release_refused";
 }else if(h.kind=="treaty_terms_replied"){
  const auto* record=x.replies.find(ref.id);const auto* r=record?std::get_if<TreatyReplySource>(record):nullptr;if(!r)fail("TreatyReplySourceMissing",ref.id);actor_id=r->respondent.actor_id;treaty_id=r->treaty_id;
  terms_source=r->terms_source;causes.push_back(r->presentation_source.id);outcome=r->decision==TreatyReplyDecision::accept?"accepted":"refused";
 }else if(h.kind=="treaty_terms_ratified"){
  const auto* r=&signature(x,ref);actor_id=treaty_signature_actor(*r);treaty_id=r->treaty_id;
  terms_source=r->terms_source;causes.push_back(r->terms_source.id);outcome="ratified";
 }else if(h.kind=="treaty_negotiation_mandated"){
  const auto* r=x.mandates.find(ref.id);if(!r)fail("TreatyMandateSourceMissing",ref.id);actor_id=r->issuer.actor_id;treaty_id=r->treaty_id;
  terms_source=r->original_terms_source;causes.push_back(r->original_terms_source.id);outcome="mandated";
 }else{
  // 客观activation/termination和原呈示来源仍留原合法time source，不伪造见证者。
  ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.push_back({ref,1});return {true,std::move(out),{}};
 }
 auto body=take(read_treaty_revision_source(x.revisions,terms_source,1));if(!body.definition||actor_id.empty()||body.treaty_id!=treaty_id)fail("TreatyProcedureProjectionOriginalMismatch",h.id);
 DynamicFactViewV2 fact;fact.id=h.id;fact.kind=h.kind;fact.rule_version=body.definition->terms.rule_version;fact.occurred_at=std::move(at);
 fact.time_basis=OwnerFactMoment{ref,1};fact.cause_fact_ids=std::move(causes);fact.subject_id=std::move(treaty_id);fact.outcome=std::move(outcome);fact.publicly_visible=false;
 const SubjectRef issuer{SubjectKind::actor,actor_id};fact.participants.push_back({issuer,"issuer"});
 ExactOwnerFactRead out;out.fact=std::move(fact);out.reportable=true;out.direct_observers.push_back(issuer);return {true,std::move(out),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
