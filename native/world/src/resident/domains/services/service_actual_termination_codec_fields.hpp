// One field order for JSON, checkpoint and projection. The source schema is the
// explicit enclosing selection, never inferred from the chosen union branch.
#pragma once
#include "service_actual_termination_contract.hpp"
#include "resident/model/commitment_ref_codec.hpp"
#include "resident/domains/tasks/work_execution/work_follow_through_codec.hpp"
#include "resident/domains/growth/study_service_ref_codec_fields.hpp"
namespace hanmo::resident::service_termination_codec {
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status scope(A&a,AcceptedServiceScopeRef&v,Integer selected){
 if(selected!=1&&selected!=2&&selected!=3)return a.reject("UnsupportedServiceContractSchema");Integer tag=static_cast<Integer>(v.index());a.field("tag",tag);if(tag<0||tag>=(selected+4))return a.reject("ServiceContractBranchNotSelected");
 if(a.reading()){switch(tag){case 0:v=OrderServiceLine{};break;case 1:v=ProbeService{};break;case 2:v=GroupEnrollment{};break;case 3:v=RepairService{};break;case 4:v=TreatmentService{};break;case 5:v=S3WorkCommission{};break;case 6:v=GrowthStudyService{};break;}}
 auto fields=std::visit([&](auto&x)->Status{using T=std::decay_t<decltype(x)>;
  if constexpr(std::is_same_v<T,OrderServiceLine>){a.field("order_id",x.order_id);a.field("line_id",x.line_id);a.object("accepted_terms_ref",[&](auto&c){return commitment_codec::terms(c,x.accepted_terms_ref);});}
  else if constexpr(std::is_same_v<T,ProbeService>){a.field("probe_id",x.probe_id);a.object("acceptance_source_ref",[&](auto&c){return source(c,x.acceptance_source_ref);});}
  else if constexpr(std::is_same_v<T,GroupEnrollment>){a.field("course_id",x.course_id);a.field("enrollment_id",x.enrollment_id);a.object("accepted_terms_ref",[&](auto&c){return commitment_codec::terms(c,x.accepted_terms_ref);});}
  else if constexpr(std::is_same_v<T,RepairService>){a.object("repair_activity_ref",[&](auto&c){return source(c,x.repair_activity_ref);});a.field("instance_id",x.instance_id);a.object("repair_terms_ref",[&](auto&c){return commitment_codec::terms(c,x.repair_terms_ref);});}
  else if constexpr(std::is_same_v<T,TreatmentService>){a.field("case_id",x.case_id);a.field("treatment_id",x.treatment_id);a.object("accepted_plan_terms_ref",[&](auto&c){return commitment_codec::terms(c,x.accepted_plan_terms_ref);});}
  else if constexpr(std::is_same_v<T,GrowthStudyService>){a.field("action_id",x.action_id);a.object("accepted_execution_ref",[&](auto&c){return study_service_ref_codec_fields::accepted(c,x.accepted_execution_ref);});a.object("accepted_terms_ref",[&](auto&c){return study_service_ref_codec_fields::terms(c,x.accepted_terms_ref);});}
  else return work_follow_through_codec::scope(a,x);
  return a.status();
 },v);if(!fields.ok)return fields;auto valid=validate_accepted_service_scope_shape(v,selected);return valid.ok?a.status():a.reject(valid.error.original.code);
}
template<class A>Status terminal(A&a,OriginalServiceTerminalRef&v,Integer selected){
 if(selected!=1&&selected!=2&&selected!=3)return a.reject("UnsupportedServiceContractSchema");Integer tag=static_cast<Integer>(v.index());a.field("tag",tag);if(tag<0||tag>=(selected+4))return a.reject("ServiceContractBranchNotSelected");
 if(a.reading()){switch(tag){case 0:v=OrderLineTerminal{};break;case 1:v=ProbeTerminal{};break;case 2:v=EnrollmentTerminal{};break;case 3:v=RepairTerminal{};break;case 4:v=TreatmentTerminal{};break;case 5:v=WorkCommissionTerminal{};break;case 6:v=StudyTerminal{};break;}}
 auto fields=std::visit([&](auto&x)->Status{using T=std::decay_t<decltype(x)>;
  if constexpr(std::is_same_v<T,OrderLineTerminal>){a.field("order_id",x.order_id);a.field("line_id",x.line_id);a.field("result_id",x.result_id);}
  else if constexpr(std::is_same_v<T,ProbeTerminal>){a.field("probe_id",x.probe_id);a.field("fact_id",x.fact_id);}
  else if constexpr(std::is_same_v<T,EnrollmentTerminal>){a.field("course_id",x.course_id);a.field("enrollment_id",x.enrollment_id);a.field("boundary_result_id",x.boundary_result_id);}
  else if constexpr(std::is_same_v<T,RepairTerminal>){a.object("repair_activity_ref",[&](auto&c){return source(c,x.repair_activity_ref);});a.field("repair_result_id",x.repair_result_id);}
  else if constexpr(std::is_same_v<T,TreatmentTerminal>){a.field("case_id",x.case_id);a.field("treatment_id",x.treatment_id);a.field("terminal_result_id",x.terminal_result_id);}
  else if constexpr(std::is_same_v<T,StudyTerminal>){auto status=study_service_ref_codec_fields::terminal(a,x);if(!status.ok)return status;Integer slot=0,schema=1;a.field("terminal_slot",slot);a.field("source_schema",schema);return slot==0&&schema==1?a.status():a.reject("StudyOriginalTerminationSlotRequired");}
  else return work_follow_through_codec::commission_terminal_ref(a,x);
  if constexpr(!std::is_same_v<T,StudyTerminal>)return a.field("source_schema",x.source_schema);
  else return a.status();
 },v);if(!fields.ok)return fields;auto valid=validate_original_service_terminal_shape(v,selected);return valid.ok?a.status():a.reject(valid.error.original.code);
}
}
