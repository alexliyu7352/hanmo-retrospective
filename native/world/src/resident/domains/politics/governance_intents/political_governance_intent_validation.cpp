// 原三域全史之后核有限计划；模板和旧邀请不授予新的签字权或重新开启已结束Task。
#include "political_governance_intent_validation.hpp"
#include "resident/kernel/governance_integration.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
// 新marker严格要求原政治治理、Task与公共赈济，旧目录不新增空段。
Status validate_political_governance_intent_modules(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
 std::map<StableId,Integer> count;for(const auto& m:modules)if(m.first==political_governance_intents_module||(enabled&&(m.first=="politics"||m.first=="politics_governance"||m.first=="governance_tasks"||m.first=="public_relief"))){if(m.second!="1"||++count[m.first]!=1)return common_failure("GovernanceIntentModuleMismatch",m.first);}
 if(count[political_governance_intents_module]!=(enabled?1:0))return common_failure("GovernanceIntentProfileMismatch");if(enabled)for(const auto* name:{"politics","politics_governance","governance_tasks","public_relief"})if(count[name]!=1)return common_failure("GovernanceIntentDependenciesRequired",name);return common_success();
}
// 受领Source被原Task全验后才用于计划认领；同Action必须属于原意图的真实本人契约。
Status validate_political_governance_intent_director(const DirectorCatalog& catalog,const DirectorState& state,const GovernanceReadContext* context){
 const auto* intents=catalog.governance_intents.get();if(intents){if(!context)return common_failure("GovernanceIntentProfileRequired");auto valid=validate_political_governance_intents(*intents,context->common_catalog,context->politics_catalog,context->governance_catalog,context->organization_catalog);if(!valid.ok)return valid;}
 const auto& policies=catalog.dynamic_subjects?catalog.policies:catalog.subjects;std::set<StableId> used;
 for(const auto& row:policies.rows()){const auto& p=row.second;const bool selected=std::find(p.templates.begin(),p.templates.end(),DirectorTemplate::governance_commission)!=p.templates.end();if(selected!=p.governance_intent_id.has_value())return common_failure("GovernanceIntentDirectorPolicyMismatch",row.first);if(!selected)continue;const auto* intent=intents?intents->intents.find(*p.governance_intent_id):nullptr;const auto* j=intent&&context?context->politics_catalog.jurisdictions.find(intent->jurisdiction_id):nullptr;if(!j||p.organization_id!=j->political_organization_id||(!catalog.dynamic_subjects&&row.first!=intent->issuer_actor_id&&row.first!=intent->executor_actor_id))return common_failure("GovernanceIntentDirectorSubjectMismatch",row.first);used.insert(intent->id);}
 if(intents&&used.size()!=intents->intents.size())return common_failure("GovernanceIntentUnused");
 const auto check=[&](const auto& plans,bool archived)->Status{for(const auto& row:plans.rows()){const auto& p=row.second;if(p.template_id!=political_governance_intent_template)continue;const auto* policy=policies.find(catalog.dynamic_subjects?p.policy_id:p.subject.id);const auto* intent=policy&&policy->governance_intent_id&&intents?intents->intents.find(*policy->governance_intent_id):nullptr;
  if(!context||!intent||p.subject.kind!=SubjectKind::actor||(p.subject.id!=intent->issuer_actor_id&&p.subject.id!=intent->executor_actor_id)||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::governance_commission)==policy->templates.end()||!p.commitment_ids.empty()||p.livelihood||p.pending_life_action_id||p.last_life_fact_id||(p.phase!=PlanPhase::waiting&&p.phase!=PlanPhase::delivering&&p.phase!=PlanPhase::accepting&&p.phase!=PlanPhase::completed))return common_failure("GovernanceIntentDirectorPlanMismatch",row.first);
  // 原唯一producer保留next_review=tick+max(delay,review)，归档只复制该值；不能拿今天补旧完成。
  const Integer delay=std::max(catalog.retry_delay_ticks,catalog.review_interval_ticks);if(delay<1||p.next_review_tick<delay)return common_failure("GovernanceIntentScheduleRequired",row.first);const Integer observed=p.next_review_tick-delay;if(observed>context->common.tick())return common_failure("GovernanceIntentScheduleRequired",row.first);if(!archived){const auto* schedule=state.schedules.find(p.subject.id);if(!schedule||schedule->last_decision_tick!=observed||schedule->next_tick!=p.next_review_tick)return common_failure("GovernanceIntentScheduleRequired",row.first);}
  if(p.phase==PlanPhase::completed){if(p.current_action_id)return common_failure("GovernanceIntentCompletedActionMismatch",row.first);auto valid=validate_governance_intent_completed_source(*context,*intent,p.subject.id,p.goal_ref,observed);if(!valid.ok)return valid;continue;}
  if(p.goal_ref!=intent->id)return common_failure("GovernanceIntentGoalMismatch",row.first);
  if(p.phase==PlanPhase::accepting){
   if(p.subject.id==intent->issuer_actor_id){if(p.current_action_id)return common_failure("GovernanceIntentIssueActionMismatch",row.first);bool issued=false;for(const auto& a:context->governance.authorities.rows())if(validate_governance_intent_completed_source(*context,*intent,p.subject.id,a.first,observed).ok)issued=true;if(!issued)return common_failure("GovernanceIntentIssuedSourceRequired",row.first);}
   else{if(!archived&&!p.current_action_id)return common_failure("GovernanceIntentAcceptedActionRequired",row.first);bool accepted=false;for(const auto& task:context->governance_tasks.governance_contracts.rows())if((archived||task.second.action_id==*p.current_action_id)&&validate_governance_intent_completed_source(*context,*intent,p.subject.id,task.second.acceptance.fact_id,observed).ok)accepted=true;if(!accepted)return common_failure("GovernanceIntentAcceptedSourceRequired",row.first);}
  }
 }return common_success();};auto checked=check(state.plans,false);return checked.ok?check(state.archived_plans,true):checked;
}
}
