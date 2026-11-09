#include "resident/kernel/catalog_section_observation.hpp"
#include "native_political_treaty_roots.hpp"
#include "original_catalog_identity.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/treaty/treaty_authority_author_loader.hpp"
#include "resident/domains/politics/treaty/treaty_resource_state_read2.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
constexpr const char *module = "political_treaty_terms";
}
Outcome<Integer>
selected_native_political_treaty_version(const CatalogIdentity &identity) {
  Integer selected = 0;
  for (const auto &entry : identity.module_versions) {
    if (entry.first != module)
      continue;
    if (selected || (entry.second != "1" && entry.second != "2"))
      return {
          false,
          {},
          common_failure("PoliticalTreatyRootVersionMismatch", module).error};
    selected = entry.second == "2" ? 2 : 1;
  }
  return {true, selected, {}};
}
Status validate_native_political_treaty_catalog_layout(
    const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 150, "PreparedCatalog", "validate_native_political_treaty_catalog_layout", &catalog);

  auto selected = selected_native_political_treaty_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(catalog.political_treaty_terms) ||
      (selected.value == 2) != bool(catalog.treaty_authority_mapping))
    return common_failure("PoliticalTreatyCatalogSelectionMismatch", module);
  if (!selected.value)
    return common_success();
  if (catalog.identity.content_digest.empty() ||
      !same_original_catalog_identity(catalog.identity,
                                      catalog.political_treaty_terms->identity))
    return common_failure("PoliticalTreatyCatalogIdentityMismatch", module);
  if (catalog.treaty_authority_mapping) {
    if (!same_original_catalog_identity(
            catalog.identity, catalog.treaty_authority_mapping->identity))
      return common_failure("TreatyAuthorityMappingIdentityMismatch", module);
    return validate_treaty_authority_mapping_catalog(
        *catalog.treaty_authority_mapping, *catalog.political_treaty_terms);
  }
  return common_success();
}
Status prepare_native_political_treaty_catalog(
    const BoundaryValue &input, const BoundaryValue &scenario,
    PreparedCatalog &catalog, const TreatyTermsOriginalRead &originals) {
  auto selected = selected_native_political_treaty_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value || catalog.political_treaty_terms ||
      catalog.treaty_authority_mapping)
    return common_failure("PoliticalTreatyCatalogPrepareSelectionRequired",
                          module);
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::political_treaty_terms);
  auto terms =
      decode_political_treaty_terms_catalog(input, catalog.identity, originals);
  if (!terms.ok)
    return {false, {}, terms.error};
  auto owned = checkpoint_make_shared<const PoliticalTreatyTermsCatalog>(
      std::move(terms.value));
  std::shared_ptr<const TreatyAuthorityMappingCatalog> mapping;
  if (selected.value == 2) {
    std::optional<ScopedCheckpointJsonDecode> mapping_scope;
    if (ScopedCheckpointBoundaryDecode::current())
      mapping_scope.emplace(
          CheckpointCatalogJsonKind::treaty_authority_mapping);
    auto made = make_treaty_authority_mapping_from_scenario(
        scenario, catalog.identity, *owned);
    if (!made.ok)
      return {false, {}, made.error};
    mapping = std::move(made.value);
  } else if (scenario.find("treaty_authority_mapping")) {
    return common_failure("UnexpectedTreatyAuthorityMapping", module);
  }
  // 两个已完整作者验证的对象一起交同一个私有 Prepared
  // 候选，失败不留下半个目录。
  catalog.political_treaty_terms = std::move(owned);
  catalog.treaty_authority_mapping = std::move(mapping);
  return validate_native_political_treaty_catalog_layout(catalog);
}
Status initialize_native_political_treaty_seed(const PreparedCatalog &catalog,
                                               InitialWorldSeed &seed) {
  auto valid = validate_native_political_treaty_catalog_layout(catalog);
  if (!valid.ok)
    return valid;
  if (seed.political_treaty_terms)
    return common_failure("PoliticalTreatySeedAlreadyInitialized", module);
  auto selected = selected_native_political_treaty_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value)
    return common_success();
  auto state = initialize_political_treaty_terms_state(selected.value);
  if (!state.ok)
    return {false, {}, state.error};
  seed.political_treaty_terms =
      checkpoint_make_shared<const PoliticalTreatyTermsState>(
          std::move(state.value));
  return common_success();
}
Status validate_native_political_treaty_root_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("PoliticalTreatyWorldCatalogRequired", module,
                          ErrorKind::internal);
  auto valid = validate_native_political_treaty_catalog_layout(*world.catalog);
  if (!valid.ok)
    return valid;
  auto selected =
      selected_native_political_treaty_version(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(world.political_treaty_terms))
    return common_failure("PoliticalTreatyWorldRootSelectionMismatch", module);
  if (!selected.value)
    return common_success();
  const auto &state = *world.political_treaty_terms;
  if (state.schema_version != selected.value ||
      bool(state.resource_body) != (selected.value == 2) ||
      (selected.value == 2 && !state.signatures.rows().empty()))
    return common_failure("PoliticalTreatyWorldRootVersionMismatch", module);
  return common_success();
}
Status validate_native_political_treaty_resources(
    const WorldBundle &world,
    const PoliticalTreatyResourceReadContext2 &context) {
  auto valid = validate_native_political_treaty_root_layout(world);
  if (!valid.ok)
    return valid;
  if (!world.political_treaty_terms ||
      !world.catalog->treaty_authority_mapping ||
      &context.state != &*world.political_treaty_terms ||
      &context.catalog != world.catalog->political_treaty_terms.get() ||
      &context.identity != &world.catalog->identity ||
      &context.mapping != world.catalog->treaty_authority_mapping.get() ||
      &context.signatures.links.revisions.common != &world.common ||
      &context.signatures.links.revisions.organization != &world.organization ||
      &context.signatures.links.revisions.organization_catalog !=
          world.catalog->organization.get())
    return common_failure("PoliticalTreatyWorldOriginalGraphMismatch", module);
  return validate_political_treaty_resources_state2(context);
}
} // namespace hanmo::resident
