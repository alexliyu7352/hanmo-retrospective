#include "population_qualification_history.hpp"
#include "local_economy_military.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore&c,const SourceRef&s){return take(read_actual_common_fact_position(c,s));}
void add(std::vector<SourceRef>&v,const SourceRef&s){if(std::none_of(v.begin(),v.end(),[&](const auto&old){return same_source_ref(old,s);}))v.push_back(s);}
void context(const PopulationQualificationHistoryContext&x){const auto&p=x.population;if(&x.training.common!=&p.common||&x.training.organization!=&p.organizations||&x.training.economy!=&p.economy)fail("PopulationQualificationSnapshotMismatch");}
bool relevant(const OrganizationState&s,const OrganizationFact&f,const StableId&cohort){const auto*a=s.activities.find(f.activity_id);if(!a)fail("PopulationQualificationTrainingSourceMissing",f.id);return std::any_of(a->participation.begin(),a->participation.end(),[&](const auto&p){return p.subject.kind==SubjectKind::cohort&&p.subject.id==cohort;});}
OriginalPopulationQualification read(const PopulationQualificationHistoryContext&x,const StableId&cohort,const SourceRef&cut,const WorldMoment&at){
 context(x);const auto&p=x.population;const auto stop=position(p.common,cut);if(compare(at,take(p.clock.read_current_moment()))>0)fail("PopulationQualificationFromFuture",cohort);
 const auto*seed=x.catalog.cohorts.find(cohort);const auto*current=p.economy.cohorts.find(cohort);if(!seed||!current)fail("PopulationQualificationOriginalCohortRequired",cohort);
 auto initial=take(read_population_revision_source(p.economy,p.population,{cohort,1,{}}));if(initial.headcount!=seed->headcount||initial.labor_capacity!=seed->labor_capacity||initial.condition_source_ref||!same_source_ref(initial.location_binding_ref,{"local_economy","initial_cohort_site",cohort}))fail("PopulationQualificationInitialMismatch",cohort);
 OriginalPopulationQualification out;Integer revision=1;add(out.original_sources,{"local_economy_catalog","cohort",cohort});
 for(const auto&e:p.population.revision_sources)if(e.cohort_id==cohort){const auto changed=take(read_population_original_change_time(p,e));const auto pos=position(p.common,e.actual_source);if(compare(changed.occurred_at,at)<0&&pos>stop)fail("PopulationQualificationAdoptionCausality",e.actual_source.id);if(pos<=stop){if(compare(changed.occurred_at,at)>0)fail("PopulationQualificationInputFromFuture",e.actual_source.id);revision=e.revision_after;add(out.original_sources,e.actual_source);}}
 // Existing transport bindings must be supplied by their original source join;
 // an absent binding is not inferred from today's location or cohort type.
 out.population=take(read_population_revision_source(p.economy,p.population,{cohort,revision,{}}));if(out.population.condition_source_ref)fail("PopulationExactConditionQualificationHistoryRequired",out.population.condition_source_ref->id);
 PopulationStatus status=seed->status;auto assignment=seed->assignment_id;out.status=status;out.assignment=assignment;
 for(const auto&id:p.common.state().facts.order()){
  const auto*h=p.common.state().facts.find(id);if(!h)fail("PopulationQualificationOriginalHeaderRequired",id);
  if(h->domain=="local_economy"&&h->kind=="population_transfer"){
   const auto*transfer=p.economy.population_transfers.find(id);if(!transfer)fail("PopulationQualificationOriginalTransferRequired",id);
   if(transfer->to_cohort_id==cohort){const auto*cause=p.common.state().facts.find(transfer->source_fact_id);if(cause&&cause->domain=="population_military"){
    if(!p.military||&p.military->common!=&p.common||&p.military->economy!=&p.economy)fail("PopulationQualificationMilitarySnapshotRequired",id);
    const auto changed=take(read_original_military_population_destination(*p.military,{h->domain,h->kind,id}));if(changed.cohort_id!=cohort)fail("PopulationQualificationMilitaryCohortMismatch",id);status=changed.status;
    const auto pos=position(p.common,changed.population_transfer);if(compare(changed.occurred_at,at)<0&&pos>stop)fail("PopulationQualificationMilitaryAdoptionCausality",id);if(pos<=stop){if(compare(changed.occurred_at,at)>0)fail("PopulationQualificationMilitaryFromFuture",id);out.status=status;add(out.original_sources,changed.original_military_source.source_ref);}
   }}continue;
  }
  if(h->domain!="organization"||(h->kind!="population_training_approved"&&h->kind!="population_training_completed"))continue;
  const auto*f=p.organizations.facts.find(id);if(!f)fail("PopulationQualificationTrainingSourceMissing",id);if(!relevant(p.organizations,*f,cohort))continue;
  const auto changed=take(read_organization_population_training_change(x.training,{{h->domain,h->kind,h->id},1}));if(changed.cohort_id!=cohort||status!=changed.before_status||assignment!=changed.before_assignment)fail("PopulationQualificationTrainingContinuity",id);status=changed.after_status;assignment=changed.after_assignment;
  const auto pos=position(p.common,changed.actual_source.source_ref);if(compare(changed.occurred_at,at)<0&&pos>stop)fail("PopulationQualificationTrainingAdoptionCausality",id);if(pos<=stop){if(compare(changed.occurred_at,at)>0)fail("PopulationQualificationTrainingFromFuture",id);out.status=status;out.assignment=assignment;add(out.original_sources,changed.actual_source.source_ref);}
 }
 if(status!=current->status||assignment!=current->assignment_id)fail("PopulationQualificationOriginalHistoryRequired",cohort);
 return out;
}
}
Outcome<OriginalPopulationQualification> read_original_population_qualification_after_source(const PopulationQualificationHistoryContext&x,const StableId&cohort,const SourceRef&cut,const WorldMoment&at){try{return {true,read(x,cohort,cut,at),{}};}catch(const Failure&e){return {false,{},e.error};}}
Status validate_original_population_qualification_window(const PopulationQualificationHistoryContext&x,const StableId&cohort,const SourceRef&cut,const MomentSpan&span){try{
 if(compare(span.from,span.to)>=0||compare(span.to,take(x.population.clock.read_current_moment()))>0)fail("PopulationQualificationActualWindowRequired",cohort);read(x,cohort,cut,span.from);const auto&p=x.population;const auto stop=position(p.common,cut);
 for(const auto&e:p.population.revision_sources)if(e.cohort_id==cohort&&position(p.common,e.actual_source)>stop){const auto changed=take(read_population_original_change_time(p,e));if(compare(changed.occurred_at,span.to)<0)fail("PopulationQualificationWindowCrossesRevision",e.actual_source.id);}
 for(const auto&r:p.organizations.facts.rows()){const auto&f=r.second;if(f.kind!="population_training_approved"&&f.kind!="population_training_completed")continue;if(relevant(p.organizations,f,cohort)&&position(p.common,{"organization",f.kind,f.id})>stop){const auto changed=take(read_organization_population_training_change(x.training,{{"organization",f.kind,f.id},1}));if(compare(changed.occurred_at,span.to)<0)fail("PopulationQualificationWindowCrossesTraining",f.id);}}
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}
}
