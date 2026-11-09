// 明确 Face3 的独立原源段及同 World 校验；不把旧三表通讯冒充新选型。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "native_household_purchase_report_world.hpp"
namespace hanmo::resident {
struct PreparedCatalog;
struct WorldBundle;
struct InteractionState;
struct BoundaryValue;
// 仅本轮具名 Face3 被中央装配；旧 /1、/2 独立 codec 语义不变。
Outcome<Integer> selected_native_face_to_face_version(const CatalogIdentity &);
Status append_native_face_to_face_module(const BoundaryValue &scenario,
                                         CatalogIdentity &);
Status validate_native_face_to_face_catalog(const PreparedCatalog &);
Status initialize_native_face_to_face_sources(const PreparedCatalog &,
                                              InteractionState &);
Status validate_native_face_to_face_layout(const PreparedCatalog &,
                                           const InteractionState &);
// 依赖由中央编译配置和真目录提供，不能由命令或存档自报。
Outcome<NativeHouseholdPurchaseReportWorldDependencies>
make_native_household_report_dependencies(const WorldBundle &);
Status validate_native_face_to_face_world(const WorldBundle &);
} // namespace hanmo::resident
