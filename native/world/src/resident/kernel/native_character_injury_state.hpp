// 明确 source/1 与 C8 成对根；不从 Field 或现有身体推导新格式。
#pragma once
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CatalogIdentity;
struct WorldBundle;
struct BoundaryValue;
struct ExactTimeProfile;
struct PersonalCatalog;
// 只读锁定身份；新对必须具有原完整 phase4 与另行明选的
// Field1、Personal1 及其 lifecycle1，旧选择没有新义务。
Outcome<Integer>
selected_native_character_injury_source_version(const CatalogIdentity &);
// 原作者块只准 version:1；必须在原 Exact 模块已登记之后调用。
Status append_native_character_injury_module(const BoundaryValue &,
                                             CatalogIdentity &,
                                             const ExactTimeProfile *,
                                             const PersonalCatalog &);
// 只校验物理选择与真实根存在；不代替伤势完整原来源反验。
Status validate_native_character_injury_layout(const WorldBundle &);
} // namespace hanmo::resident
