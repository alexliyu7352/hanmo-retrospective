#include "economic_debt_needs.hpp"
#include "economic_phase_clock.hpp"
#include "fiscal/occupation_fiscal_debt_source.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> result){if(!result.ok)throw Failure{result.error};return std::move(result.value);}
void take(Status result){if(!result.ok)throw Failure{result.error};}
std::vector<StableId> ordered_sources(const CommonStore& common,const std::map<StableId,Integer>& originals,Integer through){
 std::vector<StableId> ids;std::set<StableId> found;
 for(const auto& fact:common.state().facts.rows()){const auto it=originals.find(fact.first);if(it==originals.end())continue;if(fact.second.domain!="local_economy"||!found.insert(fact.first).second)fail("EconomicDebtNeedOriginalHeaderMismatch",fact.first);if(it->second==through)ids.push_back(fact.first);}
 if(found.size()!=originals.size())fail("EconomicDebtNeedOriginalHeaderMissing");return ids;
}
void add_fiscal(std::map<StableId,Integer>& originals,const StableId& id,const std::vector<FiscalDebtObservation>& values){
 if(values.empty())return;const auto tick=values.front().fact_tick;
 for(const auto& value:values)if(value.source_fact_id!=id||value.fact_tick!=tick)fail("FiscalNeedObservationSourceMismatch",id);
 if(!originals.emplace(id,tick).second)fail("DuplicateFiscalNeedOriginalSource",id);
}
}
Status reconcile_economic_debt_needs_at_calendar_boundary_in_candidate(CommonStore& common,AffairsState& affairs,const EconomicDebtNeedClosingContext& x,const ExactPeriodContext& period){try{
 auto actual=take(x.clock.read_current_moment());auto current=take(read_economic_calendar_tick(common,&x.clock));
 if(period.period_id.empty()||period.opening_at.half_day_tick<0||period.opening_at.half_day_tick==INT64_MAX||period.through_tick!=period.opening_at.half_day_tick+1||take(compare_world_moments(period.opening_at,{period.opening_at.half_day_tick,{}}))||take(compare_world_moments(actual,{period.through_tick,{}}))||current!=period.through_tick||x.economy.last_step_tick!=current)fail("EconomicDebtNeedActualClosingRequired");
 if(x.fiscal){
  const auto& f=*x.fiscal;take(validate_fiscal_need_catalog(f.policy,f.taxes));std::map<StableId,Integer> sources;
  for(const auto& row:f.fiscal.assessments.rows())add_fiscal(sources,row.first,take(observe_fiscal_debt_fact(common,x.economy,f.fiscal,f.taxes,row.first,&x.clock)));
  for(const auto& row:f.fiscal.payments.rows())add_fiscal(sources,row.first,take(observe_fiscal_debt_fact(common,x.economy,f.fiscal,f.taxes,row.first,&x.clock)));
  if(f.occupation){for(const auto& row:f.occupation->assessments.rows())add_fiscal(sources,row.first,take(observe_occupation_fiscal_debt_fact(common,x.economy,*f.occupation,f.taxes,row.first,&x.clock)));for(const auto& row:f.occupation->payments.rows())add_fiscal(sources,row.first,take(observe_occupation_fiscal_debt_fact(common,x.economy,*f.occupation,f.taxes,row.first,&x.clock)));}
  const auto ids=ordered_sources(common,sources,current);
  if(f.occupation){take(reconcile_fiscal_needs_in_candidate(common,affairs,x.economy,f.fiscal,f.taxes,f.policy,ids,*f.occupation,&x.clock));FiscalNeedAccess read{x.economy,f.fiscal,f.taxes,f.policy,*f.occupation,&x.clock};take(read.validate_fiscal_need_state(common,affairs));}
  else{take(reconcile_fiscal_needs_in_candidate(common,affairs,x.economy,f.fiscal,f.taxes,f.policy,ids,&x.clock));FiscalNeedAccess read{x.economy,f.fiscal,f.taxes,f.policy,&x.clock};take(read.validate_fiscal_need_state(common,affairs));}
 }
 if(x.cohort_wages){
  const auto& w=*x.cohort_wages;take(validate_cohort_wage_need_catalog(w.policy,x.catalog));std::vector<StableId> obligations;for(const auto& row:w.policy.policies.rows())obligations.push_back(row.second.obligation_id);
  auto history=take(observe_cohort_wage_history(common,x.economy,x.catalog,obligations,w.takeover,w.execution,&x.clock));std::map<StableId,Integer> sources;
  for(const auto& observation:history)if(!sources.emplace(observation.source_fact_id,observation.fact_tick).second)fail("DuplicateCohortDebtNeedOriginalSource",observation.source_fact_id);
  const auto ids=ordered_sources(common,sources,current);take(reconcile_cohort_wage_needs_in_candidate(common,affairs,x.economy,x.catalog,w.policy,ids,w.takeover,w.execution,&x.clock));CohortWageNeedAccess read{x.economy,x.catalog,w.policy,w.takeover,w.execution,&x.clock};take(read.validate_cohort_wage_need_state(common,affairs));
 }
 return common_success();
 }catch(const Failure& error){return {false,{},error.error};}}
}
