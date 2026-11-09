// 仅Military15资格段编解码；共同点数仍沿原BattleUnit，不建立第二份资源账。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_command_resources_model.hpp"
#include "military_command_resources_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_command_resources_codec {
Status emit_fields(ProjectionWriter&,const MilitaryCommandResourcesState&);
BoundaryValue encode(const MilitaryCommandResourcesState&);
Outcome<MilitaryCommandResourcesState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryCommandResourcesState&);
}
