#pragma once
#include "care_facility_use.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_care_facility_use_binding(CodecVisitor&,CareFacilityUseBinding&,Integer selected_care_version);
Status emit_care_facility_use_binding_fields(ProjectionWriter&,const CareFacilityUseBinding&,Integer selected_care_version);
Outcome<BoundaryValue> encode_care_facility_use_binding(const CareFacilityUseBinding&,Integer selected_care_version);
Outcome<CareFacilityUseBinding> decode_care_facility_use_binding(const BoundaryValue&,Integer selected_care_version);
}
