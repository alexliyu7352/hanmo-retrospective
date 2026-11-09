#pragma once
#include "military_tactical_intent_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_accepted_intent3(CodecVisitor&,MilitaryAcceptedIntent3&);
Outcome<MilitaryAcceptedIntent3> decode_military_accepted_intent3(const BoundaryValue&);
Outcome<BoundaryValue> encode_military_accepted_intent3(const MilitaryAcceptedIntent3&);
Status emit_military_accepted_intent3(ProjectionWriter&,std::string_view,const MilitaryAcceptedIntent3&);
Status emit_military_accepted_intent3_fields(ProjectionWriter&,const MilitaryAcceptedIntent3&);
}
