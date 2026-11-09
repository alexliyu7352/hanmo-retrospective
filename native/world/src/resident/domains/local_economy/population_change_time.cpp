#include "population_change_time.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{x.error};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{x.error};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
const FactRecord& head(const CommonStore& c,const SourceRef& s){const auto* h=c.state().facts.find(s.id);if(!h||h->domain!=s.domain||h->kind!=s.kind)fail("PopulationChangeOriginalHeaderRequired",s.id);return *h;}
std::size_t order(const CommonStore& c,const StableId& id){const auto& rows=c.state().facts.order();for(std::size_t i=0;i<rows.size();++i)if(rows[i]==id)return i;fail("PopulationChangeOriginalHeaderRequired",id);}
PopulationOriginalChangeTime read(const PopulationChangeTimeContext& x,const PopulationRevisionSource& event){
 const PopulationRevisionSource* actual=nullptr;for(const auto& e:x.population.revision_sources)if(e.cohort_id==event.cohort_id&&e.revision_after==event.revision_after){if(actual)fail("PopulationRevisionHistoryMismatch",event.cohort_id);actual=&e;}
 if(!actual||actual->revision_before!=event.revision_before||!same_source_ref(actual->actual_source,event.actual_source))fail("PopulationRevisionOriginalSourceRequired",event.cohort_id);
 const auto& source=actual->actual_source;head(x.common,source);WorldMoment at;MomentOwnerSource owner;
 const PopulationTransfer* transfer=nullptr;const PopulationLoss* loss=nullptr;
 if(source.domain=="local_economy"&&source.kind=="population_transfer")transfer=x.economy.population_transfers.find(source.id);
 else if(source.domain=="local_economy"&&source.kind=="population_loss")loss=x.economy.population_losses.find(source.id);
 if(transfer||loss){
  const StableId& cause_id=transfer?transfer->source_fact_id:loss->source_fact_id;const auto* cause=x.common.state().facts.find(cause_id);if(!cause||order(x.common,cause_id)>=order(x.common,source.id))fail("PopulationCountCauseOrderMismatch",source.id);
  if(transfer&&(event.cohort_id!=transfer->from_cohort_id&&event.cohort_id!=transfer->to_cohort_id))fail("PopulationRevisionTransferMismatch",source.id);if(loss&&event.cohort_id!=loss->cohort_id)fail("PopulationRevisionLossMismatch",source.id);
  if(transfer&&cause->domain=="organization"&&cause->kind=="population_training_approved"){
   const auto* f=x.organizations.facts.find(cause_id);const auto* activity=f?x.organizations.activities.find(f->activity_id):nullptr;
   if(!f||!f->occurred_at||f->kind!=cause->kind||!activity||activity->kind!=OrgActivityKind::population_training||transfer->reason!="voluntary_training"||f->amount!=transfer->count||f->tick!=transfer->effective_tick||f->tick!=f->occurred_at->half_day_tick||transfer->labor_eligible_count<=0||transfer->labor_eligible_count>transfer->count)fail("PopulationTrainingOriginalTransferMismatch",source.id);
   Integer participants=0;for(const auto& p:activity->participation)if(p.binding_kind==ParticipationKind::cohort_allocation&&p.subject.kind==SubjectKind::cohort&&p.subject.id==transfer->to_cohort_id)++participants;if(participants!=1)fail("PopulationTrainingOriginalTransferMismatch",source.id);
   at=*f->occurred_at;owner={{"organization",f->kind,f->id},1};
  }else if(transfer&&cause->domain=="travel"&&cause->kind=="transport_registration"){
   const auto* r=x.transport.registrations.find(cause_id);if(!r||transfer->reason!="transport_selection"||transfer->effective_tick!=r->occurred_at.half_day_tick)fail("PopulationTransportOriginalTransferMismatch",source.id);
   Integer bindings=0;for(const auto& b:x.population.transport_bindings.rows())if(b.second.source_population_transfer_ref==std::optional<StableId>{transfer->id}){if(b.second.transport_cohort_id!=transfer->to_cohort_id||!same_source_ref(b.second.registration_source_ref,{"travel","transport_registration",r->id})||b.second.transport_unit_id!=r->transport_unit_id)fail("PopulationTransportOriginalTransferMismatch",source.id);++bindings;}if(bindings!=1)fail("PopulationTransportOriginalTransferMismatch",source.id);at=r->occurred_at;owner={{"travel","transport_registration",r->id},1};
  }else if(cause->domain=="population_military"){
    if (!x.military || &x.military->common != &x.common ||
        &x.military->economy != &x.economy || x.military->selected_version != 4)
      fail("PopulationMilitaryPreciseHistoryRequired", cause_id);
    const auto *f = x.military->state.facts.find(cause_id);
    if (!f || !f->occurred_at || f->kind != cause->kind ||
        std::find(f->population_receipt_ids.begin(),
                  f->population_receipt_ids.end(),
                  source.id) == f->population_receipt_ids.end())
      fail("PopulationMilitaryCountSourceMismatch", source.id);
    const SourceRef original{"population_military", f->kind, f->id};
    const auto schema = take(
        read_population_military_event_source_schema(*x.military, original));
    at = take(read_exact_population_military_event_moment(*x.military, original,
                                                          schema));
    take(population_military_continuity(*x.military));
    owner = {original, schema};
  }else fail("PopulationCountPreciseOwnerRequired",source.id);
  if((transfer?transfer->effective_tick:loss->effective_tick)!=at.half_day_tick)fail("PopulationCountTimeProjectionMismatch",source.id);
 }else if(source.domain=="travel"&&source.kind=="transport_registration"){
  const auto* r=x.transport.registrations.find(source.id);Integer count=0;for(const auto& b:x.population.transport_bindings.rows())if(b.second.transport_cohort_id==event.cohort_id&&same_source_ref(b.second.registration_source_ref,source)){if(!r||b.second.transport_unit_id!=r->transport_unit_id)fail("PopulationTransportRegistrationMismatch",source.id);++count;}if(!r||count!=1)fail("PopulationTransportRegistrationMismatch",source.id);at=r->occurred_at;owner={source,1};
 }else if(source.domain=="travel"&&source.kind=="transport_release"){
  const auto* r=x.transport.releases.find(source.id);Integer count=0;for(const auto& release:x.population.releases)if(same_source_ref(release.actual_handover_source,source)){const auto* b=x.population.transport_bindings.find(release.binding_id);if(!b||b->transport_cohort_id!=event.cohort_id)continue;if(!r||b->transport_unit_id!=r->transport_unit_id||std::find(r->population_binding_ids.begin(),r->population_binding_ids.end(),b->binding_id)==r->population_binding_ids.end())fail("PopulationTransportReleaseMismatch",source.id);++count;}if(!r||count!=1)fail("PopulationTransportReleaseMismatch",source.id);at=r->occurred_at;owner={source,1};
 }else fail("PopulationRevisionPreciseOwnerRequired",source.id);
 take(encode_world_moment(at));if(compare(at,take(x.clock.read_current_moment()))>0)fail("PopulationChangeFromFuture",source.id);return {source,owner,at};
}
}
Outcome<PopulationOriginalChangeTime> read_population_original_change_time(const PopulationChangeTimeContext&x,const PopulationRevisionSource&e){try{return {true,read(x,e),{}};}catch(const Failure&f){return {false,{},f.error};}}
}
