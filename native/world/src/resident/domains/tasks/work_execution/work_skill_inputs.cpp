#include "work_skill_inputs.hpp"
#include "work_execution_detail.hpp"
#include "resident/domains/growth/growth_supervision_qualification.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class Fn>Outcome<T> guard(Fn fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
void selected(const CommonStore&c,const TaskState&t){if(!t.work_execution||!task_work_skill_inputs_version(t.work_execution->version))reject("WorkSkillInputsPhysical4Required");(void)c;}
void context(const WorkSkillInputsContext&x){selected(x.common,x.tasks);if(&x.common!=&x.body.common)reject("WorkSkillCandidateMismatch");}
void requirement(const WorkSkillRequirement&r){if(r.actor_id.empty()||r.skill_id.empty()||r.minimum_level<0||r.minimum_level>4)reject("OriginalWorkSkillRequirementMissing");}
// 资格规则借用原作者字段；空指针不是无资格门的替代值。
void requirement(const WorkFrozenSkillRequirementView&r){if(!r.skill_id||r.skill_id->empty()||r.minimum_level<0||r.minimum_level>4)reject("OriginalWorkSkillRequirementMissing");}
SourceRef result_ref(const WorkResult&r){return {"tasks",work_detail::result_kind(r),r.fact_id};}
SourceRef start_ref(const SupervisionActionStartSource&s){return {"tasks","supervision_action_started",s.fact_id};}
void qualification_identity(const GrowthSkillQualificationV2&q,const WorkSkillRequirement&r,const SourceRef&source,const WorldMoment&at){
 requirement(r);if(q.actor_id!=r.actor_id||q.skill_id!=r.skill_id||q.actual_boundary_ref.source_schema!=1||!work_detail::same(q.actual_boundary_ref.original_source_ref,source)||cmp(q.observed_at,at)||q.coverage_kind!=GrowthSkillQualificationV2::Coverage::complete)reject("WorkCompleteSkillQualificationRequired",source.id);
}
void qualification_identity(const GrowthSkillQualificationV2&q,const StableId&actor,const WorkFrozenSkillRequirementView&r,const SourceRef&source,const WorldMoment&at){
 requirement(r);if(q.actor_id!=actor||q.skill_id!=*r.skill_id||q.actual_boundary_ref.source_schema!=1||!work_detail::same(q.actual_boundary_ref.original_source_ref,source)||cmp(q.observed_at,at)||q.coverage_kind!=GrowthSkillQualificationV2::Coverage::complete)reject("WorkCompleteSkillQualificationRequired",source.id);
}
}
Outcome<GrowthSkillActualBoundary> NativeWorkSkillBoundaryRead::read_actual_skill_boundary(const StableId&actor,const SkillQualificationBoundaryRefV2&ref)const{return guard<GrowthSkillActualBoundary>([&]{
 selected(common_,tasks_);if(ref.source_schema!=1||ref.original_source_ref.domain!="tasks"||actor.empty())reject("WorkSkillOriginalBoundaryRequired");const auto&source=ref.original_source_ref;const auto&state=*tasks_.work_execution;
 if(!work_detail::header(common_,source.id,source.kind))reject("WorkSkillBoundaryHeaderMissing",source.id);
 if(source.kind=="supervision_action_started"){
  const auto*s=state.supervision_starts.find(source.id);const auto*e=s?state.episodes.find(s->work_id):nullptr;const auto*a=e?state.acceptances.find(e->acceptance_source_ref):nullptr;const auto*p=e?std::get_if<SupervisedWorkPlan>(&e->frozen_plan):nullptr;
  if(!s||s->fact_id!=source.id||!e||!a||!p||s->actor_id!=actor||s->condition_at_start.actor_id!=actor||!work_detail::same(s->work_acceptance_ref,{"tasks","work_accepted",a->fact_id})||!work_detail::header(common_,a->fact_id,"work_accepted")||cmp(s->started_at,a->earliest_service_at)<0)reject("WorkSkillStartBoundaryMismatch",source.id);
  const auto* actual=take(rules_.read_supervision_executor_at_start(*e,*s));if(!actual||*actual!=actor)reject("WorkSkillStartExecutorSourceMismatch",source.id);
  return GrowthSkillActualBoundary{s->started_at,&s->condition_at_start};
 }
 if(source.kind=="patrol_work_result"||source.kind=="supervised_work_result"){
  const auto*r=state.results.find(source.id);const auto*e=r?state.episodes.find(r->work_id):nullptr;
  if(!r||r->fact_id!=source.id||work_detail::result_kind(*r)!=source.kind||!e||e->terminal_result_ref!=r->fact_id||!work_detail::same(e->authorization,r->authorization)||r->task_id!=e->authorization.accepted_task_id||!r->skill_input||r->skill_input->condition_before.actor_id!=actor)reject("WorkSkillResultBoundaryMismatch",source.id);
  // The locked Patrol gain reads the actual last provided/inspected leader.
  // The source owner proves that identity without recursively reading skill.
  if(r->kind!=WorkKind::patrol||r->disposition!=WorkResultDisposition::completed||e->accepted_period_refs.empty())reject("WorkResultSkillUseUnsupported",source.id);
  const auto*last=state.period_receipts.find(e->accepted_period_refs.back());if(!last||last->work_id!=e->work_id||last->leader_actor_id!=actor||!work_detail::header(common_,last->fact_id,"work_period_receipt")||cmp(last->actual_until,r->finished_at)>0)reject("WorkResultActualSkillLeaderMismatch",source.id);
  return GrowthSkillActualBoundary{r->finished_at,&r->skill_input->condition_before};
 }
 reject("UnsupportedWorkSkillBoundary",source.id);
});}
Outcome<std::optional<WorkFrozenSkillRequirementView>> NativeWorkSkillInputsRead::read_start_requirement(const WorkEpisode&e)const{return guard<std::optional<WorkFrozenSkillRequirementView>>([&]{
 context(context_);auto rule=take(context_.rules.read_supervision_start_skill(e));const auto*p=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);if(!p||p->required_skill_ref.has_value()!=rule.has_value())reject("SupervisionSkillRuleCoverageMismatch",e.work_id);if(rule)requirement(*rule);return rule;
});}
Outcome<std::optional<WorkResultSkillInput>> NativeWorkSkillInputsRead::prepare_result_input(const WorkEpisode&e,const WorkResult&r)const{return guard<std::optional<WorkResultSkillInput>>([&]{
 const auto&x=context_;context(x);if(cmp(r.finished_at,take(x.clock.read_current_moment()))||cmp(x.body.actual_now,r.finished_at)||r.work_id!=e.work_id)reject("WorkResultCaptureMomentMismatch",r.fact_id);
 auto rule=take(x.rules.read_work_result_skill(e,r));if(!rule)return std::optional<WorkResultSkillInput>{};requirement(*rule);auto body=take(read_current_character_condition(x.body,rule->actor_id));
 return std::optional<WorkResultSkillInput>{{std::move(body.head),rule->skill_id,{}}};
});}
Outcome<GrowthSkillQualificationV2> NativeWorkSkillInputsRead::read_result_qualification(const WorkEpisode&e,const WorkResult&r)const{return guard<GrowthSkillQualificationV2>([&]{
 const auto&x=context_;context(x);auto rule=take(x.rules.read_work_result_skill(e,r));if(!rule||!r.skill_input||r.skill_input->condition_before.actor_id!=rule->actor_id||r.skill_input->skill_id!=rule->skill_id)reject("WorkResultSkillRuleMismatch",r.fact_id);
 auto q=take(read_growth_skill_qualification_before_source_v2(x.body,x.growth,rule->actor_id,rule->skill_id,{result_ref(r),1},boundaries_));qualification_identity(q,*rule,result_ref(r),r.finished_at);return q;
});}
Status NativeWorkSkillInputsRead::validate_result_input(const WorkEpisode&e,const WorkResult&r)const{return guard<Done>([&]{
 const auto&x=context_;context(x);auto rule=take(x.rules.read_work_result_skill(e,r));if(rule.has_value()!=r.skill_input.has_value())reject("WorkResultSkillInputCoverageMismatch",r.fact_id);if(!rule)return Done{};
 requirement(*rule);const auto&i=*r.skill_input;if(!i.qualification||i.condition_before.actor_id!=rule->actor_id||i.skill_id!=rule->skill_id)reject("WorkResultSkillInputMissing",r.fact_id);
 qualification_identity(*i.qualification,*rule,result_ref(r),r.finished_at);take(validate_growth_skill_qualification_before_source_v2(x.body,x.growth,*i.qualification,boundaries_));return Done{};
});}
Outcome<std::optional<GrowthSkillQualificationV2>> NativeWorkSkillInputsRead::read_start_qualification(const WorkEpisode&e,const SupervisionActionStartSource&s)const{return guard<std::optional<GrowthSkillQualificationV2>>([&]{
 const auto&x=context_;context(x);const auto*actor=take(x.rules.read_supervision_executor_at_start(e,s));if(!actor||*actor!=s.actor_id)reject("SupervisionSkillActorMismatch",s.fact_id);
 auto rule=take(read_start_requirement(e));if(!rule)return std::optional<GrowthSkillQualificationV2>{};requirement(*rule);
 auto q=take(read_growth_skill_qualification_before_source_v2(x.body,x.growth,*actor,*rule->skill_id,{start_ref(s),1},boundaries_));qualification_identity(q,*actor,*rule,start_ref(s),s.started_at);if(!take(read_growth_supervision_skill_eligible(x.body,x.growth,q,boundaries_,*actor,{*rule->skill_id,rule->minimum_level},{s.started_at,s.started_at})))reject("SupervisionOriginalSkillLevelInsufficient",s.fact_id);return std::optional<GrowthSkillQualificationV2>{std::move(q)};
});}
Status NativeWorkSkillInputsRead::validate_start_qualification(const WorkEpisode&e,const SupervisionActionStartSource&s)const{return guard<Done>([&]{
 const auto&x=context_;context(x);const auto*actor=take(x.rules.read_supervision_executor_at_start(e,s));if(!actor||*actor!=s.actor_id)reject("SupervisionSkillActorMismatch",s.fact_id);
 auto rule=take(read_start_requirement(e));if(rule.has_value()!=s.skill_qualification.has_value())reject("SupervisionSkillInputCoverageMismatch",s.fact_id);if(!rule)return Done{};
 qualification_identity(*s.skill_qualification,*actor,*rule,start_ref(s),s.started_at);if(!take(read_growth_supervision_skill_eligible(x.body,x.growth,*s.skill_qualification,boundaries_,*actor,{*rule->skill_id,rule->minimum_level},{s.started_at,s.started_at})))reject("SupervisionOriginalSkillLevelInsufficient",s.fact_id);return Done{};
});}
Status NativeWorkSkillInputsRead::validate_service_skill(const WorkEpisode&e,const SupervisionActionStartSource&s,const MomentSpan&span)const{return guard<Done>([&]{
 const auto&x=context_;take(validate_start_qualification(e,s));auto rule=take(read_start_requirement(e));if(!rule)return Done{};
 if(!s.skill_qualification||!take(read_growth_supervision_skill_eligible(x.body,x.growth,*s.skill_qualification,boundaries_,s.actor_id,{*rule->skill_id,rule->minimum_level},span)))reject("SupervisionOriginalSkillLevelInsufficient",s.fact_id);return Done{};
});}

}
