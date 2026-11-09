#pragma once
#include "care_arrangement.hpp"
namespace hanmo::resident {
class CodecVisitor;class ProjectionWriter;
Status visit_accepted_caregiver_record(CodecVisitor&,AcceptedCaregiver&,Integer selected_care_version);
Status emit_accepted_caregiver_record_fields(ProjectionWriter&,const AcceptedCaregiver&,Integer selected_care_version);
Outcome<BoundaryValue> encode_accepted_caregiver_record(const AcceptedCaregiver&,Integer selected_care_version);
Outcome<AcceptedCaregiver> decode_accepted_caregiver_record(const BoundaryValue&,Integer selected_care_version);
}
