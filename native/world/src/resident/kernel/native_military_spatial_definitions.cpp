#include "native_military_spatial_definitions.hpp"

#include "hanmo/resident/model_contract.hpp"
#include "native_military_state.hpp"
#include "original_catalog_identity.hpp"

namespace hanmo::resident {

// 职责：从调用者保活的最终 Prepared 根借用原定义并绑定完整目录锁。
// 未选、选中缺表、错锁及缺定义不会降为一个可用空来源。
Outcome<NativeMilitarySpatialDefinitionView>
read_native_military_spatial_definition(const PreparedCatalog &catalog,
                                        const CatalogIdentity &expected_lock,
                                        const StableId &id,
                                        ExactArithmeticBudget budget) {
  if (!same_original_catalog_identity(catalog.identity, expected_lock))
    return {false,
            {},
            common_failure("MilitarySpatialDefinitionCatalogLockMismatch", id)
                .error};
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.dynamic_military)
    return {false,
            {},
            common_failure("MilitarySpatialDefinitionNotSelected", id,
                           ErrorKind::unsupported_capability)
                .error};
  if (!catalog.military_spatial_definitions)
    return {false,
            {},
            common_failure("MilitarySpatialDefinitionCatalogRequired", id,
                           ErrorKind::unsupported_capability)
                .error};
  auto definition = read_military_spatial_definition(
      *catalog.military_spatial_definitions, id, budget);
  if (!definition.ok)
    return {false, {}, std::move(definition.error)};
  return {true, {&catalog, &catalog.identity, definition.value}, {}};
}

// 职责：先证三指针归属，再核同一原记录的完整来源引用。
// 外来或冷恢复前的 view 不解引用；此目录锁不是会话运行锁。
Status validate_native_military_spatial_definition_view(
    const PreparedCatalog &catalog, const CatalogIdentity &expected_lock,
    const StableId &id, const SourceRef &source,
    const NativeMilitarySpatialDefinitionView &view,
    ExactArithmeticBudget budget) {
  if (view.owner != &catalog || view.catalog_lock != &catalog.identity ||
      !view.definition)
    return common_failure("MilitarySpatialDefinitionViewOwnerMismatch", id,
                          ErrorKind::internal);
  auto actual = read_native_military_spatial_definition(catalog, expected_lock,
                                                        id, budget);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  if (view.definition != actual.value.definition)
    return common_failure("MilitarySpatialDefinitionViewOwnerMismatch", id,
                          ErrorKind::internal);
  const auto &original = actual.value.definition->actual_definition_source;
  if (source.domain != original.domain || source.kind != original.kind ||
      source.id != original.id)
    return common_failure("MilitarySpatialDefinitionSourceMismatch", id);
  return common_success();
}
} // namespace hanmo::resident
