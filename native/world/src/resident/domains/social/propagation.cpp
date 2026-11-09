#include "knowledge_report_category.hpp"
#include "propagation.hpp"
#include "social_time2_version.hpp"
#include "social_knowledge_cut.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void reject(const StableId& c,const StableId& id={},ErrorKind k=ErrorKind::rule){throw Failure{common_failure(c,id,k).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool present(const SourceRef&r){return !r.domain.empty()&&!r.kind.empty()&&!r.id.empty();}
bool same(const CatalogIdentity&a,const CatalogIdentity&b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
bool same(const FixedLockedRuleInput&a,const FixedLockedRuleInput&b){return a.rule_ref==b.rule_ref&&same(a.catalog_identity,b.catalog_identity);}
Integer plus(Integer a,Integer b){if(a<0||b<1||a>std::numeric_limits<Integer>::max()-b)reject("PropagationTickOverflow");return a+b;}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const DynamicSocialState& state(const PropagationReadContext&x){if(!x.social.dynamic||!x.social.dynamic->time2)reject("ExactSocialPropagationStateRequired");return *x.social.dynamic;}
bool selected(const PropagationPolicy&p,KnowledgeCategory category){return std::find(p.enabled_categories.begin(),p.enabled_categories.end(),category)!=p.enabled_categories.end();}
std::optional<KnowledgeCategory> category(const PropagationReadContext&x,const Report&r){return take(read_exact_report_category(x.common,r,x.exact,x.exact_stamp));}
bool recipient_in(const std::vector<SubjectRef>& values,const SubjectRef&s){return std::any_of(values.begin(),values.end(),[&](const auto&v){return same_social_subject(v,s);});}
void unique_subjects(const std::vector<SubjectRef>& values){for(std::size_t i=0;i<values.size();++i){if(values[i].id.empty())reject("PropagationSubjectMissing");for(std::size_t j=0;j<i;++j)if(same_social_subject(values[i],values[j]))reject("DuplicatePropagationRecipient",values[i].id);}}
std::shared_ptr<const ReadStamp> capture(const PropagationReadContext& x,const SubjectRef&subject,const Report*report,const std::vector<SubjectRef>&recipients={}){
 std::vector<ResourceKey> resources;if(subject.kind==SubjectKind::actor)resources.push_back({ResourceKey::Kind::actor,subject.id});for(const auto&r:recipients)if(r.kind==SubjectKind::actor)resources.push_back({ResourceKey::Kind::actor,r.id});
 std::vector<DomainReadKey> domains{{"social","propagation_policy",x.policy.policy_id},{"social","social_policy",subject.id}};
 std::vector<StableId> sources;if(report)sources.push_back(report->fact_id);
 auto stamp=take(x.clock.capture(resources,domains,sources));if(!stamp)reject("PropagationReadStampRequired");return stamp;
}
void registered(const PropagationReadContext&x,const SubjectRef&s,Integer epoch,Integer seq,const WorldMoment&now){
 if(s.kind!=SubjectKind::actor)reject("PropagationSubjectKnowledgeAdapterRequired",s.id);
 if(!x.director.subject2||!x.director_catalog.subjects2||!x.director_catalog.dynamic_subjects)reject("PropagationDirectorRegistryRequired");
 const auto key=director_subject_key2(s);const auto*r=x.director.subject2->registrations.find(key);const auto*q=x.director.schedules.find(key);
 if(!r||!q||r->lifecycle!=DirectorLifecycle::active||r->obligations_only||r->epoch!=epoch||q->decision_seq==std::numeric_limits<Integer>::max()||seq!=q->decision_seq+1||q->next_tick>now.half_day_tick||q->last_decision_tick>=now.half_day_tick)reject("StalePropagationReview",s.id);
 if(s.id==x.common.state().metadata.player_actor_id)reject("PlayerDirectorOverlap2",s.id);
 Integer used=0;for(const auto&v:x.director.schedules.rows())if(v.second.last_decision_tick==now.half_day_tick)++used;
 if(x.director_catalog.decision_budget<1||used>=x.director_catalog.decision_budget)reject("DirectorBoundaryBudgetExhausted",s.id);
 if(take(compare_fractions(now.sub_half_day,CanonicalFraction{})))reject("PropagationOriginalReviewBoundaryRequired");
}
bool already_sent(const DynamicSocialState&s,const SubjectRef&sender,const StableId&report,const SubjectRef&recipient){for(const auto&row:s.time2->propagation_decisions.rows()){const auto&d=row.second;if(same_social_subject(d.subject,sender)&&d.report_id==report&&recipient_in(d.selected_recipients,recipient)&&!d.actual_message_refs.empty())return true;}return false;}
void shape(const DynamicSocialState&s){
 if(!s.time2)reject("SocialTime2StateRequired");
 for(const auto&row:s.time2->propagation_decisions.rows()){const auto&d=row.second;
  if(row.first!=propagation_decision_key(d.subject,d.registration_epoch,d.decision_seq)||d.subject.kind!=SubjectKind::actor||d.registration_epoch<1||d.decision_seq<1||d.report_id.empty()||d.policy_ref.rule_ref.empty()||!same_social_subject(d.subject,d.source_known_key.receiver)||d.report_id!=d.source_known_key.report_id||d.evaluated_at<0||d.next_review_tick<=d.evaluated_at||d.considered_recipients.size()>64||d.selected_recipients.size()>16||d.source_relation_basis.size()!=d.considered_recipients.size()||d.disposition_policy_basis.size()!=d.considered_recipients.size()||d.causal_source_refs.size()>8)reject("InvalidPropagationDecision",row.first);
  if(social_propagation_cut_version(s.version)!=bool(d.actual_adoption))reject("PropagationAdoptionPhysicalMismatch",row.first);
  if(d.actual_adoption){const auto&a=*d.actual_adoption;if(a.original_prefix.common_fact_count<0||bool(a.original_prefix.common_fact_count)!=bool(a.original_prefix.last_common_fact)||cmp(a.adopted_at,WorldMoment{d.evaluated_at,{}}))reject("PropagationAdoptionCutShapeMismatch",row.first);if(a.original_prefix.last_common_fact&&!present(*a.original_prefix.last_common_fact))reject("PropagationAdoptionAnchorRequired",row.first);}
  unique_subjects(d.considered_recipients);unique_subjects(d.selected_recipients);
  const bool sent=d.outcome==SocialPropagationOutcome::queued||d.outcome==SocialPropagationOutcome::delivered_locally;
  if(static_cast<Integer>(d.outcome)<0||static_cast<Integer>(d.outcome)>3||sent!=( !d.actual_message_refs.empty())||d.actual_message_refs.size()!=d.selected_recipients.size())reject("PropagationActualDispatchCoverageMismatch",row.first);
  for(std::size_t i=0;i<d.considered_recipients.size();++i){const auto&r=d.source_relation_basis[i];if(d.considered_recipients[i].kind!=SubjectKind::actor||r.from_actor_id!=d.subject.id||r.to_actor_id!=d.considered_recipients[i].id||r.effective_trust<0||r.effective_trust>100||r.effective_affinity<0||r.effective_affinity>100||d.disposition_policy_basis[i].interval_ticks<1||d.disposition_policy_basis[i].interval_ticks>120)reject("PropagationDispositionBasisMismatch",row.first);}
  for(const auto&r:d.selected_recipients)if(!recipient_in(d.considered_recipients,r))reject("UnconsideredPropagationRecipient",row.first);
  for(std::size_t i=0;i<d.actual_message_refs.size();++i){const auto*message=s.pending_messages.find(d.actual_message_refs[i]);if(!message||message->report_id!=d.report_id||!same_social_subject(message->sender,d.subject)||message->created_tick!=d.evaluated_at)reject("PropagationMessageSourceMissing",row.first);for(std::size_t j=0;j<i;++j)if(d.actual_message_refs[j]==d.actual_message_refs[i])reject("DuplicatePropagationMessage",row.first);}
  for(const auto&r:d.causal_source_refs)if(!present(r))reject("PropagationCauseMissing",row.first);
 }
 for(const auto&row:s.time2->propagation_cooldowns.rows()){const auto&c=row.second;if(row.first!=propagation_cooldown_key(c.subject,c.report_id,c.recipient,c.policy_ref)||c.last_actual_dispatch_tick<0||c.next_eligible_tick<=c.last_actual_dispatch_tick||c.source_decision_ref.domain!="social"||c.source_decision_ref.kind!="propagation_decision")reject("InvalidPropagationCooldown",row.first);const auto*d=s.time2->propagation_decisions.find(c.source_decision_ref.id);if(!d||!same_social_subject(c.subject,d->subject)||c.report_id!=d->report_id||!same(c.policy_ref,d->policy_ref)||d->evaluated_at!=c.last_actual_dispatch_tick||d->next_review_tick!=c.next_eligible_tick||!recipient_in(d->selected_recipients,c.recipient)||d->actual_message_refs.empty())reject("PropagationCooldownWithoutDispatch",row.first);}
 for(const auto&row:s.time2->propagation_decisions.rows())for(const auto&recipient:row.second.selected_recipients){const auto&d=row.second;const auto*c=s.time2->propagation_cooldowns.find(propagation_cooldown_key(d.subject,d.report_id,recipient,d.policy_ref));if(!c||c->source_decision_ref.id!=row.first)reject("PropagationDispatchCooldownMissing",row.first);}
}
}
Status validate_propagation_original_dispatch_after_cut(const CommonStore&common,const PropagationActualDispatch&requested,const PropagationActualDispatch&actual,const OriginalSocialRelationCut&cut){return guard<Done>([&]{
 if(requested.message_id!=actual.message_id||requested.report_id!=actual.report_id||!same_social_subject(requested.sender,actual.sender)||!same_social_subject(requested.recipient,actual.recipient)||requested.channel!=actual.channel||!same(requested.original_business_source,actual.original_business_source)||cmp(requested.created_at,actual.created_at)||bool(requested.delivered_at)!=bool(actual.delivered_at)||(requested.delivered_at&&cmp(*requested.delivered_at,*actual.delivered_at)))reject("PropagationOriginalDispatchProofMismatch",requested.message_id);
 if(cmp(actual.created_at,cut.adopted_at))reject("PropagationOriginalDispatchMomentMismatch",actual.message_id);
 const auto count=take(read_actual_common_fact_prefix(common,cut.original_prefix));const auto&source=actual.original_business_source;
 const auto*head=common.state().facts.find(source.id);if(!head||head->id!=source.id||head->domain!=source.domain||head->kind!=source.kind)reject("PropagationOriginalDispatchCommonSourceRequired",source.id);
 const auto&ids=common.state().facts.order();const auto after=std::find(ids.begin(),ids.end(),source.id);if(after==ids.end()||static_cast<std::size_t>(after-ids.begin())<count)reject("PropagationDispatchBeforeAdoptionCut",source.id);
 return Done{};
});}
StableId propagation_decision_key(const SubjectRef&s,Integer epoch,Integer seq){return director_adoption_key2(s,epoch,seq);}
StableId propagation_cooldown_key(const SubjectRef&s,const StableId&r,const SubjectRef&to,const FixedLockedRuleInput&p){auto part=[](const StableId&v){return std::to_string(v.size())+":"+v;};return part(social_subject_key(s))+part(r)+part(social_subject_key(to))+part(p.catalog_identity.artifact_sha256)+part(p.rule_ref);}
Status validate_propagation_composition(const PropagationPolicy&p,const PropagationPolicyLimits&limits,const PropagationChannelRead&channels){auto valid=validate_propagation_policy(p,limits);if(!valid.ok)return valid;for(auto channel:p.allowed_channels)if(!channels.supports(channel))return common_failure("PropagationOriginalChannelWriterRequired",message_channel_name(channel),ErrorKind::unsupported_capability);for(auto category:p.enabled_categories)if(!channels.supports_report(category))return common_failure("PropagationDisclosureReaderRequired",propagation_category_name(category),ErrorKind::unsupported_capability);return common_success();}
Status validate_social_propagation_shape(const DynamicSocialState&s){return guard<Done>([&]{shape(s);return Done{};});}
Outcome<std::optional<PropagationProposal>> prepare_propagation_review(const PropagationReadContext&x,const SubjectRef&subject,Integer epoch,Integer seq){return guard<std::optional<PropagationProposal>>([&]()->std::optional<PropagationProposal>{
 take(validate_propagation_composition(x.policy,x.limits,x.channels));const auto now=take(x.clock.read_current_moment());registered(x,subject,epoch,seq,now);const auto&s=state(x);if(!social_propagation_cut_version(s.version))reject("PropagationActualAdoptionProfileRequired",{},ErrorKind::unsupported_capability);const auto*known=find_social_knowledge_table(s,subject);if(!known)return {};
 struct Candidate {const Report*report;WorldMoment learned;};std::vector<Candidate> reports;
 for(const auto&row:known->rows()){const auto*r=s.reports.find(row.first);if(!r)reject("PropagationKnownReportMissing",row.first);auto cat=category(x,*r);if(!cat||!selected(x.policy,*cat))continue;if(r->valid_until_tick&&now.half_day_tick>*r->valid_until_tick)continue;if(now.half_day_tick<r->observation_tick||now.half_day_tick-r->observation_tick>x.policy.expiry_ticks)continue;
  auto receipt=take(read_knowledge_receipt_view_v2(x.common,x.social,x.exact,subject,row.first,x.exact_stamp));if(!receipt)reject("PropagationKnowledgeReceiptMissing",row.first);if(cmp(receipt->learned_at,now)>=0)continue;
  Candidate item{r,receipt->learned_at};auto position=reports.begin();while(position!=reports.end()&&(cmp(position->learned,item.learned)<0||(cmp(position->learned,item.learned)==0&&position->report->id<r->id)))++position;reports.insert(position,std::move(item));if(reports.size()>static_cast<std::size_t>(x.policy.max_candidates))reports.pop_back();
 }
 for(const auto&candidate:reports){PropagationProposal proposal;auto&d=proposal.decision;d.subject=subject;d.registration_epoch=epoch;d.decision_seq=seq;d.report_id=candidate.report->id;d.policy_ref={x.catalog_identity,x.policy.policy_id};d.source_known_key={subject,d.report_id};d.evaluated_at=now.half_day_tick;d.next_review_tick=plus(d.evaluated_at,x.policy.cooldown_ticks);
  auto initial=capture(x,subject,candidate.report);auto recipients=take(x.channels.known_recipients(subject,x.director.fairness_cursor,x.policy.max_candidates,*initial));if(recipients.size()>static_cast<std::size_t>(x.policy.max_candidates))reject("PropagationRecipientBudgetExceeded");
  std::vector<SubjectRef> subjects;for(const auto&r:recipients){if(r.subject.kind!=SubjectKind::actor||same_social_subject(r.subject,subject)||!present(r.actual_known_contact_source))reject("PropagationKnownContactSourceRequired",r.subject.id);subjects.push_back(r.subject);}unique_subjects(subjects);proposal.stamp=capture(x,subject,candidate.report,subjects);
  struct Ranked {Integer score;PropagationChannelProposal proposal;};std::vector<Ranked> ranked;bool deferred=false;
  for(const auto&r:recipients){if(already_sent(s,subject,d.report_id,r.subject))continue;const auto*cooldown=s.time2->propagation_cooldowns.find(propagation_cooldown_key(subject,d.report_id,r.subject,d.policy_ref));if(cooldown&&cooldown->next_eligible_tick>d.evaluated_at)continue;
   // 保密、已知联系、渠道先于关系评分。没有真实合法渠道是正常等待。
   std::optional<PropagationChannelProposal> channel;for(auto kind:x.policy.allowed_channels){channel=take(x.channels.prepare(subject,*candidate.report,r,kind,*proposal.stamp));if(channel)break;}if(!channel){deferred=true;continue;}if(!channel->original_prepared||!same_social_subject(channel->recipient,r.subject))reject("PropagationPreparedChannelMismatch",r.subject.id);
   auto disposition=take(x.disposition.read_propagation_disposition_v1(subject.id,r.subject.id,*proposal.stamp));const auto*policy=std::get_if<KnownSocialPolicyBasis>(&disposition.policy_basis);if(!policy)continue;
   if(disposition.sender_actor_id!=subject.id||disposition.candidate_receiver_actor_id!=r.subject.id||cmp(disposition.observed_at,now))reject("PropagationDispositionCandidateMismatch",r.subject.id);
   d.considered_recipients.push_back(r.subject);d.source_relation_basis.push_back(disposition.relation_projection);d.disposition_policy_basis.push_back(*policy);d.next_review_tick=std::max(d.next_review_tick,plus(d.evaluated_at,policy->interval_ticks));
   if(!policy->share_news)continue;const auto&relation=disposition.relation_projection;const Integer score=2*relation.effective_trust+relation.effective_affinity;const auto threshold=policy->kind==KnownSocialPolicyBasis::Kind::cooperative?x.policy.relation_thresholds_by_social_kind.cooperative:x.policy.relation_thresholds_by_social_kind.selfish;if(score>=threshold)ranked.push_back({score,std::move(*channel)});
  }
  if(d.considered_recipients.empty()&&!deferred)continue;
  std::sort(ranked.begin(),ranked.end(),[](const auto&a,const auto&b){return a.score!=b.score?a.score>b.score:social_subject_key(a.proposal.recipient)<social_subject_key(b.proposal.recipient);});if(ranked.size()>static_cast<std::size_t>(x.policy.max_dispatches_per_review))ranked.resize(x.policy.max_dispatches_per_review);
  for(auto&r:ranked){d.selected_recipients.push_back(r.proposal.recipient);proposal.original_dispatches.push_back(std::move(r.proposal));}d.outcome=ranked.empty()?(deferred?SocialPropagationOutcome::deferred:SocialPropagationOutcome::declined):SocialPropagationOutcome::queued;
  const auto*binding=s.time2->source_time_bindings.find(social_time_key(d.source_known_key));if(!binding)reject("PropagationKnowledgeTimeBindingMissing",d.report_id);d.causal_source_refs.push_back(binding->actual_source_ref);return proposal;
 }
 return {};
});}
Outcome<const SocialPropagationDecision*> adopt_propagation_review_in_candidate(SocialState&social,DirectorState&director,const PropagationReadContext&x,PropagationChannelWrite&writer,const PropagationProposal&p){return guard<const SocialPropagationDecision*>([&]{
 if(&social!=&x.social||&director!=&x.director)reject("PropagationCandidateMismatch");const auto key=propagation_decision_key(p.decision.subject,p.decision.registration_epoch,p.decision.decision_seq);auto&time=*state(x).time2;
 if(const auto*old=time.propagation_decisions.find(key)){if(old->report_id!=p.decision.report_id||!same(old->policy_ref,p.decision.policy_ref))reject("PropagationReplayMismatch",key);take(validate_social_propagation(x));return old;}
 if(!p.stamp)reject("PropagationReadStampRequired");take(x.authority.validate(*p.stamp,x.common.state().identity,p.stamp->mode(),x.common,x.versions));
 auto fresh=take(prepare_propagation_review(x,p.decision.subject,p.decision.registration_epoch,p.decision.decision_seq));if(!fresh||fresh->decision.report_id!=p.decision.report_id||fresh->decision.selected_recipients.size()!=p.decision.selected_recipients.size())reject("StalePropagationProposal",key);
 for(std::size_t i=0;i<p.decision.selected_recipients.size();++i)if(!same_social_subject(fresh->decision.selected_recipients[i],p.decision.selected_recipients[i]))reject("StalePropagationRecipients",key);
 auto decision=std::move(fresh->decision);const auto*report=social.dynamic->reports.find(decision.report_id);if(!report)reject("PropagationReportMissing",key);const WorldMoment evaluated{decision.evaluated_at,{}};bool all_local=true;
 // Pure preparation above has re-read the actual disposition. Capture its
 // Common cut now, before the first original channel can write any result.
 if(!social_propagation_cut_version(social.dynamic->version))reject("PropagationActualAdoptionProfileRequired",key,ErrorKind::unsupported_capability);
 const auto adoption_at=take(x.clock.read_current_moment());if(cmp(adoption_at,evaluated)||decision.actual_adoption)reject("PropagationActualAdoptionMomentMismatch",key);
 const auto prefix=take(read_current_common_fact_prefix(x.common));decision.actual_adoption.emplace(adoption_at,prefix);
 const OriginalSocialRelationCut adopted_cut{decision.actual_adoption->adopted_at,decision.actual_adoption->original_prefix};
 for(const auto&channel:fresh->original_dispatches){
  // 每次原writer之后重新签发本阶段读凭据，不能把首个写前票据复用于第二次发送。
  auto stamp=capture(x,decision.subject,report,decision.selected_recipients);take(x.authority.validate(*stamp,x.common.state().identity,stamp->mode(),x.common,x.versions));
  auto id=take(writer.adopt(channel,decision.subject,decision.report_id,*stamp));if(id.empty())reject("PropagationActualMessageRequired",key);
  auto actual=take(x.channels.read_actual_dispatch(id));if(actual.message_id!=id||actual.report_id!=decision.report_id||!same_social_subject(actual.sender,decision.subject)||!same_social_subject(actual.recipient,channel.recipient)||actual.channel!=channel.channel||cmp(actual.created_at,evaluated)||!present(actual.original_business_source))reject("PropagationActualDispatchMismatch",id);
  take(x.channels.validate_actual_dispatch_after_cut(actual,adopted_cut));
  if(!actual.delivered_at||cmp(*actual.delivered_at,evaluated)||actual.channel!=MessageChannelKind::face_to_face)all_local=false;
  decision.actual_message_refs.push_back(id);
 }
 if(!decision.actual_message_refs.empty())decision.outcome=all_local?SocialPropagationOutcome::delivered_locally:SocialPropagationOutcome::queued;
 auto&mutable_time=*social.dynamic->time2;if(!mutable_time.propagation_decisions.insert(key,std::move(decision)))reject("DuplicatePropagationDecision",key);const auto*saved=mutable_time.propagation_decisions.find(key);
 for(const auto&recipient:saved->selected_recipients){SocialPropagationCooldown cooldown{saved->subject,saved->report_id,recipient,saved->policy_ref,saved->evaluated_at,saved->next_review_tick,{"social","propagation_decision",key}};auto cooldown_key=propagation_cooldown_key(saved->subject,saved->report_id,recipient,saved->policy_ref);if(!mutable_time.propagation_cooldowns.insert(cooldown_key,std::move(cooldown)))reject("DuplicatePropagationActualDispatch",cooldown_key);}
 auto*schedule=director.schedules.find(director_subject_key2(saved->subject));if(!schedule||director.fairness_cursor==std::numeric_limits<Integer>::max())reject("PropagationScheduleMissing",key);
 schedule->decision_seq=saved->decision_seq;schedule->last_decision_tick=saved->evaluated_at;schedule->next_tick=saved->next_review_tick;schedule->waiting_since=saved->evaluated_at;schedule->last_outcome="propagation:"+key;
 for(auto&row:director.subject2->wake_sources.rows())if(same_social_subject(row.second.wake.subject,saved->subject)&&!row.second.consumed_decision_seq)row.second.consumed_decision_seq=saved->decision_seq;schedule->wake_reasons.clear();++director.fairness_cursor;director.last_boundary_tick=saved->evaluated_at;
 take(validate_social_propagation(x));return saved;
});}
Status validate_social_propagation(const PropagationReadContext&x){return guard<Done>([&]{take(validate_propagation_composition(x.policy,x.limits,x.channels));const auto&s=state(x);shape(s);const auto now=take(x.clock.read_current_moment());
 for(const auto&row:s.time2->propagation_decisions.rows()){const auto&d=row.second;const WorldMoment evaluated{d.evaluated_at,{}};
  if(d.policy_ref.rule_ref!=x.policy.policy_id||!same(d.policy_ref.catalog_identity,x.catalog_identity)||cmp(evaluated,now)>0||d.considered_recipients.size()>static_cast<std::size_t>(x.policy.max_candidates)||d.selected_recipients.size()>static_cast<std::size_t>(x.policy.max_dispatches_per_review))reject("PropagationHistoricalPolicyMismatch",row.first);
  const auto*report=s.reports.find(d.report_id);if(!report)reject("PropagationReportMissing",row.first);auto cat=category(x,*report);if(!cat||!selected(x.policy,*cat)||d.evaluated_at<report->observation_tick||d.evaluated_at-report->observation_tick>x.policy.expiry_ticks||(report->valid_until_tick&&d.evaluated_at>*report->valid_until_tick))reject("PropagationExpiredOrDisabledReport",row.first);
  auto known=take(read_knowledge_receipt_view_v2(x.common,x.social,x.exact,d.subject,d.report_id,x.exact_stamp));if(!known||cmp(known->learned_at,evaluated)>=0)reject("PropagationKnowledgeCausalityMismatch",row.first);
  const auto*binding=s.time2->source_time_bindings.find(social_time_key(d.source_known_key));if(!binding||d.causal_source_refs.empty()||!same(d.causal_source_refs.front(),binding->actual_source_ref))reject("PropagationKnowledgeCauseMismatch",row.first);
  if(d.actual_adoption){
   const auto count=take(read_actual_common_fact_prefix(x.common,d.actual_adoption->original_prefix));
   take(validate_social_knowledge_cut(x.common,s,*binding));if(!binding->learned_after_common_fact)reject("PropagationKnownInputCommonCutRequired",row.first);
   bool included=false;const auto&ids=x.common.state().facts.order();for(std::size_t n=0;n<count;++n){const auto*h=x.common.state().facts.find(ids[n]);if(!h)reject("PropagationCommonPrefixHeadMissing",ids[n]);if(h->id==binding->learned_after_common_fact->id){if(h->domain!=binding->learned_after_common_fact->domain||h->kind!=binding->learned_after_common_fact->kind)reject("PropagationKnownInputCommonHeadMismatch",row.first);included=true;}}
   if(!included)reject("PropagationKnowledgeAfterAdoptionCut",row.first);
  }
  Integer next=plus(d.evaluated_at,x.policy.cooldown_ticks);
  for(std::size_t i=0;i<d.considered_recipients.size();++i){if(d.actual_adoption)take(x.channels.validate_known_recipient_at_cut(d.subject,d.considered_recipients[i],OriginalSocialRelationCut{d.actual_adoption->adopted_at,d.actual_adoption->original_prefix}));const auto&policy=d.disposition_policy_basis[i];PropagationDispositionV1 adopted{d.subject.id,d.considered_recipients[i].id,policy,d.source_relation_basis[i],evaluated};if(d.actual_adoption)take(x.disposition.validate_adopted_propagation_disposition(adopted,{"social","propagation_decision",row.first},OriginalSocialRelationCut{d.actual_adoption->adopted_at,d.actual_adoption->original_prefix}));else take(x.disposition.validate_adopted_propagation_disposition(adopted,{"social","propagation_decision",row.first}));next=std::max(next,plus(d.evaluated_at,policy.interval_ticks));if(recipient_in(d.selected_recipients,d.considered_recipients[i])){const auto&r=d.source_relation_basis[i];const auto threshold=policy.kind==KnownSocialPolicyBasis::Kind::cooperative?x.policy.relation_thresholds_by_social_kind.cooperative:x.policy.relation_thresholds_by_social_kind.selfish;if(!policy.share_news||2*r.effective_trust+r.effective_affinity<threshold)reject("PropagationBelowActualDispositionThreshold",row.first);}}
  if(next!=d.next_review_tick)reject("PropagationReviewBoundaryMismatch",row.first);
  bool all_local=!d.actual_message_refs.empty();for(std::size_t i=0;i<d.actual_message_refs.size();++i){auto actual=take(x.channels.read_actual_dispatch(d.actual_message_refs[i]));if(actual.report_id!=d.report_id||!same_social_subject(actual.sender,d.subject)||!same_social_subject(actual.recipient,d.selected_recipients[i])||cmp(actual.created_at,evaluated)||!present(actual.original_business_source)||std::find(x.policy.allowed_channels.begin(),x.policy.allowed_channels.end(),actual.channel)==x.policy.allowed_channels.end())reject("PropagationHistoricalDispatchMismatch",row.first);take(x.channels.validate_historical_disclosure(d,actual));if(d.actual_adoption)take(x.channels.validate_actual_dispatch_after_cut(actual,OriginalSocialRelationCut{d.actual_adoption->adopted_at,d.actual_adoption->original_prefix}));if(!actual.delivered_at||cmp(*actual.delivered_at,evaluated)||actual.channel!=MessageChannelKind::face_to_face)all_local=false;}
  if(d.outcome==SocialPropagationOutcome::delivered_locally&&!all_local)reject("PropagationLocalDeliveryUnproven",row.first);
  const auto*registration=x.director.subject2?x.director.subject2->registrations.find(director_subject_key2(d.subject)):nullptr;const auto*schedule=x.director.schedules.find(director_subject_key2(d.subject));if(!registration||!schedule||d.registration_epoch>registration->epoch||d.decision_seq>schedule->decision_seq||d.evaluated_at>schedule->last_decision_tick)reject("PropagationDirectorHistoryMismatch",row.first);
  if(x.director.subject2->adoption_results.find(director_adoption_key2(d.subject,d.registration_epoch,d.decision_seq)))reject("DuplicateDirectorDecisionSequence2",row.first);
  for(const auto&other:s.time2->propagation_decisions.rows())if(other.first!=row.first&&same_social_subject(other.second.subject,d.subject)&&other.second.report_id==d.report_id)for(const auto&r:d.selected_recipients)if(recipient_in(other.second.selected_recipients,r))reject("DuplicatePropagationResponsibility",row.first);
 }
 return Done{};
});}
Outcome<Integer> social_propagation_decision_source_schema(const DynamicSocialState&s,const SourceRef&r){if(r.domain!="social"||r.kind!="propagation_decision"||!s.time2||!social_time2_version(s.version)||!s.time2->propagation_decisions.find(r.id))return {false,0,common_failure("PropagationDecisionSourceRequired",r.id,ErrorKind::unsupported_capability).error};return {true,social_propagation_cut_version(s.version)?2:1,{}};}
Outcome<const SocialPropagationDecision*> read_social_propagation_decision_source(const PropagationReadContext&x,const SourceRef&r,Integer schema){return guard<const SocialPropagationDecision*>([&]{const auto&s=state(x);if(take(social_propagation_decision_source_schema(s,r))!=schema)reject("PropagationDecisionSourceSchemaMismatch",r.id);take(validate_social_propagation(x));return s.time2->propagation_decisions.find(r.id);});}
StableId propagation_adoption_prefix_pin_key(const StableId&key,Integer ordinal){if(key.empty()||ordinal<0)return {};return std::to_string(key.size())+":"+key+":"+std::to_string(ordinal);}
Status visit_propagation_adoption_prefix_pins(const CommonStore&c,const DynamicSocialState&s,PropagationAdoptionPrefixPinSink&sink){return guard<Done>([&]{
 shape(s);for(const auto&entry:s.time2->propagation_decisions.rows()){if(!entry.second.actual_adoption)continue;const auto count=take(read_actual_common_fact_prefix(c,entry.second.actual_adoption->original_prefix));const auto&ids=c.state().facts.order();
  for(std::size_t n=0;n<count;++n){const auto*h=c.state().facts.find(ids[n]);if(!h||h->id!=ids[n]||h->domain.empty()||h->kind.empty())reject("PropagationCommonPrefixHeadMissing",ids[n]);take(sink.original_prefix_fact(entry.first,static_cast<Integer>(n),*h));}
 }return Done{};
});}
}
