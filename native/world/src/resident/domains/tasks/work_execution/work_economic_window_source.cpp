#include "work_economic_window_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "work_execution_detail.hpp"
#include "work_supervision_execution_terms.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using namespace work_detail;
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
void before(const CommonStore&c,const StableId&a,const StableId&b){const auto*left=c.state().facts.find(a);const auto*right=c.state().facts.find(b);if(!left||!right)reject("SupervisedEconomicWindowCausalOrder",b);if(take(read_actual_common_fact_position(c,*left))>=take(read_actual_common_fact_position(c,*right)))reject("SupervisedEconomicWindowCausalOrder",b);}
void eq(const WorldMoment&a,const WorldMoment&b){if(!take(same_moment(a,b)))reject("SupervisedEconomicWindowMomentMismatch");}
}
Outcome<WorkSupervisedEconomicWindowSource> read_work_supervised_window_for_economic_source(const CommonStore&c,const TaskState&t,const SourceRef&accepted,const MomentOwnerSource&segment,const WorldMoment&closed){try{
 if(!t.work_execution||!task_work_execution_version(t.work_execution->version)||accepted.domain!="tasks"||accepted.kind!="work_accepted"||segment.source_schema!=2||segment.source_ref.domain!="local_economy"||segment.source_ref.kind!="production_input_segment")reject("SupervisedEconomicWindowSourceRequired",accepted.id);
 const auto&s=*t.work_execution;const auto*a=s.acceptances.find(accepted.id);const auto*e=a?s.episodes.find(a->work_id):nullptr;const auto*plan=e?std::get_if<SupervisedWorkPlan>(&e->frozen_plan):nullptr;
 if(!a||!e||!plan||a->fact_id!=accepted.id||e->acceptance_source_ref!=accepted.id||a->work_id!=e->work_id||!same(a->authorization,e->authorization)||!header(c,accepted.id,"work_accepted"))reject("SupervisedEconomicWindowAcceptanceMismatch",accepted.id);
 const auto*h=c.state().facts.find(segment.source_ref.id);if(!h||h->domain!=segment.source_ref.domain||h->kind!=segment.source_ref.kind)reject("SupervisedEconomicWindowSegmentHeaderMissing",segment.source_ref.id);
 const WorkPeriodReceipt2*found=nullptr;
 for(const auto&row:s.period_receipts.rows()){const auto&p=row.second;if(p.work_id!=e->work_id)continue;const auto*source=std::get_if<WorkEconomicPeriodSource>(&p.source);if(!source)continue;bool contains=false;for(const auto&part:source->source_parts)if(same(part,segment)){if(contains)reject("DuplicateSupervisedEconomicSegment",p.fact_id);contains=true;}if(!contains)continue;
  if(found)reject("AmbiguousSupervisedEconomicWindow",segment.source_ref.id);if(row.first!=p.fact_id||p.kind!=plan->kind||p.rule_version!=plan->rule_version||!same(source->work_ref,plan->economic_work_ref)||!header(c,p.fact_id,"work_period_receipt"))reject("SupervisedEconomicWindowReceiptMismatch",p.fact_id);
  std::size_t adopted=0;for(const auto&id:e->accepted_period_refs)if(id==p.fact_id)++adopted;if(adopted!=1)reject("SupervisedEconomicWindowNotActuallyAdopted",p.fact_id);
  eq(source->window.from,p.window.from);eq(source->window.to,p.window.to);eq(p.window.to,closed);take(time_order(p.window.from,p.actual_until,"SupervisedEconomicActualWindowInvalid"));take(time_order(p.actual_until,p.window.to,"SupervisedEconomicActualWindowInvalid"));take(time_order(a->earliest_service_at,p.window.from,"SupervisedEconomicBeforeAcceptedService"));before(c,accepted.id,p.fact_id);before(c,segment.source_ref.id,p.fact_id);found=&p;
 }
 if(!found)reject("SupervisedEconomicWindowReceiptRequired",segment.source_ref.id);
 const auto*binding=std::get_if<WorkCommonActionBinding>(&found->execution_source);const SupervisionActionStartSource*start=nullptr;if(!binding)reject("SupervisedEconomicActionSourceRequired",found->fact_id);
 for(const auto&row:s.supervision_starts.rows())if(row.second.action_id==binding->action_id){if(start)reject("AmbiguousSupervisedEconomicExecution",binding->action_id);start=&row.second;if(row.first!=start->fact_id)reject("SupervisedEconomicExecutionKeyMismatch",row.first);}
 if(!start||start->work_id!=e->work_id||start->actor_id!=found->leader_actor_id||!same(start->work_acceptance_ref,accepted)||!header(c,start->fact_id,"supervision_action_started"))reject("SupervisedEconomicExecutionMismatch",found->fact_id);
 bool registered=false;for(const auto&ref:e->original_execution_sources)if(same(ref,MomentOwnerSource{{"tasks","supervision_action_started",start->fact_id},1}))registered=true;if(!registered)reject("SupervisedEconomicExecutionNotAdopted",start->fact_id);
 before(c,accepted.id,start->fact_id);before(c,start->fact_id,found->fact_id);take(time_order(start->started_at,found->window.from,"SupervisedEconomicBeforeActualExecution"));
 // /5 source-local financial dependency cut uses the original fixed H. The
 // unchanged until_tick prefix is the goal deadline, never an execution timer.
 // Full S3 source validation independently re-reads approval and S0 elapsed.
 if(!task_work_fixed_execution_terms_version(s.version))reject("SupervisionOriginalFixedExecutionTermsRequired",start->fact_id);
 const SupervisionExecutionTerms*terms=nullptr;
 for(const auto&row:s.supervision_execution_terms.rows())if(row.second.action_id==start->action_id){if(terms||row.first!=row.second.action_id)reject("DuplicateSupervisionExecutionTerms",start->action_id);terms=&row.second;}
 if(!terms)reject("SupervisionOriginalFixedExecutionTermsRequired",start->fact_id);
 take(validate_supervision_execution_terms_shape(*terms));
 if(terms->work_id!=e->work_id||terms->kind!=plan->kind||!same(terms->original_authorization_ref,e->authorization)||!terms->actual_start_source_ref||!same(*terms->actual_start_source_ref,MomentOwnerSource{{"tasks","supervision_action_started",start->fact_id},1}))reject("SupervisedEconomicFixedExecutionBindingMismatch",start->fact_id);
 if(start->started_at.half_day_tick>std::numeric_limits<Integer>::max()-terms->service_limit_half_days)reject("SupervisionFixedExecutionTimeOverflow",start->fact_id);
 // Integral H advances only the integral component and preserves the actual
 // fractional opening. This is exact arithmetic, not a Calendar embedding.
 const WorldMoment until{start->started_at.half_day_tick+terms->service_limit_half_days,start->started_at.sub_half_day};
 take(time_order(found->window.to,until,"SupervisedEconomicAfterFixedExecution"));return {true,{e,found,start},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
