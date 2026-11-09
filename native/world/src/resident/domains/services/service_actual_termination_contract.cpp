#include "service_actual_termination_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {namespace {
bool source(const SourceRef&v){return !v.domain.empty()&&!v.kind.empty()&&!v.id.empty();}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
Status schema(Integer version,std::size_t tag){if(version!=1&&version!=2&&version!=3)return common_failure("UnsupportedServiceContractSchema",{},ErrorKind::unsupported_capability);return tag<static_cast<std::size_t>(version+4)?common_success():common_failure("ServiceContractBranchNotSelected",{},ErrorKind::unsupported_capability);}
}
Status validate_accepted_service_scope_shape(const AcceptedServiceScopeRef&v,Integer selected){
 auto checked=schema(selected,v.index());if(!checked.ok)return checked;
 return std::visit([](const auto&x)->Status{using T=std::decay_t<decltype(x)>;
  if constexpr(std::is_same_v<T,OrderServiceLine>){if(x.order_id.empty()||x.line_id.empty())return common_failure("InvalidOrderServiceScope");return validate_terms_revision(x.accepted_terms_ref);}
  else if constexpr(std::is_same_v<T,ProbeService>)return !x.probe_id.empty()&&source(x.acceptance_source_ref)?common_success():common_failure("InvalidProbeServiceScope");
  else if constexpr(std::is_same_v<T,GroupEnrollment>){if(x.course_id.empty()||x.enrollment_id.empty())return common_failure("InvalidEnrollmentServiceScope");return validate_terms_revision(x.accepted_terms_ref);}
  else if constexpr(std::is_same_v<T,RepairService>){if(!source(x.repair_activity_ref)||x.instance_id.empty())return common_failure("InvalidRepairServiceScope");return validate_terms_revision(x.repair_terms_ref);}
  else if constexpr(std::is_same_v<T,TreatmentService>){if(x.case_id.empty()||x.treatment_id.empty())return common_failure("InvalidTreatmentServiceScope");return validate_terms_revision(x.accepted_plan_terms_ref);}
  else if constexpr(std::is_same_v<T,GrowthStudyService>){if(x.action_id.empty()||x.accepted_terms_ref.action_id!=x.action_id)return common_failure("InvalidGrowthStudyServiceScope");return std::visit([&](const auto&e){using E=std::decay_t<decltype(e)>;bool valid=e.action_id==x.action_id;if constexpr(std::is_same_v<E,TeachStudyRequestStart>)valid=valid&&!e.request_id.empty()&&!e.session_id.empty();return valid?common_success():common_failure("InvalidStudyAcceptedExecutionScope");},x.accepted_execution_ref);}
  else return !x.work_id.empty()&&source(x.accepted_task_ref)&&source(x.work_acceptance_source_ref)&&source(x.expense_terms_ref)?common_success():common_failure("InvalidWorkCommissionServiceScope");
 },v);
}
Status validate_original_service_terminal_shape(const OriginalServiceTerminalRef&v,Integer selected){
 auto checked=schema(selected,v.index());if(!checked.ok)return checked;
 return std::visit([](const auto&x)->Status{using T=std::decay_t<decltype(x)>;bool valid=true;if constexpr(!std::is_same_v<T,StudyTerminal>)valid=x.source_schema>0;
  if constexpr(std::is_same_v<T,OrderLineTerminal>)valid=valid&&!x.order_id.empty()&&!x.line_id.empty()&&!x.result_id.empty();
  else if constexpr(std::is_same_v<T,ProbeTerminal>)valid=valid&&!x.probe_id.empty()&&!x.fact_id.empty();
  else if constexpr(std::is_same_v<T,EnrollmentTerminal>)valid=valid&&!x.course_id.empty()&&!x.enrollment_id.empty()&&!x.boundary_result_id.empty();
  else if constexpr(std::is_same_v<T,RepairTerminal>)valid=valid&&source(x.repair_activity_ref)&&!x.repair_result_id.empty();
  else if constexpr(std::is_same_v<T,TreatmentTerminal>)valid=valid&&!x.case_id.empty()&&!x.treatment_id.empty()&&!x.terminal_result_id.empty();
  else if constexpr(std::is_same_v<T,StudyTerminal>)valid=!x.action_id.empty();
  else valid=valid&&!x.task_id.empty()&&source(x.accepted_task_ref)&&!x.work_id.empty()&&!x.task_terminal_fact_id.empty();
  return valid?common_success():common_failure("InvalidOriginalServiceTerminal");
 },v);
}
Status validate_service_scope_terminal_identity(const AcceptedServiceScopeRef&scope,const OriginalServiceTerminalRef&terminal,Integer selected){
 auto checked=validate_accepted_service_scope_shape(scope,selected);if(!checked.ok)return checked;checked=validate_original_service_terminal_shape(terminal,selected);if(!checked.ok)return checked;if(scope.index()!=terminal.index())return common_failure("ServiceTerminalScopeMismatch");
 bool match=std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;
  if constexpr(std::is_same_v<T,OrderServiceLine>){const auto&t=std::get<OrderLineTerminal>(terminal);return x.order_id==t.order_id&&x.line_id==t.line_id;}
  else if constexpr(std::is_same_v<T,ProbeService>)return x.probe_id==std::get<ProbeTerminal>(terminal).probe_id;
  else if constexpr(std::is_same_v<T,GroupEnrollment>){const auto&t=std::get<EnrollmentTerminal>(terminal);return x.course_id==t.course_id&&x.enrollment_id==t.enrollment_id;}
  else if constexpr(std::is_same_v<T,RepairService>)return same(x.repair_activity_ref,std::get<RepairTerminal>(terminal).repair_activity_ref);
  else if constexpr(std::is_same_v<T,TreatmentService>){const auto&t=std::get<TreatmentTerminal>(terminal);return x.case_id==t.case_id&&x.treatment_id==t.treatment_id;}
  else if constexpr(std::is_same_v<T,GrowthStudyService>)return x.action_id==std::get<StudyTerminal>(terminal).action_id;
  else{const auto&t=std::get<WorkCommissionTerminal>(terminal);return x.work_id==t.work_id&&same(x.accepted_task_ref,t.accepted_task_ref);}
 },scope);return match?common_success():common_failure("ServiceTerminalScopeMismatch");
}
bool same_accepted_service_scope(const AcceptedServiceScopeRef&a,const AcceptedServiceScopeRef&b){if(a.index()!=b.index())return false;return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);
 if constexpr(std::is_same_v<T,OrderServiceLine>)return x.order_id==y.order_id&&x.line_id==y.line_id&&same_terms_revision(x.accepted_terms_ref,y.accepted_terms_ref);
 else if constexpr(std::is_same_v<T,ProbeService>)return x.probe_id==y.probe_id&&same(x.acceptance_source_ref,y.acceptance_source_ref);
 else if constexpr(std::is_same_v<T,GroupEnrollment>)return x.course_id==y.course_id&&x.enrollment_id==y.enrollment_id&&same_terms_revision(x.accepted_terms_ref,y.accepted_terms_ref);
 else if constexpr(std::is_same_v<T,RepairService>)return same(x.repair_activity_ref,y.repair_activity_ref)&&x.instance_id==y.instance_id&&same_terms_revision(x.repair_terms_ref,y.repair_terms_ref);
 else if constexpr(std::is_same_v<T,TreatmentService>)return x.case_id==y.case_id&&x.treatment_id==y.treatment_id&&same_terms_revision(x.accepted_plan_terms_ref,y.accepted_plan_terms_ref);
 else if constexpr(std::is_same_v<T,GrowthStudyService>){if(x.action_id!=y.action_id||x.accepted_terms_ref.action_id!=y.accepted_terms_ref.action_id||x.accepted_execution_ref.index()!=y.accepted_execution_ref.index())return false;return std::visit([&](const auto&e){using E=std::decay_t<decltype(e)>;const auto&f=std::get<E>(y.accepted_execution_ref);if(e.action_id!=f.action_id)return false;if constexpr(std::is_same_v<E,TeachStudyRequestStart>)return e.request_id==f.request_id&&e.session_id==f.session_id;else return true;},x.accepted_execution_ref);}
 else return x.work_id==y.work_id&&same(x.accepted_task_ref,y.accepted_task_ref)&&same(x.work_acceptance_source_ref,y.work_acceptance_source_ref)&&same(x.expense_terms_ref,y.expense_terms_ref);
 },a);}
bool same_original_service_terminal(const OriginalServiceTerminalRef&a,const OriginalServiceTerminalRef&b){if(a.index()!=b.index())return false;return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);if constexpr(!std::is_same_v<T,StudyTerminal>){if(x.source_schema!=y.source_schema)return false;}
 if constexpr(std::is_same_v<T,OrderLineTerminal>)return x.order_id==y.order_id&&x.line_id==y.line_id&&x.result_id==y.result_id;
 else if constexpr(std::is_same_v<T,ProbeTerminal>)return x.probe_id==y.probe_id&&x.fact_id==y.fact_id;
 else if constexpr(std::is_same_v<T,EnrollmentTerminal>)return x.course_id==y.course_id&&x.enrollment_id==y.enrollment_id&&x.boundary_result_id==y.boundary_result_id;
 else if constexpr(std::is_same_v<T,RepairTerminal>)return same(x.repair_activity_ref,y.repair_activity_ref)&&x.repair_result_id==y.repair_result_id;
 else if constexpr(std::is_same_v<T,TreatmentTerminal>)return x.case_id==y.case_id&&x.treatment_id==y.treatment_id&&x.terminal_result_id==y.terminal_result_id;
 else if constexpr(std::is_same_v<T,StudyTerminal>)return x.action_id==y.action_id;
 else return x.task_id==y.task_id&&same(x.accepted_task_ref,y.accepted_task_ref)&&x.work_id==y.work_id&&x.task_terminal_fact_id==y.task_terminal_fact_id;
 },a);}
}
