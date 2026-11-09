#include "resident/kernel/catalog_section_observation.hpp"
#include "native_growth_market_sources.hpp"
#include "exact_time_profile.hpp"
#include "resident/checkpoint/common_codec_selection.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
Outcome<Integer>
selected_native_growth_market_version(const CatalogIdentity &identity) {
  Integer selected = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == "growth_market_history") {
      if (selected || (row.second != "1" && row.second != "2"))
        return {false,
                {},
                common_failure("GrowthMarketActualSelectionMismatch").error};
      selected = row.second == "2" ? 2 : 1;
    }
  return {true, selected, {}};
}
namespace {
bool exact_module(const CatalogIdentity &identity, const char *name,
                  const char *version) {
  std::size_t count = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == name) {
      if (row.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
} // namespace
Status validate_native_growth_market_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 138, "PreparedCatalog", "validate_native_growth_market_catalog", &catalog);

  auto selected = selected_native_growth_market_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(selected.value) != bool(catalog.growth_market_history) ||
      (selected.value &&
       catalog.growth_market_history->selected_version != selected.value))
    return common_failure("GrowthMarketActualRootsMismatch");
  if (selected.value != 2)
    return common_success();
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.common || !catalog.growth || !catalog.actor_foundation ||
      !catalog.growth->complete_study_sources ||
      catalog.actor_foundation->original_growth.get() != catalog.growth.get() ||
      !exact_module(catalog.identity, "world_timeline", "2") ||
      !exact_module(catalog.identity, "action_elapsed", "2") ||
      !exact_module(catalog.identity, "actor_start_context", "3") ||
      !exact_module(catalog.identity, "selected_resource_operations", "1"))
    return common_failure("GrowthMarketOriginalHistory2Required");
  auto common = select_common_codec(5, catalog.identity);
  if (!common.ok)
    return {false, {}, std::move(common.error)};
  const auto &f = common.value.features;
  const auto &r = catalog.growth_market_history->resource_codec;
  if (r.item_instance_refs_enabled != f.item_instances ||
      r.lot_property_refs_enabled != f.lot_properties ||
      r.domain_activity_reservation_refs_enabled != f.domain_activity ||
      r.carrier_location_enabled != f.carrier_location)
    return common_failure("GrowthMarketCommonLayoutMismatch");
  return common_success();
}
Status
bind_native_growth_market_catalog(PreparedCatalog &catalog,
                                  GrowthMarketHistoryCatalog &candidate) {
  if (catalog.growth_market_history.get() != &candidate)
    return common_failure("GrowthMarketPreparedCandidateMismatch");
  if (candidate.selected_version == 2) {
    auto common = select_common_codec(5, catalog.identity);
    if (!common.ok)
      return {false, {}, std::move(common.error)};
    const auto &f = common.value.features;
    candidate.resource_codec = {f.item_instances, f.lot_properties,
                                f.domain_activity, f.carrier_location};
  }
  return validate_native_growth_market_catalog(catalog);
}
Outcome<std::optional<GrowthMarketExactSources>>
read_native_growth_market_exact_sources(const WorldBundle &w) {
  if (!w.catalog)
    return {
        false, {}, common_failure("GrowthMarketActualCatalogRequired").error};
  auto valid = validate_native_growth_market_catalog(*w.catalog);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto choice = selected_native_growth_market_version(w.catalog->identity);
  if (!choice.ok)
    return {false, {}, std::move(choice.error)};
  const auto selected = choice.value;
  if (bool(selected) != bool(w.catalog->growth_market_history) ||
      (selected &&
       (w.catalog->growth_market_history->selected_version != selected ||
        w.growth_market_history.version != selected)))
    return {false, {}, common_failure("GrowthMarketActualRootsMismatch").error};
  if (selected != 2)
    return {true, {}, {}};
  if (!w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) ||
      !w.catalog->actor_foundation || !w.actor_foundation ||
      !w.catalog->growth ||
      w.catalog->actor_foundation->original_growth.get() !=
          w.catalog->growth.get())
    return {false,
            {},
            common_failure("GrowthMarketActualFoundationRequired").error};
  return {true,
          GrowthMarketExactSources{*w.catalog->actor_foundation,
                                   *w.catalog->growth_market_history,
                                   w.growth_market_history},
          {}};
}
} // namespace hanmo::resident
