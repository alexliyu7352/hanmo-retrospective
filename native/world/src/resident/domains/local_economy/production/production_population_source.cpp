#include "production_population_source.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{v.error};}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
void add(std::vector<StableId>&v,const StableId&id){if(id.empty())fail("ProductionPopulationOriginalCohortRequired");if(std::find(v.begin(),v.end(),id)==v.end())v.push_back(id);}
}
bool is_original_production_population_change(const MomentOwnerSource&s){const auto&r=s.source_ref;return s.source_schema==1&&((r.domain=="local_economy"&&(r.kind=="population_transfer"||r.kind=="population_loss"))||(r.domain=="travel"&&(r.kind=="transport_registration"||r.kind=="transport_release"))||(r.domain=="organization"&&r.kind=="population_training_completed"));}
Outcome<OriginalProductionPopulationChange> read_original_production_population_change(const OriginalProductionPopulationChangeContext&x,const MomentOwnerSource&s){try{
 if(!is_original_production_population_change(s))fail("ProductionPopulationChangeKindRequired",s.source_ref.id);
 const auto&p=x.population;const auto&r=s.source_ref;
 if(&x.training.common!=&p.common||&x.training.economy!=&p.economy||&x.training.organization!=&p.organizations)fail("ProductionPopulationSnapshotMismatch",r.id);
 const auto*h=p.common.state().facts.find(r.id);if(!h||h->id!=r.id||h->domain!=r.domain||h->kind!=r.kind)fail("ProductionPopulationOriginalHeaderRequired",r.id);
 OriginalProductionPopulationChange out;out.actual_source=s;
 if(r.domain=="organization"){
  const auto actual=take(read_organization_population_training_change(x.training,s));
  if(actual.actual_source.source_schema!=s.source_schema||!same_source_ref(actual.actual_source.source_ref,r))fail("ProductionQualificationSourceMismatch",r.id);
  out.occurred_at=actual.occurred_at;add(out.cohort_ids,actual.cohort_id);
 }else{
  take(validate_population_count_revisions(p.common,p.economy,p.population));bool found=false;
  for(const auto&event:p.population.revision_sources){if(!same_source_ref(event.actual_source,r))continue;
   const auto actual=take(read_population_original_change_time(p,event));
   if(!same_source_ref(actual.population_source,r))fail("ProductionPopulationSourceMismatch",r.id);
   if(found&&compare(actual.occurred_at,out.occurred_at))fail("ProductionPopulationChangeTimeMismatch",r.id);
   found=true;out.occurred_at=actual.occurred_at;add(out.cohort_ids,event.cohort_id);
  }
  if(!found)fail("ProductionPopulationRevisionSourceRequired",r.id);
  if(r.domain=="local_economy"&&r.kind=="population_transfer"){
   const auto*t=p.economy.population_transfers.find(r.id);if(!t||std::find(out.cohort_ids.begin(),out.cohort_ids.end(),t->from_cohort_id)==out.cohort_ids.end())fail("ProductionPopulationTransferSourceMismatch",r.id);
   // A materialized transport target has the registration as its own revision
   // source. The actual transfer still names both affected cohorts uniquely.
   add(out.cohort_ids,t->to_cohort_id);
  }
 }
 if(compare(out.occurred_at,take(p.clock.read_current_moment()))>0)fail("ProductionPopulationChangeFromFuture",r.id);
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
}
