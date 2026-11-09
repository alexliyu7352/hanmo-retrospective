// 只在Military14写人物资格段；旧格式没有空数组或存在位。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_commanded_damage_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_commanded_damage_codec {
Status emit_fields(ProjectionWriter&,const MilitaryCommandedDamageState&);
BoundaryValue encode(const MilitaryCommandedDamageState&);
Outcome<MilitaryCommandedDamageState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryCommandedDamageState&);
}
