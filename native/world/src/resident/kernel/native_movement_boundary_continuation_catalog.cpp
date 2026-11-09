#include "native_movement_boundary_continuation_catalog.hpp"
#include "native_movement_boundary_catalog.hpp"
#include "world_bundle.hpp"

namespace hanmo::resident {
namespace {
constexpr const char *kind = "movement_boundary_continuation";

// 此前置 selector 已严格配对 Field/1、原 travel 与 boundary_contacts/1。
Outcome<Integer> prerequisites(const CatalogIdentity &identity) {
  return selected_native_movement_boundary_version(identity);
}
} // namespace

Outcome<Integer> selected_native_movement_boundary_continuation_version(
    const CatalogIdentity &identity) {
  auto parent = prerequisites(identity);
  if (!parent.ok)
    return {false, {}, std::move(parent.error)};
  bool child = false, origin = false;
  for (const auto &[name, version] : identity.module_versions) {
    if (name == movement_boundary_continuation_module) {
      if (child || version != "1")
        return {false, {}, common_failure(
            "MovementBoundaryContinuationModuleMismatch", name).error};
      child = true;
    } else if (name == movement_boundary_continuation_derivation_key) {
      if (origin || version != movement_boundary_continuation_derivation)
        return {false, {}, common_failure(
            "MovementBoundaryContinuationDerivationMismatch", name).error};
      origin = true;
    }
  }
  if (child != bool(parent.value) || origin != bool(parent.value))
    return {false, {}, common_failure(
        "MovementBoundaryContinuationDerivedSelectionRequired",
        movement_boundary_continuation_module).error};
  return {true, parent.value ? Integer{1} : Integer{0}, {}};
}

Status append_native_movement_boundary_continuation_module(
    const BoundaryValue &scenario, CatalogIdentity &identity) {
  try {
    if (const auto *dynamic = scenario.find("dynamic_world"))
      for (const auto &[name, value] : boundary::object(*dynamic, "dynamic_world")) {
        (void)value;
        if (name == movement_boundary_continuation_module ||
            name == movement_boundary_continuation_derivation_key)
          return common_failure("MovementBoundaryContinuationIsDerived", name);
      }
    for (const auto &[name, version] : identity.module_versions) {
      (void)version;
      if (name == movement_boundary_continuation_module ||
          name == movement_boundary_continuation_derivation_key)
        return common_failure("DuplicateMovementBoundaryContinuationModule", name);
    }
    auto parent = prerequisites(identity);
    if (!parent.ok)
      return {false, {}, std::move(parent.error)};
    if (parent.value) {
      identity.module_versions.emplace_back(movement_boundary_continuation_module, "1");
      identity.module_versions.emplace_back(movement_boundary_continuation_derivation_key,
                                            movement_boundary_continuation_derivation);
    }
    return common_success();
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Status validate_native_movement_boundary_continuation_headers(
    const CommonState &common, const CatalogIdentity &identity,
    const MovementExactState *movement) {
  auto selected = selected_native_movement_boundary_continuation_version(identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value && !movement)
    return common_failure("MovementBoundaryContinuationOriginalRootRequired");
  if (movement && !selected.value && !movement->boundary_continuations.empty())
    return common_failure("UnselectedMovementBoundaryContinuationSources");
  if (movement)
    for (const auto &[key, row] : movement->boundary_continuations.rows()) {
      const auto *head = common.facts.find(key);
      const auto *prefix = movement->prefixes.find(row.prefix_id);
      const auto *old = movement->units.find(row.closed_unit_ref);
      const auto *next = movement->units.find(row.new_unit_ref);
      const auto *boundary = movement->boundary_contacts.find(row.boundary_contact.id);
      if (key.empty() || row.id != key || !head || head->id != key ||
          head->domain != "travel" || head->kind != kind || row.held ||
          !prefix || prefix->id != row.prefix_id || !old || !next ||
          old->id != row.closed_unit_ref || next->id != row.new_unit_ref ||
          old->id == next->id || !boundary ||
          boundary->id != row.boundary_contact.id ||
          boundary->prefix_id != row.prefix_id ||
          boundary->motion_source.unit_ref != old->id ||
          row.boundary_contact.domain != "travel" ||
          row.boundary_contact.kind != "movement_boundary_contact" ||
          row.field_contact.domain != "military" ||
          row.field_contact.kind != "field_battle_contact" || row.field_contact.id.empty() ||
          row.terminal_segment.domain != "military" ||
          row.terminal_segment.kind != "field_battle_segment" || row.terminal_segment.id.empty())
        return common_failure("MovementBoundaryContinuationPhysicalSourceMismatch", key);
    }
  for (const auto &[key, head] : common.facts.rows())
    if (head.kind == kind &&
        (!selected.value || !movement || head.domain != "travel" ||
         head.id != key || !movement->boundary_continuations.find(key)))
      return common_failure("MovementBoundaryContinuationPhysicalCoverageMismatch", key);
  // 缺行连同Common头一起被删时，原basis/绑定仍不能借空表早返逃过反覆盖。
  if (movement) {
    const auto owned = [&](const SourceRef &source) {
      return source.kind != kind ||
          (selected.value && source.domain == "travel" && !source.id.empty() &&
           movement->boundary_continuations.find(source.id));
    };
    for (const auto &[key, basis] : movement->exertion_bases.rows())
      if (!owned(basis.original_rule_source))
        return common_failure("MovementBoundaryContinuationPhysicalBasisOwnerRequired", key);
    for (const auto &[key, binding] : movement->spatial_bindings.rows())
      if (!owned(binding.binding_source))
        return common_failure("MovementBoundaryContinuationPhysicalBindingOwnerRequired", key);
  }
  return common_success();
}

Status validate_native_movement_boundary_continuation_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("MovementBoundaryContinuationOriginalCatalogRequired");
  auto original = validate_native_movement_boundary_layout(world);
  if (!original.ok)
    return original;
  return validate_native_movement_boundary_continuation_headers(
      world.common.state(), world.catalog->identity,
      world.movement_exact ? &*world.movement_exact : nullptr);
}
} // namespace hanmo::resident
