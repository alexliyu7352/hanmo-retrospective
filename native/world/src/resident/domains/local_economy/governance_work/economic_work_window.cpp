#include "economic_work_window.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};[[noreturn]]void fail(const StableId&c,const StableId&id={}){throw Failure{common_failure(c,id,ErrorKind::internal).error};}
template<class T>T take(Outcome<T>r){if(!r.ok)throw Failure{r.error};return std::move(r.value);}void take(Status r){if(!r.ok)throw Failure{r.error};}
template<class T,class F>Outcome<T> guard(F&&f){try{return {true,f(),{}};}catch(const Failure&e){return {false,{},e.error};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const SupervisedWorkPlan& plan(const WorkEpisode&w){const auto*p=std::get_if<SupervisedWorkPlan>(&w.frozen_plan);if(!p||p->kind==WorkKind::patrol||p->economic_work_ref.owner_domain!="local_economy"||p->economic_work_ref.kind==EconomicWorkKind::facility_construction)fail("EconomicSupervisionOwnerMismatch",w.work_id);return *p;}
bool same(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
// Only this one already-validated consumer boundary is admitted. All real input
// changes and source validation remain the original production provider's job.
class AcceptedWindowInputRead final:public ProductionInputSourceRead {
 const ProductionInputSourceRead& original_;const StableId&unit_;const SourceRef&accepted_;const WorldMoment&at_;
public:AcceptedWindowInputRead(const ProductionInputSourceRead&o,const StableId&u,const SourceRef&r,const WorldMoment&at):original_(o),unit_(u),accepted_(r),at_(at){}
 Outcome<ProductionInputRead> read_production_segment_inputs(const StableId&u,const StableId&c,const WorldMoment&from,const WorldMoment&to)const override{return original_.read_production_segment_inputs(u,c,from,to);}
 Outcome<ProductionInputRead> read_production_inputs_after_change(const StableId&u,const StableId&c,const WorldMoment&from,const WorldMoment&to,const MomentOwnerSource&change)const override{return original_.read_production_inputs_after_change(u,c,from,to,change);}
 Outcome<ProductionInputRead> read_validated_production_segment_source(const ProductionSegmentSource&s)const override{return original_.read_validated_production_segment_source(s);}
 Status validate_production_segment_source(const ProductionSegmentSource&s)const override{return original_.validate_production_segment_source(s);}
 Outcome<std::optional<WorldMoment>> read_next_production_rule_boundary(const StableId&u,const StableId&c)const override{return original_.read_next_production_rule_boundary(u,c); }
 Status validate_production_input_change(const StableId&u,const WorldMoment&at,const SourceRef&r)const override{if(u==unit_&&same_source_ref(r,accepted_)&&!cmp(at,at_))return common_success();return original_.validate_production_input_change(u,at,r);}
};
}
Status validate_supervised_economic_window_request(const CommonStore&common,const WorkExecutionRead&work,const WorkOriginalRead&original,const SupervisedEconomicWindowRequest&r,const WorldMoment&now){return guard<Done>([&]{
 const auto*w=take(work.read_work(r.work.work_id));const auto&p=plan(*w);
 if(r.work_acceptance_ref.domain!="tasks"||r.work_acceptance_ref.kind!="work_accepted"||r.work_acceptance_ref.id!=w->acceptance_source_ref||cmp(r.window.from,r.window.to)>=0||cmp(r.window.to,now)||!w->current_execution||!std::holds_alternative<WorkCommonActionBinding>(*w->current_execution)||w->phase!=WorkPhase::active)fail("EconomicSupervisionWindowMismatch",w->work_id);
 const auto*header=common.state().facts.find(r.work_acceptance_ref.id);if(!header||header->domain!=r.work_acceptance_ref.domain||header->kind!=r.work_acceptance_ref.kind)fail("EconomicSupervisionAcceptanceMissing",w->work_id);
 if(std::none_of(w->original_execution_sources.begin(),w->original_execution_sources.end(),[&](const auto&s){return same(s,r.actual_action_execution);}))fail("EconomicSupervisionExecutionMissing",w->work_id);
 auto execution=take(original.read_original_execution(*w,r.actual_action_execution));const auto*action=std::get_if<WorkCommonActionBinding>(&execution.binding);
 if(!action||action->action_id!=std::get<WorkCommonActionBinding>(*w->current_execution).action_id||execution.actual_leader_actor_id!=p.supervisor_actor_id||cmp(r.window.from,execution.started_at)<0||cmp(r.window.to,execution.finite_until)>0||(execution.ended_at&&cmp(r.window.to,*execution.ended_at)>0))fail("EconomicSupervisionExecutionWindowMismatch",w->work_id);
 auto accepted=take(original.read_accepted_work(w->authorization));if(cmp(r.window.from,accepted.earliest_service_at)<0)fail("EconomicSupervisionBeforeAcceptedService",w->work_id);
 // The original Action itself must still be the current actual worker. Later
 // cold reads use the retained segment and immutable S3 period; they do not call this writer gate.
 const auto*a=common.action(action->action_id);if(!a||a->actor_id!=p.supervisor_actor_id)fail("EconomicSupervisionActionMissing",action->action_id);
 return Done{};
});}
namespace {
template<class Result,class Consumer>Outcome<Result> with_closed_window(CommonStore&common,const LocalEconomyState&economy,const LocalEconomyCatalog&catalog,EconomicProductionContext&production,const WorkExecutionRead&work,const WorkOriginalRead&original,const SupervisedEconomicWindowRequest&r,Consumer&&consume){return guard<Result>([&]{
 const auto now=take(production.clock.read_current_moment());take(validate_supervised_economic_window_request(common,work,original,r,now));const auto*w=take(work.read_work(r.work.work_id));const auto&p=plan(*w);
 // 旧集合必须先经普通来源全验；本次栈作用域不能替之前未采用的孤立段补证。
 take(validate_economic_work_sources(common,economy,catalog,production.catalog,production.state,production.inputs,production.units_per_half_day));
 const auto*cycle_id=production.state.active_cycles_by_unit.find(p.economic_work_ref.work_id);const auto*cycle=cycle_id?production.state.cycles.find(*cycle_id):nullptr;if(!cycle)fail("ActiveProductionCycleRequired",p.economic_work_ref.work_id);
 AcceptedWindowInputRead source(production.inputs,p.economic_work_ref.work_id,r.work_acceptance_ref,r.window.to);
 if(cmp(cycle->last_accounted_at,r.window.to)<0)take(close_production_input_segment_in_candidate(common,economy,catalog,production.catalog,production.state,source,p.economic_work_ref.work_id,r.window.to,r.work_acceptance_ref,production.units_per_half_day));
 const auto closed=take(read_economic_work_window(common,economy,production.catalog,production.state,source,p.economic_work_ref,r.window,common.state().identity));
 return consume(closed,source);
});}
}
Outcome<EconomicWorkWindowRead2> close_supervised_economic_window_in_candidate(CommonStore&common,const LocalEconomyState&economy,const LocalEconomyCatalog&catalog,EconomicProductionContext&production,const WorkExecutionRead&work,const WorkOriginalRead&original,const SupervisedEconomicWindowRequest&r){return with_closed_window<EconomicWorkWindowRead2>(common,economy,catalog,production,work,original,r,[](const auto&closed,const ProductionInputSourceRead&){return closed;});}
Outcome<StableId> close_and_consume_supervised_economic_window_in_candidate(CommonStore&common,const LocalEconomyState&economy,const LocalEconomyCatalog&catalog,EconomicProductionContext&production,const WorkExecutionRead&work,const WorkOriginalRead&original,const SupervisedEconomicWindowRequest&r,AppliedSupervisedEconomicWindowConsumer&consumer){return with_closed_window<StableId>(common,economy,catalog,production,work,original,r,[&](const EconomicWorkWindowRead2&closed,const ProductionInputSourceRead&scoped){
 const auto id=take(consumer.consume_applied_window(closed,scoped));
 // 消费者返回后只走普通 S3/S5 全来源读；missing receipt、漏费用或跨scope均失败。
 const auto*actual=take(work.read_period(id));const auto*source=actual?std::get_if<WorkEconomicPeriodSource>(&actual->source):nullptr;
 if(!source||actual->work_id!=r.work.work_id||cmp(actual->actual_until,r.window.to)||cmp(source->window.from,r.window.from)||cmp(source->window.to,r.window.to)<0||source->work_ref.owner_domain!=closed.work_ref.owner_domain||source->work_ref.kind!=closed.work_ref.kind||source->work_ref.work_id!=closed.work_ref.work_id||source->source_parts.size()!=closed.source_parts.size())fail("SupervisedActualPeriodAdoptionMismatch",id);
 for(std::size_t n=0;n<closed.source_parts.size();++n)if(source->source_parts[n].source_schema!=2||!same_source_ref(source->source_parts[n].source_ref,closed.source_parts[n].receipt_ref))fail("SupervisedActualPeriodSourcesMismatch",id);
 if(cmp(take(production.clock.read_current_moment()),r.window.to))fail("SupervisedConsumptionPhaseChanged",id);
 take(validate_economic_work_sources(common,economy,catalog,production.catalog,production.state,production.inputs,production.units_per_half_day));return id;
});}
}
