// Military11只保存同一会战军级映射/一次原因；旧Battle访客不追加存在位或空数组。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_command_shock_contract.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_command_shock_codec {
Status emit_fields(ProjectionWriter&,const MilitaryCommandShockState&);
BoundaryValue encode(const MilitaryCommandShockState&);
Outcome<MilitaryCommandShockState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryCommandShockState&);
}
