// 显式13只保存原入战条件；旧1..12没有空存在位。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_combat_conditions_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_combat_conditions_codec {
Status emit_fields(ProjectionWriter&,const MilitaryCombatConditionState&);
BoundaryValue encode(const MilitaryCombatConditionState&);
Outcome<MilitaryCombatConditionState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryCombatConditionState&);
}
