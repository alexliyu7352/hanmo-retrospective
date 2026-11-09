// 正式编译输入的海军块严格解码；旧情景没有此块就没有隐式舰船模块。
#pragma once
#include "naval_assets.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct NavalAuthoringSeed { NavalAssetCatalog catalog;NavalAssetState state; };
// 返回独立候选，未知字段/沉没结果/伪造舰队一律拒绝。
Outcome<NavalAuthoringSeed> decode_naval_authoring(const BoundaryValue&);
// 模块选择只来自本情景显式块，须同时具备动态军旅、十六技能和真实同图。
Outcome<std::optional<NavalAuthoringSeed>> decode_naval_profile(const BoundaryValue& data,const BoundaryValue& scenario);
// 重建作者原船只seed，不造舰队、Action、人物或资源。
NavalAssetState initial_naval_assets(const NavalAssetCatalog&);
}
