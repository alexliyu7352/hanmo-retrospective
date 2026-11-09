// 单 prefix/静态 Field 边界的独立物理段；旧 travel 段不追加隐式字段。
#pragma once
#include "resident/domains/travel/movement_boundary_codec.hpp"

namespace hanmo::resident {
struct PreparedCatalog;
struct WorldBundle;
struct CommonState;

// 只承认原作者明确选择的 /1；表中有行不构成能力选择。
Outcome<Integer>
selected_native_movement_boundary_version(const CatalogIdentity &);
Status append_native_movement_boundary_module(const BoundaryValue &scenario,
                                              CatalogIdentity &);
// 只从原作者data捕获；旧未选分支不持新owner，不从运行态flags反填。
Status
prepare_native_movement_boundary_flags(const BoundaryValue &original_data,
                                       PreparedCatalog &);
Status validate_native_movement_boundary_catalog(const PreparedCatalog &);
Status validate_native_movement_boundary_layout(const WorldBundle &);

// 物理双向覆盖：同原 Common 头、prefix 和 unit；不是几何或入场业务验收。
Status validate_native_movement_boundary_headers(const CommonState &,
                                                 const CatalogIdentity &,
                                                 const MovementExactState &);
Outcome<MovementRecordCodecSelection>
native_movement_boundary_codec_selection(const PreparedCatalog &);
} // namespace hanmo::resident
