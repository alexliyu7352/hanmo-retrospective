// Military17原参与账独立序列；不向旧SiegeRecord或旧1..16 visitor写空字段。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_siege_lifecycle.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::military_siege_lifecycle_codec {
Status emit_fields(ProjectionWriter&,const MilitarySiegeLifecycleFact&);
Status emit_fields(ProjectionWriter&,const MilitarySiegeLifecycleState&);
// 单事实仍由原Common事实表持有头；完整状态codec不产生第二份事实。
BoundaryValue encode_fact(const MilitarySiegeLifecycleFact&);
Outcome<MilitarySiegeLifecycleFact> decode_fact(const BoundaryValue&);
BoundaryValue encode(const MilitarySiegeLifecycleState&);
Outcome<MilitarySiegeLifecycleState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitarySiegeLifecycleState&);
}
