#pragma once
#include "social_event_original_source.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_social_event_original_application(CodecVisitor&,SocialEventOriginalApplication&);
Status emit_social_event_original_application_fields(ProjectionWriter&,const SocialEventOriginalApplication&);
Outcome<BoundaryValue> encode_social_event_original_application(const SocialEventOriginalApplication&);
Outcome<SocialEventOriginalApplication> decode_social_event_original_application(const BoundaryValue&);
}
