#include "care_requirement_contract.hpp"
#include "care_coverage.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include <algorithm>
#include <limits>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool same(const SourceRef& a,const SourceRef& b){return std::tie(a.domain,a.kind,a.id)==std::tie(b.domain,b.kind,b.id);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
SourceRef transition_ref(const StableId& id){return {"care_requirements","care_requirement_changed",id};}
SourceRef birth_ref(const BirthFactRef& ref){return {"family_birth","family_birth_recorded",ref.birth_fact_id};}
bool is_transition(const SourceRef& ref){return !ref.id.empty()&&ref.domain=="care_requirements"&&ref.kind=="care_requirement_changed";}
const FactRecord& header(const CommonStore& c,const SourceRef& ref){const auto* value=c.state().facts.find(ref.id);if(!value||value->id!=ref.id||value->domain!=ref.domain||value->kind!=ref.kind||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),ref.id)!=1)fail("CareOriginalFactRequired",ref.id);return *value;}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){header(c,a);header(c,b);const auto& order=c.state().facts.order();auto first=std::find(order.begin(),order.end(),a.id),last=std::find(order.begin(),order.end(),b.id);if(first==order.end()||last==order.end()||first>=last)fail("CareOriginalSourceOrderMismatch",b.id);}
void rule_shape(const CareRequirementRule& value){for(const auto* r:{&value.rule_ref,&value.dependency_rule_ref,&value.cancellation_rule_ref,&value.disclosure_rule_ref})if(r->id.empty()||r->version<1||r->catalog_digest.empty())fail("CareOriginalRuleRequired");if(value.service_kind.empty()||value.state_need_policy_ref.empty())fail("CareOriginalServicePolicyRequired");}
WorldMoment context(const CareGoalSourceContext& x){if(x.rules.selected_care_version()<3||x.rules.selected_care_version()>6||!x.births.bound_to(x.common)||!x.rules.bound_to(x.common))fail("CareExactOriginalSourcesRequired",{},ErrorKind::unsupported_capability);return take(x.clock.read_current_moment());}
void writable(CommonStore& c,CareGoalWriteTables out,const CareGoalSourceContext& x){if(&c!=&x.common||&out.requirements!=&x.goals.requirements||&out.transitions!=&x.goals.transitions||&out.temporal_events!=&x.goals.temporal_events)fail("CareSameCandidateRequired");}
const CareRequirementRule& selected_rule(const CareGoalSourceContext& x,const CareRequirement& requirement){const auto* selected=take(x.rules.read_care_requirement_rule(requirement.rule_version));if(!selected||!actor_foundation_rule_equal(selected->rule_ref,requirement.rule_version))fail("CareOriginalRuleMismatch",requirement.id);rule_shape(*selected);return *selected;}
const CareTemporalEvent& event_for(const CareGoalSourceContext& x,const CareRequirementTransition& transition){const auto* event=x.goals.temporal_events.find(transition.transition_id);if(!event||event->event_id!=transition.transition_id||!same(event->original_transition_ref,transition_ref(transition.transition_id)))fail("CareTemporalEventCoverageMismatch",transition.transition_id);return *event;}
struct Fold {const CareTemporalEvent* first=nullptr;const CareTemporalEvent* last=nullptr;Integer revision=0,episode=0;StateGoalState state=StateGoalState::unsatisfied;std::optional<StableId> accepted,binding;const CareTemporalEvent* satisfaction=nullptr;};
// Source validation is separated from the current-life terminal check only at
// this private writer entry. A dead subject must be closed before publication.
Fold chain(const CareGoalSourceContext& x,const CareRequirement& goal,const WorldMoment& now,bool check_current_life,const SourceRef* actual_before=nullptr){
 if(goal.id.empty()||goal.subject_actor_id.empty()||goal.revision<1||goal.source_episode<1||!is_transition(goal.current_transition_ref)||!x.common.actor(goal.subject_actor_id))fail("InvalidCareRequirement",goal.id);
 const auto& rule=selected_rule(x,goal);const auto* original_birth=std::get_if<BirthFactRef>(&goal.care_basis);if(!original_birth)fail("DependentCareApplicabilitySourceRequired",goal.id,ErrorKind::unsupported_capability);
 auto born=take(x.births.read_original_birth(*original_birth));if(!born.birth||!born.accepted||!born.original_fact||born.birth->actor_id!=goal.subject_actor_id)fail("CareBirthSubjectMismatch",goal.id);
 const auto* original_rule=take(x.rules.read_birth_care_rule(born));if(!original_rule||!actor_foundation_rule_equal(original_rule->rule_ref,rule.rule_ref))fail("CareBirthRuleMismatch",goal.id);
 for(const auto& row:x.goals.transitions.rows())if(row.second.requirement_id==goal.id){if(row.first!=row.second.transition_id)fail("CareTransitionKeyMismatch",row.first);header(x.common,transition_ref(row.first));event_for(x,row.second);}
 Fold fold;const CareRequirementTransition* previous=nullptr;
 // Only the actual original Common append order establishes same-M order.
 for(const auto& id:x.common.state().facts.order()){
  if(actual_before&&id==actual_before->id)break;
  const auto* transition=x.goals.transitions.find(id);if(!transition||transition->requirement_id!=goal.id)continue;
  if(transition->transition_id!=id)fail("CareTransitionKeyMismatch",id);const auto& event=event_for(x,*transition);header(x.common,event.original_transition_ref);
  if(!actor_foundation_rule_equal(event.rule_ref,goal.rule_version)||take(compare_world_moments(event.occurred_at,now))>0)fail("CareTemporalEventRuleOrTimeMismatch",id);
  if(previous){if(!transition->previous_transition_ref||!same(*transition->previous_transition_ref,transition_ref(previous->transition_id))||take(compare_world_moments(fold.last->occurred_at,event.occurred_at))>0)fail("CareTransitionPredecessorMismatch",id);before(x.common,*transition->previous_transition_ref,event.original_transition_ref);}
  else if(transition->previous_transition_ref)fail("CareUnexpectedInitialPredecessor",id);
  if(transition->revision_before!=fold.revision||fold.revision==std::numeric_limits<Integer>::max()||transition->revision_after!=fold.revision+1||transition->source_episode_before!=fold.episode)fail("CareTransitionRevisionMismatch",id);
  if(transition->kind==CareRequirementTransition::Kind::requirement_created){
   const auto* created=std::get_if<CareRequirementCreatedEvent>(&event.body);const auto* origin=created?std::get_if<BirthDueCareOrigin>(&created->origin):nullptr;
   if(previous||!origin||created->requirement_id!=goal.id||created->source_episode!=1||transition->source_episode_after!=1||origin->birth_ref.birth_fact_id!=original_birth->birth_fact_id||!same(transition->cause_ref,birth_ref(*original_birth))||!same(origin->scheduled_source_ref,SourceRef{"family_birth","family_birth_accepted",born.birth->accepted_id})||!same(event.occurred_at,born.occurred_at)||goal.created_tick!=event.occurred_at.half_day_tick)fail("CareRequirementCreatedSourceMismatch",id);
   before(x.common,origin->scheduled_source_ref,transition->cause_ref);before(x.common,transition->cause_ref,event.original_transition_ref);fold.first=&event;fold.episode=1;
  }else if(transition->kind==CareRequirementTransition::Kind::handover_completed){
   const auto* completed=std::get_if<CareHandoverCompletedEvent>(&event.body);
   if(!previous||fold.state!=StateGoalState::unsatisfied||!completed||completed->requirement_id!=goal.id||completed->source_episode!=fold.episode||transition->source_episode_after!=fold.episode)fail("CareHandoverGoalPredecessorMismatch",id);
   if(!x.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);
   check(validate_original_care_handover_coverage(*x.coverage,goal,*transition,event));
   const auto* binding=x.coverage->tables.bindings.find(completed->binding_id);if(!binding)fail("CareCompletedBindingMissing",id);
   fold.state=StateGoalState::satisfied;fold.binding=binding->binding_id;fold.accepted=binding->accepted_ref;fold.satisfaction=&event;
  }else if(transition->kind==CareRequirementTransition::Kind::coverage_lost){
   const auto* lost=std::get_if<CareCoverageLostEvent>(&event.body);
   if(!previous||fold.state!=StateGoalState::satisfied||!fold.binding||!lost||lost->old_binding_ref!=*fold.binding||lost->prior_source_episode!=fold.episode||fold.episode==std::numeric_limits<Integer>::max()||lost->next_source_episode!=fold.episode+1||transition->source_episode_after!=lost->next_source_episode)fail("CareLossGoalPredecessorMismatch",id);
   if(!x.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);
   check(validate_original_care_coverage_loss(*x.coverage,goal,*transition,event));
   fold.state=StateGoalState::unsatisfied;fold.episode=lost->next_source_episode;fold.binding.reset();fold.accepted.reset();fold.satisfaction=nullptr;
  }else if(transition->kind==CareRequirementTransition::Kind::requirement_closed){
   const auto* closed=std::get_if<CareRequirementClosedEvent>(&event.body);const auto* death=closed?std::get_if<CareSubjectDeath>(&closed->cause):nullptr;
   if(!previous||(fold.state!=StateGoalState::unsatisfied&&fold.state!=StateGoalState::satisfied)||!closed||!death||closed->requirement_id!=goal.id||closed->source_episode!=fold.episode||transition->source_episode_after!=fold.episode||closed->final_state!=StateGoalState::invalid)fail("CareSubjectDeathClosureMismatch",id);
   auto actual=take(read_verified_death4(x.common,x.deaths,x.life,death->death_ref,now));const SourceRef source{death->death_ref.domain,death->death_ref.kind,death->death_ref.id};
   if(actual.actor_id!=goal.subject_actor_id||actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(transition->cause_ref,source)||!same(event.occurred_at,actual.effective_at.exact_moment)||!same(event.occurred_at,actual.recorded_at.exact_moment))fail("CareSubjectDeathClosureSourceMismatch",id);
   before(x.common,source,event.original_transition_ref);fold.state=StateGoalState::invalid;fold.binding.reset();fold.accepted.reset();fold.satisfaction=nullptr;
  }else fail("CareHandoverAndCoverageOriginalSourceReadRequired",id,ErrorKind::unsupported_capability);
  fold.revision=transition->revision_after;fold.last=&event;previous=transition;
 }
 if(!previous||!fold.first)fail("CareRequirementPrefixMissing",goal.id);
 if(actual_before)return fold;
 if(!same(goal.current_transition_ref,transition_ref(previous->transition_id))||goal.revision!=fold.revision||goal.source_episode!=fold.episode||goal.state!=fold.state)fail("CareRequirementCurrentSourceMismatch",goal.id);
 if(goal.accepted_care_terms_ref!=fold.accepted||goal.current_care_binding_ref!=fold.binding)fail("CareGoalBindingProjectionMismatch",goal.id);
 if(check_current_life&&x.coverage)check(validate_current_care_coverage(*x.coverage,goal,now));
 if(check_current_life&&(fold.state==StateGoalState::unsatisfied||fold.state==StateGoalState::satisfied)&&!x.common.actor(goal.subject_actor_id)->alive)fail("CareSubjectDeathTransitionMissing",goal.id);
 return fold;
}
void unique_birth(const CareGoalSourceContext& x,const CareRequirement& goal){const auto* birth=std::get_if<BirthFactRef>(&goal.care_basis);if(!birth)return;for(const auto& row:x.goals.requirements.rows())if(row.first!=goal.id){const auto* other=std::get_if<BirthFactRef>(&row.second.care_basis);if(other&&(row.second.subject_actor_id==goal.subject_actor_id||other->birth_fact_id==birth->birth_fact_id))fail("ConflictingCareRequirement",goal.subject_actor_id);}}
void append(CommonStore& c,CareGoalWriteTables tables,CareRequirementTransition transition,CareTemporalEvent event){const auto id=transition.transition_id;if(event.event_id!=id||!same(event.original_transition_ref,transition_ref(id)))fail("CareTransitionEventIdentityMismatch",id);if(!tables.transitions.insert(id,std::move(transition))||!tables.temporal_events.insert(id,std::move(event)))fail("DuplicateCareTransition",id);check(c.append_fact({id,"care_requirement_changed","care_requirements"}));}
}
Outcome<Integer> care_goal_transition_source_schema(const SourceRef& ref){return is_transition(ref)?Outcome<Integer>{true,3,{}}:Outcome<Integer>{false,{},common_failure("UnknownCareTransitionSource",ref.id,ErrorKind::unsupported_capability).error};}
Status validate_care_requirement_goal_sources(const CareGoalSourceContext& x){try{
 const auto now=context(x);for(const auto& row:x.goals.requirements.rows()){if(row.first!=row.second.id)fail("CareRequirementKeyMismatch",row.first);unique_birth(x,row.second);chain(x,row.second,now,true);}
 for(const auto& row:x.goals.transitions.rows()){if(row.first!=row.second.transition_id||!x.goals.requirements.find(row.second.requirement_id))fail("OrphanCareTransition",row.first);event_for(x,row.second);header(x.common,transition_ref(row.first));}
 for(const auto& row:x.goals.temporal_events.rows())if(row.first!=row.second.event_id||!x.goals.transitions.find(row.first)||!same(row.second.original_transition_ref,transition_ref(row.first)))fail("OrphanCareTemporalEvent",row.first);
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="care_requirements"&&row.second.kind=="care_requirement_changed"&&!x.goals.transitions.find(row.first))fail("CareTransitionPayloadMissing",row.first);
 if(x.coverage)check(validate_care_coverage_source_closure(*x.coverage));
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareTemporalEvent*> read_care_goal_transition_source(const CareGoalSourceContext& x,const SourceRef& ref){try{if(!is_transition(ref))fail("UnknownCareTransitionSource",ref.id,ErrorKind::unsupported_capability);const auto now=context(x);const auto* row=x.goals.transitions.find(ref.id);if(!row)fail("CareTransitionMissing",ref.id);const auto* goal=x.goals.requirements.find(row->requirement_id);if(!goal)fail("CareRequirementMissing",row->requirement_id);unique_birth(x,*goal);chain(x,*goal,now,true);return {true,&event_for(x,*row),{}};}catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CareGoalBeforeFact> read_care_requirement_before_original_fact(const CareGoalSourceContext& x,const StableId& id,const SourceRef& consumer){try{
 const auto now=context(x);header(x.common,consumer);const auto* goal=x.goals.requirements.find(id);if(!goal)fail("CareRequirementMissing",id);unique_birth(x,*goal);auto actual=chain(x,*goal,now,false,&consumer);return {true,{actual.revision,actual.episode,actual.state,actual.last},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareRequirement*> read_birth_care_requirement(const CareGoalSourceContext& x,const BirthFactRef& birth){try{const auto now=context(x);take(x.births.read_original_birth(birth));const CareRequirement* result=nullptr;for(const auto& row:x.goals.requirements.rows()){const auto* source=std::get_if<BirthFactRef>(&row.second.care_basis);if(source&&source->birth_fact_id==birth.birth_fact_id){if(result)fail("ConflictingCareRequirement",birth.birth_fact_id);result=&row.second;}}if(result){unique_birth(x,*result);chain(x,*result,now,true);}return {true,result,{}};}catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CareRequirementApplication> create_birth_care_requirement_in_candidate(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,const BirthFactRef& ref){try{
 writable(common,tables,x);const auto now=context(x);auto existing=take(read_birth_care_requirement(x,ref));if(existing){auto original=chain(x,*existing,now,true);return {true,{existing->id,original.first->original_transition_ref,true},{}};}
 auto original=take(x.births.read_original_birth(ref));const auto* rule=take(x.rules.read_birth_care_rule(original));if(!rule)fail("BirthCareRequirementRuleRequired",ref.birth_fact_id,ErrorKind::unsupported_capability);rule_shape(*rule);
 if(!original.birth||!same(original.occurred_at,now)||!common.actor(original.birth->actor_id)||!common.actor(original.birth->actor_id)->alive)fail("CareRequirementMustFollowActualBirth",ref.birth_fact_id);
 CareRequirement goal;goal.id=take(common.next_id("care_requirement"));goal.subject_actor_id=original.birth->actor_id;goal.created_tick=now.half_day_tick;goal.rule_version=rule->rule_ref;goal.care_basis=ref;const auto id=take(common.next_id("care_transition"));goal.current_transition_ref=transition_ref(id);unique_birth(x,goal);
 CareRequirementTransition transition{id,goal.id,0,1,0,1,CareRequirementTransition::Kind::requirement_created,std::nullopt,birth_ref(ref)};
 CareTemporalEvent event{id,goal.current_transition_ref,now,rule->rule_ref,CareRequirementCreatedEvent{goal.id,1,BirthDueCareOrigin{ref,{"family_birth","family_birth_accepted",original.birth->accepted_id}}}};
 const auto requirement=goal.id;if(!tables.requirements.insert(requirement,std::move(goal)))fail("DuplicateCareRequirement",requirement);append(common,tables,std::move(transition),std::move(event));chain(x,*tables.requirements.find(requirement),now,true);return {true,{requirement,transition_ref(id),false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> close_care_requirement_after_subject_death_in_candidate(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,const StableId& id,const DeathRef& death){try{
 writable(common,tables,x);const auto now=context(x);auto* goal=tables.requirements.find(id);if(!goal)fail("CareRequirementMissing",id);unique_birth(x,*goal);const auto original=chain(x,*goal,now,false);auto actual=take(read_verified_death4(common,x.deaths,x.life,death,now));if(actual.actor_id!=goal->subject_actor_id)fail("CareSubjectDeathMismatch",id);
 if(original.state==StateGoalState::invalid){const auto* closed=std::get_if<CareRequirementClosedEvent>(&original.last->body);const auto* cause=closed?std::get_if<CareSubjectDeath>(&closed->cause):nullptr;if(!cause||!same(SourceRef{cause->death_ref.domain,cause->death_ref.kind,cause->death_ref.id},SourceRef{death.domain,death.kind,death.id}))fail("ConflictingCareClosure",id);return {true,{id,goal->current_transition_ref,true},{}};}
 if(actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(now,actual.effective_at.exact_moment)||!same(now,actual.recorded_at.exact_moment))fail("CareImmediateSubjectDeathSourceRequired",id);
 if(goal->revision==std::numeric_limits<Integer>::max())fail("CareRevisionOverflow",id);const auto transition_id=take(common.next_id("care_transition"));CareRequirementTransition transition{transition_id,id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode,CareRequirementTransition::Kind::requirement_closed,goal->current_transition_ref,{death.domain,death.kind,death.id}};
 CareTemporalEvent event{transition_id,transition_ref(transition_id),now,goal->rule_version,CareRequirementClosedEvent{id,goal->source_episode,StateGoalState::invalid,CareSubjectDeath{death}}};append(common,tables,std::move(transition),std::move(event));goal->current_transition_ref=transition_ref(transition_id);++goal->revision;goal->state=StateGoalState::invalid;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();chain(x,*goal,now,true);return {true,{id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<StateGoalSnapshot> read_care_requirement_goal(const CareGoalSourceContext& x,const CareCoverageGoal& query){try{
 const auto now=context(x);const auto* row=x.goals.requirements.find(query.care_requirement_id);if(!row||row->subject_actor_id!=query.subject_actor_id)fail("CareGoalIdentityMismatch",query.care_requirement_id);unique_birth(x,*row);const auto actual=chain(x,*row,now,true);
 StateGoalSnapshot result;result.goal_ref=query;result.source_revision=row->revision;result.source_episode=row->source_episode;result.state=row->state;result.checked_tick=now.half_day_tick;result.source_ref=row->current_transition_ref;result.claim_scope_ref={ScopeRef::Kind::activity,row->id};if(row->state==StateGoalState::invalid)result.invalid_reason="subject_death";
 if(actual.satisfaction){const auto& satisfied=std::get<CareHandoverCompletedEvent>(actual.satisfaction->body);result.satisfaction_ref=CareGoalSatisfaction{care_coverage_satisfied_source(satisfied.satisfied_fact_id),3};result.satisfaction_at=actual.satisfaction->occurred_at;}return {true,std::move(result),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareRequirementRule*> read_care_requirement_rule_for_goal(const CareGoalSourceContext& x,const CareCoverageGoal& query){try{
 const auto now=context(x);const auto* row=x.goals.requirements.find(query.care_requirement_id);if(!row||row->subject_actor_id!=query.subject_actor_id)fail("CareGoalIdentityMismatch",query.care_requirement_id);unique_birth(x,*row);chain(x,*row,now,true);return {true,&selected_rule(x,*row),{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
namespace {
Outcome<BirthCareNeedApplication> reconcile_applied_care_goal(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,CareRequirementApplication applied,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goals,StateTaskConsumer& tasks){try{
 writable(common,tables,x);const auto* requirement=tables.requirements.find(applied.requirement_id);if(!requirement)fail("CareRequirementMissing",applied.requirement_id);CareCoverageGoal goal{requirement->id,requirement->subject_actor_id};
 // A real earlier application returns its own retained Need episode. It never
 // reconciles the current episode as a side effect of replaying an old source.
 const auto* original=take(read_care_goal_transition_source(x,applied.transition_ref));const auto* transition=tables.transitions.find(original->event_id);
 if(!transition||transition->requirement_id!=requirement->id)fail("CareActualAppliedTransitionRequired",applied.requirement_id);
 if(applied.already_applied){const auto* need=take(read_state_need_for_source_episode(common,affairs,goal,transition->source_episode_after,goals,tasks,take(x.clock.read_current_moment()),policies));if(!need)fail("CareAppliedNeedEpisodeMissing",requirement->id);return {true,{std::move(applied),StateNeedRef{need->need_id,need->episode}}, {}};}
 if(!same(requirement->current_transition_ref,applied.transition_ref))fail("CareActualAppliedTransitionRequired",applied.requirement_id);
 auto actual=take(read_care_requirement_goal(x,goal));auto projected=take(goals.read_care_goal(goal));
 if(state_goal_key(actual.goal_ref)!=state_goal_key(projected.goal_ref)||actual.source_revision!=projected.source_revision||actual.source_episode!=projected.source_episode||actual.state!=projected.state||actual.checked_tick!=projected.checked_tick||!same(actual.source_ref,projected.source_ref)||actual.claim_scope_ref.kind!=projected.claim_scope_ref.kind||actual.claim_scope_ref.id!=projected.claim_scope_ref.id||actual.invalid_reason!=projected.invalid_reason||actual.satisfaction_ref.has_value()!=projected.satisfaction_ref.has_value()||actual.satisfaction_at.has_value()!=projected.satisfaction_at.has_value())fail("CareStateGoalLeaseMismatch",requirement->id);
 if(actual.satisfaction_ref){const auto* a=std::get_if<CareGoalSatisfaction>(&*actual.satisfaction_ref);const auto* b=std::get_if<CareGoalSatisfaction>(&*projected.satisfaction_ref);if(!a||!b||a->source_schema!=b->source_schema||!same(a->source,b->source))fail("CareStateGoalSatisfactionLeaseMismatch",requirement->id);}
 if(actual.satisfaction_at&&!same(*actual.satisfaction_at,*projected.satisfaction_at))fail("CareStateGoalSatisfactionMomentMismatch",requirement->id);
 const auto& rule=selected_rule(x,*requirement);const auto* policy=take(policies.state_need_policy(goal));if(!policy||policy->id!=rule.state_need_policy_ref||policy->goal_kind!=0||policy->owner_rule!=rule.rule_ref.id||policies.state_need_catalog_identity().content_digest!=rule.rule_ref.catalog_digest)fail("CareStateNeedPolicyMismatch",requirement->id);
 auto need=take(reconcile_state_need(common,affairs,goal,*policy,goals,tasks,take(x.clock.read_current_moment())));return {true,{std::move(applied),std::move(need)}, {}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}

}
Outcome<BirthCareNeedApplication> reconcile_applied_care_goal_in_candidate(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,CareRequirementApplication applied,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goals,StateTaskConsumer& tasks){try{
 writable(common,tables,x);const auto* goal=tables.requirements.find(applied.requirement_id);if(!goal||(!applied.already_applied&&!same(goal->current_transition_ref,applied.transition_ref)))fail("CareActualAppliedTransitionRequired",applied.requirement_id);
 return reconcile_applied_care_goal(common,tables,x,std::move(applied),affairs,policies,goals,tasks);
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<BirthCareNeedApplication> create_birth_care_requirement_and_need_in_candidate(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,const BirthFactRef& birth,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goals,StateTaskConsumer& tasks){
 auto applied=create_birth_care_requirement_in_candidate(common,tables,x,birth);if(!applied.ok)return {false,{},applied.error};return reconcile_applied_care_goal(common,tables,x,std::move(applied.value),affairs,policies,goals,tasks);
}
Outcome<BirthCareNeedApplication> close_care_requirement_and_need_after_subject_death_in_candidate(CommonStore& common,CareGoalWriteTables tables,const CareGoalSourceContext& x,const StableId& id,const DeathRef& death,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goals,StateTaskConsumer& tasks){
 auto applied=close_care_requirement_after_subject_death_in_candidate(common,tables,x,id,death);if(!applied.ok)return {false,{},applied.error};return reconcile_applied_care_goal(common,tables,x,std::move(applied.value),affairs,policies,goals,tasks);
}
Status visit_selected_care_goals(const CareGoalSourceContext& x,SelectedCareGoalVisitor& visitor){
 auto valid=validate_care_requirement_goal_sources(x);if(!valid.ok)return valid;
 for(const auto& row:x.goals.requirements.rows()){valid=visitor.selected_care_goal(row.second.id,row.second.subject_actor_id);if(!valid.ok)return valid;}return common_success();
}

}
