// 巡察仅完成时最后实际leader读取原gain技能，固定增量不捕获；监督读取作者最低资格。
#include "governance_work_skill_rules.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const Failure& e){return {false,{},e.error};}}
bool same_authorization(const WorkAuthorizationRef&a,const WorkAuthorizationRef&b){return same_source_ref(a.program_ref,b.program_ref)&&a.target_line_id==b.target_line_id&&a.accepted_task_id==b.accepted_task_id&&same_terms_revision(a.terms_revision,b.terms_revision);}
const TaskWorkExecutionState& state(const GovernanceWorkHistoryContext& x,const WorkEpisode& e){
 if(!x.tasks.work_execution||!task_work_skill_inputs_version(x.tasks.work_execution->version)||x.tasks.work_execution->episodes.find(e.work_id)!=&e||
    !x.task_definitions.bound_to(x.programs.common,x.tasks))fail("GovernanceWorkSkillInputsRequired",e.work_id);
 return *x.tasks.work_execution;
}
void source(const CommonStore& c,const StableId& id,const StableId& kind){const auto*f=c.state().facts.find(id);if(!f||f->domain!="tasks"||f->kind!=kind)fail("GovernanceWorkSkillOriginalSourceRequired",id);}
}
Outcome<std::optional<WorkFrozenSkillRequirementView>> NativeGovernanceWorkSkillRuleRead::read_supervision_start_skill(const WorkEpisode& e)const{
 return guard<std::optional<WorkFrozenSkillRequirementView>>([&]{
  state(context_,e);auto h=take(read_governance_accepted_work_history(context_,e));
  if(!h.supervision)fail("GovernanceSupervisionPlanRequired",e.work_id);
  const auto* q=take(read_governance_executor_qualification(context_.programs.catalog,*h.specification,h.supervision->required_skill_ref));
  if(!q)return std::optional<WorkFrozenSkillRequirementView>{};
  return std::optional<WorkFrozenSkillRequirementView>{{&q->skill_id,q->minimum_level}};
 });
}
// Growth 开始边界只下降到原 retained 身份叶，不回调本资格捕获。
Outcome<const StableId*> NativeGovernanceWorkSkillRuleRead::read_supervision_executor_at_start(const WorkEpisode& e,const SupervisionActionStartSource& start)const{
 return guard<const StableId*>([&]{const auto& s=state(context_,e);if(s.supervision_starts.find(start.fact_id)!=&start||start.work_id!=e.work_id)fail("GovernanceWorkSkillActualStartRequired",start.fact_id);const SourceRef cut{"tasks","supervision_action_started",start.fact_id};return take(read_governance_supervision_executor_at(context_,e,start.started_at,&cut));});
}
Outcome<std::optional<WorkSkillRequirement>> NativeGovernanceWorkSkillRuleRead::read_work_result_skill(const WorkEpisode& e,const WorkResult& result)const{
 return guard<std::optional<WorkSkillRequirement>>([&]{
  const auto& s=state(context_,e);
  if(result.work_id!=e.work_id||result.task_id!=e.authorization.accepted_task_id||!same_authorization(result.authorization,e.authorization))
   fail("GovernanceWorkSkillResultIdentityMismatch",e.work_id);
  // 此时真实result可能正在原writer栈内准备；不得要求其未来header或递归read_result。
  if(result.kind!=WorkKind::patrol||result.disposition!=WorkResultDisposition::completed)return std::optional<WorkSkillRequirement>{};
  // The same original S6/S3 acceptance/history gate serves both work kinds;
  // never recreate a weaker patrol-only copy during result skill capture.
  auto accepted=take(read_governance_accepted_work_history(context_,e));
  const auto* target=std::get_if<GovernancePatrol>(&accepted.approval->target);
  const auto* plan=accepted.patrol;
  if(!target||!plan)fail("GovernanceWorkSkillPatrolTargetMismatch",e.work_id);
  if(std::holds_alternative<FixedApprovedDelta>(target->gain))return std::optional<WorkSkillRequirement>{};
  const auto& ref=std::get<CompletedSkillRule>(target->gain).rule_ref;
  const auto* rule=context_.programs.catalog.patrol_gain_rules.find(ref.id);
  if(!rule||rule->rule_ref.version!=ref.version||rule->rule_ref.catalog_digest!=ref.catalog_digest||
     ref.catalog_digest!=context_.programs.catalog.identity.content_digest||!growth_skill_defined(rule->skill_id,true))
   fail("PatrolGainRuleMissing",ref.id);
  if(e.accepted_period_refs.empty()||result.credited_half_days!=result.required_half_days||result.required_half_days!=plan->required_half_days)
   fail("PatrolCompletedWorkPeriodsRequired",e.work_id);
  const auto* last=s.period_receipts.find(e.accepted_period_refs.back());
  if(!last||last->fact_id!=e.accepted_period_refs.back()||last->work_id!=e.work_id||last->kind!=WorkKind::patrol||last->leader_actor_id.empty())
   fail("PatrolActualLeaderMissing",e.work_id);
  // 原S3随后反验该真实receipt的S2来源与全结果；此纯规则口不重做服务累计。
  source(context_.programs.common,last->fact_id,"work_period_receipt");
  return std::optional<WorkSkillRequirement>{{last->leader_actor_id,rule->skill_id,0}};
 });
}
}
