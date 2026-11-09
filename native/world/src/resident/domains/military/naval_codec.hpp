// 同一Naval资产唯一账的显式版本边界；无combat_policy仍逐字沿资产段1。
#pragma once
#include "naval_battle_codec.hpp"
namespace hanmo::resident::naval_codec {
Integer version(const NavalCatalog&);
Status emit_fields(ProjectionWriter&,const NavalState&,const NavalCatalog&);
BoundaryValue encode(const NavalState&,const NavalCatalog&);
Outcome<NavalState> decode(const BoundaryValue&,const NavalCatalog&);
Status visit(CodecVisitor&,NavalState&,const NavalCatalog&);
}
