// 独立物理段的四路同字段存读；旧 travel 段不增加字段。
#pragma once
#include "movement_exact_model.hpp"
namespace hanmo::resident {
class CodecVisitor; class ProjectionWriter; struct MovementRecordCodecSelection;
inline constexpr Integer local_move_codec_revision = 1;
Status visit_local_move_checkpoint(CodecVisitor&, MovementExactState&, Integer, const MovementRecordCodecSelection&);
Status emit_local_move_projection(ProjectionWriter&, std::string_view, const MovementExactState&, Integer, const MovementRecordCodecSelection&);
Outcome<BoundaryValue> encode_local_moves(const MovementExactState&, Integer, const MovementRecordCodecSelection&);
Status decode_local_moves(const BoundaryValue&, MovementExactState&, Integer, const MovementRecordCodecSelection&);
} // namespace hanmo::resident
