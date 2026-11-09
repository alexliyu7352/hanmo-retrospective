// 军事17独立工程准备序列；旧1..16调用者不得写空新段或按最高版本默认启用。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_siege_engineering.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::military_siege_engineering_codec {
Status emit_fields(ProjectionWriter&,const MilitarySiegeEngineeringPreparation&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitarySiegeEngineeringState&);
// 单事实仍由原Common事实表持有头；完整状态codec不产生第二份事实。
BoundaryValue encode_fact(const MilitarySiegeEngineeringPreparation&);
Outcome<MilitarySiegeEngineeringPreparation> decode_fact(const BoundaryValue&);
BoundaryValue encode(const MilitarySiegeEngineeringState&);
Outcome<MilitarySiegeEngineeringState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitarySiegeEngineeringState&);
}
