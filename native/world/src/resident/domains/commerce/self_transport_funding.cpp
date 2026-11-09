// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "self_transport_funding.hpp"
#include "resident/domains/tasks/task_transport_agreement.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{v.error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);const auto&order=c.state().facts.order();auto found=std::find(order.begin(),order.end(),s.id);if(!h||h->id!=s.id||h->domain!=s.domain||h->kind!=s.kind||found==order.end())fail("SelfFundingOriginalHeaderRequired",s.id);return static_cast<std::size_t>(found-order.begin());}
void same_context(const CommonStore&c,const PrivateTransportFundingState&s,const SelfTransportFundingContext&x){if(&c!=&x.common||&s!=&x.state||s.schema_version!=1)fail("SelfFundingCandidateMismatch");}
void owner(const PrivateWalletTitleView&t,const StableId&wallet,const StableId&actor){if(t.wallet_id!=wallet||t.legal_owner.extra||t.legal_owner.kind!=Owner::Kind::actor||t.legal_owner.id!=actor)fail("SelfFundingPersonalWalletTitleRequired",wallet);}
void proposal_shape(const SelfTransportFundingProposal&p){if(p.principal_actor_id.empty()||p.wallet_id.empty()||p.carrier_subject.id.empty()||p.carrier_subject.kind!=SubjectKind::actor||p.destination_site_id.empty()||p.gross_cap<0||!encode_world_moment(p.until).ok)fail("InvalidSelfTransportFundingProposal");}
const TaskCarrierAgreement& plan(const SelfTransportFundingContext&x,const SourceRef&s){auto actual=take(read_original_task_carrier_acceptance(x.common,x.tasks,s,1));if(!actual.agreement)fail("SelfFundingAcceptedTransportPlanRequired",s.id);return *actual.agreement;}
void plan_scope(const TaskCarrierAgreement&a,const SelfTransportFundingProposal&p,const WorldMoment&accepted){
 if(a.terms.principal.kind!=SubjectKind::actor||a.terms.principal.id!=p.principal_actor_id||a.contact.actor_id!=p.principal_actor_id||!same(a.terms.carrier_subject,p.carrier_subject)||a.terms.destination_site_id!=p.destination_site_id||compare(a.accepted_at,accepted)!=0||compare(p.until,{a.terms.labor_until_tick,{}})>0||compare(accepted,p.until)>=0)fail("SelfFundingAcceptedPlanScopeMismatch",a.id);
}
SelfTransportFundingReadView view(const SelfTransportFundingTerms&t,const TaskCarrierAgreement&a){return {&t,t.acceptance_ref,{t.id,t.principal_actor_id,t.wallet_id},{SubjectKind::actor,t.principal_actor_id},a.terms.carrier_subject,a.terms.destination_site_id,t.until,{t.acceptance_ref}};}
const TaskCarrierAgreement& validate_row(const SelfTransportFundingContext&x,const SelfTransportFundingTerms&t,bool finalized){
 take(validate_self_transport_funding_terms_shape(t));
 for(const auto&row:x.state.terms.rows())if(row.first!=t.id&&same(row.second.journey_or_plan_ref,t.journey_or_plan_ref))fail("DuplicateSelfFundingPlan",t.journey_or_plan_ref.id);
 const auto&a=plan(x,t.journey_or_plan_ref);
 SelfTransportFundingProposal p{t.principal_actor_id,t.wallet_id,a.terms.carrier_subject,a.terms.destination_site_id,t.gross_cap,t.until};
 plan_scope(a,p,t.valid_from);
 if(t.allowed_destination_scope.size()!=1||t.allowed_destination_scope.front()!=a.terms.destination_site_id)fail("SelfFundingDestinationScopeMismatch",t.id);
 if(position(x.common,t.journey_or_plan_ref)>=position(x.common,t.acceptance_ref))fail("SelfFundingAcceptanceOrderMismatch",t.id);
 if(compare(t.valid_from,take(x.clock.read_current_moment()))>0)fail("FutureSelfFundingAcceptance",t.id);
 owner(take(x.titles.read_private_wallet_title_before_source(t.wallet_id,t.acceptance_ref)),t.wallet_id,t.principal_actor_id);
 if(finalized){const auto*authority=std::get_if<SelfFundedExpenseAuthority>(&a.terms.expense_authority);if(!authority||authority->funding_terms_id!=t.id||authority->principal_actor_id!=t.principal_actor_id||authority->payer_wallet_id!=t.wallet_id||a.terms.payment_source_refs.size()!=1||!same(a.terms.payment_source_refs.front(),t.acceptance_ref))fail("SelfFundingTransportBindingMismatch",t.id);}
 return a;
}
void validate_retained_sources(const SelfTransportFundingContext&x){
 if(x.state.schema_version!=1)fail("UnsupportedPrivateTransportFundingVersion");
 for(const auto&row:x.state.terms.rows()){if(row.first!=row.second.id)fail("SelfFundingRecordKeyMismatch",row.first);validate_row(x,row.second,true);for(const auto&other:x.state.terms.rows())if(other.first!=row.first&&same(other.second.journey_or_plan_ref,row.second.journey_or_plan_ref))fail("DuplicateSelfFundingPlan",row.second.journey_or_plan_ref.id);}
 for(const auto&h:x.common.state().facts.rows())if(h.second.domain=="commerce"&&h.second.kind=="self_transport_funding_accepted"&&!x.state.terms.find(h.first))fail("MissingSelfFundingAcceptancePayload",h.first);
}
}
SourceRef self_transport_funding_acceptance_source(const StableId&id){return {"commerce","self_transport_funding_accepted",id};}
Status validate_self_transport_funding_terms_shape(const SelfTransportFundingTerms&t){
 if(t.id.empty()||t.principal_actor_id.empty()||t.wallet_id.empty()||t.currency_id!="qian"||t.gross_cap<0||t.revision!=1||t.journey_or_plan_ref.domain!="tasks"||t.journey_or_plan_ref.kind!="task_carrier_agreement_accepted"||t.journey_or_plan_ref.id.empty()||!same(t.acceptance_ref,self_transport_funding_acceptance_source(t.id))||t.continuity!=SelfTransportFundingContinuity::honor_committed_safe_return||t.allowed_destination_scope.size()!=1||t.allowed_destination_scope.front().empty())return common_failure("InvalidSelfTransportFundingTerms",t.id);
 auto window=compare_world_moments(t.valid_from,t.until);if(!window.ok)return {false,{},window.error};if(window.value>=0)return common_failure("InvalidSelfTransportFundingWindow",t.id);return common_success();
}
Outcome<SelfTransportFundingQuote> preview_self_transport_funding(const SelfTransportFundingContext&x,const SelfTransportFundingProposal&p){try{
 if(x.state.schema_version!=1)fail("UnsupportedPrivateTransportFundingVersion");proposal_shape(p);auto now=take(x.clock.read_current_moment());if(compare(now,p.until)>=0||now.half_day_tick<x.common.tick())fail("SelfFundingProposalOutsideWindow");
 if(!x.common.actor(p.principal_actor_id)||!x.common.actor(p.carrier_subject.id)||!x.common.wallet(p.wallet_id))fail("SelfFundingActualPersonOrWalletRequired");
 owner(take(x.titles.read_current_private_wallet_title(p.wallet_id)),p.wallet_id,p.principal_actor_id);
 return {true,{p,x.common.state().identity,x.common.read_version({ResourceKey::Kind::wallet,p.wallet_id}),now},{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<SelfTransportFundingReadView> accept_self_transport_funding_in_candidate(CommonStore&c,PrivateTransportFundingState&s,const SelfTransportFundingContext&x,const SelfTransportFundingQuote&q,const SourceRef&accepted){try{
 same_context(c,s,x);proposal_shape(q.proposal);const auto&a=plan(x,accepted);plan_scope(a,q.proposal,a.accepted_at);
 // A true replay stays valid after the world, title or wallet version advances.
 const SelfTransportFundingTerms*old=nullptr;for(const auto&row:s.terms.rows())if(same(row.second.journey_or_plan_ref,accepted)){if(old)fail("DuplicateSelfFundingPlan",accepted.id);old=&row.second;}
 if(old){const auto&actual=validate_row(x,*old,true);if(old->principal_actor_id!=q.proposal.principal_actor_id||old->wallet_id!=q.proposal.wallet_id||old->gross_cap!=q.proposal.gross_cap||compare(old->until,q.proposal.until)!=0||old->allowed_destination_scope.front()!=q.proposal.destination_site_id||!same(actual.terms.carrier_subject,q.proposal.carrier_subject))fail("SelfFundingAcceptanceReplayMismatch",old->id);return {true,view(*old,actual),{}};}
 // The one proven in-flight Task has no funding tail yet. Retained funding
 // sources are fully checked here; all Task reverse bindings are checked by
 // validate_self_transport_fundings after this original acceptance is finalized.
 validate_retained_sources(x);auto fresh=take(preview_self_transport_funding(x,q.proposal));const auto&identity=c.state().identity;
 if(q.identity.world_id!=identity.world_id||q.identity.execution_epoch!=identity.execution_epoch||q.identity.revision>identity.revision||q.identity.tick!=identity.tick||compare(q.quoted_at,fresh.quoted_at)!=0||compare(a.accepted_at,fresh.quoted_at)!=0||q.wallet_version.resource.kind!=ResourceKey::Kind::wallet||q.wallet_version.resource.id!=q.proposal.wallet_id||q.wallet_version.version!=fresh.wallet_version.version)fail("StaleSelfFundingQuote");
 if(a.stopped_source||!a.terms.payment_source_refs.empty())fail("SelfFundingPlanAlreadyFundedOrStopped",a.id);
 auto id=take(c.next_id("self_transport_funding"));SelfTransportFundingTerms t;
 t.id=id;t.principal_actor_id=q.proposal.principal_actor_id;t.wallet_id=q.proposal.wallet_id;t.journey_or_plan_ref=accepted;t.gross_cap=q.proposal.gross_cap;t.valid_from=fresh.quoted_at;t.until=q.proposal.until;t.allowed_destination_scope={q.proposal.destination_site_id};t.acceptance_ref=self_transport_funding_acceptance_source(id);
 take(validate_self_transport_funding_terms_shape(t));take(c.append_fact({id,"self_transport_funding_accepted","commerce"}));if(!s.terms.insert(id,std::move(t)))fail("DuplicateSelfFundingAcceptance",id);
 const auto*stored=s.terms.find(id);validate_row(x,*stored,false);return {true,view(*stored,a),{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<SelfTransportFundingReadView> read_self_transport_funding(const SelfTransportFundingContext&x,const SelfFundedExpenseAuthority&a){try{
 if(x.state.schema_version!=1)fail("UnsupportedPrivateTransportFundingVersion");const auto*t=x.state.terms.find(a.funding_terms_id);if(!t||t->id!=a.funding_terms_id||t->principal_actor_id!=a.principal_actor_id||t->wallet_id!=a.payer_wallet_id)fail("SelfFundingAuthoritySourceMismatch",a.funding_terms_id);const auto&actual=validate_row(x,*t,true);return {true,view(*t,actual),{}};
}catch(const Failure&e){return {false,{},e.error};}}
Status validate_self_transport_fundings(const SelfTransportFundingContext&x){try{
 validate_retained_sources(x);
 if(x.tasks.transport_agreements)for(const auto&row:x.tasks.transport_agreements->agreements.rows()){const auto*a=std::get_if<SelfFundedExpenseAuthority>(&row.second.terms.expense_authority);if(!a)continue;const auto*t=x.state.terms.find(a->funding_terms_id);if(!t||!same(t->journey_or_plan_ref,{"tasks","task_carrier_agreement_accepted",row.first})||t->principal_actor_id!=a->principal_actor_id||t->wallet_id!=a->payer_wallet_id)fail("TaskSelfFundingReverseBindingMismatch",row.first);}
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<WorldMoment> read_self_transport_funding_moment(const SelfTransportFundingContext&x,const SourceRef&s,Integer schema){try{
 if(schema!=1||!same(s,self_transport_funding_acceptance_source(s.id)))fail("UnsupportedSelfFundingSource",s.id);const auto*t=x.state.terms.find(s.id);if(!t)fail("MissingSelfFundingAcceptancePayload",s.id);validate_row(x,*t,true);return {true,t->valid_from,{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<std::uint64_t> read_self_transport_funding_domain_version(const PrivateTransportFundingState&s,const DomainReadKey&key){if(s.schema_version!=1||key.domain!="commerce"||key.kind!="self_transport_funding")return {false,{},common_failure("UnsupportedSelfFundingDomainVersion",key.id).error};const auto*t=s.terms.find(key.id);if(!t||t->id!=key.id||t->revision!=1)return {false,{},common_failure("SelfFundingDomainSourceMissing",key.id).error};return {true,static_cast<std::uint64_t>(t->revision),{}};}
}
