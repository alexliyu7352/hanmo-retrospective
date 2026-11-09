#include "resident/kernel/catalog_section_observation.hpp"
// 明确选型和真实原表物理身份；完整 boundary source 仍由军旅原 reader 核验。
#include "native_movement_boundary_catalog.hpp"
#include "exact_complete_world.hpp"
#include "native_transport_catalog.hpp"
#include "original_catalog_identity.hpp"
#include "resident/domains/interaction/immutable_authored_flags.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "world_bundle.hpp"

namespace hanmo::resident {
namespace {
constexpr const char *module = "movement_boundary_contacts";
constexpr const char *kind = "movement_boundary_contact";

// 只读锁定目录的实际原 transport/Field 选型，不由空 payload 推断依赖。
Status paired(const CatalogIdentity &identity) {
  auto transport = selected_native_transport_versions(identity);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  auto field = selected_field_battle_version(identity);
  if (!field.ok)
    return {false, {}, std::move(field.error)};
  if (!transport.value.travel || field.value != 1)
    return common_failure("MovementBoundaryOriginalProfilesRequired", module,
                          ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace

Outcome<Integer>
selected_native_movement_boundary_version(const CatalogIdentity &identity) {
  Integer selected = 0;
  for (const auto &entry : identity.module_versions) {
    if (entry.first != module)
      continue;
    if (selected || entry.second != "1")
      return {false,
              {},
              common_failure("MovementBoundaryModuleVersionMismatch", module)
                  .error};
    selected = 1;
  }
  if (selected) {
    auto checked = paired(identity);
    if (!checked.ok)
      return {false, {}, std::move(checked.error)};
  }
  return {true, selected, {}};
}

Status append_native_movement_boundary_module(const BoundaryValue &scenario,
                                              CatalogIdentity &identity) {
  try {
    const auto *dynamic = scenario.find("dynamic_world");
    const BoundaryValue *block = nullptr;
    if (dynamic) {
      for (const auto &entry : boundary::object(*dynamic, "dynamic_world")) {
        if (entry.first != module)
          continue;
        if (block)
          return common_failure("DuplicateMovementBoundaryAuthorSelection",
                                module);
        block = &entry.second;
      }
    }
    for (const auto &entry : identity.module_versions)
      if (entry.first == module)
        return common_failure("DuplicateMovementBoundaryModule", module);
    if (!block)
      return common_success();
    boundary::fields(*block, {"version"}, {}, module);
    if (boundary::object(*block, module).size() != 1 ||
        boundary::get_int(*block, "version", module) != 1)
      return common_failure("UnsupportedMovementBoundaryAuthorVersion", module,
                            ErrorKind::unsupported_capability);
    auto checked = paired(identity);
    if (!checked.ok)
      return checked;
    identity.module_versions.emplace_back(module, "1");
    return common_success();
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Status
prepare_native_movement_boundary_flags(const BoundaryValue &original_data,
                                       PreparedCatalog &catalog) {
  auto selected = selected_native_movement_boundary_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value) {
    if (catalog.immutable_interaction_flags)
      return common_failure("UnselectedImmutableInteractionFlags", module);
    return common_success();
  }
  // 这一个显式原native边界分支不安装业务flag
  // writer；未来writer不能借此证明历史不变。
  // capture只收原data和最终完整锁，不接受今日InteractionState作为作者凭据。
  if (catalog.immutable_interaction_flags)
    return common_failure("DuplicateImmutableInteractionFlagsOwner", module);
  auto captured =
      AuthoredInteractionFlags::capture(original_data, catalog.identity);
  if (!captured.ok)
    return {false, {}, std::move(captured.error)};
  catalog.immutable_interaction_flags = std::move(captured.value);
  return common_success();
}

Status
validate_native_movement_boundary_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 146, "PreparedCatalog", "validate_native_movement_boundary_catalog", &catalog);

  auto selected = selected_native_movement_boundary_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value) {
    if (catalog.immutable_interaction_flags)
      return common_failure("UnselectedImmutableInteractionFlags", module);
    return common_success();
  }
  auto transport = validate_native_transport_catalog(catalog);
  if (!transport.ok)
    return transport;
  if (!catalog.common || !catalog.travel || !catalog.military ||
      !catalog.field_battle || !catalog.interaction ||
      !catalog.immutable_interaction_flags || !catalog.exact_time ||
      !exact_complete_selected(*catalog.exact_time))
    return common_failure("MovementBoundaryOriginalCatalogsRequired", module,
                          ErrorKind::unsupported_capability);
  const auto &seed = *catalog.immutable_interaction_flags;
  if (!same_original_catalog_identity(seed.identity(), catalog.identity) ||
      !same_original_catalog_identity(catalog.field_battle->identity,
                                      catalog.identity))
    return common_failure("MovementBoundaryAuthoredFlagsLockMismatch", module);
  const auto &ids = catalog.interaction->flags;
  if (ids.size() != seed.rows().size())
    return common_failure("MovementBoundaryAuthoredFlagsCatalogMismatch",
                          module);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j)
      if (ids[i] == ids[j])
        return common_failure("DuplicateMovementBoundaryFlagId", ids[i]);
    bool found = false;
    for (const auto &row : seed.rows())
      if (row.id == ids[i]) {
        if (found)
          return common_failure("DuplicateMovementBoundaryFlagId", ids[i]);
        found = true;
      }
    if (!found)
      return common_failure("MovementBoundaryAuthoredFlagRequired", ids[i]);
  }
  return common_success();
}

Status
validate_native_movement_boundary_headers(const CommonState &common,
                                          const CatalogIdentity &identity,
                                          const MovementExactState &movement) {
  auto selected = selected_native_movement_boundary_version(identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value && !movement.boundary_contacts.empty())
    return common_failure("UnselectedMovementBoundarySources", module);
  for (const auto &entry : movement.boundary_contacts.rows()) {
    const auto &row = entry.second;
    const auto *head = common.facts.find(entry.first);
    const auto *prefix = movement.prefixes.find(row.prefix_id);
    const auto *unit = movement.units.find(row.motion_source.unit_ref);
    if (entry.first.empty() || row.id != entry.first || !head ||
        head->id != entry.first || head->domain != "travel" ||
        head->kind != kind || !prefix || !unit ||
        unit->id != row.motion_source.unit_ref || prefix->id != row.prefix_id ||
        row.original_field_source.domain != "military" ||
        row.original_field_source.kind != "field_battle_session" ||
        row.original_field_source.id.empty() || row.field_hash.empty())
      return common_failure("MovementBoundaryPhysicalSourceMismatch",
                            entry.first);
  }
  for (const auto &entry : common.facts.rows()) {
    const auto &head = entry.second;
    if (head.domain != "travel" || head.kind != kind)
      continue;
    if (!selected.value || head.id != entry.first ||
        !movement.boundary_contacts.find(entry.first))
      return common_failure("MovementBoundaryPhysicalCoverageMismatch",
                            entry.first);
  }
  return common_success();
}

Status validate_native_movement_boundary_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("MovementBoundaryWorldCatalogRequired");
  auto checked = validate_native_movement_boundary_catalog(*world.catalog);
  if (!checked.ok)
    return checked;
  auto selected =
      selected_native_movement_boundary_version(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value) {
    checked = validate_immutable_interaction_flags(
        *world.catalog->immutable_interaction_flags, world.catalog->identity,
        world.interaction);
    if (!checked.ok)
      return checked;
  }
  if (!world.movement_exact) {
    if (selected.value)
      return common_failure("MovementBoundaryOriginalMovementRootRequired");
    for (const auto &entry : world.common.state().facts.rows())
      if (entry.second.domain == "travel" && entry.second.kind == kind)
        return common_failure("UnselectedMovementBoundarySources", entry.first);
    return common_success();
  }
  if (selected.value && !world.field_battle)
    return common_failure("MovementBoundaryOriginalFieldRootRequired");
  return validate_native_movement_boundary_headers(
      world.common.state(), world.catalog->identity, *world.movement_exact);
}

Outcome<MovementRecordCodecSelection>
native_movement_boundary_codec_selection(const PreparedCatalog &catalog) {
  auto checked = validate_native_movement_boundary_catalog(catalog);
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  auto selected = selected_native_movement_boundary_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 1)
    return {
        false, {}, common_failure("MovementBoundaryNotSelected", module).error};
  auto transport = native_transport_physical_selection(catalog);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  const auto &original = transport.value;
  return {true,
          {original.travel_version, original.group_transport_version,
           original.common_resources, original.carrier_location_enabled,
           original.typed_reservations, original.domain_activity_reservations},
          {}};
}
} // namespace hanmo::resident
