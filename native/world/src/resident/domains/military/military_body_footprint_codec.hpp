#pragma once
#include "military_body_footprint.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_body_footprint_definition2(CodecVisitor&,MilitaryBodyFootprintDefinition2&);
Outcome<MilitaryBodyFootprintDefinition2> decode_military_body_footprint_definition2(const BoundaryValue&);
Outcome<BoundaryValue> encode_military_body_footprint_definition2(const MilitaryBodyFootprintDefinition2&);
Status emit_military_body_footprint_definition2(ProjectionWriter&,std::string_view,const MilitaryBodyFootprintDefinition2&);
Status emit_military_body_footprint_definition2_fields(ProjectionWriter&,const MilitaryBodyFootprintDefinition2&);
}
