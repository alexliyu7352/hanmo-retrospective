#pragma once
#include "care_period.hpp"
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
Status visit_care_period_record(CodecVisitor&,CarePeriod&,Integer selected_care_version);
Status emit_care_period_record_fields(ProjectionWriter&,const CarePeriod&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_period_record(const CarePeriod&,Integer selected_care_version);
Outcome<CarePeriod> decode_care_period_record(const BoundaryValue&,Integer selected_care_version);
Status visit_care_period_source_record(CodecVisitor&,CarePeriodSource&,Integer selected_care_version);
Status emit_care_period_source_record_fields(ProjectionWriter&,const CarePeriodSource&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_period_source_record(const CarePeriodSource&,Integer selected_care_version);
Outcome<CarePeriodSource> decode_care_period_source_record(const BoundaryValue&,Integer selected_care_version);
}
