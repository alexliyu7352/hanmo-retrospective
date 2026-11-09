#include "work_growth_contribution.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "work_execution_detail.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {namespace {
using namespace work_detail;
struct Failure {RuleError error;};
[[noreturn]]void reject(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T>T take(Outcome<T>value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void equal_moment(const WorldMoment&a,const WorldMoment&b,const char*c){if(!take(same_moment(a,b)))reject(c);}
std::size_t fact_position(const CommonStore&common,const StableId&id){const auto*head=common.state().facts.find(id);if(!head)reject("WorkContributionFactOrderMissing",id);return take(read_actual_common_fact_position(common,*head));}
void before(const CommonStore&c,const StableId&a,const StableId&b){if(fact_position(c,a)>=fact_position(c,b))reject("WorkContributionCausalOrderMismatch",b);}
void original_header(const CommonStore&c,const MomentOwnerSource&s){const auto*h=c.state().facts.find(s.source_ref.id);if(s.source_schema<1||!h||h->id!=s.source_ref.id||h->kind!=s.source_ref.kind||h->domain!=s.source_ref.domain)reject("WorkContributionOriginalHeaderMissing",s.source_ref.id);}
}
Outcome<WorkGrowthContributionSource> read_work_growth_contribution_source(const CommonStore&c,const TaskState&t,const StableId&actor,const WorkResultSource&source,std::span<const StableId>participation,const WorldMoment&now){try{
 if(!t.work_execution||!task_work_skill_inputs_version(t.work_execution->version)||!c.actor(actor))reject("WorkGrowthContributionPhysical4Required",actor);
 const StableId*id=nullptr;const char*kind=nullptr;if(const auto*p=std::get_if<PatrolResult>(&source)){id=&p->fact_id;kind="patrol_work_result";}else if(const auto*p=std::get_if<SupervisedResult>(&source)){id=&p->fact_id;kind="supervised_work_result";}else reject("LegacyWorkGrowthContributionUnsupported");
 const auto&s=*t.work_execution;const auto*r=s.results.find(*id);const auto*e=r?s.episodes.find(r->work_id):nullptr;const auto*a=e?s.acceptances.find(e->acceptance_source_ref):nullptr;const auto*h=c.state().facts.find(*id);
 if(!r||!e||!a||!h||r->fact_id!=*id||!header(c,*id,kind)||result_kind(*r)!=kind||r->disposition!=WorkResultDisposition::completed||e->phase!=WorkPhase::work_complete||e->terminal_result_ref!=*id||r->stop_reason||r->actual_stop_source||!same(r->authorization,e->authorization)||r->task_id!=e->authorization.accepted_task_id||r->kind!=work_detail::kind(e->frozen_plan)||r->plan_rule_version!=rule(e->frozen_plan))reject("WorkGrowthCompletedSourceRequired",*id);
 take(authorization_shape(e->authorization));take(plan_shape(e->frozen_plan));
 const auto*task=t.tasks.find(r->task_id);const auto*accepted=s.work_task_acceptances.find(a->original_task_acceptance.source_ref.id);
 if(a->fact_id!=e->acceptance_source_ref||a->work_id!=e->work_id||!same(a->authorization,e->authorization)||!same(a->expense_terms_ref,expense(e->frozen_plan))||!header(c,a->fact_id,"work_accepted")||!task||task->id!=r->task_id||!accepted||accepted->fact_id!=a->original_task_acceptance.source_ref.id||accepted->task_id!=r->task_id||accepted->executor_actor_id!=initial_leader(e->frozen_plan)||a->original_task_acceptance.source_schema!=1||!same(a->original_task_acceptance.source_ref,{"tasks","work_task_accepted",accepted->fact_id})||!header(c,accepted->fact_id,"work_task_accepted"))reject("WorkGrowthAcceptanceSourceMismatch",*id);
 before(c,accepted->fact_id,a->fact_id);before(c,a->fact_id,*id);equal_moment(a->accepted_at,accepted->accepted_at,"WorkGrowthAcceptanceMomentMismatch");equal_moment(a->earliest_service_at,accepted->earliest_service_at,"WorkGrowthServiceWindowMismatch");take(time_order(r->finished_at,now,"FutureWorkGrowthContribution"));
 const auto total=take(credited(s,*e));if(total<=0||total!=required(e->frozen_plan)||r->credited_half_days!=total||r->required_half_days!=total||r->period_summary_refs.size()!=e->accepted_period_refs.size()||e->accepted_period_refs.empty())reject("WorkGrowthCreditedResultMismatch",*id);
 std::size_t actual_count=0,selected_index=0;Integer actor_credit=0;const WorkPeriodReceipt2*previous=nullptr;
 for(const auto&period_id:e->accepted_period_refs){const auto*p=s.period_receipts.find(period_id);if(!p||p->fact_id!=period_id||p->work_id!=e->work_id||p->kind!=r->kind||p->rule_version!=r->plan_rule_version||!header(c,period_id,"work_period_receipt")||!same(r->period_summary_refs[actual_count],MomentOwnerSource{{"tasks","work_period_receipt",period_id},2}))reject("WorkGrowthPeriodSourceMismatch",period_id);
  before(c,a->fact_id,period_id);before(c,period_id,*id);take(time_order(a->earliest_service_at,p->window.from,"WorkGrowthBeforeServiceWindow"));take(time_order(p->window.from,p->actual_until,"WorkGrowthInvalidActualSpan"));take(time_order(p->actual_until,p->window.to,"WorkGrowthInvalidActualSpan"));take(time_order(p->actual_until,r->finished_at,"WorkGrowthResultBeforeService"));if(previous){take(time_order(previous->actual_until,p->window.from,"WorkGrowthOverlappingPeriods"));before(c,previous->fact_id,period_id);}take(execution_shape(p->execution_source));
  const auto duration=take(elapsed_world_duration(p->window.from,p->window.to));if(take(compare_fractions(duration.value,take(normalize_fraction("1","1")))))reject("WorkGrowthPeriodMustBeWholeHalfDay",period_id);
  if(p->leader_actor_id.empty()||!c.actor(p->leader_actor_id)||p->actual_anchor_sources.empty()||p->credited_half_days<0||p->credited_half_days>1)reject("WorkGrowthActualLeaderMissing",period_id);
  if(const auto*m=std::get_if<WorkMilitaryPeriodSource>(&p->source)){if(r->kind!=WorkKind::patrol||m->source_schema!=2)reject("WorkGrowthDutySourceMismatch",period_id);original_header(c,{m->fact_ref,m->source_schema});before(c,m->fact_ref.id,period_id);}else {const auto&v=std::get<WorkEconomicPeriodSource>(p->source);const auto*plan=std::get_if<SupervisedWorkPlan>(&e->frozen_plan);if(!plan||!same(v.work_ref,plan->economic_work_ref)||v.source_parts.empty())reject("WorkGrowthEconomicSourceMismatch",period_id);equal_moment(v.window.from,p->window.from,"WorkGrowthEconomicWindowMismatch");equal_moment(v.window.to,p->window.to,"WorkGrowthEconomicWindowMismatch");for(const auto&part:v.source_parts){original_header(c,part);before(c,part.source_ref.id,period_id);}}
  if(p->credited_half_days){if(p->disposition!=S3WorkPeriodDisposition::credited||p->reason!=WorkPeriodReason::qualified||p->reason_source)reject("WorkGrowthUnqualifiedCredit",period_id);equal_moment(p->actual_until,p->window.to,"WorkGrowthIncompleteCreditWindow");const auto&gate=deadline(e->frozen_plan);if(!take(calendar_deadline_accepts(gate.tick,gate.inclusive,p->actual_until)))reject("WorkGrowthLateCredit",period_id);if(p->leader_actor_id==actor){if(selected_index>=participation.size()||participation[selected_index++]!=period_id)reject("WorkGrowthCompleteParticipationRequired",period_id);++actor_credit;}}
  else if(p->disposition==S3WorkPeriodDisposition::credited)reject("WorkGrowthZeroCreditedPeriod",period_id);
  ++actual_count;previous=p;
 }
 std::size_t owned_count=0;for(const auto&row:s.period_receipts.rows())if(row.second.work_id==e->work_id)++owned_count;if(owned_count!=actual_count||selected_index!=participation.size()||actor_credit<=0)reject("WorkGrowthCompleteParticipationRequired",actor);
 if(!e->actual_start_ref||!r->first_work_at)reject("WorkGrowthActualStartRequired",*id);const auto*start=s.actual_starts.find(*e->actual_start_ref);if(!start||start->work_id!=e->work_id||start->start_fact_id!=*e->actual_start_ref||!header(c,start->start_fact_id,"work_actual_start")||!same(start->authorization,e->authorization))reject("WorkGrowthActualStartMismatch",*id);equal_moment(start->started_at,*r->first_work_at,"WorkGrowthActualStartMomentMismatch");before(c,start->start_fact_id,*id);take(time_order(start->started_at,r->finished_at,"WorkGrowthNoPositiveService",true));
 return {true,{r,e,h,previous,actor_credit},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_work_growth_contributors(const CommonStore&c,const TaskState&t,const WorkResultSource&source,const WorldMoment&now,WorkGrowthContributorVisitor&visitor){try{
 if(!t.work_execution||!task_work_skill_inputs_version(t.work_execution->version))reject("WorkGrowthContributionPhysical4Required");
 const StableId*id=nullptr;if(const auto*p=std::get_if<PatrolResult>(&source))id=&p->fact_id;else if(const auto*p=std::get_if<SupervisedResult>(&source))id=&p->fact_id;else reject("LegacyWorkGrowthContributionUnsupported");
 const auto&s=*t.work_execution;const auto*r=s.results.find(*id);const auto*e=r?s.episodes.find(r->work_id):nullptr;if(!r||!e)reject("WorkGrowthCompletedSourceRequired",*id);
 bool any=false;
 for(std::size_t index=0;index<e->accepted_period_refs.size();++index){const auto*p=s.period_receipts.find(e->accepted_period_refs[index]);if(!p)reject("WorkGrowthPeriodSourceMismatch",e->accepted_period_refs[index]);if(p->credited_half_days<=0)continue;
  bool earlier=false;for(std::size_t before_index=0;before_index<index;++before_index){const auto*old=s.period_receipts.find(e->accepted_period_refs[before_index]);if(!old)reject("WorkGrowthPeriodSourceMismatch",e->accepted_period_refs[before_index]);if(old->credited_half_days>0&&old->leader_actor_id==p->leader_actor_id){earlier=true;break;}}if(earlier)continue;
  CheckpointMemoryCharge memory;CheckpointVector<StableId> refs;std::size_t count=0;
  for(const auto&key:e->accepted_period_refs){const auto*value=s.period_receipts.find(key);if(!value)reject("WorkGrowthPeriodSourceMismatch",key);if(value->credited_half_days>0&&value->leader_actor_id==p->leader_actor_id){++count;}}
  (memory).grow(0);refs.reserve(count);for(const auto&key:e->accepted_period_refs){const auto*value=s.period_receipts.find(key);if(value->credited_half_days>0&&value->leader_actor_id==p->leader_actor_id)refs.emplace_back(key);}
  const std::span<const StableId> borrowed(refs.data(),refs.size());take(read_work_growth_contribution_source(c,t,p->leader_actor_id,source,borrowed,now));take(visitor.visit(p->leader_actor_id,borrowed));any=true;
 }
 if(!any)reject("WorkGrowthCompleteParticipationRequired",*id);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}

}
