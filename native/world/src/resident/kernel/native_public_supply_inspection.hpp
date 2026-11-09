// 显式供货告示选择及唯一世界根；不从同处位置或旧订阅推断知识。
#pragma once
#include "resident/domains/interaction/public_supply_inspection.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
Outcome<Integer>
selected_native_public_supply_inspection(const CatalogIdentity &);
Status
append_native_public_supply_inspection_module(const BoundaryValue &scenario,
                                              CatalogIdentity &);
Status
prepare_native_public_supply_inspection_catalog(const BoundaryValue &scenario,
                                                PreparedCatalog &);
Status
validate_native_public_supply_inspection_catalog(const PreparedCatalog &);
Status validate_native_public_supply_inspection_layout(
    const PreparedCatalog &, const CommonStore &,
    const PublicSupplyInspectionState *);
Status validate_native_public_supply_inspection(const WorldBundle &);
// 原命令回调只在自身栈中捕获P10，actual非空时借同core，不重开世界工厂。
Status preview_native_public_supply_inspection(
    const WorldBundle &, const CompleteExactWorldReadBindings *,
    const StableId &actor, const StableId &prop);
Status apply_native_public_supply_inspection(
    WorldBundle &, const CompleteExactWorldReadBindings *,
    const StableId &actor, const StableId &prop);
} // namespace hanmo::resident
