#include "native_condition_application_world.hpp"
#include "original_catalog_identity.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
namespace {
Status layout(const CommonStore &common, const PreparedCatalog &catalog,
              const AuthoredActorFoundationState *state) {
  auto selected = condition_application_facts_selected(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value && (!state || !catalog.actor_foundation))
    return common_failure("ConditionApplicationWorldRootsRequired");
  if (bool(state) != bool(catalog.actor_foundation))
    return common_failure("ConditionApplicationWorldRootsMismatch");
  if (catalog.actor_foundation &&
      !same_original_catalog_identity(catalog.identity,
                                      catalog.actor_foundation->identity))
    return common_failure("ConditionApplicationWorldCatalogMismatch");
  if (!state)
    for (const auto &row : common.state().facts.rows())
      if (owns_condition_application_fact_kind(row.second.kind))
        return common_failure("ConditionApplicationWorldRootsRequired",
                              row.first);
  return common_success();
}
class HeaderWriter final : public ConditionApplicationFactVisitor {
  ProjectionWriter &writer_;

public:
  explicit HeaderWriter(ProjectionWriter &writer) : writer_(writer) {
  }
  Status original_condition_application(
      const ConditionApplicationFactView &actual) override {
    if (!actual.header || !actual.receipt || actual.source_schema != 2)
      return common_failure("ConditionApplicationOriginalHeaderRequired");
    return writer_.object(actual.header->id, [&](auto &fields) {
      fields.text("kind", actual.header->kind);
      return fields.text("domain", actual.header->domain);
    });
  }
};
}
Status validate_native_condition_application_headers(
    const CommonStore &common, const PreparedCatalog &catalog,
    const AuthoredActorFoundationState *state) {
  auto valid = layout(common, catalog, state);
  if (!valid.ok || !state)
    return valid;
  return validate_condition_application_fact_headers(common, *state,
                                                     *catalog.actor_foundation);
}
Status emit_native_condition_application_headers(
    ProjectionWriter &writer, const CommonStore &common,
    const PreparedCatalog &catalog, const AuthoredActorFoundationState *state) {
  auto valid = layout(common, catalog, state);
  if (!valid.ok || !state)
    return valid;
  HeaderWriter headers(writer);
  return visit_condition_application_fact_headers(
      common, *state, *catalog.actor_foundation, headers);
}
}
