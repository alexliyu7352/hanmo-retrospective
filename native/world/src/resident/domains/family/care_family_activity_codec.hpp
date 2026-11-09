#pragma once
#include "care_family_activity.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_care_family_activity_record(CodecVisitor&,CareFamilyActivity&,Integer selected_version);
Status emit_care_family_activity_record_fields(ProjectionWriter&,const CareFamilyActivity&,Integer selected_version);
Outcome<BoundaryValue> encode_care_family_activity_record(const CareFamilyActivity&,Integer selected_version);
Outcome<CareFamilyActivity> decode_care_family_activity_record(const BoundaryValue&,Integer selected_version);
}
