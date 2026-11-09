#include "production_population_change.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
void take(Status v){if(!v.ok)throw Failure{v.error};}
void snapshot(const ProductionPopulationChangeContext&x){const auto&p=x.original.population;if(&p.common!=&x.common||&p.economy!=&x.economy||&p.clock!=&x.production.clock)fail("ProductionPopulationMutationSnapshotMismatch");}
std::size_t position(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);const auto&order=c.state().facts.order();const auto i=std::find(order.begin(),order.end(),s.id);if(!h||h->domain!=s.domain||h->kind!=s.kind||i==order.end())fail("ProductionPopulationOriginalHeaderRequired",s.id);return static_cast<std::size_t>(i-order.begin());}
bool relevant(const ProductionPopulationChangeContext&x,const StableId&unit,const std::vector<StableId>&cohorts){const auto*u=x.catalog.production_units.find(unit);if(!u)fail("ProductionPopulationOriginalUnitRequired",unit);for(const auto&id:u->labor_allocation_ids){const auto*a=x.catalog.labor_allocations.find(id);if(!a||a->activity_or_production_id!=unit)fail("ProductionPopulationOriginalAllocationRequired",id);if(std::find(cohorts.begin(),cohorts.end(),a->cohort_id)!=cohorts.end())return true;}return false;}
std::vector<StableId> units(const ProductionPopulationChangeContext&x,const std::vector<StableId>&cohorts){std::vector<StableId>out;for(const auto&row:x.production.catalog.production_profiles.rows()){if(!relevant(x,row.first,cohorts))continue;auto scope=take(x.adoptions.read_original_production_asset_scope(row.first));if(scope.unit_id!=row.first)fail("ProductionPopulationOriginalUnitMismatch",row.first);out.push_back(row.first);}return out;}
const AccumulatedProductionState2* active(const ProductionPopulationChangeContext&x,const StableId&unit){const auto*id=x.production.state.active_cycles_by_unit.find(unit);const auto*c=id?x.production.state.cycles.find(*id):nullptr;if(id&&(!c||c->unit_id!=unit))fail("ProductionPopulationCycleMissing",unit);return c;}
}
struct ProductionPopulationChangeAccess {
 static Outcome<PreparedProductionPopulationChange> prepare(const ProductionPopulationChangeContext&x,const std::vector<StableId>&cohorts){try{
  snapshot(x);if(cohorts.empty())fail("ProductionPopulationOriginalScopeRequired");for(std::size_t i=0;i<cohorts.size();++i)if(!x.economy.cohorts.find(cohorts[i])||std::find(cohorts.begin(),cohorts.begin()+i,cohorts[i])!=cohorts.begin()+i)fail("ProductionPopulationOriginalCohortRequired",cohorts[i]);
  PreparedProductionPopulationChange out;out.common_=&x.common;out.sources_=&x.production.state;out.at_=take(x.production.clock.read_current_moment());out.fact_count_=x.common.state().facts.size();if(out.fact_count_)out.last_fact_=x.common.state().facts.order().back();
  for(const auto&unit:units(x,cohorts)){PreparedProductionPopulationChange::Entry entry;entry.unit_id=unit;const auto*c=active(x,unit);if(c&&!c->settlement_receipt_ref)entry.close.emplace(take(prepare_production_input_close(x.common,x.economy,x.catalog,x.production,unit,out.at_)));out.entries_.push_back(std::move(entry));}
  out.valid_=true;return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
 static Outcome<std::vector<SourceRef>> apply(ProductionPopulationChangeContext&x,PreparedProductionPopulationChange&&prepared,const std::vector<MomentOwnerSource>&changes){try{
  snapshot(x);if(!prepared.valid_||prepared.common_!=&x.common||prepared.sources_!=&x.production.state)fail("ProductionPopulationPreparedCandidateMismatch");prepared.valid_=false;
  const auto&order=x.common.state().facts.order();if(order.size()<prepared.fact_count_||(prepared.fact_count_&&order[prepared.fact_count_-1]!=prepared.last_fact_)||compare(prepared.at_,take(x.production.clock.read_current_moment())))fail("ProductionPopulationPreparedBoundaryMismatch");
  if(changes.empty())fail("ProductionPopulationActualChangeRequired");std::vector<OriginalProductionPopulationChange>actual;std::vector<std::size_t>positions;std::vector<StableId>cohorts;
  for(std::size_t i=0;i<changes.size();++i){for(std::size_t j=0;j<i;++j)if(same_source_ref(changes[j].source_ref,changes[i].source_ref))fail("ProductionPopulationDuplicateChange",changes[i].source_ref.id);auto changed=take(read_original_production_population_change(x.original,changes[i]));const auto pos=position(x.common,changes[i].source_ref);if(pos<prepared.fact_count_||compare(changed.occurred_at,prepared.at_))fail("ProductionPopulationActualBoundaryMismatch",changes[i].source_ref.id);for(const auto&id:changed.cohort_ids)if(std::find(cohorts.begin(),cohorts.end(),id)==cohorts.end())cohorts.push_back(id);positions.push_back(pos);actual.push_back(std::move(changed));}
  const auto expected=units(x,cohorts);if(expected.size()!=prepared.entries_.size())fail("ProductionPopulationAffectedUnitsChanged");std::vector<SourceRef>results;
  for(std::size_t i=0;i<expected.size();++i){auto&entry=prepared.entries_[i];if(entry.unit_id!=expected[i])fail("ProductionPopulationAffectedUnitsChanged",entry.unit_id);std::optional<std::size_t>last;for(std::size_t j=0;j<actual.size();++j)if(relevant(x,entry.unit_id,actual[j].cohort_ids)&&(!last||positions[*last]<positions[j]))last=j;if(!last)fail("ProductionPopulationUnitChangeRequired",entry.unit_id);
   const auto*c=active(x,entry.unit_id);if(!entry.close){if(c&&!c->settlement_receipt_ref)fail("ProductionPopulationCycleOpenedDuringChange",entry.unit_id);continue;}
   const auto&source=actual[*last].actual_source;auto closed=take(apply_prepared_production_input_close_in_candidate(x.common,x.production,std::move(*entry.close),source.source_ref));if(closed)results.push_back({"local_economy","production_input_segment",*closed});
   c=active(x,entry.unit_id);if(!c)fail("ProductionPopulationCycleMissing",entry.unit_id);if(compare(prepared.at_,{c->due_tick,{}})<0)take(install_changed_production_input_source_in_candidate(x.production.state,x.production.inputs,entry.unit_id,prepared.at_,source));
  }
  return {true,std::move(results),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
};
Outcome<PreparedProductionPopulationChange> prepare_production_population_change(const ProductionPopulationChangeContext&x,const std::vector<StableId>&cohorts){return ProductionPopulationChangeAccess::prepare(x,cohorts);}
Outcome<std::vector<SourceRef>> apply_production_population_change_in_candidate(ProductionPopulationChangeContext&x,PreparedProductionPopulationChange&&p,const std::vector<MomentOwnerSource>&changes){return ProductionPopulationChangeAccess::apply(x,std::move(p),changes);}
}
