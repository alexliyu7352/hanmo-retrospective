// 入场本领段只由明确Military16访客调用；1..15不读取或写出存在位。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_tactic_entry.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_tactic_entry_codec {
Status emit_fields(ProjectionWriter&,const MilitaryTacticEntryState&);
BoundaryValue encode(const MilitaryTacticEntryState&);
Outcome<MilitaryTacticEntryState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryTacticEntryState&,std::uint32_t selected_military_version);
}
