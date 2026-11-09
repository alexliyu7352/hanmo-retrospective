#pragma once
#include "care_handover_execution.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status visit_care_handover_execution(CodecVisitor&,CareHandoverExecution&,Integer selected_version);
Status emit_care_handover_execution_fields(ProjectionWriter&,const CareHandoverExecution&,Integer selected_version);
Outcome<BoundaryValue> encode_care_handover_execution(const CareHandoverExecution&,Integer selected_version);
Outcome<CareHandoverExecution> decode_care_handover_execution(const BoundaryValue&,Integer selected_version);
Status visit_care_handover_work_period2(CodecVisitor&,CareHandoverWorkPeriod2&);
Status emit_care_handover_work_period2_fields(ProjectionWriter&,const CareHandoverWorkPeriod2&);
Outcome<BoundaryValue> encode_care_handover_work_period2(const CareHandoverWorkPeriod2&);
Outcome<CareHandoverWorkPeriod2> decode_care_handover_work_period2(const BoundaryValue&);
Status visit_care_handover_execution_boundary1(CodecVisitor&,CareHandoverExecutionBoundary1&);
Status emit_care_handover_execution_boundary1_fields(ProjectionWriter&,const CareHandoverExecutionBoundary1&);
Outcome<BoundaryValue> encode_care_handover_execution_boundary1(const CareHandoverExecutionBoundary1&);
Outcome<CareHandoverExecutionBoundary1> decode_care_handover_execution_boundary1(const BoundaryValue&);
}
