// I25.5/IC8/ST3 的同一静态服务合同。原 owner 持唯一接受与终局。
// /1 五支、/2 加 Work、/3 加原 Study；不持服务进度、资金或中央终止表。
#pragma once
#include "resident/domains/tasks/work_execution/work_follow_through.hpp"
#include "resident/model/commitment_ref.hpp"
#include "resident/domains/growth/study_service_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct OrderServiceLine {StableId order_id,line_id;TermsRevisionRef accepted_terms_ref;};
struct ProbeService {StableId probe_id;SourceRef acceptance_source_ref;};
struct GroupEnrollment {StableId course_id,enrollment_id;TermsRevisionRef accepted_terms_ref;};
struct RepairService {SourceRef repair_activity_ref;StableId instance_id;TermsRevisionRef repair_terms_ref;};
struct TreatmentService {StableId case_id,treatment_id;TermsRevisionRef accepted_plan_terms_ref;};
struct GrowthStudyService {StableId action_id;StudyAcceptedExecutionRef accepted_execution_ref;StudyFactTermsRef accepted_terms_ref;};
using AcceptedServiceScopeRef=std::variant<OrderServiceLine,ProbeService,GroupEnrollment,RepairService,TreatmentService,S3WorkCommission,GrowthStudyService>;
struct OrderLineTerminal {StableId order_id,line_id,result_id;Integer source_schema=0;};
struct ProbeTerminal {StableId probe_id,fact_id;Integer source_schema=0;};
struct EnrollmentTerminal {StableId course_id,enrollment_id,boundary_result_id;Integer source_schema=0;};
struct RepairTerminal {SourceRef repair_activity_ref;StableId repair_result_id;Integer source_schema=0;};
struct TreatmentTerminal {StableId case_id,treatment_id,terminal_result_id;Integer source_schema=0;};
using OriginalServiceTerminalRef=std::variant<OrderLineTerminal,ProbeTerminal,EnrollmentTerminal,RepairTerminal,TreatmentTerminal,WorkCommissionTerminal,StudyTerminal>;
// Shape/version gates never prove actual acceptance, authority or unavailability.
// The caller must use the matching original owner's historical source reader.
Status validate_accepted_service_scope_shape(const AcceptedServiceScopeRef&,Integer selected_schema);
Status validate_original_service_terminal_shape(const OriginalServiceTerminalRef&,Integer selected_schema);
Status validate_service_scope_terminal_identity(const AcceptedServiceScopeRef&,const OriginalServiceTerminalRef&,Integer selected_schema);
bool same_accepted_service_scope(const AcceptedServiceScopeRef&,const AcceptedServiceScopeRef&);
bool same_original_service_terminal(const OriginalServiceTerminalRef&,const OriginalServiceTerminalRef&);
Outcome<AcceptedServiceScopeRef> decode_accepted_service_scope(const BoundaryValue&,Integer selected_schema);
Outcome<BoundaryValue> encode_accepted_service_scope(const AcceptedServiceScopeRef&,Integer selected_schema);
Status visit_accepted_service_scope_checkpoint(CodecVisitor&,AcceptedServiceScopeRef&,Integer selected_schema);
Status emit_accepted_service_scope_fields(ProjectionWriter&,const AcceptedServiceScopeRef&,Integer selected_schema);
Outcome<OriginalServiceTerminalRef> decode_original_service_terminal(const BoundaryValue&,Integer selected_schema);
Outcome<BoundaryValue> encode_original_service_terminal(const OriginalServiceTerminalRef&,Integer selected_schema);
Status visit_original_service_terminal_checkpoint(CodecVisitor&,OriginalServiceTerminalRef&,Integer selected_schema);
Status emit_original_service_terminal_fields(ProjectionWriter&,const OriginalServiceTerminalRef&,Integer selected_schema);
}
