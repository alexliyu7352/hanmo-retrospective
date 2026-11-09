#include "native_core_budget_responsibilities.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Status NativeCoreBudgetResponsibilityRead::validate_same_job()const {
 const auto&a=aid_context_;const auto&w=work_context_;
 if(&a.common!=&w.common||&a.economy!=&w.economy||&a.budget!=w.budget_execution||
    &a.clock!=&w.clock||a.budget.schema_version!=2||a.state.schema_version!=1||w.state.version!=2||
    !a.resources.bound_to(a.common))return common_failure("NativeBudgetOriginalSameJobRequired");
 if(!a.obligations||a.obligations!=w.obligations||&a.obligations->common!=&a.common||
    &a.obligations->economy!=&a.economy||&a.obligations->clock!=&a.clock||
    !a.obligations->original.bound_to(a.common,a.economy))return common_failure("NativeBudgetOriginalObligationRootRequired");
 if(aid_root2_){if(aid_root2_->schema_version!=2||a.selected_treaty_sources!=2||&aid_root2_->principal!=&a.state||!compensation_context_||!compensation_||a.obligations->sources.schema_version!=2)return common_failure("NativeBudgetSelectedAidP11GraphMismatch");}
 else {if(compensation_context_||compensation_||a.obligations->sources.schema_version!=1||a.selected_treaty_sources!=1)return common_failure("NativeBudgetSelectedAidP11GraphMismatch");for(const auto&r:a.common.state().facts.rows())if(r.second.domain=="local_economy"&&(r.second.kind=="aid_compensation_accepted"||r.second.kind=="aid_compensation_disposed"))return common_failure("NativeBudgetCompensationSourceOutsideSelectedRoot",r.first);}
 auto now=a.clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 auto shape=encode_world_moment(now.value);if(!shape.ok)return {false,{},shape.error};
 if(now.value.half_day_tick<a.common.tick())return common_failure("NativeBudgetClockBeforeCommon");
 return common_success();
}
Outcome<ActualBudgetResponsibility>NativeCoreBudgetResponsibilityRead::read_budget_responsibility(const BudgetExecutionBinding&b)const {
 auto same=validate_same_job();if(!same.ok)return {false,{},same.error};
 if(b.responsibility_ref.domain=="local_economy"&&b.responsibility_ref.kind=="aid_compensation_accepted"){
  if(!compensation_||b.purpose!=BudgetExecutionPurpose::material_aid||b.responsibility_source_schema!=2)return {false,{},common_failure("NativeBudgetCompensationSelectionRequired",b.responsibility_ref.id).error};
  return compensation_->read_budget_responsibility(b);
 }
 if(b.responsibility_source_schema!=1||b.responsibility_ref.domain!="local_economy")
  return {false,{},common_failure("UnsupportedNativeBudgetOriginalSource",b.responsibility_ref.id,ErrorKind::unsupported_capability).error};
 switch(b.purpose){
 case BudgetExecutionPurpose::governance_work_expense:
  if(b.responsibility_ref.kind!="governance_work_expense_terms")break;
  return work_.read_budget_responsibility(b);
 case BudgetExecutionPurpose::material_aid:
  if(b.responsibility_ref.kind!="aid_schedule_accepted")break;
  return aid_.read_budget_responsibility(b);
 default:return {false,{},common_failure("UnsupportedNativeBudgetPurpose",budget_execution_purpose_key(b.purpose),ErrorKind::unsupported_capability).error};
 }
 return {false,{},common_failure("NativeBudgetPurposeSourceMismatch",b.responsibility_ref.id).error};
}
Status NativeCoreBudgetResponsibilityRead::validate_whole_original_coverage()const {
 auto checked=validate_same_job();if(!checked.ok)return checked;
 // Genuine original whole roots perform their reverse scans of accepted
 // sources, schedules, charges, actual payments and Common headers. In
 // particular, an empty bindings vector cannot hide an orphan original fact.
 checked=validate_governance_work_expenses(work_context_);if(!checked.ok)return checked;
 checked=validate_treaty_material_aid(aid_context_);if(!checked.ok)return checked;
 if(aid_root2_){checked=validate_treaty_material_aid_values2(*aid_root2_);if(!checked.ok)return checked;checked=validate_material_aid_compensation_original_coverage2(*compensation_context_);if(!checked.ok)return checked;}
 const auto&executions=aid_context_.budget;
 for(const auto&row:work_context_.state.terms.rows()){
  const SourceRef source{"local_economy","governance_work_expense_terms",row.first};
  std::size_t n=0;for(const auto&b:executions.bindings)if(same_source_ref(b.responsibility_ref,source))++n;
  if(n!=1)return common_failure("NativeBudgetWorkReverseCoverage",row.first);
 }
 for(const auto&row:aid_context_.state.schedule_commitments.rows()){
  const auto&c=row.second;const SourceRef source=aid_schedule_commitment_source(c.id);
  std::size_t n=0;
  for(const auto&b:executions.bindings)if(same_source_ref(b.responsibility_ref,source)){
   ++n;if(std::count(c.budget_binding_refs.begin(),c.budget_binding_refs.end(),b.id)!=1)
    return common_failure("NativeBudgetAidReverseCoverage",c.id);
  }
  if(n!=c.budget_binding_refs.size())return common_failure("NativeBudgetAidReverseCoverage",c.id);
 }
 for(const auto&b:executions.bindings){auto actual=read_budget_responsibility(b);if(!actual.ok)return {false,{},actual.error};}
 return common_success();
}
}
