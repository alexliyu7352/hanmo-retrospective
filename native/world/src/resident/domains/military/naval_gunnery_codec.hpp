// Naval6独立准备序列；仅新段显式访问，原Naval1..5不追加空表或改旧字段。
#pragma once
#include "naval_gunnery_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::naval_gunnery_codec {
Status emit_fields(ProjectionWriter&,const NavalGunneryState&);
BoundaryValue encode(const NavalGunneryState&);
Outcome<NavalGunneryState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,NavalGunneryState&);
}
