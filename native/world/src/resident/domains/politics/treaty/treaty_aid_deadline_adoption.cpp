#include "treaty_aid_deadline_adoption.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
// 真实财务与期次借用同一原世界、条款与clock；不按任意source字符串跨候选拼接。
void context(const MaterialAidReadContext& f,const TreatyAidPerformanceReadContext& p){
 if(&f.common!=&p.common||&f.schedules!=&p.schedules||&f.terms!=&p.terms||!f.resources.bound_to(f.common))
  fail("TreatyAidDeadlineCandidateMismatch");
 if(take(compare_world_moments(take(f.clock.read_current_moment()),take(p.clock.read_organization_authority_now()))))
  fail("TreatyAidDeadlineClockMismatch");
}
// 只从原S5债找已经实际建立的唯一S6期次，不补建遗漏的成立绑定。
const TreatyAidOccurrenceBinding& occurrence(const TreatyAidPerformanceReadContext& p,const StableId& debt){
 const TreatyAidOccurrenceBinding* found=nullptr;
 for(const auto& row:p.occurrence_bindings.rows())if(row.second.original_obligation_ref.owner_domain=="local_economy"&&
    row.second.original_obligation_ref.obligation_id==debt){
  if(found||row.first!=row.second.id)fail("TreatyAidDeadlineOccurrenceAmbiguous",debt);
  found=take(read_treaty_aid_occurrence_binding(p,treaty_aid_occurrence_source(row.second),1));
 }
 if(!found)fail("TreatyAidDeadlineOccurrenceRequired",debt);return *found;
}
// 一次key由既有incident原writer核；这里额外保全部late的反向覆盖，不能后来补造。
const TreatyIncident& incident(const TreatyIncidentReadContext& x,const SourceRef& actual){
 const TreatyIncident* found=nullptr;
 for(const auto& row:x.incidents.rows())if(same_source_ref(row.second.source_fact_ref,actual)){
  if(found||row.first!=row.second.id)fail("TreatyAidDeadlineIncidentAmbiguous",actual.id);
  found=take(read_treaty_incident(x,treaty_incident_source(row.second),1));
 }
 if(!found)fail("TreatyAidDeadlineIncidentRequired",actual.id);return *found;
}
void coverage(const MaterialAidReadContext& f,const TreatyAidPerformanceReadContext& p,
 const OrderedTable<TreatyIncident>& rows){
 context(f,p);NativeTreatyMaterialAidIncidentRead source(f);
 TreatyIncidentReadContext incidents{f.common,rows,source,p.clock};
 for(const auto& row:f.state.deadline_results.rows()){
  const auto actual=material_aid_deadline_source(row.first);
  const auto* late=take(read_material_aid_deadline_result(f,actual));
  const auto& b=occurrence(p,late->obligation_id);
  if(!b.deadline_result_ref||!same_source_ref(*b.deadline_result_ref,actual))
   fail("TreatyAidDeadlineBindingCoverage",b.id);
  const auto& event=incident(incidents,actual);
  const auto* schedule=p.schedules.find(b.schedule_ref.id);
  if(!schedule||event.treaty_id!=schedule->treaty_id||event.terms_revision!=schedule->terms_revision||event.term_id!=schedule->term_id)
   fail("TreatyAidDeadlineIncidentScopeMismatch",event.id);
 }
 for(const auto& row:p.occurrence_bindings.rows())if(row.second.deadline_result_ref){
  const auto* late=take(read_material_aid_deadline_result(f,*row.second.deadline_result_ref));
  if(row.second.original_obligation_ref.owner_domain!="local_economy"||row.second.original_obligation_ref.obligation_id!=late->obligation_id)
   fail("TreatyAidDeadlineOriginalDebtMismatch",row.first);
 }
 for(const auto& row:rows.rows())if(row.second.source_fact_ref.domain=="local_economy"&&row.second.source_fact_ref.kind=="material_aid_late"){
  take(read_material_aid_deadline_result(f,row.second.source_fact_ref));
  if(row.first!=row.second.id)fail("TreatyAidDeadlineIncidentIdentityMismatch",row.first);
  take(read_treaty_incident(incidents,treaty_incident_source(row.second),1));
 }
}
}
Status validate_treaty_aid_deadline_adoption(const MaterialAidReadContext& f,
 const TreatyAidPerformanceReadContext& p,const OrderedTable<TreatyIncident>& rows){
 try{coverage(f,p,rows);return common_success();}catch(const Failure& e){return {false,{},e.error};}
}
Outcome<std::optional<TreatyAidDeadlineAdopted>> close_and_record_treaty_aid_deadline_in_candidate(
 CommonStore& c,TreatyMaterialAidState& state,OrderedTable<TreatyAidOccurrenceBinding>& bindings,
 OrderedTable<TreatyIncident>& rows,const MaterialAidReadContext& f,
 const TreatyAidPerformanceReadContext& p,const SourceRef& binding_source,
 const std::optional<ObligationRef>& compensation){try{
 context(f,p);
 if(&c!=&f.common||&state!=&f.state||&bindings!=&p.occurrence_bindings)fail("TreatyAidDeadlineCandidateMismatch");
 coverage(f,p,rows);
 const auto* b=take(read_treaty_aid_occurrence_binding(p,binding_source,1));
 const auto debt=b->original_obligation_ref;
 const auto actual=take(close_material_aid_deadline_in_candidate(c,state,f,debt,compensation));
 if(!actual){
  if(b->deadline_result_ref)fail("TreatyAidDeadlineUnexpectedBinding",b->id);
  coverage(f,p,rows);return {true,std::nullopt,{}};
 }
 const auto* late=take(read_material_aid_deadline_result(f,*actual));
 if(late->obligation_id!=debt.obligation_id)fail("TreatyAidDeadlineOriginalDebtMismatch",binding_source.id);
 auto* adopted=bindings.find(binding_source.id);
 if(!adopted||!same_obligation_ref(adopted->original_obligation_ref,debt)||
    (adopted->deadline_result_ref&&!same_source_ref(*adopted->deadline_result_ref,*actual)))
  fail("TreatyAidDeadlineBindingMismatch",binding_source.id);
 adopted->deadline_result_ref=*actual;
 const auto* schedule=p.schedules.find(adopted->schedule_ref.id);
 if(!schedule)fail("TreatyAidScheduleMissing",adopted->schedule_ref.id);
 NativeTreatyMaterialAidIncidentRead source(f);TreatyIncidentReadContext incidents{c,rows,source,p.clock};
 const auto recorded=take(record_treaty_incident_in_candidate(c,rows,incidents,
  schedule->treaty_id,schedule->terms_revision,schedule->term_id,*actual));
 take(validate_treaty_aid_occurrence_prefixes(p));coverage(f,p,rows);
 return {true,TreatyAidDeadlineAdopted{*actual,recorded},{}};
 }catch(const Failure& e){return {false,{},e.error};}
}
}
