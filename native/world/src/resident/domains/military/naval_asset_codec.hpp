// 独立海军资产段的严格冷边界；不替中央选择版本/feature，不给旧军事段加存在位。
#pragma once
#include "naval_assets.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::naval_asset_codec {
Status emit_fields(ProjectionWriter&,const NavalAssetState&);
BoundaryValue encode(const NavalAssetState&);
Outcome<NavalAssetState> decode(const BoundaryValue&,const NavalAssetCatalog&);
Status visit(CodecVisitor&,NavalAssetState&,const NavalAssetCatalog&);
// 仅Naval2组合段可传true；旧入口固定false且字节完全不追加。
Outcome<NavalAssetState> decode(const BoundaryValue&,const NavalAssetCatalog&,bool combat_enabled);
Status visit(CodecVisitor&,NavalAssetState&,const NavalAssetCatalog&,bool combat_enabled);
}
