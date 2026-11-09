#include "work_supervision_execution_terms.hpp"
#include "work_execution_detail.hpp"
namespace hanmo::resident {namespace {
bool supervised(WorkKind kind){return kind==WorkKind::agronomy_supervision||kind==WorkKind::industry_supervision||kind==WorkKind::construction_supervision;}
Status source(const MomentOwnerSource&v){return work_detail::valid(v.source_ref)&&v.source_schema>0?common_success():common_failure("SupervisionOriginalExecutionTermsSourceRequired");}
}
Status validate_supervision_execution_terms_shape(const SupervisionExecutionTerms&v){
 if(v.work_id.empty()||v.execution_ordinal<0||v.action_id.empty()||!supervised(v.kind)||v.service_limit_half_days<=0)return common_failure("InvalidSupervisionExecutionTerms",v.work_id);
 auto s=work_detail::authorization_shape(v.original_authorization_ref);if(!s.ok)return s;s=source(v.original_approved_execution_terms_ref);if(!s.ok)return s;
 if(v.actual_start_source_ref){s=source(*v.actual_start_source_ref);if(!s.ok)return s;if(v.actual_start_source_ref->source_schema!=1||v.actual_start_source_ref->source_ref.domain!="tasks"||v.actual_start_source_ref->source_ref.kind!="supervision_action_started")return common_failure("SupervisionActualActionStartRequired",v.action_id);}
 if(v.actual_terminal_result_ref){s=source(*v.actual_terminal_result_ref);if(!s.ok)return s;if(!v.actual_start_source_ref||v.actual_terminal_result_ref->source_schema!=1||v.actual_terminal_result_ref->source_ref.domain!="tasks"||(v.actual_terminal_result_ref->source_ref.kind!="supervision_window_closed"&&v.actual_terminal_result_ref->source_ref.kind!="supervision_action_ended"))return common_failure("SupervisionActualExecutionTerminalRequired",v.action_id);}
 if(v.initial_successor_acceptance_ref&&(v.execution_ordinal!=0||!work_detail::valid(*v.initial_successor_acceptance_ref)||v.initial_successor_acceptance_ref->domain!="tasks"||v.initial_successor_acceptance_ref->kind!="work_reassignment_accepted"))return common_failure("SupervisionInitialSuccessorAcceptanceRequired",v.action_id);
 return common_success();
}
Status validate_approved_supervision_execution(const WorkEpisode&e,const ApprovedSupervisionExecution&v){
 const auto*p=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);
 if(!p||!supervised(v.kind)||p->kind!=v.kind||v.service_limit_half_days<=0||!v.actual_executor_actor_id||v.actual_executor_actor_id->empty()||!work_detail::same(v.authorization,e.authorization))return common_failure("SupervisionOriginalExecutionApprovalMismatch",e.work_id);
 if(v.initial_successor_acceptance&&(v.initial_successor_acceptance->work_id!=e.work_id||v.initial_successor_acceptance->new_actor_id!=*v.actual_executor_actor_id||v.initial_successor_acceptance->fact_id.empty()))return common_failure("SupervisionInitialSuccessorAcceptanceMismatch",e.work_id);
 auto s=work_detail::authorization_shape(v.authorization);return s.ok?source(v.original_terms_source):s;
}
}
