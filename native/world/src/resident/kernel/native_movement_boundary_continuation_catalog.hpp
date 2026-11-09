// Field＋原边界选型的必备派生段；来源身份沿既有 CatalogIdentity 保存。
#pragma once
#include "resident/domains/travel/movement_boundary_continuation_codec.hpp"

namespace hanmo::resident {
struct PreparedCatalog;
struct WorldBundle;
struct CommonState;

inline constexpr const char *movement_boundary_continuation_module =
    "movement_boundary_continuations";
inline constexpr const char *movement_boundary_continuation_derivation_key =
    "movement_boundary_continuations_derived_from";
inline constexpr const char *movement_boundary_continuation_derivation =
    "field_battle/1+movement_boundary_contacts/1";

// 前置选中即必须同时有 /1 与精确来源；不由空表或存在行反推选择。
Outcome<Integer> selected_native_movement_boundary_continuation_version(
    const CatalogIdentity &);
// 只在原作者前置模块完成后派生；禁止作者另填此技术子模块或覆盖来源。
Status append_native_movement_boundary_continuation_module(
    const BoundaryValue &scenario, CatalogIdentity &);
// 物理双向覆盖不替代完整 Field reader；无原根/未选残留/孤头均拒绝。
Status validate_native_movement_boundary_continuation_headers(
    const CommonState &, const CatalogIdentity &, const MovementExactState *);
Status validate_native_movement_boundary_continuation_layout(const WorldBundle &);
} // namespace hanmo::resident
