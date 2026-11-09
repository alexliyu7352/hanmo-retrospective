// 职责：把原物理定义读取绑定到最终已锁的同一个 PreparedCatalog。
// 不构造完整 Battle provider，也不冒代 DynamicSession.definition_lock_ref。
#pragma once

#include "resident/domains/military/military_spatial_definition_catalog.hpp"

namespace hanmo::resident {
struct PreparedCatalog;
struct CatalogIdentity;

// 职责：只借最终目录及其中的原定义，完整 identity 不另复制或缩成 digest。
// 外层 complete lease 必须保活 owner；冷恢复后须重新取得 view。
struct NativeMilitarySpatialDefinitionView {
  const PreparedCatalog *owner = nullptr;
  const CatalogIdentity *catalog_lock = nullptr;
  const MilitarySpatialGeometryDefinition2 *definition = nullptr;
};

// 职责：核原已选 Military 身份及调用者锁，再读取目录中的确切原行。
// 未选、选中但缺表、未知 id 和错锁分别拒绝，不从 chart/current 反推。
Outcome<NativeMilitarySpatialDefinitionView>
read_native_military_spatial_definition(const PreparedCatalog &,
                                        const CatalogIdentity &expected_lock,
                                        const StableId &definition_id,
                                        ExactArithmeticBudget = {});

// 职责：复核三指针都属于当前最终目录，并逐槽匹配原 definition source。
// 不解引用未证归属的传入指针；旧世界 view 不能复用为新世界的原来源。
Status validate_native_military_spatial_definition_view(
    const PreparedCatalog &, const CatalogIdentity &expected_lock,
    const StableId &definition_id, const SourceRef &actual_definition_source,
    const NativeMilitarySpatialDefinitionView &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
