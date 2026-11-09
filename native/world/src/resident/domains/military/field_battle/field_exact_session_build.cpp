#include "field_exact_session_build.hpp"
#include "field_author_data.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/model/common_rng.hpp"
#include "resident/model/common_string_hash.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
Outcome<CanonicalFraction> exact_integer(Integer value,
                                         ExactArithmeticBudget budget) {
  char digits[32];
  auto written = std::to_chars(digits, digits + sizeof(digits), value);
  if (value < 0 || written.ec != std::errc{})
    return {false, {}, common_failure("FieldExactRoadIntegerRequired").error};
  return normalize_fraction(
      std::string_view(digits, std::size_t(written.ptr - digits)), "1", budget);
}
Outcome<CanonicalFraction>
original_progress_at_window_begin(const FieldExactRoadEndpointRead &endpoint,
                                  const WorldMoment &begin,
                                  ExactArithmeticBudget budget) {
  const auto &basis = *endpoint.original_basis;
  const auto &mapping = endpoint.original_mapping->original_mapping;
  CanonicalFraction cursor;
  if (const auto run = std::get_if<RunningMovementMap>(&mapping)) {
    auto elapsed = elapsed_world_duration(run->world_from, begin, budget);
    if (!elapsed.ok)
      return {false, {}, elapsed.error};
    auto work = multiply_fractions(run->rate, elapsed.value.value, budget);
    if (!work.ok)
      return {false, {}, work.error};
    auto sum = add_fractions(run->cursor_at_from, work.value, budget);
    if (!sum.ok)
      return {false, {}, sum.error};
    cursor = sum.value;
  } else {
    const auto &held = std::get<HeldMovementMap>(mapping);
    auto prior = compare_world_moments(held.effective_at, begin, budget);
    if (!prior.ok)
      return {false, {}, prior.error};
    if (prior.value > 0)
      return {false, {}, common_failure("FieldExactContactBeforeHold").error};
    cursor = held.held_cursor;
  }
  auto one = exact_integer(1, budget);
  if (!one.ok)
    return {false, {}, one.error};
  auto inside = compare_fractions(cursor, one.value, budget);
  if (!inside.ok)
    return {false, {}, inside.error};
  if (inside.value > 0)
    return {
        false, {}, common_failure("FieldExactContactCursorOutsideBasis").error};
  auto direction = compare_fractions(basis.exact_end_progress,
                                     basis.exact_start_progress, budget);
  if (!direction.ok)
    return {false, {}, direction.error};
  const bool forward = direction.value >= 0;
  auto distance = forward
                      ? subtract_fractions(basis.exact_end_progress,
                                           basis.exact_start_progress, budget)
                      : subtract_fractions(basis.exact_start_progress,
                                           basis.exact_end_progress, budget);
  if (!distance.ok)
    return {false, {}, distance.error};
  auto consumed = multiply_fractions(distance.value, cursor, budget);
  if (!consumed.ok)
    return {false, {}, consumed.error};
  return forward
             ? add_fractions(basis.exact_start_progress, consumed.value, budget)
             : subtract_fractions(basis.exact_start_progress, consumed.value,
                                  budget);
}
} // namespace
Outcome<FieldExactSideAssignmentRead> read_field_exact_side_assignment(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldExactSideAssignmentRead> {
    return {false, {}, common_failure(code, source.id).error};
  };
  auto actual = read_field_exact_road_contact_source(
      x, source, catalog.parameters, budget);
  if (!actual.ok)
    return {false, {}, actual.error};
  std::array<const FieldExactRoadEndpointRead *, 2> endpoints{};
  for (const auto &endpoint : actual.value.endpoints)
    endpoints[endpoint.geometry_endpoint ==
                      &actual.value.contact->original_hit.road->first
                  ? 0
                  : 1] = &endpoint;
  if (!endpoints[0] || !endpoints[1] ||
      *endpoints[0]->faction_id == *endpoints[1]->faction_id)
    return fail("FieldExactContactDistinctFactionsRequired");
  std::array<const TravelRoad *, 2> edges{};
  std::array<Integer, 2> directions{};
  std::array<CanonicalFraction, 2> starts;
  for (Integer i = 0; i != 2; ++i) {
    const auto &endpoint = *endpoints[i];
    edges[i] =
        x.travel.roads.find(endpoint.original_basis->original_inputs.edge_id);
    if (!edges[i] || edges[i]->duration_ticks <= 0 ||
        edges[i]->duration_ticks > std::numeric_limits<Integer>::max() / 100)
      return fail("FieldExactContactRoadRequired");
    auto sign = compare_fractions(endpoint.original_basis->exact_end_progress,
                                  endpoint.original_basis->exact_start_progress,
                                  budget);
    if (!sign.ok)
      return {false, {}, sign.error};
    directions[i] = std::holds_alternative<HeldMovementMap>(
                        endpoint.original_mapping->original_mapping)
                        ? 0
                        : sign.value;
    auto begin = original_progress_at_window_begin(
        endpoint, actual.value.contact->window_from, budget);
    if (!begin.ok)
      return {false, {}, begin.error};
    auto length = exact_integer(edges[i]->duration_ticks * 100, budget);
    if (!length.ok)
      return {false, {}, length.error};
    auto normalized = divide_fractions(begin.value, length.value, budget);
    if (!normalized.ok)
      return {false, {}, normalized.error};
    starts[i] = normalized.value;
  }
  if (edges[1]->origin != edges[0]->origin) {
    directions[1] = -directions[1];
    auto one = exact_integer(1, budget);
    if (!one.ok)
      return {false, {}, one.error};
    auto reversed = subtract_fractions(one.value, starts[1], budget);
    if (!reversed.ok)
      return {false, {}, reversed.error};
    starts[1] = reversed.value;
  }
  Integer first_side = directions[0] < 0 ? 1 : 0;
  if (directions[0] == 0)
    first_side = directions[1] > 0 ? 1 : 0;
  if (directions[0] != 0 && directions[0] == directions[1]) {
    auto order = compare_fractions(starts[0], starts[1], budget);
    if (!order.ok)
      return {false, {}, order.error};
    first_side =
        (directions[0] > 0 ? order.value < 0 : order.value > 0) ? 0 : 1;
  }
  const bool reverse_ends = directions[0] < 0 && directions[0] == directions[1];
  FieldExactSideAssignmentRead result;
  for (Integer endpoint = 0; endpoint != 2; ++endpoint) {
    const Integer side = endpoint == 0 ? first_side : 1 - first_side;
    result.side_by_geometry_endpoint[endpoint] = side;
    result.geometry_end_by_side[side] = reverse_ends ? 1 - side : side;
    result.faction_by_side[side] = endpoints[endpoint]->faction_id;
  }
  return {true, result, {}};
}
Outcome<FieldSessionPreparation> prepare_field_exact_road_session(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, const StableId &battle_id,
    const StableId &contact_id, const StableId &player, Integer battle_seed,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldSessionPreparation> {
    return {false, {}, common_failure(code, source.id).error};
  };
  if (battle_id.empty() || contact_id.empty())
    return fail("FieldExactContactAllocatedIdentityRequired");
  auto actual = read_field_exact_road_contact_source(
      x, source, catalog.parameters, budget);
  if (!actual.ok)
    return {false, {}, actual.error};
  auto field = regenerate_field_exact_road_battle(x, source, catalog, budget);
  if (!field.ok)
    return {false, {}, field.error};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value,
                                    actual.value.contact->occurred_at, budget);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return fail("FieldExactContactActualInstallationMomentRequired");
  std::array<const FieldExactRoadEndpointRead *, 2> endpoints{};
  for (const auto &endpoint : actual.value.endpoints)
    endpoints[endpoint.geometry_endpoint ==
                      &actual.value.contact->original_hit.road->first
                  ? 0
                  : 1] = &endpoint;
  if (!endpoints[0] || !endpoints[1] ||
      *endpoints[0]->faction_id == *endpoints[1]->faction_id)
    return fail("FieldExactContactDistinctFactionsRequired");
  auto assignment =
      read_field_exact_side_assignment(x, catalog, source, budget);
  if (!assignment.ok)
    return {false, {}, assignment.error};
  FieldSessionPreparation result;
  result.field = std::move(field.value);
  auto &session = result.session;
  session.battle_id = battle_id;
  session.created_tick = now.value.half_day_tick;
  session.now = actual.value.field_at;
  session.anchor = result.field.anchor;
  session.field_hash = result.field.field_hash;
  for (Integer i = 0; i != 2; ++i) {
    const auto &endpoint = *endpoints[i];
    const Integer side = assignment.value.side_by_geometry_endpoint[i];
    auto formations = read_field_army_formations_at_original_clock(
        x.common, x.military, x.military_catalog, x.field, endpoint.army->id,
        side, player, x.clock, x.formations);
    if (!formations.ok)
      return {false, {}, formations.error};
    if (formations.value.size() >
        std::size_t(catalog.parameters.max_formations_per_side))
      return fail("InvalidFieldFormationCount");
    auto &record = session.sides[side];
    record.side = side;
    record.faction_id = *endpoint.faction_id;
    // Departure authority is not the later contact commander. Read the
    // existing command-history reducer at this actual Common contact cut.
    auto commander = query_military_army_command_source_before_at(
        x.common, x.military, endpoint.army->id, source.id, now.value,
        &x.formations);
    if (!commander.ok)
      return {false, {}, commander.error};
    if (commander.value.commander_actor_id != endpoint.army->commander_actor_id)
      return fail("FieldExactContactCurrentCommanderMismatch");
    record.commander_actor_id = commander.value.commander_actor_id;
    if (record.commander_actor_id == player)
      record.controller = FieldPlayerController{player};
    const Integer end = assignment.value.geometry_end_by_side[side];
    record.deploy_zone = result.field.sides[end].deploy_cells;
    record.exits = result.field.sides[end].exit_cells;
    session.ai[side].side = side;
    session.ai[side].last_eval = session.now;
    for (auto &formation : formations.value) {
      formation.joined_at = session.now;
      const auto id = formation.formation_id;
      if (!session.formations.emplace(id, std::move(formation)).second)
        return fail("FieldFormationRepeated");
    }
  }
  auto battle_hash = godot_string_hash_utf8(battle_id),
       contact_hash = godot_string_hash_utf8(contact_id);
  if (!battle_hash.ok)
    return {false, {}, battle_hash.error};
  if (!contact_hash.ok)
    return {false, {}, contact_hash.error};
  const Integer seed = battle_seed ^ battle_hash.value ^ contact_hash.value;
  session.rng_seed = std::to_string(seed);
  session.rng_state = std::to_string(godot_pcg32_seed_state(seed));
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
