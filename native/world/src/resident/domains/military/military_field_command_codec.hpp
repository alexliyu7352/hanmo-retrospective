// Military7独立尾段与具名命令格式；重复权源键在JSON入口先拒绝。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_field_command_model.hpp"
#include "military_field_command_profile.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::military_field_command_codec {
Status emit_fields(ProjectionWriter&,const MilitaryFieldCommandFact&,bool apply_order=true);
Status validate(const MilitaryFieldCommandFact&);
BoundaryValue encode(const MilitaryFieldCommandFact&);
Outcome<MilitaryFieldCommandFact> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryFieldCommandFact&);
Outcome<MilitaryFieldCommandAssign> decode_command(const BoundaryValue&);
BoundaryValue encode_quote(const MilitaryFieldCommandQuote&);
}
