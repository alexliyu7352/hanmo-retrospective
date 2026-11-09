// Military12独立休整尾段；旧1..11不写存在位或空数组。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_garrison_rest_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_garrison_rest_codec {
Status emit_fields(ProjectionWriter&,const MilitaryGarrisonRestFact&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitaryGarrisonRestState&);

BoundaryValue encode_fact(const MilitaryGarrisonRestFact&);
Outcome<MilitaryGarrisonRestFact> decode_fact(const BoundaryValue&);
BoundaryValue encode(const MilitaryGarrisonRestState&);
Outcome<MilitaryGarrisonRestState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryGarrisonRestState&);
}
