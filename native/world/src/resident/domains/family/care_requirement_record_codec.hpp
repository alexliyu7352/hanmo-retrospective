#pragma once
#include "care_requirement_contract.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status visit_care_requirement_record(CodecVisitor&,CareRequirement&);
Status visit_care_requirement_transition_record(CodecVisitor&,CareRequirementTransition&);
Status visit_care_temporal_event_record(CodecVisitor&,CareTemporalEvent&);
Status emit_care_requirement_record_fields(ProjectionWriter&,const CareRequirement&);
Status emit_care_requirement_transition_record_fields(ProjectionWriter&,const CareRequirementTransition&);
Status emit_care_temporal_event_record_fields(ProjectionWriter&,const CareTemporalEvent&);
Outcome<CareRequirement> decode_care_requirement_record(const BoundaryValue&);
Outcome<CareRequirementTransition> decode_care_requirement_transition_record(const BoundaryValue&);
Outcome<CareTemporalEvent> decode_care_temporal_event_record(const BoundaryValue&);
}
