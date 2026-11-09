#include "political_head_handover_time2.hpp"
#include "political_head_handover_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
struct Issue {
 SourceRef source;const PoliticalHeadHandoverExactIssue* exact=nullptr;
 const PoliticalHeadHandoverOffer* offer=nullptr;StableId signer,command;Integer tick=0;bool uses_authority=false;
};
Issue issue(const CommonStore& c,const PoliticalHeadHandoverState& s,const SourceRef& source){
 if(s.version!=2||!political_head_handover_fact_sequence(c,source.id))fail("HeadHandoverTime2SourceRequired",source.id);
 const auto* head=c.state().facts.find(source.id);
 if(!head||head->id!=source.id||source.domain!=political_head_handover_domain||head->domain!=source.domain||head->kind!=source.kind||
    Integer(bool(s.offers.find(source.id)))+Integer(bool(s.answers.find(source.id)))+Integer(bool(s.withdrawals.find(source.id)))!=1)fail("HeadHandoverTime2SourceMismatch",source.id);
 Issue out;out.source=source;
 if(const auto* o=s.offers.find(source.id)){
  if(o->id!=source.id||source.kind!="political_head_handover_offered")fail("HeadHandoverTime2SourceMismatch",source.id);
  out.offer=o;out.exact=o->exact_issue?&*o->exact_issue:nullptr;out.signer=o->issuer_actor_id;
  out.command=political_head_handover_offer_command;out.tick=o->issued_tick;out.uses_authority=true;
 }else if(const auto* a=s.answers.find(source.id)){
  if(a->id!=source.id||source.kind!=(a->accepted?"political_head_handover_accepted":"political_head_handover_declined"))fail("HeadHandoverTime2SourceMismatch",source.id);
  out.offer=s.offers.find(a->offer_fact_id);out.exact=a->exact_issue?&*a->exact_issue:nullptr;
  // 接受重核原让职者仍有效的同一专项权，候选的本人同意仍是独立原答复。
  out.signer=out.offer?out.offer->issuer_actor_id:StableId{};out.command=political_head_handover_offer_command;
  out.tick=a->tick;out.uses_authority=a->accepted;
 }else if(const auto* w=s.withdrawals.find(source.id)){
  if(w->id!=source.id||source.kind!="political_head_handover_withdrawn")fail("HeadHandoverTime2SourceMismatch",source.id);
  out.offer=s.offers.find(w->offer_fact_id);out.exact=w->exact_issue?&*w->exact_issue:nullptr;
  out.signer=w->actor_id;out.command=political_head_handover_withdraw_command;out.tick=w->tick;out.uses_authority=true;
 }else fail("HeadHandoverTime2PayloadRequired",source.id);
 if(!out.offer||!out.exact||out.exact->occurred_at.half_day_tick!=out.tick||
    out.exact->authority_basis.has_value()!=out.uses_authority)
  fail("HeadHandoverTime2PayloadMismatch",source.id);
 if(out.offer->id!=source.id){
  const auto& order=c.state().facts.order();const auto a=std::find(order.begin(),order.end(),out.offer->id),b=std::find(order.begin(),order.end(),source.id);
  const auto* origin=c.state().facts.find(out.offer->id);
  if(!origin||origin->domain!=political_head_handover_domain||origin->kind!="political_head_handover_offered"||
     a==order.end()||b==order.end()||a>=b)fail("HeadHandoverTime2OfferCutMismatch",source.id);
 }
 return out;
}
SourceRef source(const CommonStore& c,const StableId& id){
 const auto* h=c.state().facts.find(id);if(!h)fail("HeadHandoverTime2SourceRequired",id);return {h->domain,h->kind,id};
}
SingleGrantAuthorityRequest request(const PoliticalSuccessionPolicy& policy,const ActingAuthority& actor,const StableId& command){
 return {actor,command,{ScopeRef::Kind::site,policy.seat_place_id},0,{},false,{}};
}
void exact_context(const PoliticalHeadHandoverContext& x){
 if(x.state.version!=2||!x.exact||&x.exact->ages.common!=&x.common||
    !x.organization_catalog.authority_lifecycle_enabled||!x.organization_catalog.exact_fact_clock_enabled)
  fail("HeadHandoverTime2OriginalContextRequired");
 if(x.exact->stamp.mode()!=ReadMode::phase_current&&x.exact->stamp.mode()!=ReadMode::committed_current)
  fail("HeadHandoverTime2CurrentStampRequired");
 take(x.exact->stamp_authority.validate(x.exact->stamp,x.common.state().identity,x.exact->stamp.mode(),x.common,x.exact->versions));
}
VerifiedOfficeTransition transition(const PoliticalHeadHandoverContext& x,const SourceRef& ref){
 exact_context(x);auto original=issue(x.common,x.state,ref);
 const auto* a=x.state.answers.find(ref.id);const auto* o=original.offer;
 if(!a||!a->accepted||ref.kind!="political_head_handover_accepted"||a->actor_id!=o->candidate_actor_id||
    a->expected_office_epoch!=o->office_epoch||a->expected_office_epoch==std::numeric_limits<Integer>::max())
  fail("HeadHandoverAcceptedOriginalRequired",ref.id);
 const auto* policy=x.catalog.policies.find(o->policy_id);
 const auto* base=policy?x.succession_catalog.policies.find(policy->succession_policy_id):nullptr;
 if(!policy||!base)fail("PoliticalHeadHandoverOriginalPolicyRequired",ref.id);
 take(validate_head_handover_authority2(x,*policy,*base,ref.id));
 take(validate_head_handover_office2(x,*base,o->issuer_actor_id,o->office_epoch,ref.id));
 return {ref,o->organization_id,o->office_id,o->office_epoch,o->issuer_actor_id,a->actor_id,
  OfficeTransitionKind::term_handover,original.exact->occurred_at};
}
bool same_transition(const VerifiedOfficeTransition& a,const VerifiedOfficeTransition& b){
 return same_source_ref(a.source,b.source)&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&
  a.expected_epoch==b.expected_epoch&&a.old_holder==b.old_holder&&a.new_holder==b.new_holder&&a.kind==b.kind&&!cmp(a.effective_moment,b.effective_moment);
}
}
Outcome<WorldMoment> political_head_handover_now(const PoliticalHeadHandoverContext& x){
 if(x.state.version==2&&!x.exact)return {false,{},common_failure("HeadHandoverTime2OriginalContextRequired").error};
 return read_organization_authority_now(x.common,x.state.version==2?&x.exact->clock:nullptr);
}
Outcome<WorldMoment> read_political_head_handover_moment2(const PoliticalHeadHandoverContext& x,const SourceRef& ref){
 return guard<WorldMoment>([&]{exact_context(x);auto value=issue(x.common,x.state,ref);
  if(cmp(value.exact->occurred_at,take(political_head_handover_now(x)))>0)fail("HeadHandoverTime2FutureSource",ref.id);
  return value.exact->occurred_at;});
}
Outcome<bool> political_head_handover_response_open(const PoliticalHeadHandoverContext& x,
 const PoliticalHeadHandoverOffer& offer,const WorldMoment& at){return guard<bool>([&]{
 if(x.state.version==1)return at.half_day_tick<offer.response_until;
 exact_context(x);const auto* p=x.catalog.policies.find(offer.policy_id);
 if(!p||!offer.exact_issue||p->response_ticks<=0)fail("HeadHandoverExactDeadlineRequired",offer.id);
 const auto due=take(add_world_duration(offer.exact_issue->occurred_at,
  HalfDayDuration{take(normalize_fraction(std::to_string(p->response_ticks),"1"))}));
 if(due.half_day_tick!=offer.response_until)fail("HeadHandoverExactDeadlineMismatch",offer.id);
 return cmp(at,due)<0;
});}
Outcome<AuthorityIssueSourceView> NativeHeadHandoverAuthorityRead::read_authority_issue_source(const SourceRef& ref)const{
 return guard<AuthorityIssueSourceView>([&]{auto value=issue(common_,state_,ref);
  if(!value.uses_authority)fail("HeadHandoverDeclineHasNoAuthorityIssue",ref.id);
  ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=value.exact->occurred_at;
  if(cmp(at.exact_moment,take(clock_.read_organization_authority_now()))>0)fail("HeadHandoverTime2FutureSource",ref.id);
  return AuthorityIssueSourceView{ref,value.signer,value.command,at,{}};
 });
}
Outcome<SingleGrantAuthorityRead> read_current_head_handover_authority2(const PoliticalHeadHandoverContext& x,
 const PoliticalHeadHandoverPolicy& policy,const PoliticalSuccessionPolicy& base,const ActingAuthority& actor,const StableId& command){
 return guard<SingleGrantAuthorityRead>([&]{exact_context(x);
  auto actual=take(check_current_grant_authority(x.common,x.organizations,x.organization_catalog,
   x.organization_catalog.authority_catalog_identity,request(base,actor,command),&x.exact->clock));
  if(actual.effective_grant.id!=policy.handover_grant_id)fail("PoliticalHeadHandoverSpecificGrantRequired",policy.id);
  return actual;
 });
}
Status validate_head_handover_authority2(const PoliticalHeadHandoverContext& x,
 const PoliticalHeadHandoverPolicy& policy,const PoliticalSuccessionPolicy& base,const StableId& id){
 return guard<Done>([&]{exact_context(x);auto value=issue(x.common,x.state,source(x.common,id));
  if(!value.uses_authority)fail("HeadHandoverDeclineHasNoAuthorityIssue",id);
  NativeHeadHandoverAuthorityRead original(x.common,x.state,x.exact->clock);
  const auto& ref=*value.exact->authority_basis;
  if(ref.source_schema!=1||!same_source_ref(ref.issued_source,value.source))fail("HeadHandoverAuthoritySourceMismatch",id);
  const auto* basis=take(read_registered_authority_basis(x.common,x.exact->runtime.authority_bases,
   x.organizations,x.organization_catalog,x.organization_catalog.authority_catalog_identity,ref,original));
  if(basis->actor_id!=value.signer||basis->organization_id!=base.organization_id||basis->office_id!=base.leader_office_id||
     basis->office_epoch!=value.offer->office_epoch||basis->grant_id!=policy.handover_grant_id||
     basis->command_kind!=value.command||basis->scope.kind!=ScopeRef::Kind::site||basis->scope.id!=base.seat_place_id||
     basis->amount!=0||!basis->resources.empty()||!basis->amount_components.empty()||basis->budget_ref)
   fail("HeadHandoverAuthoritySourceMismatch",id);
  return Done{};
 });
}
Outcome<PoliticalSuccessionOfficeSource> read_political_head_handover_current_office(
 const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& policy){
 if(x.state.version==1)return read_political_succession_office(x.common,x.organizations,x.organization_catalog,policy);
 return guard<PoliticalSuccessionOfficeSource>([&]{exact_context(x);
  const auto* seed=x.organization_catalog.offices.find(policy.leader_office_id);
  const auto* org=x.organizations.organizations.find(policy.organization_id);
  if(!seed||!seed->holder_actor_id||seed->organization_id!=policy.organization_id||!org||
     org->succession_policy_id!=policy.id||org->headquarters_site!=policy.seat_place_id)
   fail("PoliticalSuccessionOriginalOfficeRequired",policy.id);
  const auto term=take(read_current_office_term(x.common,x.organizations,x.organization_catalog,
   x.organization_catalog.authority_catalog_identity,policy.leader_office_id,&x.exact->clock));
  PoliticalSuccessionOfficeSource out{x.common.state().identity,policy.id,policy.organization_id,seed->id,
   policy.seat_place_id,*seed->holder_actor_id,{},{},term.holder_actor_id,term.epoch,
   take(political_head_handover_now(x)).half_day_tick,term.status==OfficeStatus::vacant};
  if(const auto* changed=std::get_if<OfficeChangedAuthoritySource>(&term.source))out.latest_office_fact_id=changed->fact_id;
  return out;
 });
}
Status validate_head_handover_office2(const PoliticalHeadHandoverContext& x,const PoliticalSuccessionPolicy& policy,
 const StableId& actor,Integer epoch,const StableId& boundary){return guard<Done>([&]{
 exact_context(x);const auto& identity=x.organization_catalog.authority_catalog_identity;
 const auto* initial=x.organization_catalog.offices.find(policy.leader_office_id);
 const auto* current=x.organizations.offices.find(policy.leader_office_id);
 const auto* initial_org=x.organization_catalog.organizations.find(policy.organization_id);
 const auto* org=x.organizations.organizations.find(policy.organization_id);
 if(!initial_org||!org||initial_org->succession_policy_id!=policy.id||org->succession_policy_id!=policy.id||
    initial_org->headquarters_site!=policy.seat_place_id||org->headquarters_site!=policy.seat_place_id)
  fail("PoliticalSuccessionOriginalOfficeRequired",policy.id);
 if(!initial||!current||initial->deputy_actor_id||(boundary.empty()&&current->deputy_actor_id)||
    initial->holder_actor_id!=std::optional<StableId>{actor}||epoch!=initial->epoch)
  fail("PoliticalHeadHandoverOriginalLeaderRequired",policy.id);
 const auto term=boundary.empty()?take(read_current_office_term(x.common,x.organizations,x.organization_catalog,identity,policy.leader_office_id,&x.exact->clock)):
  take(read_office_term_before_fact_at(x.common,x.organizations,x.organization_catalog,identity,policy.leader_office_id,
   take(read_political_head_handover_moment2(x,source(x.common,boundary))),boundary,x.exact->clock));
 if(term.organization_id!=policy.organization_id||term.epoch!=epoch||term.holder_actor_id!=std::optional<StableId>{actor}||term.status!=OfficeStatus::active)
  fail("PoliticalHeadHandoverOriginalLeaderRequired",policy.id);
 return Done{};
});}
Status validate_head_handover_person2(const PoliticalHeadHandoverContext& x,const StableId& actor,const StableId& boundary){
 return guard<Done>([&]{exact_context(x);
  const auto at=boundary.empty()?take(political_head_handover_now(x)):take(read_political_head_handover_moment2(x,source(x.common,boundary)));
  auto age=take(read_life_age_basis(x.exact->ages,actor,at,boundary));
  if(age.age_years<18)fail("OfficeRequiresAdult",actor);
  if(boundary.empty()){const auto* p=x.common.actor(actor);if(!p||!p->alive)fail("PoliticalHeadHandoverSignerDead",actor);}
  else{auto present=take(read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,actor,at.half_day_tick,boundary));
   if(!present.alive)fail("PoliticalHeadHandoverSignerDead",actor);}
  return Done{};
 });
}
Status validate_political_head_handover_exact_sources(const PoliticalHeadHandoverContext& x){return guard<Done>([&]{
 if(x.state.version==1){
  for(const auto& r:x.state.offers.rows())if(r.second.exact_issue)fail("HeadHandoverTime2ProfileRequired",r.first);
  for(const auto& r:x.state.answers.rows())if(r.second.exact_issue)fail("HeadHandoverTime2ProfileRequired",r.first);
  for(const auto& r:x.state.withdrawals.rows())if(r.second.exact_issue)fail("HeadHandoverTime2ProfileRequired",r.first);
  return Done{};
 }
 exact_context(x);std::optional<WorldMoment> previous;std::set<StableId> transitions;
 for(const auto& id:x.common.state().facts.order()){
  const auto* h=x.common.state().facts.find(id);if(!h||h->domain!=political_head_handover_domain)continue;
  auto value=issue(x.common,x.state,{h->domain,h->kind,id});const auto at=take(read_political_head_handover_moment2(x,value.source));
  if(previous&&cmp(*previous,at)>0)fail("HeadHandoverExactSourceOrder",id);previous=at;
  if(value.uses_authority){const auto* policy=x.catalog.policies.find(value.offer->policy_id);
   const auto* base=policy?x.succession_catalog.policies.find(policy->succession_policy_id):nullptr;
   if(!policy||!base)fail("PoliticalHeadHandoverOriginalPolicyRequired",id);
   take(validate_head_handover_authority2(x,*policy,*base,id));}
  if(const auto* answer=x.state.answers.find(id);answer&&answer->accepted){
   const auto expected=transition(x,value.source);const auto* saved=x.exact->runtime.office_transition_sources.find(id);
   const auto* changed=x.organizations.facts.find(answer->office_change_fact_id);
   if(!saved||!same_transition(*saved,expected)||!changed||!changed->office_change_payload||!changed->occurred_at||
      cmp(*changed->occurred_at,at)||!transitions.insert(id).second)fail("HeadHandoverOriginalOfficeTransitionMissing",id);
   const auto& p=*changed->office_change_payload;const auto* cause=std::get_if<ByOfficeTransition>(&p.cause);
   if(!cause||!same_source_ref(cause->transition_source_ref,value.source)||p.before_epoch!=expected.expected_epoch||
      p.after_epoch!=expected.expected_epoch+1||p.old_holder!=expected.old_holder||p.new_holder!=expected.new_holder||
      cmp(p.effective_moment,at))fail("HeadHandoverOriginalOfficeTransitionMismatch",id);
  }
 }
 for(const auto& r:x.exact->runtime.authority_bases.rows())if(r.second.issued_source.domain==political_head_handover_domain){
  auto original=issue(x.common,x.state,r.second.issued_source);
  if(r.first!=r.second.issued_source.id||!original.uses_authority)fail("OrphanHeadHandoverAuthorityBasis",r.first);
 }
 for(const auto& r:x.exact->runtime.office_transition_sources.rows())if(r.second.source.domain==political_head_handover_domain&&
    (r.first!=r.second.source.id||!transitions.count(r.first)))fail("OrphanHeadHandoverOfficeTransition",r.first);
 return Done{};
});}
Outcome<VerifiedOfficeTransition> NativeHeadHandoverOfficeRead::read_office_transition(const SourceRef& ref,const ReadStamp& stamp)const{
 return guard<VerifiedOfficeTransition>([&]{
  exact_context(context_);
  if((stamp.mode()!=ReadMode::phase_current&&stamp.mode()!=ReadMode::committed_current)||
     std::find(stamp.source_ids().begin(),stamp.source_ids().end(),ref.id)==stamp.source_ids().end())
   fail("HeadHandoverOfficeSourceStampCoverage",ref.id);
  take(context_.exact->stamp_authority.validate(stamp,context_.common.state().identity,stamp.mode(),context_.common,context_.exact->versions));
  take(validate_political_head_handover_sources(context_));return transition(context_,ref);
 });
}
Outcome<OfficeChangeResult> apply_head_handover_office2(CommonStore& common,OrganizationState& org,
 OrganizationRuntime& runtime,PoliticalHeadHandoverState& state,const PoliticalHeadHandoverContext& x,const StableId& id){
 return guard<OfficeChangeResult>([&]{
  if(&common!=&x.common||&org!=&x.organizations||&state!=&x.state||!x.exact||&runtime!=&x.exact->runtime)
   fail("HeadHandoverTime2CandidateMismatch",id);
  const auto ref=source(common,id);auto expected=transition(x,ref);
  auto* answer=state.answers.find(id);
  if(!answer||!answer->office_change_fact_id.empty()||!common.state().facts.order().size()||common.state().facts.order().back()!=id||
     cmp(expected.effective_moment,take(political_head_handover_now(x))))fail("HeadHandoverActualAcceptanceTailRequired",id);
  const auto* offer=state.offers.find(answer->offer_fact_id);const auto* policy=offer?x.catalog.policies.find(offer->policy_id):nullptr;
  const auto* base=policy?x.succession_catalog.policies.find(policy->succession_policy_id):nullptr;
  if(!offer||!policy||!base||answer->actor_id!=common.state().metadata.player_actor_id||!x.player_before)
   fail("HeadHandoverActualAcceptanceRequired",id);
  take(validate_head_handover_authority2(x,*policy,*base,offer->id));
  take(validate_head_handover_office2(x,*base,offer->issuer_actor_id,offer->office_epoch,offer->id));
  take(head_handover_detail::seat_source(x,*base,expected.effective_moment.half_day_tick,id));
  const auto actual_player=take(x.player_before(expected.effective_moment.half_day_tick,id));
  if(actual_player!=answer->actor_id||!take(political_head_handover_response_open(x,*offer,expected.effective_moment)))
   fail("HeadHandoverActualAcceptanceRequired",id);
  for(const auto& actor:{offer->issuer_actor_id,answer->actor_id}){
   take(validate_head_handover_person2(x,actor,id));take(head_handover_detail::member_before(x,*base,actor,answer->tick,id));
   take(head_handover_detail::contact(x,*base,actor));
  }
  if(!runtime.office_transition_sources.insert(id,expected))fail("DuplicateHeadHandoverOfficeTransition",id);
  // 仅本栈允许答复尚未回填结果ID。对外冷读仍要求完整源+唯一原Office结果。
  class Pending final:public OfficeTransitionSourceRead {
   const VerifiedOfficeTransition& value_;
  public:explicit Pending(const VerifiedOfficeTransition& v):value_(v){}
   Outcome<VerifiedOfficeTransition> read_office_transition(const SourceRef& s,const ReadStamp&)const override{
    return same_source_ref(s,value_.source)?Outcome<VerifiedOfficeTransition>{true,value_,{}}:
     Outcome<VerifiedOfficeTransition>{false,{},common_failure("HeadHandoverPendingSourceMismatch",s.id).error};
   }
  } pending(expected);
  return take(consume_office_transition(common,org,x.organization_catalog,ref,x.exact->stamp,pending));
 });
}
}
