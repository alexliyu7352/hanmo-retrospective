// Naval5导航与航损的独立持久格式，旧Naval1..4不得添加空记录或存在位。
#pragma once
#include "naval_navigation_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::naval_navigation_codec {
Status validate(const NavalNavigationState&);
Status emit_fields(ProjectionWriter&,const NavalNavigationState&);
BoundaryValue encode(const NavalNavigationState&);
Outcome<NavalNavigationState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,NavalNavigationState&);
}
