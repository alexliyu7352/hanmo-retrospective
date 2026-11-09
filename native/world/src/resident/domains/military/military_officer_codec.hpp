// 新调任字段仅在显式Military6写入；旧1..5不添加空序列或新的回执字段。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_officer_model.hpp"
#include "military_officer_profile.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::military_officer_codec {
Status emit_fields(ProjectionWriter&,const MilitaryOfficerAssignmentFact&,bool apply_order=true);
// 中央已独占预留16384给Military6；不能借用条约或经济特性。
inline constexpr std::uint32_t checkpoint_feature=military_officer_checkpoint_feature;
Status validate_json_unique_keys(const BoundaryValue&);
Status validate(const MilitaryOfficerAssignmentFact&);
BoundaryValue encode(const MilitaryOfficerAssignmentFact&);
Outcome<MilitaryOfficerAssignmentFact> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryOfficerAssignmentFact&);
Outcome<MilitaryOfficerAssign> decode_command(const BoundaryValue&);
BoundaryValue encode_quote(const MilitaryOfficerAssignmentQuote&);
}
