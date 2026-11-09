#include "resident/kernel/catalog_section_observation.hpp"
#include "native_public_supply_inspection.hpp"
#include "exact_complete_world.hpp"
#include "exact_time_profile.hpp"
#include "native_exact_result_owners.hpp"
#include "native_social_selection.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
namespace {
// 作者目录唯一键在两条准入路径复用；缺席不构造默认公告。
Outcome<std::optional<PublicSupplyInspectionCatalog>>
authored(const BoundaryValue &scenario) {
  try {
    const auto *dynamic = scenario.find("dynamic_world");
    if (!dynamic)
      return {true, std::nullopt, {}};
    const BoundaryValue *selected = nullptr;
    for (const auto &row : boundary::object(*dynamic, "scenario/dynamic_world"))
      if (row.first == "public_supply_inspection") {
        if (selected)
          boundary::fail("DuplicatePublicSupplyInspectionSelection",
                         "scenario/dynamic_world");
        selected = &row.second;
      }
    if (!selected)
      return {true, std::nullopt, {}};
    auto decoded = [&] {
      if (checkpoint_boundary_decode_active()) {
        ScopedCheckpointJsonDecode scope(
            CheckpointCatalogJsonKind::public_supply_inspection);
        return decode_public_supply_inspection_catalog(*selected);
      }
      return decode_public_supply_inspection_catalog(*selected);
    }();
    if (!decoded.ok)
      return {false, {}, std::move(decoded.error)};
    auto exact = decode_exact_time_profile(scenario);
    if (!exact.ok)
      return {false, {}, std::move(exact.error)};
    if (!exact.value || !exact_complete_selected(*exact.value))
      boundary::unsupported("PublicSupplyInspectionCompleteProfileRequired",
                            "public_supply_inspection");
    return {true, std::move(decoded.value), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace
Outcome<Integer>
selected_native_public_supply_inspection(const CatalogIdentity &identity) {
  Integer version = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == "public_supply_inspection") {
      if (version || row.second != "1")
        return {false, 0,
                common_failure("PublicSupplyInspectionModuleMismatch").error};
      version = 1;
    }
  if (version) {
    bool complete = false;
    for (const auto &row : identity.module_versions)
      if (row.first == "actor_start_context") {
        if (complete || row.second != "3")
          return {
              false, 0,
              common_failure("PublicSupplyInspectionCompleteProfileRequired")
                  .error};
        complete = true;
      }
    auto cut = selected_native_social_knowledge_cut(identity);
    if (!cut.ok)
      return {false, 0, std::move(cut.error)};
    if (!complete || !cut.value)
      return {
          false, 0,
          common_failure("PublicSupplyInspectionKnowledgeCutRequired").error};
  }
  return {true, version, {}};
}
Status
append_native_public_supply_inspection_module(const BoundaryValue &scenario,
                                              CatalogIdentity &identity) {
  auto selected = authored(scenario);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  for (const auto &row : identity.module_versions)
    if (row.first == "public_supply_inspection")
      return common_failure("DuplicatePublicSupplyInspectionMarker");
  if (selected.value)
    identity.module_versions.emplace_back("public_supply_inspection", "1");
  auto version = selected_native_public_supply_inspection(identity);
  return version.ok ? common_success()
                    : Status{false, {}, std::move(version.error)};
}
Status validate_native_public_supply_inspection_catalog(
    const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 154, "PreparedCatalog", "validate_native_public_supply_inspection_catalog", &catalog);

  auto selected = selected_native_public_supply_inspection(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(selected.value) != bool(catalog.public_supply_inspection))
    return common_failure("PublicSupplyInspectionCatalogSelectionMismatch");
  if (!selected.value)
    return common_success();
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.interaction || !catalog.local_economy || !catalog.social)
    return common_failure("PublicSupplyInspectionOriginalCatalogsRequired");
  auto social = validate_native_social_catalog_selection(catalog);
  if (!social.ok)
    return social;
  return validate_public_supply_inspection_catalog(
      *catalog.public_supply_inspection, *catalog.interaction,
      *catalog.local_economy);
}
Status
prepare_native_public_supply_inspection_catalog(const BoundaryValue &scenario,
                                                PreparedCatalog &catalog) {
  if (catalog.public_supply_inspection)
    return common_failure("PublicSupplyInspectionCatalogAlreadyPrepared");
  auto selected = authored(scenario);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value)
    catalog.public_supply_inspection =
        checkpoint_make_shared<const PublicSupplyInspectionCatalog>(
            std::move(*selected.value));
  return validate_native_public_supply_inspection_catalog(catalog);
}
Status validate_native_public_supply_inspection_layout(
    const PreparedCatalog &catalog, const CommonStore &common,
    const PublicSupplyInspectionState *state) {
  auto valid = validate_native_public_supply_inspection_catalog(catalog);
  if (!valid.ok)
    return valid;
  if (bool(catalog.public_supply_inspection) != bool(state))
    return common_failure("PublicSupplyInspectionStateSelectionMismatch");
  if (state && state->version != 1)
    return common_failure("PublicSupplyInspectionStateVersionMismatch");
  // 未选时反扫原 Common 头，不能把孤来源藏在空根之外。
  for (const auto &row : common.state().facts.rows())
    if (row.second.kind == public_supply_inspection_source_kind &&
        (!state || row.second.domain != "interaction" ||
         !state->results.find(row.first)))
      return common_failure("PublicSupplyInspectionOriginalSourceRequired",
                            row.first);
  return common_success();
}
Status validate_native_public_supply_inspection(const WorldBundle &world) {
  auto valid = validate_native_public_supply_inspection_layout(
      *world.catalog, world.common,
      world.public_supply_inspection ? &*world.public_supply_inspection
                                     : nullptr);
  if (!valid.ok || !world.public_supply_inspection)
    return valid;
  WorldCurrentMomentRead clock(world);
  PublicSupplyInspectionReadContext context{
      world.common,
      world.social,
      *world.catalog->interaction,
      *world.catalog->local_economy,
      world.local_economy,
      *world.catalog->public_supply_inspection,
      *world.public_supply_inspection,
      clock};
  return validate_public_supply_inspection_sources(context);
}

namespace {
// 先完成原来源构造与不可变捕获，再调用纯预览或唯一候选writer。
template <class World, class Consume>
Status with_inspection_command(World &world,
                               const CompleteExactWorldReadBindings *actual,
                               const StableId &actor, const StableId &prop,
                               Consume consume) {
  auto valid = validate_native_public_supply_inspection_layout(
      *world.catalog, world.common,
      world.public_supply_inspection ? &*world.public_supply_inspection
                                     : nullptr);
  if (!valid.ok)
    return valid;
  if (!world.catalog->public_supply_inspection ||
      !world.catalog->public_supply_inspection->notice_sources.find(prop))
    return common_success();
  if (actual && !actual->original_core)
    return common_failure("PublicSupplyInspectionActualCoreRequired");
  auto owners = actual ? make_native_exact_result_sources_bound(
                             world, *actual->original_core)
                       : make_native_exact_result_sources(world);
  if (!owners.ok)
    return {false, {}, std::move(owners.error)};
  if (!owners.value)
    return common_failure("PublicSupplyInspectionOriginalOwnerRequired");
  auto retained = owners.value->retained_sources_owned();
  if (!retained.ok)
    return {false, {}, std::move(retained.error)};
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto results =
      capture_exact_result_time_read(world.common, *world.world_timeline,
                                     *owners.value, retained.value.sources(), now.value);
  if (!results.ok)
    return {false, {}, std::move(results.error)};
  WorldCurrentMomentRead clock(world);
  PublicSupplyInspectionReadContext context{
      world.common,
      world.social,
      *world.catalog->interaction,
      *world.catalog->local_economy,
      world.local_economy,
      *world.catalog->public_supply_inspection,
      *world.public_supply_inspection,
      clock};
  auto stamp = results.value->stamp();
  return consume(context, *results.value, stamp, actor, prop);
}
} // namespace
Status preview_native_public_supply_inspection(
    const WorldBundle &world, const CompleteExactWorldReadBindings *actual,
    const StableId &actor, const StableId &prop) {
  return with_inspection_command(
      world, actual, actor, prop,
      [](const auto &context, const auto &results, const auto &stamp,
         const auto &actor, const auto &prop) {
        auto original = preview_public_supply_inspection(context, results,
                                                         stamp, actor, prop);
        return original.ok ? common_success()
                           : Status{false, {}, std::move(original.error)};
      });
}
Status apply_native_public_supply_inspection(
    WorldBundle &world, const CompleteExactWorldReadBindings *actual,
    const StableId &actor, const StableId &prop) {
  if (!world.active_exact_write_context() ||
      !world.common.actor_condition_capture_identity())
    return common_failure("PublicSupplyInspectionActualCaptureRequired", actor);
  return with_inspection_command(
      world, actual, actor, prop,
      [&](const auto &context, const auto &results, const auto &stamp,
          const auto &actor, const auto &prop) {
        return inspect_public_supply_in_candidate(
            context, world.common, world.social,
            *world.public_supply_inspection, results, stamp, actor, prop);
      });
}
} // namespace hanmo::resident
