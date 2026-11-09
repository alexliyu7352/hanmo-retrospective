#include "field_boundary_geometry.hpp"
#include "field_author_data.hpp"
#include "field_contact_moment.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
namespace {
Outcome<FieldBoundaryGeometryRead>
read_geometry(const FieldExactContactReadContext &x,
              const FieldBattleCatalog &catalog, const StableId &battle_id,
              ExactArithmeticBudget budget,
              const SourceRef *historical_contact) {
  auto fail = [&](const char *code) -> Outcome<FieldBoundaryGeometryRead> {
    return {false, {}, common_failure(code, battle_id).error};
  };
  if (!x.clock.bound_to(x.common, x.military, x.field))
    return fail("CrossCandidateFieldBoundaryGeometry");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto found = x.field.sessions.find(battle_id);
  if (found == x.field.sessions.end() || found->second.battle_id != battle_id)
    return fail("FieldBoundaryOriginalSessionRequired");
  const auto &session = found->second;
  if (session.state != FieldBattleState::Council &&
      session.state != FieldBattleState::Deploying &&
      session.state != FieldBattleState::Running &&
      !(historical_contact && session.state == FieldBattleState::Ended))
    return fail("FieldBoundaryActiveSessionRequired");
  if (historical_contact) {
    const auto &source = *historical_contact;
    auto row = x.movement.boundary_contacts.find(source.id);
    if (source.domain != "travel" ||
        source.kind != "movement_boundary_contact" || !row ||
        row->id != source.id ||
        row->original_field_source.domain != "military" ||
        row->original_field_source.kind != "field_battle_session" ||
        row->original_field_source.id != battle_id)
      return fail("FieldBoundaryOriginalTravelSourceRequired");
    auto contact_order = read_actual_common_fact_position(x.common, source);
    if (!contact_order.ok)
      return {false, {}, contact_order.error};
    auto creation_order =
        read_actual_common_fact_position(x.common, row->original_field_source);
    if (!creation_order.ok)
      return {false, {}, creation_order.error};
    if (creation_order.value >= contact_order.value)
      return fail("FieldBoundaryBeforeCreationSource");
    for (const auto &settlement : session.settlements)
      if (settlement.battle_fact_id) {
        auto terminal = read_actual_common_fact_position(
            x.common,
            {"military", "military_battle", *settlement.battle_fact_id});
        if (!terminal.ok)
          return {false, {}, terminal.error};
        if (contact_order.value >= terminal.value)
          return fail("FieldBoundaryAfterTerminalSource");
      }
  }
  const FieldBattleContactRecord *origin = nullptr;
  for (const auto &[key, row] : x.field.contacts)
    if (row.battle_id == battle_id) {
      if (origin || key != row.contact_id)
        return fail("FieldBoundaryOriginalContactAmbiguous");
      origin = &row;
    }
  if (!origin)
    return fail("FieldBoundaryOriginalContactRequired");
  auto original = read_field_initial_contact_actual_moment(
      x.common, x.military, x.field, catalog,
      {"military", "field_battle_contact", origin->contact_id}, x, budget);
  if (!original.ok)
    return {false, {}, original.error};
  auto order =
      compare_world_moments(*original.value.occurred_at, now.value, budget);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return fail("FieldBoundaryBeforeCreation");
  auto generated = regenerate_field_contact_battle(x.travel, catalog,
                                                   origin->source, &x, budget);
  if (!generated.ok)
    return {false, {}, generated.error};
  const auto &g = generated.value;
  if (session.field_hash != g.field_hash ||
      session.anchor.route_id != g.anchor.route_id ||
      session.anchor.point_m.x != g.anchor.point_m.x ||
      session.anchor.point_m.y != g.anchor.point_m.y ||
      session.anchor.tangent.x != g.anchor.tangent.x ||
      session.anchor.tangent.y != g.anchor.tangent.y)
    return fail("FieldBoundaryOriginalGeometryMismatch");
  auto terrain =
      read_field_road_terrain(x.travel, catalog, session.anchor.route_id);
  if (!terrain.ok)
    return {false, {}, terrain.error};
  return {true,
          {&session, origin, &catalog.parameters, now.value,
           &terrain.value->metadata, original.value.occurred_at},
          {}};
}
} // namespace
Outcome<FieldBoundaryGeometryRead> read_field_boundary_geometry(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const StableId &battle, ExactArithmeticBudget budget) {
  return read_geometry(x, catalog, battle, budget, nullptr);
}
Outcome<FieldBoundaryGeometryRead>
read_field_boundary_geometry_at_original_contact(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget budget) {
  const auto *row = x.movement.boundary_contacts.find(source.id);
  if (!row)
    return {
        false,
        {},
        common_failure("FieldBoundaryOriginalTravelSourceRequired", source.id)
            .error};
  return read_geometry(x, catalog, row->original_field_source.id, budget,
                       &source);
}
} // namespace hanmo::resident
