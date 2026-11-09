#pragma once
#include "care_coverage.hpp"
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
Status visit_care_binding_record(CodecVisitor&,CareBinding&,Integer selected_care_version);
Status emit_care_binding_record_fields(ProjectionWriter&,const CareBinding&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_binding_record(const CareBinding&,Integer selected_care_version);
Outcome<CareBinding> decode_care_binding_record(const BoundaryValue&,Integer selected_care_version);
Status visit_care_coverage_satisfied_record(CodecVisitor&,CareCoverageSatisfied&,Integer selected_care_version);
Status emit_care_coverage_satisfied_record_fields(ProjectionWriter&,const CareCoverageSatisfied&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_coverage_satisfied_record(const CareCoverageSatisfied&,Integer selected_care_version);
Outcome<CareCoverageSatisfied> decode_care_coverage_satisfied_record(const BoundaryValue&,Integer selected_care_version);
Status visit_care_coverage_lost_record(CodecVisitor&,CareCoverageLost&,Integer selected_care_version);
Status emit_care_coverage_lost_record_fields(ProjectionWriter&,const CareCoverageLost&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_coverage_lost_record(const CareCoverageLost&,Integer selected_care_version);
Outcome<CareCoverageLost> decode_care_coverage_lost_record(const BoundaryValue&,Integer selected_care_version);
}
