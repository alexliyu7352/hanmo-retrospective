#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "field_exact_contact_source.hpp"
#include "field_author_data.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {
Outcome<FieldExactRoadContactRead> read_field_exact_road_contact_source(
    const FieldExactContactReadContext &x, const SourceRef &source,
    const FieldBattleParameters &parameters, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldExactRoadContactRead> {
    return {false, {}, common_failure(code, source.id).error};
  };
  if (source.domain != "travel" || source.kind != "movement_contact" ||
      !x.clock.bound_to(x.common, x.military, x.field) ||
      !x.formations.bound_to(x.common.state(), x.military))
    return fail("FieldExactContactOriginalOwnerRequired");
  const auto contact = x.movement.contacts.find(source.id);
  if (!contact || contact->id != source.id || !contact->original_hit.road ||
      contact->first_prefix_id == contact->second_prefix_id)
    return fail("FieldExactRoadContactRequired");
  auto head = read_actual_common_fact_position(x.common, source);
  if (!head.ok)
    return {false, {}, head.error};
  auto geometry = validate_exact_contact_geometry_source(
      x.movement, x.travel, *contact, budget, &x.groups);
  if (!geometry.ok)
    return {false, {}, geometry.error};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto future = compare_world_moments(contact->occurred_at, now.value, budget);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return fail("FieldExactContactInFuture");
  FieldExactRoadContactRead result;
  result.contact = contact;
  for (Integer endpoint = 0; endpoint != 2; ++endpoint) {
    const auto &prefix_id =
        endpoint == 0 ? contact->first_prefix_id : contact->second_prefix_id;
    const auto &witness = endpoint == 0 ? contact->first_motion_source
                                        : contact->second_motion_source;
    const auto prefix = x.movement.prefixes.find(prefix_id);
    const auto execution =
        prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
               : nullptr;
    const auto carrier =
        execution ? std::get_if<ArmyCarrierRef>(&execution->carrier) : nullptr;
    if (!witness || !carrier)
      return fail("FieldExactArmyGroupContactRequired");
    const auto journey = x.carriers.journeys.find(execution->journey_id);
    if (!journey || spatial_carrier_key(journey->carrier) !=
                        spatial_carrier_key(execution->carrier))
      return fail("FieldExactContactOriginalJourneyRequired");
    const GroupMovementOpeningSource *opening = nullptr;
    for (const auto &row : x.groups.openings.rows())
      if (row.second.journey_id == journey->journey_id) {
        if (opening || row.first != row.second.id)
          return fail("FieldExactContactOpeningAmbiguous");
        opening = &row.second;
      }
    if (!opening)
      return fail("FieldExactContactOriginalOpeningRequired");
    const SourceRef opening_source{"travel", "group_movement_opening",
                                   opening->id};
    auto opened = read_actual_common_fact_position(x.common, opening_source);
    if (!opened.ok)
      return {false, {}, opened.error};
    if (opened.value >= head.value)
      return fail("FieldExactContactOpeningOrderMismatch");
    auto accepted = read_military_group_order_acceptance(
        x.common, x.military, x.military_catalog, x.carriers, x.movement,
        x.groups, x.conditions, opening_source, now.value, x.author, budget,
        &x.formations, x.original_finite_returns);
    if (!accepted.ok)
      return {false, {}, accepted.error};
    if (!accepted.value || accepted.value->army_id != carrier->army_id)
      return fail("FieldExactContactOriginalOrderMismatch");
    const ArmyRecord *army = nullptr;
    for (const auto &row : x.military.armies)
      if (row.id == carrier->army_id) {
        if (army)
          return fail("FieldExactContactArmyAmbiguous");
        army = &row;
      }
    if (!army)
      return fail("FieldExactContactOriginalArmyRequired");
    auto formed = read_original_military_formation(x.common.state(), x.military,
                                                   *army, contact->occurred_at,
                                                   &x.formations, budget);
    if (!formed.ok)
      return {false, {}, formed.error};
    if (!formed.value.original_terms)
      return fail("FieldExactContactOriginalFormationRequired");
    const MovementUnitCloseSource *closed = nullptr;
    for (const auto &row : x.movement.unit_closes.rows())
      if (row.second.unit_ref == witness->unit_ref) {
        if (closed || row.first != row.second.id ||
            row.second.prefix_id != prefix_id ||
            row.second.kind != MovementCloseKind::contact ||
            !same_source_ref(row.second.actual_cause, source))
          return fail("FieldExactContactOriginalCloseMismatch");
        closed = &row.second;
      }
    if (!closed)
      return fail("FieldExactContactRoadCloseRequired");
    auto original_anchor = read_exact_contact_endpoint_anchor(
        x.movement, *contact, prefix_id, budget);
    if (!original_anchor.ok)
      return {false, {}, original_anchor.error};
    // A retained exact profile must keep its fractional anchor; ordinary
    // original integer geometry keeps the original OnRoute anchor unchanged.
    if (closed->actual_anchor.index() != original_anchor.value.index())
      return fail("FieldExactContactRoadCloseRequired");
    auto anchor_matches = same_spatial_anchor(closed->actual_anchor,
                                              original_anchor.value, budget);
    if (!anchor_matches.ok)
      return {false, {}, anchor_matches.error};
    if (!anchor_matches.value)
      return fail("FieldExactContactRoadCloseMismatch");
    auto close_at = compare_world_moments(closed->occurred_at,
                                          contact->occurred_at, budget);
    if (!close_at.ok)
      return {false, {}, close_at.error};
    if (close_at.value != 0)
      return fail("FieldExactContactCloseMomentMismatch");
    auto close_head = read_actual_common_fact_position(
        x.common, {"travel", "movement_unit_close", closed->id});
    if (!close_head.ok)
      return {false, {}, close_head.error};
    if (head.value >= close_head.value)
      return fail("FieldExactContactCloseOrderMismatch");
    const auto &road = *contact->original_hit.road;
    const auto expected_key = spatial_carrier_key(execution->carrier);
    const RoadContactEndpoint *geo = nullptr;
    for (const auto *candidate : {&road.first, &road.second})
      if (candidate->actor_id == expected_key) {
        if (geo || candidate->trace.actor_id != expected_key)
          return fail("FieldExactContactCarrierGeometryMismatch");
        geo = candidate;
      }
    if (!geo)
      return fail("FieldExactContactCarrierGeometryRequired");
    const auto unit = x.movement.units.find(witness->unit_ref);
    const RoadMovementBasis *basis = nullptr;
    if (unit)
      for (const auto &ref : unit->effort_basis_refs) {
        const auto effort = x.movement.exertion_bases.find(ref.id);
        const auto road_basis =
            effort ? std::get_if<RoadMovementBasis>(&effort->actual_basis)
                   : nullptr;
        if (!road_basis || road_basis->basis_ref != witness->basis_ref)
          continue;
        if (basis)
          return fail("FieldExactContactOriginalBasisAmbiguous");
        basis = road_basis;
      }
    if (!basis)
      return fail("FieldExactContactOriginalRoadBasisRequired");
    result.endpoints[endpoint] = {army,
                                  &formed.value.original_terms->organization_id,
                                  journey,
                                  opening,
                                  accepted.value,
                                  closed,
                                  geo,
                                  basis,
                                  &*witness};
  }
  if (result.endpoints[0].army == result.endpoints[1].army)
    return fail("FieldExactContactDistinctArmiesRequired");
  // The existing Field projection floors to its fixed step. Only actual S0 M
  // is projected; neither arbitrary tech slices nor local seconds write M.
  if (parameters.window_s <= 0 || parameters.step_s <= 0 ||
      parameters.window_s % parameters.step_s)
    return fail("InvalidFieldBattleParameters");
  char digits[32];
  auto text =
      std::to_chars(digits, digits + sizeof(digits), parameters.window_s);
  if (text.ec != std::errc{})
    return fail("FieldContactTimeProjectionOverflow");
  auto scale = normalize_fraction(
      std::string_view(digits, std::size_t(text.ptr - digits)), "1", budget);
  if (!scale.ok)
    return {false, {}, scale.error};
  auto seconds = multiply_fractions(contact->occurred_at.sub_half_day,
                                    scale.value, budget);
  if (!seconds.ok)
    return {false, {}, seconds.error};
  auto whole = floor_fraction(seconds.value, budget);
  if (!whole.ok)
    return {false, {}, whole.error};
  result.field_at = {contact->occurred_at.half_day_tick,
                     whole.value / parameters.step_s * parameters.step_s};
  return {true, result, {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GeneratedBattleField> regenerate_field_exact_road_battle(
    const FieldExactContactReadContext &x, const SourceRef &source,
    const FieldBattleCatalog &catalog, ExactArithmeticBudget budget) {
  auto original = read_field_exact_road_contact_source(
      x, source, catalog.parameters, budget);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &actual = original.value;
  const FieldExactRoadEndpointRead *first = nullptr, *second = nullptr;
  for (const auto &endpoint : actual.endpoints) {
    if (endpoint.geometry_endpoint == &actual.contact->original_hit.road->first)
      first = &endpoint;
    if (endpoint.geometry_endpoint ==
        &actual.contact->original_hit.road->second)
      second = &endpoint;
  }
  if (!first || !second)
    return {
        false,
        {},
        common_failure("FieldExactGeometryEndpointRequired", source.id).error};
  struct RoadValue {
    const StableId *edge;
    CanonicalFraction progress;
  };
  const auto road_value =
      [&](const MovementAnchor &value) -> Outcome<RoadValue> {
    if (const auto fractional = std::get_if<RoadFractionalAnchor>(&value))
      return {true, {&fractional->edge_id, fractional->progress}, {}};
    const auto old = std::get_if<OriginalMovementAnchor>(&value);
    const auto road =
        old ? std::get_if<OnRouteLocation>(&old->location) : nullptr;
    if (!road || road->progress < 0)
      return {
          false,
          {},
          common_failure("FieldExactGeometryRoadRequired", source.id).error};
    char digits[32];
    const auto chars =
        std::to_chars(digits, digits + sizeof(digits), road->progress);
    if (chars.ec != std::errc{})
      return {false, {}, common_failure("FieldExactGeometryOverflow").error};
    auto fraction = normalize_fraction(
        std::string_view(digits, std::size_t(chars.ptr - digits)), "1", budget);
    if (!fraction.ok)
      return {false, {}, fraction.error};
    return {true, {&road->edge_id, std::move(fraction.value)}, {}};
  };
  auto anchor = road_value(first->closed->actual_anchor),
       other_anchor = road_value(second->closed->actual_anchor);
  if (!anchor.ok)
    return {false, {}, anchor.error};
  if (!other_anchor.ok)
    return {false, {}, other_anchor.error};
  const auto edge = x.travel.roads.find(*anchor.value.edge);
  if (!edge || edge->id != first->geometry_endpoint->trace.edge_id ||
      *other_anchor.value.edge != second->geometry_endpoint->trace.edge_id ||
      edge->duration_ticks < 1 ||
      edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
    return {false,
            {},
            common_failure("FieldExactGeometryRoadRequired", source.id).error};
  auto terrain = read_field_road_terrain(x.travel, catalog, edge->id);
  auto other =
      read_field_road_terrain(x.travel, catalog, *other_anchor.value.edge);
  if (!terrain.ok)
    return {false, {}, terrain.error};
  if (!other.ok)
    return {false, {}, other.error};
  if (terrain.value != other.value)
    return {
        false, {}, common_failure("TravelRouteMapMismatch", source.id).error};
  auto geometry = read_field_road_geometry(x.travel, catalog,
                                           terrain.value->metadata, edge->id);
  if (!geometry.ok)
    return {false, {}, geometry.error};
  auto integer = [&](Integer value) -> Outcome<CanonicalFraction> {
    char chars[32];
    auto result = std::to_chars(chars, chars + sizeof(chars), value);
    if (result.ec != std::errc{})
      return {false, {}, common_failure("FieldExactGeometryOverflow").error};
    return normalize_fraction(
        std::string_view(chars, std::size_t(result.ptr - chars)), "1", budget);
  };
  auto duration = integer(edge->duration_ticks * 100);
  auto length = integer(field_road_length_dm(geometry.value));
  if (!duration.ok)
    return {false, {}, duration.error};
  if (!length.ok)
    return {false, {}, length.error};
  auto unit = divide_fractions(anchor.value.progress, duration.value, budget);
  if (!unit.ok)
    return {false, {}, unit.error};
  auto distance = multiply_fractions(unit.value, length.value, budget);
  if (!distance.ok)
    return {false, {}, distance.error};
  auto along = floor_fraction(distance.value, budget);
  if (!along.ok)
    return {false, {}, along.error};
  return generate_battle_field(*terrain.value, geometry.value, along.value,
                               catalog.parameters,
                               catalog.identity.content_version);
}
} // namespace hanmo::resident
