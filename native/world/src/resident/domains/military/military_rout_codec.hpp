// 显式Military10永久轮次证明；旧1..9不读写此独立尾段。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_rout_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Status validate_military_rout_shape(const MilitaryRoutState &);
namespace military_rout_codec {
// 校验原溃退事实后输出既有字段；失败原样返回，字段顺序由调用方选择。
Status emit_fields(ProjectionWriter &, const MilitaryRoutEntryFact &,
                   bool apply_order);
Status emit_fields(ProjectionWriter &, const MilitaryRoutRoundFact &,
                   bool apply_order);
// 校验并输出完整溃退状态，保留原字段顺序与失败结果。
Status emit_fields(ProjectionWriter &, const MilitaryRoutState &);
BoundaryValue encode_entry(const MilitaryRoutEntryFact &);
Outcome<MilitaryRoutEntryFact> decode_entry(const BoundaryValue &);
BoundaryValue encode_round(const MilitaryRoutRoundFact &);
Outcome<MilitaryRoutRoundFact> decode_round(const BoundaryValue &);
BoundaryValue encode(const MilitaryRoutState &);
Outcome<MilitaryRoutState> decode(const BoundaryValue &);
Status visit(CodecVisitor &, MilitaryRoutState &);
} // namespace military_rout_codec
} // namespace hanmo::resident
