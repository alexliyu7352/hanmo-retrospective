#include "field_army_exit_group_adoption.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "field_army_exit_stable_capture.hpp"
#include "group_movement.hpp"
#include "movement_boundary_apply.hpp"
#include "movement_boundary_resume.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/domains/local_economy/household_transport_read.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/field_battle/field_boundary_continuation_source.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ready.hpp"
#include "resident/domains/military/field_battle/field_army_exit_return.hpp"
#include "field_battle_exit_placement.hpp"
#include "spatial_location_capture.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/military/field_battle/field_winner_continuation.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "transport_authority_codec.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
void validate_original_group_unit_terms(const CommonStore&,const MovementExactState&,const GroupMovementExecutionState&,const GroupMovementUnitTerms&,ExactArithmeticBudget);
bool same_supply_mapping(const ActivityWorldMap&,const ActivityWorldMap&,ExactArithmeticBudget);
const TransportExpenseAuthority &
commercial_funding(const GroupMovementJourney &journey) {
  const auto *funding =
      read_commercial_group_funding(journey.expense_authority);
  if (!funding)
    fail("MilitaryMarchCannotUseCommercialPayment", journey.journey_id);
  return *funding;
}
Integer checked_sum(Integer a, Integer c) {
  if (a < 0 || c < 0 || a > std::numeric_limits<Integer>::max() - c)
    fail("TransportCostOverflow");
  return a + c;
}
terrain::Mode mode(const StableId &value) {
  if (value == "foot")
    return terrain::Mode::foot;
  if (value == "army")
    return terrain::Mode::army;
  if (value == "boat")
    return terrain::Mode::boat;
  fail("InvalidTravelMode", value);
}
struct LiveGroupComposition {
  StableId subject_kind, transport_unit_id, position_ref;
  SourceRef registration_source_ref;
  CheckpointVector<const StableId *> named_participants;
};
LiveGroupComposition live_group(const CommonStore &common,
                                const SpatialCarrierState &carriers,
                                const MovementExactState &movement,
                                const GroupMovementExecutionState &executions,
                                const GroupMovementContext &context,
                                const GroupMovementJourney &journey) {
  if (const auto *ref =
          std::get_if<TransportUnitCarrierRef>(&journey.carrier)) {
    const auto *unit = carriers.transport_units.find(
        ref->transport_unit_ref.transport_unit_id);
    if (!unit || unit->state != TransportUnitLifecycle::active ||
        unit->active_journey_id !=
            std::optional<StableId>{journey.journey_id} ||
        unit->composition_revision != journey.composition_revision)
      fail("GroupJourneyCompositionMismatch", journey.journey_id);
    return {"transport_unit", unit->transport_unit_id, unit->position_ref,
            unit->registration_source_ref,
            take(read_active_group_named_members(
                common, movement, executions, journey, unit->named_participants,
                context.member_releases))};
  }
  const auto *army = std::get_if<ArmyCarrierRef>(&journey.carrier);
  if (!army)
    fail("FleetGroupExecutionOriginalBindingRequired", journey.journey_id);
  const GroupMovementOpeningSource *opening = nullptr;
  for (const auto &row : executions.openings.rows())
    if (row.second.journey_id == journey.journey_id) {
      if (opening)
        fail("DuplicateMilitaryGroupOpening", journey.journey_id);
      opening = &row.second;
    }
  const auto *origin =
      opening ? std::get_if<ArmyCarrierRef>(&opening->carrier_origin) : nullptr;
  const auto *binding =
      carriers.bindings.find(spatial_carrier_key(journey.carrier));
  const auto *direct =
      binding ? std::get_if<DirectGroupMovement>(&binding->execution_owner)
              : nullptr;
  if (!origin || origin->army_id != army->army_id || !opening->military_order ||
      take(read_group_composition_revision_before(common, executions, journey.journey_id)) != journey.composition_revision ||
      !binding || binding->position_ref != journey.position_ref || !direct ||
      direct->group_movement_id != journey.journey_id)
    fail("MilitaryGroupJourneyBindingMismatch", journey.journey_id);
  return {"army",
          army->army_id,
          journey.position_ref,
          {"travel", "group_movement_opening", opening->id},
          take(read_active_group_named_members(
              common, movement, executions, journey,
              opening->named_participants, context.member_releases))};
}
void release_active_group(SpatialCarrierState &carriers,
                          const GroupMovementJourney &journey) {
  if (const auto *ref =
          std::get_if<TransportUnitCarrierRef>(&journey.carrier)) {
    auto *unit = carriers.transport_units.find(
        ref->transport_unit_ref.transport_unit_id);
    if (!unit ||
        unit->active_journey_id != std::optional<StableId>{journey.journey_id})
      fail("GroupJourneyCompositionMismatch", journey.journey_id);
    unit->active_journey_id.reset();
  }
}
struct GroupRouteInput {
  TravelDestination destination;
  std::vector<StableId> via;
  StableId payer_wallet_id;
};
Integer fare(const TravelCatalog &catalog, const MovementBasis &basis,
             const GroupMotionProfile &profile) {
  Integer base = 0;
  if (const auto *terrain = std::get_if<TerrainCostBasis>(&basis)) {
    const auto *map = catalog.maps.find(terrain->original_inputs.map_id);
    if (!map)
      fail("GroupMovementMapRequired");
    base = map->money_per_tick;
  } else {
    const auto *road = catalog.roads.find(
        std::get<RoadMovementBasis>(basis).original_inputs.edge_id);
    if (!road)
      fail("GroupMovementRoadRequired");
    base = road->cost_per_tick;
  }
  return checked_sum(base, profile.additional_cost_per_unit);
}
Integer fatigue(const TravelCatalog &catalog, const MovementBasis &basis) {
  if (const auto *terrain = std::get_if<TerrainCostBasis>(&basis))
    return terrain->original_inputs.fatigue_per_tick;
  const auto *road = catalog.roads.find(
      std::get<RoadMovementBasis>(basis).original_inputs.edge_id);
  if (!road)
    fail("GroupMovementRoadRequired");
  return road->fatigue_per_tick;
}
Integer rate_budget(const terrain::TerrainMap &map,
                    const GroupMotionProfile &profile) {
  auto selected = map.budgets.find(mode(profile.original_profile.mode));
  if (selected == map.budgets.end() || selected->second < 1 ||
      profile.original_profile.load_weight < 0)
    fail("GroupTravelBudgetSourceRequired");
  if (profile.original_profile.mode == "boat") {
    if (profile.original_profile.naval_budget < 1)
      fail("GroupVesselTravelSourceRequired");
    return std::min(selected->second, profile.original_profile.naval_budget);
  }
  const __int128 result =
      static_cast<__int128>(selected->second) * 100 /
      (100 + static_cast<__int128>(profile.original_profile.load_weight) * 4);
  return std::max<Integer>(1, static_cast<Integer>(result));
}
PointI target(const TravelCatalog &catalog, const TravelDestination &dest) {
  if (const auto *id = std::get_if<StableId>(&dest)) {
    const auto *city = catalog.settlements.find(*id);
    if (!city)
      fail("InvalidDestination", *id);
    return city->map_xy;
  }
  return std::get<PointI>(dest);
}
MovementAnchor terrain_origin(const TravelCatalog &catalog,
                              const MovementAnchor &actual,
                              const SourceRef &source) {
  if (const auto *original = std::get_if<OriginalMovementAnchor>(&actual)) {
    if (const auto *city =
            std::get_if<SettlementLocation>(&original->location)) {
      const auto *place = catalog.settlements.find(city->settlement_id);
      if (!place || place->entry_place != city->place_id)
        fail("NotAtCityExit");
      return OriginalMovementAnchor{WildernessLocation{catalog.world_map_id,
                                                       city->settlement_id,
                                                       place->map_xy},
                                    source};
    }
    if (std::holds_alternative<WildernessLocation>(original->location))
      return OriginalMovementAnchor{original->location, source};
  }
  if (std::holds_alternative<TerrainContinuousAnchor>(actual))
    return actual;
  fail("GroupTerrainOriginRequired");
}
// 从原锚点规划群体地形路线，保留途经点、地图和空路线拒绝条件。
TerrainTravelParams
plan_terrain(const TravelCatalog &catalog, const TravelServices &services,
             const GroupMotionProfile &profile, const GroupRouteInput &request,
             const MovementAnchor &origin, const StableId &journey,
             ExactArithmeticBudget budget) {
  const auto *map = catalog.maps.find(catalog.world_map_id);
  if (!map)
    fail("UnknownWorldMap");
  if (request.via.size() > 8)
    fail("InvalidWaypoints");
  TerrainTravelParams p;
  p.plan_id = journey;
  p.map_id = map->id;
  p.mode = profile.original_profile.mode;
  p.payer_wallet_id = request.payer_wallet_id;
  p.budget = rate_budget(*map, profile);
  p.money_per_tick =
      checked_sum(map->money_per_tick, profile.additional_cost_per_unit);
  p.fatigue_per_tick = map->fatigue_per_tick;
  p.destination = request.destination;
  std::vector<PointI> stops;
  std::set<StableId> seen;
  for (const auto &id : request.via) {
    const auto *city = catalog.settlements.find(id);
    if (!city || !seen.insert(id).second ||
        (std::holds_alternative<StableId>(request.destination) &&
         id == std::get<StableId>(request.destination)))
      fail("InvalidWaypoints");
    stops.push_back(city->map_xy);
  }
  stops.push_back(target(catalog, request.destination));
  std::optional<PointI> integer_start;
  std::optional<terrain::PointD> continuous_start;
  if (const auto *a = std::get_if<OriginalMovementAnchor>(&origin)) {
    const auto *w = std::get_if<WildernessLocation>(&a->location);
    if (!w || w->map_id != map->id)
      fail("GroupTerrainOriginRequired");
    integer_start = w->point;
    p.origin = w->origin;
  } else {
    const auto &continuous = std::get<TerrainContinuousAnchor>(origin);
    if (continuous.map_id != map->id)
      fail("GroupTerrainMapMismatch");
    continuous_start =
        terrain::PointD{take(decode_movement_binary64(continuous.x_bits)),
                        take(decode_movement_binary64(continuous.y_bits))};
    p.origin = continuous.origin;
  }
  for (const auto &stop : stops) {
    if (integer_start) {
      auto route = take(
          catalog.route_cache
              ? catalog.route_cache->plan(*map, *integer_start, stop,
                                          mode(p.mode),
                                          services.terrain_flags())
              : terrain::plan_prepared(*map, *integer_start, stop, mode(p.mode),
                                       services.terrain_flags()));
      p.points.insert(p.points.end(), route.points.begin() + 1,
                      route.points.end());
    } else {
      auto route = take(terrain::plan_continuous_prepared(
          *map, *continuous_start, stop, mode(p.mode), services.terrain_flags(),
          budget));
      p.points.insert(p.points.end(), route.points.begin(), route.points.end());
    }
    integer_start = stop;
    continuous_start.reset();
  }
  if (p.points.empty())
    fail("AlreadyThere");
  return p;
}
MovementBasis
initial_basis(CommonStore &common, const GroupMovementContext &context,
              const GroupMotionProfile &profile, const GroupRouteInput &request,
              const SpatialCarrierRef &carrier, const MovementAnchor &actual,
              const StableId &journey, const StableId &unit,
              const SourceRef &source, ExactArithmeticBudget budget) {
  auto basis = take(common.next_id("movement_basis"));
  if (!context.travel_catalog.world_map_id.empty()) {
    auto origin = terrain_origin(context.travel_catalog, actual, source);
    auto p = plan_terrain(context.travel_catalog, context.travel_services,
                          profile, request, origin, journey, budget);
    if (const auto *a = std::get_if<OriginalMovementAnchor>(&origin)) {
      auto motion = take(terrain_geometry_motion(
          context.travel_catalog, context.travel_services,
          std::get<WildernessLocation>(a->location).point, p, true,
          spatial_carrier_key(carrier), journey));
      return take(make_original_terrain_cost_basis(
          motion, basis, "terrain_original/1", unit, source, budget));
    }
    return take(make_continued_terrain_cost_basis(
        context.travel_catalog, context.travel_services, origin, p,
        integer(p.budget, budget), basis, unit, source, budget));
  }
  const auto *a = std::get_if<OriginalMovementAnchor>(&actual);
  const auto *origin =
      a ? std::get_if<SettlementLocation>(&a->location) : nullptr;
  if (!origin)
    fail("GroupRoadOriginRequired");
  auto quote =
      take(plan_road_route(context.travel_catalog, origin->settlement_id,
                           request.destination, common.tick(), request.via));
  if (!quote.road || quote.road->path.empty())
    fail("AlreadyThere");
  RoadTravelParams p;
  p.destination = request.destination;
  p.via = request.via;
  p.path = quote.road->path;
  p.payer_wallet_id = request.payer_wallet_id;
  const auto *road = context.travel_catalog.roads.find(p.path.front());
  if (!road || road->duration_ticks < 1 ||
      road->duration_ticks > std::numeric_limits<Integer>::max() / 100)
    fail("GroupMovementRoadRequired");
  p.reverse_first = road->destination == origin->settlement_id;
  const Integer end = std::min<Integer>(100, road->duration_ticks * 100);
  RoadTravelMotion trace{"", journey, road->id, origin->settlement_id,
                         0,  end,     true,     p};
  return take(make_original_road_movement_basis(trace, basis, "road_original/1",
                                                unit, source, budget));
}
const StableId &participant_id(const StableId &id) { return id; }
const StableId &participant_id(const StableId *id) { return *id; }
template <class Participants>
void install_unit(CommonStore &common, MovementExactState &movement,
                  GroupMovementExecutionState &executions,
                  const GroupMovementContext &context,
                  MovementPrefixState &prefix, MovementUnitState unit,
                  const GroupMotionProfile &profile,
                  const Participants &participants, const WorldMoment &at,
                  const SourceRef &source,
                  CheckpointMemoryCharge *retained = nullptr) {
  // The new exact boundary continuation owns these first copies in the same
  // candidate. Legacy callers retain their original allocation path.
  if (retained) {


    const auto &execution = std::get<GroupMovementExecution>(prefix.execution);
      // exertion execution
    (void)0;  // unit terms journey
    (void)0;               // exertion unit
    (void)0;               // unit terms unit
    (void)0; // actual ':effort' identity
    (void)0;


    (*retained).grow(0);
  }
  TravelExertionBasis effort{unit.id + ":effort",
                             unit.id,
                             prefix.execution,
                             std::nullopt,
                             at,
                             {},
                             fatigue(context.travel_catalog, prefix.basis),
                             source,
                             prefix.basis,
                             prefix.cursor};
  effort.actual_participants.reserve(participants.size());
  for (const auto &person : participants)
    effort.actual_participants.push_back(participant_id(person));
  unit.effort_basis_refs.push_back(
      {"travel", "travel_exertion_basis", effort.id});
  GroupMovementUnitTerms terms{
      unit.id, std::get<GroupMovementExecution>(prefix.execution).journey_id,
      profile, fare(context.travel_catalog, prefix.basis, profile), source};
  {
    const auto key = terms.unit_ref;
    executions.unit_terms.set(key, std::move(terms));
  }
  {
    const auto key = effort.id;
    movement.exertion_bases.set(key, std::move(effort));
  }
  take(common.append_fact({unit.id, "movement_unit", "travel"}));
  take(common.append_fact(
      {unit.id + ":effort", "travel_exertion_basis", "travel"}));
  {
    const auto key = unit.id;
    movement.units.set(key, std::move(unit));
  }
}
void project_group(CommonStore &common, SpatialCarrierState &carriers,
                   MovementExactState &movement,
                   const GroupMovementExecutionState &executions,
                   const GroupMovementContext &context,
                   const GroupMovementJourney &journey,
                   const MovementAnchor &anchor, const SourceRef &source,
                   std::vector<BoundSpatialLocationWrite> &,
                   ExactArithmeticBudget budget) {
  const auto &unit =
      live_group(common, carriers, movement, executions, context, journey);
  auto *position = carriers.positions.find(unit.position_ref);
  if (!position)
    fail("CarrierPositionRequired", unit.position_ref);
  position->location = take(project_movement_location(anchor, budget));
  position->source_ref = source;
  if (position->revision == std::numeric_limits<Integer>::max())
    fail("CarrierPositionRevisionOverflow", unit.position_ref);
  ++position->revision;
  for (const auto *member : unit.named_participants) {
    const auto &id = *member;
    const auto *actor = common.actor(id);
    const auto *location =
        actor ? std::get_if<CarriedLocation>(&actor->location) : nullptr;
    const auto *binding = movement.spatial_bindings.find("actor:" + id);
    const auto *carried =
        binding ? std::get_if<CarriedMovementWriter>(&binding->writer)
                : nullptr;
    const auto *in_army =
        actor ? std::get_if<InArmyLocation>(&actor->location) : nullptr;
    const bool projected =
        unit.subject_kind == "army"
            ? in_army && in_army->army_id == unit.transport_unit_id
            : location &&
                  location->carrier_kind ==
                      CarriedLocation::CarrierKind::transport_unit &&
                  location->carrier_id == unit.transport_unit_id;
    if (!projected || !carried ||
        spatial_carrier_key(carried->carrier_ref) !=
            spatial_carrier_key(journey.carrier))
      fail("TransportParticipantCarriedBindingMismatch", id);
  }
}
} // namespace
Outcome<Location>
read_group_route_destination_location(const TravelCatalog &catalog,
                                      const TravelDestination &destination,
                                      const MovementAnchor &actual_origin) {
  return guard<Location>([&]() -> Location {
    if (const auto *id = std::get_if<StableId>(&destination)) {
      const auto *city = catalog.settlements.find(*id);
      if (!city || city->entry_place.empty())
        fail("GroupArrivalDestinationRequired", *id);
      return SettlementLocation{*id, city->entry_place};
    }
    if (catalog.world_map_id.empty() ||
        !catalog.maps.find(catalog.world_map_id))
      fail("GroupTerrainMapRequired");
    StableId origin;
    if (const auto *actual =
            std::get_if<OriginalMovementAnchor>(&actual_origin)) {
      if (const auto *site = std::get_if<SettlementLocation>(&actual->location))
        origin = site->settlement_id;
      else if (const auto *wilderness =
                   std::get_if<WildernessLocation>(&actual->location)) {
        if (wilderness->map_id != catalog.world_map_id)
          fail("GroupTerrainMapMismatch");
        origin = wilderness->origin;
      } else
        fail("GroupTerrainOriginRequired");
    } else if (const auto *actual =
                   std::get_if<TerrainContinuousAnchor>(&actual_origin)) {
      if (actual->map_id != catalog.world_map_id)
        fail("GroupTerrainMapMismatch");
      origin = actual->origin;
    } else
      fail("GroupTerrainOriginRequired");
    return WildernessLocation{catalog.world_map_id, origin,
                              std::get<PointI>(destination)};
  });
}
NativeGroupTransportExpenseRead::NativeGroupTransportExpenseRead(
    const MovementExactState &movement, const SpatialCarrierState &carriers,
    const GroupMovementExecutionState &executions, const TravelCatalog &catalog,
    const SpatialCarrierSources &spatial)
    : movement_(movement), carriers_(carriers), executions_(executions),
      catalog_(catalog), spatial_sources_(spatial) {}
Outcome<WorldMoment>
NativeGroupTransportExpenseRead::current_transport_expense_moment() const {
  return spatial_sources_.current_moment();
}
Status NativeGroupTransportExpenseRead::validate_transport_expense_authority(
    const SourceRef &ref, Integer schema,
    const TransportExpenseAuthority &authority) const {
  return status([&] {
    const auto actual = take(read_actual_transport_expense(ref, schema));
    const auto *journey = carriers_.journeys.find(actual.journey_id);
    if (!journey || !same_transport_expense_authority(
                        journey->expense_authority, authority))
      fail("GroupExpenseAcceptedAuthorityMismatch", ref.id);
  });
}
Outcome<GroupExpenseBasisRead> read_verified_group_expense_basis(
    const MovementExactState &movement, const SpatialCarrierState &carriers,
    const GroupMovementExecutionState &executions, const TravelCatalog &catalog,
    const SourceRef &ref, Integer schema) {
  return guard<GroupExpenseBasisRead>([&] {
    const auto *source = ref.domain == "travel" &&
                                 ref.kind == "group_transport_expense" &&
                                 schema == 1
                             ? executions.expenses.find(ref.id)
                             : nullptr;
    const auto *terms =
        source ? executions.unit_terms.find(source->unit_ref) : nullptr;
    const auto *prefix =
        source ? movement.prefixes.find(source->prefix_ref) : nullptr;
    const auto *unit = source ? movement.units.find(source->unit_ref) : nullptr;
    const auto *effort =
        unit && unit->effort_basis_refs.size() > 0
            ? movement.exertion_bases.find(unit->effort_basis_refs.front().id)
            : nullptr;
    const auto *journey =
        source ? carriers.journeys.find(source->journey_id) : nullptr;
    if (!source || !terms || !prefix || !unit || !effort || !journey ||
        terms->journey_id != journey->journey_id ||
        journey->movement_prefix_ref != prefix->id ||
        execution_key(unit->execution) != execution_key(effort->execution) ||
        fare(catalog, effort->actual_basis, terms->motion_profile) !=
            terms->fare_amount ||
        !source_valid(terms->motion_profile.actual_rule_source) ||
        terms->motion_profile.source_schema < 1)
      fail("GroupExpenseSourceRequired", ref.id);
    if (const auto *positive =
            std::get_if<GroupExpensePrefixCause>(&source->actual_cause)) {
      if (take(compare_world_moments(positive->first_actual_span.from,
                                     positive->first_actual_span.to)) >= 0 ||
          !same_moment(positive->first_actual_span.from, source->occurred_at))
        fail("GroupExpenseFirstPrefixMismatch", ref.id);
      if (!unit->activity_spans.empty()) {
        if (!same_moment(unit->activity_spans.front().span.from,
                         positive->first_actual_span.from) ||
            take(compare_world_moments(positive->first_actual_span.to,
                                       unit->activity_spans.front().span.to)) >
                0)
          fail("GroupExpenseActivityMismatch", ref.id);
      } else {
        if (compare(unit->active_elapsed.value, {}) != 0 ||
            prefix->unit_ref != unit->id)
          fail("GroupExpenseMissingActualActivity", ref.id);
        take(prepare_movement_prefix(movement, prefix->id,
                                     positive->first_actual_span));
      }
    } else {
      const auto &contact =
          std::get<GroupExpenseContactCause>(source->actual_cause);
      if (contact.actual_contact.domain == "travel" &&
          contact.actual_contact.kind == "movement_boundary_contact") {
        const auto *hit =
            movement.boundary_contacts.find(contact.actual_contact.id);
        if (!hit || hit->prefix_id != prefix->id ||
            hit->motion_source.unit_ref != unit->id ||
            !std::holds_alternative<RunningMovementMap>(
                hit->motion_source.original_mapping) ||
            compare(hit->entry_fraction, {}) != 0 ||
            !same_moment(hit->occurred_at, source->occurred_at))
          fail("GroupExpenseBoundaryContactRequired", ref.id);
      } else {
        const auto *hit =
            contact.actual_contact.domain == "travel" &&
                    contact.actual_contact.kind == "movement_contact"
                ? movement.contacts.find(contact.actual_contact.id)
                : nullptr;
        if (!hit || compare(take(read_exact_contact_fraction(*hit)), {}) != 0 ||
            !same_moment(hit->occurred_at, source->occurred_at) ||
            (hit->first_prefix_id != prefix->id &&
             hit->second_prefix_id != prefix->id))
          fail("GroupExpenseContactRequired", ref.id);
      }
    }
    return GroupExpenseBasisRead{source, terms};
  });
}
Outcome<ActualTransportExpense>
NativeGroupTransportExpenseRead::read_actual_transport_expense(
    const SourceRef &ref, Integer schema) const {
  return guard<ActualTransportExpense>([&] {
    const auto verified = take(read_verified_group_expense_basis(
        movement_, carriers_, executions_, catalog_, ref, schema));
    const auto &source = *verified.source;
    const auto &terms = *verified.terms;
    const auto *journey = carriers_.journeys.find(source.journey_id);
    if (!journey || !read_commercial_group_funding(journey->expense_authority))
      fail("MilitaryMarchCannotUseCommercialPayment", ref.id);
    return ActualTransportExpense{ref,
                                  1,
                                  source.journey_id,
                                  source.unit_ref,
                                  terms.motion_profile.actual_rule_source.id,
                                  terms.motion_profile.fare_payee_wallet_id,
                                  terms.motion_profile.fare_destination_id,
                                  TransportExpensePurpose::base_fare,
                                  terms.fare_amount,
                                  source.occurred_at};
  });
}
Outcome<StableId> start_group_journey_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StartGroupJourney &request,
    const WorldMoment &at,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    const auto *read = carriers.transport_units.find(
        request.transport_unit_ref.transport_unit_id);
    if (!read ||
        read->subject.kind != request.transport_unit_ref.subject.kind ||
        read->subject.id != request.transport_unit_ref.subject.id ||
        read->state != TransportUnitLifecycle::active ||
        read->active_journey_id ||
        read->composition_revision != request.expected_composition_revision)
      fail("GroupJourneyUnitUnavailable");
    take(encode_world_moment(at, budget));
    if (!same_moment(at, take(context.carriers.current_moment()), budget))
      fail("GroupJourneyMomentMismatch");
    const auto unit = *read;
    const SpatialCarrierRef carrier = TransportUnitCarrierRef{
        request.transport_unit_ref, unit.composition_revision};
    auto manifest = take(read_transport_manifest(
        common, carriers, movement, context.carriers, carrier, budget));
    take(quote_cargo_capacity(common, carriers, movement, context.carriers,
                              {carrier,
                               {},
                               CargoCapacityOperation::retain,
                               manifest.manifest_revision},
                              budget));
    take(context.actual_sources.validate_start_authority(request, unit,
                                                         manifest));
    take(context.carriers.verify_initiator(
        request.initiator, request.transport_unit_ref, request.sponsor,
        unit.registration_source_ref));
    auto profile = take(context.actual_sources.read_motion_profile(
        manifest, request.route_profile, at));
    take(context.actual_sources.validate_motion_profile(manifest, profile, at));
    if (profile.original_profile.payer_wallet_id != request.payer_wallet_id ||
        !source_valid(profile.actual_rule_source) ||
        profile.source_schema < 1 || profile.additional_cost_per_unit < 0 ||
        profile.fare_payee_wallet_id.empty() ||
        profile.fare_destination_id.empty())
      fail("GroupMovementCostSourceRequired");
    for (const auto &id : unit.named_participants) {
      const auto *actor = common.actor(id);
      if (!actor || !actor->alive || !actor->available ||
          !actor->action_id.empty())
        fail("TransportParticipantBusy", id);
    }
    const auto journey_id = take(common.next_id("group_journey")),
               prefix_id = take(common.next_id("movement_prefix")),
               unit_id = take(common.next_id("movement_unit")),
               opening_id = take(common.next_id("group_movement_opening"));
    SourceRef source{"travel", "group_movement_opening", opening_id};
    GroupMovementJourney journey;
    journey.journey_id = journey_id;
    journey.carrier = carrier;
    journey.position_ref = unit.position_ref;
    journey.composition_revision = unit.composition_revision;
    journey.initiator = request.initiator;
    journey.sponsor = request.sponsor;
    journey.payer_wallet_id = request.payer_wallet_id;
    journey.expense_authority = request.expense_authority;
    journey.shipment_refs = request.shipment_refs;
    journey.route_profile = request.route_profile;
    journey.started_at = at;
    journey.last_step_at = at;
    journey.movement_prefix_ref = prefix_id;
    MovementPrefixState prefix;
    prefix.id = prefix_id;
    prefix.execution = GroupMovementExecution{journey_id, carrier, 0};
    journey.movement_execution =
        std::get<GroupMovementExecution>(prefix.execution);
    prefix.basis = initial_basis(
        common, context, profile,
        {request.destination, request.via, request.payer_wallet_id}, carrier,
        manifest.actual_anchor, journey_id, unit_id, source, budget);
    if (std::holds_alternative<HouseholdPolicyGroupInitiator>(
            request.initiator)) {
      if (!context.household_transport)
        fail("HouseholdTransportOriginalSourcesRequired");
      const auto destination = take(read_group_route_destination_location(
          context.travel_catalog, request.destination, manifest.actual_anchor));
      take(validate_household_transport_journey_request(
          *context.household_transport, request, unit, manifest,
          OriginalMovementAnchor{destination, profile.actual_rule_source}));
    }
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = at;
    MovementUnitState moving_unit;
    moving_unit.id = unit_id;
    moving_unit.execution = prefix.execution;
    CanonicalFraction rate = integer(1, budget), funding;
    if (const auto *terrain = std::get_if<TerrainCostBasis>(&prefix.basis)) {
      funding = integer(terrain->original_inputs.budget, budget);
      rate = terrain->parameter_limit.value;
      moving_unit.budget = TerrainUnitBudget{};
    }
    prefix.activity_world_map =
        RunningMovementMap{at, {}, rate, funding, source};
    GroupMovementOpeningSource opening{opening_id,
                                       journey_id,
                                       request.transport_unit_ref,
                                       unit.composition_revision,
                                       at,
                                       manifest.actual_anchor,
                                       profile,
                                       manifest.source_refs,
                                       unit.named_participants};
    executions.openings.set(opening_id, std::move(opening));
    take(common.append_fact({opening_id, "group_movement_opening", "travel"}));
    install_unit(common, movement, executions, context, prefix,
                 std::move(moving_unit), profile, unit.named_participants, at,
                 source);
    movement.prefixes.set(prefix_id, prefix);
    take(common.append_fact({prefix_id, "movement_prefix", "travel"}));
    take(common.append_fact({journey_id, "group_journey", "travel"}));
    carriers.journeys.set(journey_id, journey);
    carriers.transport_units.find(unit.transport_unit_id)->active_journey_id =
        journey_id;
    carriers.bindings.set(
        spatial_carrier_key(carrier),
        {carrier, unit.position_ref, DirectGroupMovement{journey_id}});
    movement.spatial_bindings.set(spatial_carrier_key(carrier),
                                  {"transport_unit", unit.transport_unit_id,
                                   WorldMovementWriter{prefix_id}, source});
    for (const auto &id : unit.named_participants)
      movement.spatial_bindings.set(
          "actor:" + id, {"actor", id, CarriedMovementWriter{carrier},
                          unit.registration_source_ref});
    project_group(common, carriers, movement, executions, context, journey,
                  take(read_movement_position(prefix, budget)),
                  {"travel", "movement_prefix", prefix_id}, actual_writes,
                  budget);
    return journey_id;
  });
}
Outcome<StableId> start_army_group_journey_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context,
    const ArmyGroupJourneyOpeningInput &request,
    MilitaryGroupOpeningAdoption &adopter, const WorldMoment &at,
    std::vector<BoundSpatialLocationWrite> &writes,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    if (!common.actor_location_capture_identity() ||
        !same_moment(at, take(context.carriers.current_moment()), budget))
      fail("ArmyGroupOpeningActualCandidateRequired", request.carrier.army_id);
    const SpatialCarrierRef carrier = request.carrier;
    const auto key = spatial_carrier_key(carrier);
    for (const auto &row : carriers.journeys.rows())
      if (spatial_carrier_key(row.second.carrier) == key &&
          !carriers.terminals.find(row.first))
        fail("ArmyAlreadyHasMovementExecution", request.carrier.army_id);
    auto manifest = take(read_transport_manifest(
        common, carriers, movement, context.carriers, carrier, budget));
    if (manifest.composition_revision !=
            request.expected_composition_revision ||
        request.named_march_participants.empty())
      fail("ArmyGroupOpeningCompositionMismatch", request.carrier.army_id);
    std::set<StableId> people;
    for (const auto &id : request.named_march_participants) {
      const auto *actor = common.actor(id);
      if (!actor || !actor->alive || !actor->available ||
          !actor->action_id.empty() || !people.insert(id).second ||
          std::find(manifest.named_actor_refs.begin(),
                    manifest.named_actor_refs.end(),
                    id) == manifest.named_actor_refs.end())
        fail("ArmyGroupParticipantUnavailable", id);
      if (const auto *binding = movement.spatial_bindings.find("actor:" + id)) {
        if (std::holds_alternative<BattleMovementWriter>(binding->writer) ||
            std::holds_alternative<WorldMovementWriter>(binding->writer))
          fail("ArmyParticipantOriginalWriterBusy", id);
        if (const auto *carried =
                std::get_if<CarriedMovementWriter>(&binding->writer))
          if (spatial_carrier_key(carried->carrier_ref) != key)
            fail("ArmyParticipantHasOtherCarrier", id);
      }
    }
    auto profile = take(context.actual_sources.read_motion_profile(
        manifest, request.route_profile, at));
    take(context.actual_sources.validate_motion_profile(manifest, profile, at));
    if (profile.original_profile.mode != "army" ||
        profile.original_profile.payer_wallet_id != request.payer_wallet_id ||
        !source_valid(profile.actual_rule_source) ||
        profile.source_schema < 1 || profile.additional_cost_per_unit < 0 ||
        !profile.fare_payee_wallet_id.empty() ||
        !profile.fare_destination_id.empty())
      fail("ArmyOriginalMovementRuleRequired", request.carrier.army_id);
    const auto journey_id = take(common.next_id("group_journey")),
               prefix_id = take(common.next_id("movement_prefix")),
               unit_id = take(common.next_id("movement_unit")),
               opening_id = take(common.next_id("group_movement_opening"));
    const SourceRef source{"travel", "group_movement_opening", opening_id};
    StableId position_id;
    Integer position_revision = 1;
    if (const auto *old = carriers.bindings.find(key)) {
      const auto *p = carriers.positions.find(old->position_ref);
      if (!p || spatial_carrier_key(p->carrier_ref) != key ||
          p->revision == std::numeric_limits<Integer>::max())
        fail("ArmyCarrierPositionRequired", request.carrier.army_id);
      position_id = p->position_id;
      position_revision = p->revision + 1;
    } else
      position_id = take(common.next_id("carrier_position"));
    GroupMovementJourney journey;
    journey.journey_id = journey_id;
    journey.carrier = carrier;
    journey.position_ref = position_id;
    journey.composition_revision = manifest.composition_revision;
    journey.initiator =
        ActorGroupInitiator{request.issuing_actor_id, request.campaign_id};
    journey.sponsor = {SubjectKind::organization, request.organization_id};
    journey.payer_wallet_id = request.payer_wallet_id;
    journey.expense_authority = MilitaryMarchExpenseAuthority{
        source, request.carrier.army_id, request.campaign_id,
        request.payer_wallet_id};
    journey.route_profile = request.route_profile;
    journey.started_at = journey.last_step_at = at;
    journey.movement_prefix_ref = prefix_id;
    MovementPrefixState prefix;
    prefix.id = prefix_id;
    prefix.execution = GroupMovementExecution{journey_id, carrier, 0};
    journey.movement_execution =
        std::get<GroupMovementExecution>(prefix.execution);
    prefix.basis = initial_basis(
        common, context, profile,
        {request.destination, request.via, request.payer_wallet_id}, carrier,
        manifest.actual_anchor, journey_id, unit_id, source, budget);
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = at;
    MovementUnitState moving;
    moving.id = unit_id;
    moving.execution = prefix.execution;
    CanonicalFraction rate = integer(1, budget), funding;
    if (const auto *terrain = std::get_if<TerrainCostBasis>(&prefix.basis)) {
      funding = integer(terrain->original_inputs.budget, budget);
      rate = terrain->parameter_limit.value;
      moving.budget = TerrainUnitBudget{};
    }
    prefix.activity_world_map =
        RunningMovementMap{at, {}, rate, funding, source};
    GroupMovementOpeningSource opening{opening_id,
                                       journey_id,
                                       request.carrier,
                                       manifest.composition_revision,
                                       at,
                                       manifest.actual_anchor,
                                       profile,
                                       manifest.source_refs,
                                       request.named_march_participants,
                                       {}};
    executions.openings.set(opening_id, std::move(opening));
    take(common.append_fact({opening_id, "group_movement_opening", "travel"}));
    install_unit(common, movement, executions, context, prefix,
                 std::move(moving), profile, request.named_march_participants,
                 at, source);
    movement.prefixes.set(prefix_id, prefix);
    take(common.append_fact({prefix_id, "movement_prefix", "travel"}));
    take(common.append_fact({journey_id, "group_journey", "travel"}));
    carriers.journeys.set(journey_id, journey);
    carriers.positions.set(
        position_id,
        {position_id, carrier,
         take(project_movement_location(manifest.actual_anchor, budget)),
         source, position_revision});
    carriers.bindings.set(
        key, {carrier, position_id, DirectGroupMovement{journey_id}});
    // This callback is the concrete original S2 preview/apply, not a caller
    // permit. Every later failure discards this complete candidate, including
    // that command.
    take(adopter.adopt_original_order_in_candidate(common, carriers, movement,
                                                   executions, journey_id));
    const auto *adopted = executions.openings.find(opening_id);
    if (!adopted || !adopted->military_order ||
        adopted->military_order->army_id != request.carrier.army_id ||
        adopted->military_order->issuing_actor_id != request.issuing_actor_id)
      fail("ArmyOriginalOrderAdoptionRequired", journey_id);
    movement.spatial_bindings.set(key,
                                  {"army", request.carrier.army_id,
                                   WorldMovementWriter{prefix_id}, source});
    const auto actual_anchor = take(read_movement_position(prefix, budget));
    for (const auto &id : request.named_march_participants) {
      movement.spatial_bindings.set(
          "actor:" + id, {"actor", id, CarriedMovementWriter{carrier}, source});
      ActorMutation mutation;
      mutation.actor_id = id;
      mutation.location = InArmyLocation{request.carrier.army_id};
      take(common.apply_actor(mutation));
      writes.push_back(take(bind_actual_spatial_location_write(
          common, movement, id, source, 1, actual_anchor, 0, budget)));
    }
    for (const auto &id : manifest.inventory_refs) {
      const auto *old = carriers.inventory_bindings.find(id);
      if (old) {
        const auto *prior = std::get_if<SpatialCarrierRef>(&old->placement);
        if (!prior || spatial_carrier_key(*prior) != key)
          fail("ArmyInventoryHasOtherSpatialWriter", id);
      }
      carriers.inventory_bindings.set(id,
                                      {id, carrier, source, position_revision});
    }
    project_group(common, carriers, movement, executions, context,
                  *carriers.journeys.find(journey_id), actual_anchor,
                  {"travel", "movement_prefix", prefix_id}, writes, budget);
    return journey_id;
  });
}


// 整军ready是唯一权限：一次候选内终结旧未来、采用真实退出点并开同军有限返程。
// 不复用玩家cancel权限，不重写原接触锚/已付unit，也不留下可单独发布的半次交接。
Outcome<SourceRef> adopt_field_army_exit_ready_in_candidate(
    CommonStore &common, MilitaryState &military, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    FieldBattleWorldState &field, const GroupMovementContext &context,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &abilities,
    const FieldBattleExitPlacementRead &placements, const SourceRef &source,
    const ArmyGroupJourneyOpeningInput &request,
    MilitaryGroupOpeningAdoption &adopter,
    std::vector<BoundSpatialLocationWrite> &writes,
    CheckpointMemoryCharge &retained, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &military != &x.military || &carriers != &x.carriers ||
        &movement != &x.movement || &executions != &x.groups ||
        &field != &x.field || &context.travel_catalog != &x.travel ||
        x.group_context != &context || abilities.exact_contacts != &x ||
        !common.actor_location_capture_identity())
      fail("FieldArmyExitReturnSameCandidateRequired", source.id);
    const auto ready = take(read_field_army_exit_ready_source(x, catalog, abilities, source, b));
    const auto adopted_at = take(context.carriers.current_moment());
    if (!ready.plan || !ready.original_journey || !ready.original_opening ||
        !ready.original_prefix || !ready.original_close ||
        !ready.original_opening->military_order || ready.plan->actual_continuation ||
        take(compare_world_moments(ready.adopted_at, adopted_at, b)) > 0 ||
        !same_moment(adopted_at, take(x.clock.read_field_window_closing_moment()), b))
      fail("FieldArmyExitReturnActualReadyRequired", source.id);
    if (take(read_field_army_exit_return_wait(x, catalog, abilities, source)))
      fail("FieldArmyExitCommanderUnavailable", source.id);
    take(validate_field_army_exit_current_adoption_window(x, catalog, abilities, source));
    const auto &old = *ready.original_journey;
    const auto &order = *ready.original_opening->military_order;
    const auto *destination = std::get_if<StableId>(&request.destination);
    if (request.carrier.army_id != ready.plan->army_id || !destination ||
        *destination != ready.plan->destination_settlement_id ||
        request.named_march_participants != ready.named_participants ||
        request.expected_composition_revision < 1 || !request.via.empty() ||
        request.campaign_id != order.campaign_id ||
        old.sponsor.kind != SubjectKind::organization ||
        request.organization_id != old.sponsor.id ||
        request.payer_wallet_id != old.payer_wallet_id ||
        request.route_profile != old.route_profile || request.issuing_actor_id.empty())
      fail("FieldArmyExitReturnOriginalOrderRequired", source.id);
    const auto *army = std::get_if<ArmyCarrierRef>(&old.carrier);
    const auto key = spatial_carrier_key(old.carrier);
    const auto *binding = movement.spatial_bindings.find(key);
    const auto *stable = binding ? std::get_if<StableMovementWriter>(&binding->writer) : nullptr;
    const auto *closed = std::get_if<MovementClosed>(&ready.original_prefix->lifecycle);
    const auto *unit = movement.units.find(ready.original_prefix->unit_ref);
    const auto *carrier_binding = carriers.bindings.find(key);
    const auto *direct = carrier_binding ? std::get_if<DirectGroupMovement>(&carrier_binding->execution_owner) : nullptr;
    const auto *position = carrier_binding ? carriers.positions.find(carrier_binding->position_ref) : nullptr;
    const auto active = field.army_bindings.find(ready.plan->army_id);
    if (!army || army->army_id != ready.plan->army_id ||
        old.state != GroupJourneyLifecycle::paused || carriers.terminals.find(old.journey_id) ||
        !old.stop_reason || !same_source(*old.stop_reason, ready.original_contact) ||
        !closed || !same_source(closed->original_result_ref, ready.original_contact) ||
        !unit || !std::holds_alternative<MovementUnitFinalized>(unit->status) ||
        ready.original_close->unit_ref != unit->id ||
        ready.original_close->kind != MovementCloseKind::contact ||
        !same_source(ready.original_close->actual_cause, ready.original_contact) ||
        !stable || !same_source(binding->binding_source, ready.original_contact) ||
        !take(same_spatial_anchor(stable->anchor, ready.original_close->actual_anchor, b)) ||
        !take(same_spatial_anchor(closed->actual_anchor, ready.original_close->actual_anchor, b)) ||
        !direct || direct->group_movement_id != old.journey_id ||
        carrier_binding->position_ref != old.position_ref || !position ||
        spatial_carrier_key(position->carrier_ref) != key ||
        position->revision == std::numeric_limits<Integer>::max() ||
        active == field.army_bindings.end() || active->second.battle_id != ready.plan->battle_id)
      fail("FieldArmyExitReturnOriginalPausedWriterRequired", source.id);
    const auto held = live_group(common, carriers, movement, executions, context, old);
    std::vector<StableId> held_ids;
    for (const auto *id : held.named_participants) held_ids.push_back(*id);
    std::sort(held_ids.begin(), held_ids.end());
    if (held_ids != ready.held_named_participants)
      fail("FieldArmyExitReturnHeldMembersMismatch", source.id);
    // 所有证明已在真实旧状态核过；下面每一步只属于同一个外层候选。
    const auto old_id = old.journey_id;
    const auto prefix_id = ready.original_prefix->id;
    const auto position_id = position->position_id;
    const auto position_revision = position->revision + 1;
    const auto carrier = old.carrier;
    const auto old_anchor = ready.original_close->actual_anchor;
    retained.grow(0); // 新旧锚仍普通复制，保同 job，不再计量。
    GroupMovementControlSource cancel{
        take(common.next_id("group_movement_control")), old_id,
        GroupControlOperation::cancel, source, adopted_at, old_anchor};
    const SourceRef cancelled{"travel", "group_movement_control", cancel.id};
    take(common.append_fact({cancel.id, "group_movement_control", "travel"}));
    executions.controls.set(cancel.id, cancel);
    movement.prefixes.find(prefix_id)->lifecycle = MovementClosed{cancelled, old_anchor};
    auto *old_journey = carriers.journeys.find(old_id);
    old_journey->state = GroupJourneyLifecycle::cancelled;
    old_journey->stop_reason = cancelled;
    old_journey->last_step_at = adopted_at;
    carriers.terminals.set(old_id,
        {old_id, cancelled, 1, adopted_at, GroupJourneyTerminalOutcome::cancelled});
    // 旧terminal仍指旧contact；只有ready给的新稳定锚能成为下一开程的原起点。
    carriers.positions.set(position_id,
        {position_id, carrier, take(project_movement_location(ready.actual_anchor, b)),
         source, position_revision});
    movement.spatial_bindings.set(key,
        {"army", request.carrier.army_id, StableMovementWriter{ready.actual_anchor}, source});
    field.army_bindings.erase(request.carrier.army_id);
    const auto *actual_point = std::get_if<OriginalMovementAnchor>(&ready.actual_anchor);
    const auto *actual_site = actual_point ? std::get_if<SettlementLocation>(&actual_point->location) : nullptr;
    if (actual_site && actual_site->settlement_id == ready.plan->destination_settlement_id) {
      // 已在原目标聚落：保原cancel的contact锚，以真实ready另证整军稳定点。
      // 不建立零行程、不触未选卸装；同候选真实具名成员仍须逐个Common tap。
      take(adopt_field_army_exit_at_destination_in_candidate(common, military, field,
          x, catalog, abilities, source, cancelled, retained));
      const auto capture = common.actor_location_capture_identity();
      for (const auto &id : ready.named_participants) {
        const auto *actor = common.actor(id);
        if (!actor || !actor->alive || !actor->available || !actor->action_id.empty())
          fail("ArmyGroupParticipantUnavailable", id);
        if (const auto *prior = movement.spatial_bindings.find("actor:" + id)) {
          if (std::holds_alternative<BattleMovementWriter>(prior->writer) ||
              std::holds_alternative<WorldMovementWriter>(prior->writer))
            fail("ArmyParticipantOriginalWriterBusy", id);
          if (const auto *carried = std::get_if<CarriedMovementWriter>(&prior->writer))
            if (spatial_carrier_key(carried->carrier_ref) != key)
              fail("ArmyParticipantHasOtherCarrier", id);
        }
        const auto offset = common.actor_location_execution_drafts().size();
        if (!capture || offset >= std::size_t(std::numeric_limits<Integer>::max()))
          fail("FieldArmyExitActualCaptureRequired", id);
        movement.spatial_bindings.set("actor:" + id,
            {"actor", id, CarriedMovementWriter{carrier}, cancelled});
        ActorMutation mutation;
        mutation.actor_id = id;
        mutation.location = InArmyLocation{request.carrier.army_id};
        take(common.apply_actor(mutation));
        const auto &drafts = common.actor_location_execution_drafts();
        if (common.actor_location_capture_identity() != capture || drafts.size() != offset + 1 ||
            drafts.back().actor_id != id || drafts.back().sequence != Integer(offset + 1) ||
            drafts.back().kind != ActorLocationExecutionDraft::Kind::mutation ||
            !drafts.back().before || !drafts.back().write_identity)
          fail("FieldArmyExitActualCaptureRequired", id);
        writes.push_back(take(bind_actual_field_army_exit_stable_location_write(
            common, movement, placements, id, MomentOwnerSource{cancelled, 1},
            capture, drafts.back().write_identity, Integer(offset + 1), b)));
      }
      take(read_field_army_exit_continuation_source(x, catalog, abilities, cancelled, b));
      return cancelled;
    }
    const auto new_journey = take(start_army_group_journey_in_candidate(
        common, carriers, movement, executions, context, request, adopter,
        adopted_at, writes, b));
    const GroupMovementOpeningSource *opening = nullptr;
    for (const auto &row : executions.openings.rows()) if (row.second.journey_id == new_journey) {
      if (opening) fail("DuplicateMilitaryGroupOpening", new_journey);
      opening = &row.second;
    }
    if (!opening || !opening->military_order ||
        opening->military_order->kind != MilitaryMarchOrderKind::finite_return ||
        !opening->military_order->original_return_cause ||
        !same_source(*opening->military_order->original_return_cause, source) ||
        !same_moment(opening->occurred_at, adopted_at, b) ||
        !take(same_spatial_anchor(opening->original_anchor, ready.actual_anchor, b)))
      fail("FieldArmyExitFiniteReturnAdoptionRequired", new_journey);
    const SourceRef actual{"travel", "group_movement_opening", opening->id};
    if (!ready.plan->actual_continuation || !same_source(*ready.plan->actual_continuation, actual))
      fail("FieldArmyExitReturnContinuationBindingRequired", new_journey);
    take(read_actual_group_journey_terminal(common, carriers, movement, executions, old_id, b));
    return actual;
  });
}

namespace {
class AdoptedTollRead final : public TransportExpenseSourceRead {
public:
  AdoptedTollRead(const GroupPoliticalPassage &p, const GroupMovementJourney &j,
                  const WorldMoment &at, const SpatialCarrierSources &sources)
      : passage(p), journey(j), moment(at), spatial(sources) {}
  Outcome<ActualTransportExpense>
  read_actual_transport_expense(const SourceRef &ref,
                                Integer schema) const override {
    auto cost = passage.read_adopted_toll(ref, journey, moment);
    if (!cost.ok)
      return cost;
    if (cost.value.source_schema != schema ||
        !same_source(cost.value.source_ref, ref) ||
        cost.value.journey_id != journey.journey_id ||
        cost.value.purpose != TransportExpensePurpose::toll ||
        !same_moment(cost.value.occurred_at, moment))
      return {
          false, {}, common_failure("PassageTollSourceMismatch", ref.id).error};
    return cost;
  }
  Outcome<WorldMoment> current_transport_expense_moment() const override {
    return spatial.current_moment();
  }
  Status validate_transport_expense_authority(
      const SourceRef &ref, Integer schema,
      const TransportExpenseAuthority &authority) const override {
    return status([&] {
      take(read_actual_transport_expense(ref, schema));
      if (!same_transport_expense_authority(journey.expense_authority,
                                            authority))
        fail("GroupExpenseAcceptedAuthorityMismatch", ref.id);
    });
  }

private:
  const GroupPoliticalPassage &passage;
  const GroupMovementJourney &journey;
  WorldMoment moment;
  const SpatialCarrierSources &spatial;
};
bool crossed(const SpatialCarrierState &carriers,
             const GroupMovementJourney &journey,
             const PassageBoundaryRef &boundary, Integer entry_ordinal) {
  for (const auto &row : carriers.crossings.rows()) {
    const auto &r = row.second;
    const auto *group = std::get_if<GroupMovementExecution>(&r.execution);
    if (group && group->journey_id == journey.journey_id &&
        r.entry_ordinal == entry_ordinal && r.boundary.kind == boundary.kind &&
        r.boundary.id == boundary.id)
      return true;
  }
  return false;
}
void enter_boundaries(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    const MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementJourney &journey, const TransportManifestRead &manifest,
    const MovementPrefixState &prefix, const GroupMovementContext &context,
    const WorldMoment &at) {
  auto requested = take(context.passage.prepare_actual_entry(journey, manifest,
                                                             prefix.basis, at));
  std::set<std::pair<PassageBoundaryKind, StableId>> unique;
  for (const auto &entry : requested) {
    take(validate_passage_purpose_context_shape(entry.request.purpose_context));
    if (!unique.insert({entry.request.boundary.kind, entry.request.boundary.id})
             .second)
      fail("DuplicatePassageEntry");
    auto actual_entry = take(read_actual_group_passage_entry(
        carriers, movement, context.travel_catalog,
        std::get<GroupMovementExecution>(prefix.execution),
        entry.request.boundary, at));
    if (crossed(carriers, journey, entry.request.boundary,
                actual_entry.entry_ordinal))
      continue;
    if (entry.request.purpose != PassagePurpose::enter ||
        !entry.request.execution ||
        execution_key(*entry.request.execution) !=
            execution_key(prefix.execution) ||
        entry.request.manifest_basis.composition_revision !=
            journey.composition_revision ||
        entry.request.manifest_basis.manifest_revision !=
            manifest.manifest_revision ||
        entry.decision.entry_tick != at.half_day_tick ||
        entry.decision.admission != PassageAdmission::allowed)
      fail("GroupPassageDenied", entry.request.boundary.id);
    take(capture_transport_entry_manifest_in_candidate(
        common, carriers, movement, executions, context.common_catalog,
        context.inventory_item_sources, context.carriers,
        context.manifest_history, prefix.execution, actual_entry.entry_ordinal,
        entry.request.boundary, at));
    auto decision =
        take(context.passage.adopt_actual_decision_in_candidate(common, entry));
    if (!source_valid(decision))
      fail("PoliticalPassageDecisionSourceRequired");
    BoundaryCrossingReceipt crossing;
    crossing.receipt_id = take(common.next_id("boundary_crossing"));
    crossing.execution = prefix.execution;
    crossing.carrier = journey.carrier;
    crossing.composition_revision = journey.composition_revision;
    crossing.manifest_revision = manifest.manifest_revision;
    crossing.boundary = entry.request.boundary;
    crossing.entry_ordinal = actual_entry.entry_ordinal;
    crossing.entry_at = at;
    crossing.decision_source_ref = decision;
    if (entry.decision.toll) {
      auto actual =
          take(context.passage.read_adopted_toll(decision, journey, at));
      if (actual.amount != entry.decision.toll->amount ||
          actual.payee_wallet_id != entry.decision.toll->payee_wallet_id ||
          actual.purpose != TransportExpensePurpose::toll)
        fail("PoliticalPassageTollMismatch");
      if (!context.expense_authorities)
        fail("OriginalTransportExpenseAuthorityRequired", journey.journey_id);
      AdoptedTollRead costs(context.passage, journey, at, context.carriers);
      auto receipt = take(pay_transport_expense_in_candidate(
          common, economy, payments,
          {costs, context.expense_authorities->get(), context.obligations,
           context.obligation_writes},
          {actual.source_ref, actual.source_schema,
           commercial_funding(journey)}));
      if (receipt.payer_wallet_id != journey.payer_wallet_id)
        fail("GroupPassagePayerMismatch");
      crossing.payment_refs.push_back(
          {"local_economy", "transport_expense_paid", receipt.id});
    }
    take(context.passage.validate_crossing_decision(crossing));
    take(common.append_fact(
        {crossing.receipt_id, "boundary_crossing", "travel"}));
    const auto key = crossing.receipt_id;
    carriers.crossings.set(key, std::move(crossing));
  }
}
void pay_first_unit(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const GroupMovementJourney &journey,
    const MovementPrefixState &prefix, MovementUnitState &unit,
    const WorldMoment &at,
    std::variant<GroupExpensePrefixCause, GroupExpenseContactCause> cause) {
  if (std::holds_alternative<MovementPaymentConsumed>(unit.payment))
    return;
  GroupTransportExpenseSource actual;
  actual.id = take(common.next_id("group_transport_expense"));
  actual.journey_id = journey.journey_id;
  actual.prefix_ref = prefix.id;
  actual.unit_ref = unit.id;
  actual.occurred_at = at;
  actual.actual_cause = std::move(cause);
  const auto expense_id = actual.id;
  const SourceRef actual_ref{"travel", "group_transport_expense", expense_id};
  executions.expenses.set(expense_id, actual);
  take(common.append_fact({expense_id, "group_transport_expense", "travel"}));
  const auto *before_wallet = common.wallet(journey.payer_wallet_id);
  if (!before_wallet)
    fail("GroupPayerWalletRequired");
  Integer before = before_wallet->balance, after = before, amount = 0;
  StableId cost_rule;
  if (const auto *military = std::get_if<MilitaryMarchExpenseAuthority>(
          &journey.expense_authority)) {
    if (!context.military_expenses)
      fail("MilitaryMarchExpenseSourceReaderRequired", journey.journey_id);
    const auto debit = take(apply_military_march_wallet_debit_in_candidate(
        common, *context.military_expenses, actual_ref, 1, *military));
    if (debit.payer_wallet_id != journey.payer_wallet_id ||
        !same_moment(debit.occurred_at, at))
      fail("MilitaryMarchDebitSourceMismatch", expense_id);
    const auto original =
        take(context.military_expenses->read_actual_military_march_expense(
            actual_ref, 1, *military));
    amount = debit.amount;
    before = debit.balance_before;
    after = debit.balance_after;
    cost_rule = original.original_cost_rule_ref;
  } else {
    if (!context.expense_authorities)
      fail("OriginalTransportExpenseAuthorityRequired", journey.journey_id);
    NativeGroupTransportExpenseRead costs(movement, carriers, executions,
                                          context.travel_catalog,
                                          context.carriers);
    const auto paid = take(pay_transport_expense_in_candidate(
        common, economy, payments,
        {costs, context.expense_authorities->get(), context.obligations,
         context.obligation_writes},
        {actual_ref, 1, commercial_funding(journey)}));
    if (paid.payer_wallet_id != journey.payer_wallet_id)
      fail("GroupExpensePayerMismatch");
    const auto *after_wallet = common.wallet(journey.payer_wallet_id);
    if (!after_wallet)
      fail("GroupPayerWalletRequired");
    amount = paid.expense.amount;
    after = after_wallet->balance;
    cost_rule = paid.expense.cost_rule_id;
    executions.expenses.find(expense_id)->original_payment_receipt =
        SourceRef{"local_economy", "transport_expense_paid", paid.id};
  }
  const auto movement_payment_id = take(common.next_id("movement_expense"));
  MovementExpenseSource payment{movement_payment_id,
                                prefix.id,
                                unit.id,
                                journey.payer_wallet_id,
                                prefix.execution,
                                at,
                                amount,
                                before,
                                after,
                                cost_rule};
  movement.expenses.set(movement_payment_id, std::move(payment));
  take(common.append_fact({movement_payment_id, "movement_expense", "travel"}));
  unit.payment = MovementPaymentConsumed{
      {"travel", "movement_expense", movement_payment_id},
      journey.payer_wallet_id,
      amount};
  carriers.journeys.find(journey.journey_id)
      ->expense_source_refs.push_back(actual_ref);
}
void accumulate(MovementPrefixState &prefix, MovementUnitState &unit,
                const PreparedMovementPrefix &actual,
                ExactArithmeticBudget budget) {
  const auto mapping = std::get<RunningMovementMap>(prefix.activity_world_map);
  MovementActivitySegment work{actual.span,           prefix.cursor,
                               actual.cursor_after,   mapping.rate,
                               mapping.budget_rate,   mapping.rate_source,
                               basis_id(prefix.basis)};
  if (!unit.activity_spans.empty()) {
    auto &last = unit.activity_spans.back();
    if (last.basis_ref == work.basis_ref &&
        same_moment(last.span.to, work.span.from, budget) &&
        same_source(last.rate_source, work.rate_source) &&
        compare(last.rate, work.rate, budget) == 0 &&
        compare(last.budget_rate, work.budget_rate, budget) == 0) {
      last.span.to = work.span.to;
      last.cursor_to = work.cursor_to;
    } else
      unit.activity_spans.push_back(work);
  } else
    unit.activity_spans.push_back(work);
  prefix.cursor = actual.cursor_after;
  prefix.last_settled_at = actual.span.to;
  unit.active_elapsed = actual.active_elapsed_after;
  if (actual.terrain_after)
    unit.budget = *actual.terrain_after;
}
} // namespace
Status adopt_group_positive_span_from_in_candidate(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StableId &journey_id,
    const MomentSpan &span, ExactArithmeticBudget budget) {
  return status([&] {
    if (take(compare_world_moments(span.from, span.to, budget)) >= 0 ||
        !same_moment(span.from, take(context.carriers.current_moment()),
                     budget))
      fail("GroupPositiveSpanFromPhaseRequired", journey_id);
    const auto *original = carriers.journeys.find(journey_id);
    if (!original || original->state != GroupJourneyLifecycle::travelling)
      fail("GroupJourneyNotRunning", journey_id);
    const auto journey = *original;
    const auto composition =
        live_group(common, carriers, movement, executions, context, journey);
    const auto actual = take(prepare_movement_prefix(
        movement, journey.movement_prefix_ref, span, budget));
    const auto prefix = *movement.prefixes.find(actual.prefix_id);
    auto unit = *movement.units.find(actual.unit_ref);
    const auto *binding =
        movement.spatial_bindings.find(spatial_carrier_key(journey.carrier));
    const auto *world =
        binding ? std::get_if<WorldMovementWriter>(&binding->writer) : nullptr;
    const auto *terms = executions.unit_terms.find(unit.id);
    if (!world || world->prefix_id != prefix.id || !terms)
      fail("GroupMovementWriterNotWorld", journey_id);
    const auto manifest = take(read_transport_manifest(
        common, carriers, movement, context.carriers, journey.carrier, budget));
    take(context.actual_sources.validate_motion_conditions(journey, manifest,
                                                           span));
    take(context.actual_sources.validate_motion_profile(
        manifest, take(read_group_effective_motion_profile(common, executions, *terms)), span.from));
    for (const auto *member : composition.named_participants) {
      const auto &id = *member;
      const auto *actor = common.actor(id);
      const auto *owned = movement.spatial_bindings.find("actor:" + id);
      const auto *carried =
          owned ? std::get_if<CarriedMovementWriter>(&owned->writer) : nullptr;
      if (!actor || !actor->alive || !actor->available ||
          !actor->action_id.empty() || !carried ||
          spatial_carrier_key(carried->carrier_ref) !=
              spatial_carrier_key(journey.carrier) ||
          !same_source(owned->binding_source,
                       composition.registration_source_ref))
        fail("GroupParticipantSpatialWriterMismatch", id);
    }
    if (compare(unit.active_elapsed.value, {}, budget) == 0)
      enter_boundaries(common, economy, payments, carriers, movement,
                       executions, journey, manifest, prefix, context,
                       span.from);
    pay_first_unit(common, economy, payments, carriers, movement, executions,
                   context, journey, prefix, unit, span.from,
                   GroupExpensePrefixCause{span});
    {
      const auto unit_key = unit.id;
      movement.units.set(unit_key, std::move(unit));
    }
  });
}
Status apply_exact_group_prefix_in_candidate(
    CommonStore &common, LocalEconomyState &, EconomicTransportExpenseState &,
    SpatialCarrierState &carriers, MovementExactState &movement,
    GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StableId &journey_id,
    const MomentSpan &span,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    ExactArithmeticBudget budget) {
  return status([&] {
    if (!same_moment(span.to, take(context.carriers.current_moment()), budget))
      fail("GroupPositiveSpanToPhaseRequired", journey_id);
    const auto *original = carriers.journeys.find(journey_id);
    if (!original || original->state != GroupJourneyLifecycle::travelling)
      fail("GroupJourneyNotRunning", journey_id);
    const auto journey = *original;
    const auto composition =
        live_group(common, carriers, movement, executions, context, journey);
    const auto *binding =
        movement.spatial_bindings.find(spatial_carrier_key(journey.carrier));
    const auto *writer =
        binding ? std::get_if<WorldMovementWriter>(&binding->writer) : nullptr;
    if (!writer || writer->prefix_id != journey.movement_prefix_ref)
      fail("GroupMovementWriterNotWorld", journey_id);
    const auto actual = take(prepare_movement_prefix(
        movement, journey.movement_prefix_ref, span, budget));
    auto prefix = *movement.prefixes.find(actual.prefix_id);
    auto unit = *movement.units.find(actual.unit_ref);
    if (!std::holds_alternative<MovementPaymentConsumed>(unit.payment))
      fail("GroupPositiveSpanFromAdoptionRequired", journey_id);
    if (unit.activity_spans.empty()) {
      const GroupTransportExpenseSource *first = nullptr;
      for (const auto &row : executions.expenses.rows())
        if (row.second.unit_ref == unit.id) {
          if (first)
            fail("DuplicateGroupExpense", unit.id);
          first = &row.second;
        }
      const auto *positive =
          first ? std::get_if<GroupExpensePrefixCause>(&first->actual_cause)
                : nullptr;
      if (!positive ||
          !same_moment(positive->first_actual_span.from, span.from, budget) ||
          !same_moment(positive->first_actual_span.to, span.to, budget))
        fail("GroupPositiveSpanFromAdoptionRequired", journey_id);
    }
    // Only the original movement writer applies this selected tail. All
    // admission, authority and first money legs were read/written at its actual
    // from phase.
    accumulate(prefix, unit, actual, budget);
    movement.units.set(unit.id, unit);
    movement.prefixes.set(prefix.id, prefix);
    auto *current = carriers.journeys.find(journey_id);
    current->last_step_at = span.to;
    project_group(common, carriers, movement, executions, context, *current,
                  actual.anchor_after, {"travel", "movement_prefix", prefix.id},
                  actual_writes, budget);
    // The entry, payment, positive activity and this political consequence are
    // a single candidate. S6 owns the actual SafeExit binding; S1 alone writes
    // its reference into the original crossing. Never adopt from a merely
    // planned span.
    for (const auto &id : carriers.crossings.order()) {
      auto *crossing = carriers.crossings.find(id);
      const auto *execution =
          crossing ? std::get_if<GroupMovementExecution>(&crossing->execution)
                   : nullptr;
      if (!execution || execution->journey_id != journey_id ||
          crossing->entry_ordinal < 1 ||
          crossing->entry_ordinal - 1 != unit.ordinal ||
          !same_moment(crossing->entry_at, span.from, budget))
        continue;
      const SourceRef ref{"travel", "boundary_crossing", crossing->receipt_id};
      const auto *head = common.state().facts.find(ref.id);
      if (!head || head->id != ref.id || head->domain != ref.domain ||
          head->kind != ref.kind)
        fail("GroupCrossingSourceHeadRequired", ref.id);
      take(validate_group_boundary_crossing_source(
          common, executions, carriers, movement, context.travel_catalog, *crossing, budget));
      const auto adopted =
          take(context.passage.adopt_after_positive_crossing_in_candidate(
              common, ref));
      crossing = carriers.crossings.find(id);
      if (!crossing)
        fail("GroupCrossingSourceRequired", id);
      if (adopted && !source_valid(*adopted))
        fail("GroupCrossingPoliticalConsumerSourceRequired", id);
      if (crossing->safe_exit_binding_ref &&
          (!adopted ||
           !same_source(*crossing->safe_exit_binding_ref, *adopted)))
        fail("GroupCrossingPoliticalConsumerReplayMismatch", id);
      crossing->safe_exit_binding_ref = adopted;
      take(context.passage.validate_after_positive_crossing(*crossing));
    }
  });
}
namespace {
void adopt_completed_boundaries(CommonStore &common,
                                const SpatialCarrierState &carriers,
                                const MovementExactState &movement,
                                const GroupMovementExecutionState &executions,
                                const GroupMovementContext &context,
                                const StableId &journey_id,
                                const WorldMoment &at,
                                ExactArithmeticBudget budget) {
  for (const auto &row : carriers.crossings.rows()) {
    const auto *execution =
        std::get_if<GroupMovementExecution>(&row.second.execution);
    if (!execution || execution->journey_id != journey_id)
      continue;
    const auto completion = take(read_actual_group_boundary_completion(
        common, carriers, movement, executions, context.travel_catalog, row.second,
        budget));
    if (!completion || !same_moment(completion->completed_at, at, budget))
      continue;
    take(context.passage.adopt_after_boundary_completion_in_candidate(
        common, {"travel", "boundary_crossing", row.first}));
    take(context.passage.validate_after_boundary_completion(row.second));
  }
}
void continue_group_unit(CommonStore &common, SpatialCarrierState &carriers,
                         MovementExactState &movement,
                         GroupMovementExecutionState &executions,
                         const GroupMovementContext &context,
                         GroupMovementJourney &journey,
                         MovementPrefixState prefix, const WorldMoment &at,
                         std::vector<BoundSpatialLocationWrite> &actual_writes,
                         ExactArithmeticBudget budget) {
  const auto old_unit = *movement.units.find(prefix.unit_ref);
  const auto transport =
      live_group(common, carriers, movement, executions, context, journey);
  auto manifest = take(read_transport_manifest(
      common, carriers, movement, context.carriers, journey.carrier, budget));
  auto profile = take(context.actual_sources.read_motion_profile(
      manifest, journey.route_profile, at));
  take(context.actual_sources.validate_motion_profile(manifest, profile, at));
  if (profile.original_profile.payer_wallet_id != journey.payer_wallet_id)
    fail("GroupPayerSourceChanged");
  const auto unit_id = take(common.next_id("movement_unit"));
  const SourceRef source{"travel", "movement_unit_close",
                         old_unit.id + ":close"};
  MovementUnitState next;
  next.id = unit_id;
  next.execution = prefix.execution;
  next.ordinal = old_unit.ordinal + 1;
  CanonicalFraction rate = integer(1, budget), funding;
  if (const auto *original = std::get_if<TerrainCostBasis>(&prefix.basis)) {
    auto terrain = *original;
    const auto *map =
        context.travel_catalog.maps.find(terrain.original_inputs.map_id);
    const auto *old = std::get_if<TerrainUnitBudget>(&old_unit.budget);
    if (!map || !old ||
        profile.original_profile.mode != terrain.original_inputs.mode)
      fail("GroupOriginalTerrainRuleRequired");
    auto credit =
        sub(sub(add(old->opening_credit.value, old->funded.value, budget),
                old->spent.value, budget),
            old->discarded.value, budget);
    TerrainUnitBudget ledger;
    ledger.opening_credit.value = credit;
    next.budget = ledger;
    auto p = terrain.original_inputs;
    p.budget = rate_budget(*map, profile);
    p.travel_credit = take(floor_fraction(credit, budget));
    p.money_per_tick =
        checked_sum(map->money_per_tick, profile.additional_cost_per_unit);
    funding = integer(p.budget, budget);
    rate = add(funding, credit, budget);
    if (compare(prefix.cursor, terrain.parameter_limit.value, budget) == 0) {
      if (terrain.reached_points == terrain.original_point_count)
        fail("MovementArrivalRequiredBeforeCarry");
      p.points.erase(p.points.begin(),
                     p.points.begin() + terrain.reached_points);
      p.route_index += terrain.reached_points;
      auto start = take(read_movement_position(prefix, budget));
      auto basis_id = take(common.next_id("movement_basis"));
      bool integer_domain = false;
      if (const auto *a = std::get_if<OriginalMovementAnchor>(&start))
        if (const auto *w = std::get_if<WildernessLocation>(&a->location))
          if (compare(integer(p.travel_credit, budget), credit, budget) == 0) {
            auto trace = take(terrain_geometry_motion(
                context.travel_catalog, context.travel_services, w->point, p,
                true, spatial_carrier_key(journey.carrier),
                journey.journey_id));
            prefix.basis = take(make_original_terrain_cost_basis(
                trace, basis_id, "terrain_original/1", unit_id, source,
                budget));
            integer_domain = true;
          }
      if (!integer_domain)
        prefix.basis = take(make_continued_terrain_cost_basis(
            context.travel_catalog, context.travel_services, start, p, rate,
            basis_id, unit_id, source, budget));
      prefix.cursor = {};
    }
  } else {
    auto road = std::get<RoadMovementBasis>(prefix.basis);
    if (compare(prefix.cursor, integer(1, budget), budget) != 0)
      fail("GroupRoadUnitBoundaryRequired");
    auto p = road.original_inputs.params;
    const auto actual_anchor = take(read_movement_position(prefix, budget));
    auto location = take(project_movement_location(actual_anchor, budget));
    const auto *fractional = std::get_if<RoadFractionalAnchor>(&actual_anchor);
    const bool retain_fraction =
        fractional &&
        compare(
            fractional->progress,
            integer(take(floor_fraction(fractional->progress, budget)), budget),
            budget) != 0;
    if (!retain_fraction &&
        take(finish_road_leg(context.travel_catalog, at.half_day_tick, p,
                             location)))
      fail("MovementArrivalRequiredBeforeCarry");
    const auto *here = std::get_if<OnRouteLocation>(&location);
    const auto *edge =
        here ? context.travel_catalog.roads.find(here->edge_id) : nullptr;
    if (!edge || edge->duration_ticks < 1 ||
        edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
      fail("GroupMovementRoadRequired");
    if (!p.returning && !road_is_open(*edge, at.half_day_tick))
      fail("RoadBlocked", edge->id);
    const auto end = p.returning ? std::max<Integer>(0, here->progress - 100)
                                 : std::min<Integer>(edge->duration_ticks * 100,
                                                     here->progress + 100);
    RoadTravelMotion trace{spatial_carrier_key(journey.carrier),
                           journey.journey_id,
                           edge->id,
                           here->origin,
                           here->progress,
                           end,
                           true,
                           p};
    auto basis = take(common.next_id("movement_basis"));
    auto next_basis = take(make_original_road_movement_basis(
        trace, basis, "road_original/1", unit_id, source, budget));
    if (retain_fraction) {
      if (!road.continued || fractional->edge_id != edge->id ||
          fractional->origin != here->origin)
        fail("GroupOriginalFractionalCarryRequired", journey.journey_id);
      const auto maximum = integer(edge->duration_ticks * 100, budget),
                 step = integer(100, budget);
      if (compare(fractional->progress, maximum, budget) >= 0)
        fail("GroupOriginalFractionalCarryRequired", journey.journey_id);
      auto exact_end = p.returning
                           ? (compare(fractional->progress, step, budget) < 0
                                  ? CanonicalFraction{}
                                  : sub(fractional->progress, step, budget))
                           : add(fractional->progress, step, budget);
      if (compare(exact_end, maximum, budget) > 0)
        exact_end = maximum;
      next_basis.continued = true;
      next_basis.exact_start_progress = fractional->progress;
      next_basis.exact_end_progress = exact_end;
      next_basis.start_anchor = actual_anchor;
      next_basis.end_anchor =
          RoadFractionalAnchor{edge->id, here->origin, exact_end, source};
      next_basis.original_inputs.end = take(floor_fraction(exact_end, budget));
    }
    prefix.basis = std::move(next_basis);
    prefix.cursor = {};
  }
  prefix.unit_ref = unit_id;
  prefix.last_settled_at = at;
  prefix.lifecycle = MovementActive{};
  prefix.activity_world_map =
      RunningMovementMap{at, prefix.cursor, rate, funding, source};
  install_unit(common, movement, executions, context, prefix, std::move(next),
               profile, transport.named_participants, at, source);
  movement.prefixes.set(prefix.id, prefix);
  project_group(common, carriers, movement, executions, context, journey,
                take(read_movement_position(prefix, budget)),
                {"travel", "movement_prefix", prefix.id}, actual_writes,
                budget);
}
} // namespace
Outcome<StableId> close_exact_group_unit_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StableId &journey_id,
    MovementCloseKind kind, const SourceRef &cause, const WorldMoment &at,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    auto *current = carriers.journeys.find(journey_id);
    if (!current)
      fail("GroupJourneyRequired", journey_id);
    auto journey = *current;
    const auto transport =
        live_group(common, carriers, movement, executions, context, journey);
    auto *p = movement.prefixes.find(journey.movement_prefix_ref);
    if (!p || !same_moment(p->last_settled_at, at, budget))
      fail("GroupMovementNotSettled");
    const auto unit_id = p->unit_ref;
    auto exertion = take(finalize_exact_movement_unit_source(
        common, movement, p->id, kind, cause, at, budget));
    p = movement.prefixes.find(journey.movement_prefix_ref);
    const auto *unit = movement.units.find(unit_id);
    const auto *effort =
        movement.exertion_bases.find(unit->effort_basis_refs.front().id);
    auto anchor = take(read_movement_position(*p, budget));
    const auto step_id = unit_id + ":group_step";
    if (!carriers.steps.find(step_id)) {
      auto first = *p;
      first.basis = effort->actual_basis;
      first.cursor = effort->opened_cursor;
      first.lifecycle = MovementActive{};
      GroupJourneyStep step{
          step_id,
          journey_id,
          unit->ordinal,
          {effort->opened_at, at},
          take(read_movement_position(first, budget)),
          anchor,
          [&] {
            CanonicalFraction progress;
            for (const auto &work : unit->activity_spans)
              progress =
                  add(progress, sub(work.cursor_to, work.cursor_from, budget),
                      budget);
            return progress;
          }(),
          {},
          exertion.empty()
              ? std::vector<SourceRef>{}
              : std::vector<SourceRef>{{"travel", "travel_exertion", exertion}},
          {},
          kind == MovementCloseKind::arrival ? GroupStepOutcome::arrived
          : kind == MovementCloseKind::carry ? GroupStepOutcome::progressed
                                             : GroupStepOutcome::interrupted};
      if (const auto *paid =
              std::get_if<MovementPaymentConsumed>(&unit->payment))
        step.cost_refs.push_back(paid->actual_expense_source);
      if (kind == MovementCloseKind::contact)
        step.contact_refs.push_back(cause);
      carriers.steps.set(step_id, std::move(step));
      take(common.append_fact({step_id, "group_journey_step", "travel"}));
      journey.movement_source_refs.push_back(
          {"travel", "group_journey_step", step_id});
    }
    if (kind == MovementCloseKind::arrival) {
      MovementArrivalSource arrival;
      arrival.id = cause.id;
      arrival.prefix_id = p->id;
      arrival.unit_ref = unit_id;
      arrival.execution = p->execution;
      arrival.occurred_at = at;
      arrival.actual_anchor = anchor;
      arrival.basis_ref = basis_id(p->basis);
      arrival.cursor = p->cursor;
      if (const auto *terrain = std::get_if<TerrainCostBasis>(&p->basis)) {
        if (terrain->reached_points != terrain->original_point_count ||
            compare(take(movement_consumed_cost(*terrain, p->cursor, budget)),
                    integer(terrain->actual_consumed_total, budget),
                    budget) != 0 ||
            (compare(p->cursor, terrain->parameter_limit.value, budget) != 0 &&
             compare(unit->active_elapsed.value, integer(1, budget), budget) !=
                 0))
          fail("GroupArrivalNotAtActualEnd");
        arrival.reached_points = terrain->reached_points;
        arrival.original_point_count = terrain->original_point_count;
        if (const auto *site =
                std::get_if<StableId>(&terrain->original_inputs.destination)) {
          const auto *city = context.travel_catalog.settlements.find(*site);
          if (!city)
            fail("GroupArrivalDestinationRequired");
          arrival.destination_settlement_id = *site;
          arrival.actual_anchor = OriginalMovementAnchor{
              SettlementLocation{*site, city->entry_place}, cause};
        }
      } else {
        auto params =
            std::get<RoadMovementBasis>(p->basis).original_inputs.params;
        auto location = take(project_movement_location(anchor, budget));
        if (compare(p->cursor, integer(1, budget), budget) != 0 ||
            !take(finish_road_leg(context.travel_catalog, at.half_day_tick,
                                  params, location)))
          fail("GroupArrivalNotAtActualEnd");
        arrival.actual_anchor = OriginalMovementAnchor{location, cause};
        arrival.destination_settlement_id =
            std::get<SettlementLocation>(location).settlement_id;
      }
      movement.arrivals.set(arrival.id, arrival);
      take(common.append_fact({arrival.id, "movement_arrival", "travel"}));
      p->lifecycle = MovementClosed{cause, arrival.actual_anchor};
      project_group(common, carriers, movement, executions, context, journey,
                    arrival.actual_anchor, cause, actual_writes, budget);
      journey.state = GroupJourneyLifecycle::arrived;
      journey.last_step_at = at;
      journey.stop_reason = cause;
      carriers.terminals.set(
          journey_id,
          {journey_id, cause, 1, at, GroupJourneyTerminalOutcome::arrived});
      release_active_group(carriers, journey);
      movement.spatial_bindings.set(
          spatial_carrier_key(journey.carrier),
          {transport.subject_kind, transport.transport_unit_id,
           StableMovementWriter{arrival.actual_anchor}, cause});
      for (const auto *member : transport.named_participants) {
        const auto &id = *member;
        movement.spatial_bindings.set(
            "actor:" + id, {"actor", id, CarriedMovementWriter{journey.carrier},
                            transport.registration_source_ref});
      }
    } else if (kind == MovementCloseKind::carry) {
      continue_group_unit(common, carriers, movement, executions, context,
                          journey, *p, at, actual_writes, budget);
    } else {
      p->lifecycle = MovementClosed{cause, anchor};
      journey.state = kind == MovementCloseKind::cancelled
                          ? GroupJourneyLifecycle::cancelled
                          : GroupJourneyLifecycle::paused;
      journey.stop_reason = cause;
      if (kind == MovementCloseKind::cancelled) {
        release_active_group(carriers, journey);
        carriers.terminals.set(
            journey_id,
            {journey_id, cause, 1, at, GroupJourneyTerminalOutcome::cancelled});
        journey.last_step_at = at;
      }
      movement.spatial_bindings.set(spatial_carrier_key(journey.carrier),
                                    {transport.subject_kind,
                                     transport.transport_unit_id,
                                     StableMovementWriter{anchor}, cause});
      for (const auto *member : transport.named_participants) {
        const auto &id = *member;
        movement.spatial_bindings.set(
            "actor:" + id, {"actor", id, CarriedMovementWriter{journey.carrier},
                            transport.registration_source_ref});
      }
    }
    carriers.journeys.set(journey_id, std::move(journey));
    if (kind != MovementCloseKind::contact)
      adopt_completed_boundaries(common, carriers, movement, executions, context,
                                 journey_id, at, budget);
    return exertion;
  });
}
Status control_group_journey_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StableId &journey_id,
    Integer revision, const GroupInitiator &initiator,
    GroupControlOperation operation, const SourceRef &authority,
    const WorldMoment &at,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    ExactArithmeticBudget budget) {
  return status([&] {
    const auto *j = carriers.journeys.find(journey_id);
    if (!j || j->composition_revision != revision)
      fail("GroupJourneyCompositionMismatch");
    if (carriers.terminals.find(journey_id) ||
        j->state == GroupJourneyLifecycle::arrived ||
        j->state == GroupJourneyLifecycle::cancelled)
      fail("GroupJourneyAlreadyTerminal", journey_id);
    if (!same_moment(at, take(context.carriers.current_moment()), budget))
      fail("GroupControlMomentMismatch");
    take(context.actual_sources.validate_control_authority(
        *j, initiator, operation, authority));
    const auto *prefix = movement.prefixes.find(j->movement_prefix_ref);
    if (!prefix)
      fail("GroupControlRequiresSettledPrefix");
    const bool closed_contact =
        std::holds_alternative<MovementClosed>(prefix->lifecycle);
    if (closed_contact) {
      const auto *close =
          movement.unit_closes.find(prefix->unit_ref + ":close");
      if (operation != GroupControlOperation::cancel || !close ||
          close->kind != MovementCloseKind::contact ||
          !same_source(close->actual_cause,
                       std::get<MovementClosed>(prefix->lifecycle)
                           .original_result_ref) ||
          take(compare_world_moments(prefix->last_settled_at, at, budget)) > 0)
        fail("GroupContactResolutionRequired", journey_id);
    } else if (!same_moment(prefix->last_settled_at, at, budget))
      fail("GroupControlRequiresSettledPrefix");
    GroupMovementControlSource source{
        take(common.next_id("group_movement_control")),
        journey_id,
        operation,
        authority,
        at,
        take(read_movement_position(*prefix, budget))};
    const auto ref = SourceRef{"travel", "group_movement_control", source.id};
    take(common.append_fact({source.id, "group_movement_control", "travel"}));
    {
      const auto key = source.id;
      executions.controls.set(key, source);
    }
    if (operation == GroupControlOperation::pause) {
      take(hold_movement_prefix(movement, prefix->id, at, ref, budget));
      carriers.journeys.find(journey_id)->state = GroupJourneyLifecycle::paused;
    } else if (operation == GroupControlOperation::resume) {
      take(resume_movement_prefix(movement, prefix->id, at, ref, budget));
      carriers.journeys.find(journey_id)->state =
          GroupJourneyLifecycle::travelling;
    } else {
      const auto *unit = movement.units.find(prefix->unit_ref);
      if (!unit)
        fail("MovementUnitRequired");
      if (closed_contact ||
          std::holds_alternative<MovementPaymentNotConsumed>(
              unit->payment)) { // 已接触关闭或未实际开始时只取消未来责任，不重复关闭、补收费或生成用力。
        auto *live = movement.prefixes.find(prefix->id);
        live->lifecycle = MovementClosed{ref, source.actual_anchor};
        auto *journey = carriers.journeys.find(journey_id);
        journey->state = GroupJourneyLifecycle::cancelled;
        journey->stop_reason = ref;
        journey->last_step_at = at;
        carriers.terminals.set(
            journey_id,
            {journey_id, ref, 1, at, GroupJourneyTerminalOutcome::cancelled});
        const auto transport = live_group(common, carriers, movement,
                                          executions, context, *journey);
        movement.spatial_bindings.set(
            spatial_carrier_key(journey->carrier),
            {transport.subject_kind, transport.transport_unit_id,
             StableMovementWriter{source.actual_anchor}, ref});
        for (const auto *member : transport.named_participants) {
          const auto &id = *member;
          movement.spatial_bindings.set(
              "actor:" + id,
              {"actor", id, CarriedMovementWriter{journey->carrier},
               transport.registration_source_ref});
        }
        release_active_group(carriers, *journey);
      } else
        take(close_exact_group_unit_in_candidate(
            common, carriers, movement, executions, context, journey_id,
            MovementCloseKind::cancelled, ref, at, actual_writes, budget));
    }
  });
}

namespace {
StableId apply_group_contact_result(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const SourceRef &source,
    const StableId &prefix_id, const WorldMoment &occurred_at,
    const MovementAnchor &anchor, const CanonicalFraction &fraction,
    std::vector<BoundSpatialLocationWrite> &writes,
    ExactArithmeticBudget budget) {
  auto *prefix = movement.prefixes.find(prefix_id);
  const auto *execution =
      prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
             : nullptr;
  const auto *j =
      execution ? carriers.journeys.find(execution->journey_id) : nullptr;
  if (!prefix || !j || j->movement_prefix_ref != prefix_id ||
      !same_moment(prefix->last_settled_at, occurred_at, budget))
    fail("GroupContactSourceRequired", prefix_id);
  const auto journey = *j;
  const auto transport =
      live_group(common, carriers, movement, executions, context, journey);
  auto *unit = movement.units.find(prefix->unit_ref);
  if (!unit)
    fail("MovementUnitRequired", prefix_id);
  const auto unit_id = unit->id;
  if (std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) &&
      !take(
          read_unstarted_held_contact_close(movement, *unit, source, budget))) {
    if (compare(fraction, {}, budget) != 0 ||
        compare(unit->active_elapsed.value, {}, budget) != 0)
      fail("UnpaidPositiveContact", prefix_id);
    auto copy = *unit;
    pay_first_unit(common, economy, payments, carriers, movement, executions,
                   context, journey, *prefix, copy, occurred_at,
                   GroupExpenseContactCause{source});
    movement.units.set(unit_id, std::move(copy));
  }
  const auto exertion = take(close_exact_group_unit_in_candidate(
      common, carriers, movement, executions, context, journey.journey_id,
      MovementCloseKind::contact, source, occurred_at, writes, budget));
  prefix = movement.prefixes.find(prefix_id);
  auto *close = movement.unit_closes.find(unit_id + ":close");
  if (!close)
    fail("MovementCloseSourceRequired", prefix_id);
  close->actual_anchor = anchor;
  prefix->lifecycle = MovementClosed{source, anchor};
  auto *current = carriers.journeys.find(journey.journey_id);
  const auto key = spatial_carrier_key(
      journey.carrier); // 原完整两锚/余程只在contact
                        // source，当前journey不复制route或进度。
  movement.spatial_bindings.set(key, {transport.subject_kind,
                                      transport.transport_unit_id,
                                      StableMovementWriter{anchor}, source});
  for (const auto *member : transport.named_participants) {
    const auto &actor = *member;
    movement.spatial_bindings.set("actor:" + actor,
                                  {"actor", actor,
                                   CarriedMovementWriter{journey.carrier},
                                   transport.registration_source_ref});
  }
  if (auto *step = carriers.steps.find(unit_id + ":group_step"))
    step->to_anchor = anchor;
  project_group(common, carriers, movement, executions, context, *current,
                anchor, source, writes, budget);
  adopt_completed_boundaries(common, carriers, movement, executions, context,
                             journey.journey_id, occurred_at, budget);
  return exertion;
}
StableId apply_group_contact_side(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const StableId &contact_id,
    const StableId &prefix_id, const StableId &encounter, bool interactive,
    std::vector<BoundSpatialLocationWrite> &writes,
    ExactArithmeticBudget budget) {
  const auto *contact = movement.contacts.find(contact_id);
  if (!contact)
    fail("GroupContactSourceRequired", prefix_id);
  const auto anchor = take(read_exact_contact_endpoint_anchor(
      movement, *contact, prefix_id, budget));
  const auto fraction = take(read_exact_contact_fraction(*contact, budget));
  return apply_group_contact_result(
      common, economy, payments, carriers, movement, executions, context,
      {"travel", "movement_contact", contact_id}, prefix_id,
      contact->occurred_at, anchor, fraction, writes, budget);
}

} // namespace
Outcome<StableId> apply_exact_group_boundary_in_candidate(
    CommonStore &common, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context,
    const FieldExactContactReadContext &field,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    std::vector<BoundSpatialLocationWrite> &writes,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    if (&field.common != &common || &field.movement != &movement ||
        &field.carriers != &carriers || &field.groups != &executions ||
        !common.actor_location_capture_identity())
      fail("MovementBoundarySameCandidateRequired", source.id);
    const auto original = take(
        read_movement_boundary_contact_source(field, catalog, source, budget));
    const auto now = take(field.clock.read_current_moment());
    if (!std::holds_alternative<GroupMovementExecution>(
            original.prefix->execution) ||
        !same_moment(now, original.contact->occurred_at, budget) ||
        !std::holds_alternative<MovementUnitOpen>(original.unit->status))
      fail("MovementBoundaryActualGroupRequired", source.id);
    // This original contact table is not mutated by close/payment/capture.
    // Borrow its source, rather than create an unowned duplicate payload.
    const auto &contact = *original.contact;
    return apply_group_contact_result(
        common, economy, payments, carriers, movement, executions, context,
        source, contact.prefix_id, contact.occurred_at, contact.actual_anchor,
        contact.entry_fraction, writes, budget);
  });
}
Outcome<std::vector<StableId>> apply_exact_world_contact_in_candidate(
    CommonStore &common, TravelState &travel, LocalEconomyState &economy,
    EconomicTransportExpenseState &payments, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    const GroupMovementContext &context, const ExactMovementContact &contact,
    const StableId &encounter, bool interactive,
    std::vector<BoundSpatialLocationWrite> &writes,
    ExactArithmeticBudget budget,
    const PersonalMovementFundingRead *personal_funding,
    const CurrentMomentRead *clock) {
  return guard<std::vector<StableId>>([&] {
    if (encounter.empty() || !common.actor_location_capture_identity())
      fail("SpatialLocationCaptureRequired");
    take(install_exact_contact_source(common, movement, contact, budget));
    std::vector<StableId> exertions;
    for (const auto &id : {contact.first_prefix_id, contact.second_prefix_id}) {
      const auto *p = movement.prefixes.find(id);
      if (!p)
        fail("ContactMovementPrefixRequired", id);
      if (std::holds_alternative<PersonalMovementExecution>(p->execution))
        exertions.push_back(take(apply_exact_personal_contact_side(
            common, travel, movement, context.travel_catalog,
            context.travel_services, contact.id, id, encounter, interactive,
            writes, budget, personal_funding, clock)));
      else if (std::holds_alternative<GroupMovementExecution>(p->execution))
        exertions.push_back(apply_group_contact_side(
            common, economy, payments, carriers, movement, executions, context,
            contact.id, id, encounter, interactive, writes, budget));
      else
        fail("UnsupportedContactExecution", id);
    }
    return exertions;
  });
}
Status validate_group_movement_execution(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const GroupMovementExecutionState &executions,
    const EconomicTransportExpenseState &payments,
    const GroupMovementContext &context, ExactArithmeticBudget budget) {
  return status([&] {
    take(validate_group_contact_member_releases(common, movement, executions,
                                                context.member_releases));
    take(validate_transport_entry_manifest_sources(
        common, carriers, movement, executions, context.manifest_history,
        budget));
    take(validate_group_journey_terminals(common, carriers, movement,
                                          executions, budget));
    for(const auto& journey:carriers.journeys.rows())if(std::holds_alternative<ArmyCarrierRef>(journey.second.carrier)){take(context.actual_sources.validate_military_supply_causes());break;}
    NativeGroupTransportExpenseRead expenses(movement, carriers, executions,
                                             context.travel_catalog,
                                             context.carriers);
    for(const auto& row:executions.military_supply_rebinds.rows()) {
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.full.rebinds", &common.state(), nullptr, {"fact", "travel", "group_military_supply_rebind", row.first, 1}, {}, -1});
      take(read_group_military_supply_rebind_source(common,carriers,movement,executions,{"travel","group_military_supply_rebind",row.first},1,budget));
      take(context.actual_sources.validate_military_supply_rebind(row.second));
      const auto* unit=movement.units.find(row.second.unit_ref);
      const TravelExertionBasis* basis=nullptr;
      for(const auto& ref:unit->effort_basis_refs){const auto* e=movement.exertion_bases.find(ref.id);if(e&&basis_id(e->actual_basis)==row.second.basis_ref)basis=e;}
      const auto* map=basis&&std::holds_alternative<TerrainCostBasis>(basis->actual_basis)?context.travel_catalog.maps.find(std::get<TerrainCostBasis>(basis->actual_basis).original_inputs.map_id):nullptr;
      if((map&&(rate_budget(*map,row.second.before_profile)!=row.second.before_budget||rate_budget(*map,row.second.after_profile)!=row.second.after_budget))||(!map&&(!basis||!std::holds_alternative<RoadMovementBasis>(basis->actual_basis)||row.second.before_budget!=0||row.second.after_budget!=0)))
        fail("MilitarySupplyRebindOriginalBudgetMismatch",row.first);
    }
    // 每段真实活动分别核生效后的映射，不允许新率回填供给前或继续用已被替换的旧率。
    for(const auto& pair:movement.units.rows()){
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.full.units", &common.state(), nullptr, {"typed_unit", "travel", "movement_unit", pair.first, 0}, {}, -1});
      const auto& unit=pair.second;const auto* execution=std::get_if<GroupMovementExecution>(&unit.execution);
      if(!execution||!std::holds_alternative<ArmyCarrierRef>(execution->carrier))continue;
      for(const auto& work:unit.activity_spans){
        world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "group.full.activity_rate", &common.state(), nullptr, {"typed_unit", "travel", "movement_unit", unit.id, 0}, {"fact", work.rate_source.domain, work.rate_source.kind, work.rate_source.id, 0}, -1});
        const GroupMilitarySupplyRebindSource* latest=nullptr;std::size_t latest_cut=0;
        for(const auto& change:executions.military_supply_rebinds.rows()){
          world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.full.activity_rebind_candidates", &common.state(), nullptr, {"fact", "travel", "group_military_supply_rebind", change.first, 1}, {}, -1});
          const auto& row=change.second;if(row.unit_ref!=unit.id||row.disposition==GroupSupplyRateDisposition::closed_unit||take(compare_world_moments(row.occurred_at,work.span.from,budget))>0)continue;
          const auto cut=take(read_actual_common_fact_position(common,{"travel","group_military_supply_rebind",row.id}));
          if(!latest||cut>latest_cut){latest=&row;latest_cut=cut;}
        }
        if(!latest)continue;
        const auto* ledger=std::get_if<TerrainUnitBudget>(&unit.budget);
        const auto rate=ledger?add(integer(latest->after_budget,budget),ledger->opening_credit.value,budget):integer(1,budget);
        if(compare(work.budget_rate,integer(latest->after_budget,budget),budget)||compare(work.rate,rate,budget))fail("MilitarySupplyActivityRateMismatch",latest->id);
        const SourceRef expected{"travel","group_military_supply_rebind",latest->id};
        if(!same_source(work.rate_source,expected)){
          const auto* resumed=work.rate_source.domain=="travel"&&work.rate_source.kind=="group_movement_control"?executions.controls.find(work.rate_source.id):nullptr;
          if(!resumed||resumed->journey_id!=execution->journey_id||resumed->operation!=GroupControlOperation::resume||
             take(read_actual_common_fact_position(common,work.rate_source))<=latest_cut||take(compare_world_moments(resumed->occurred_at,work.span.from,budget))>0)
            fail("MilitarySupplyActivitySourceMismatch",latest->id);
        }else if(latest->disposition!=GroupSupplyRateDisposition::running)fail("MilitarySupplyActivityDuringHold",latest->id);
      }
    }
    // 尚未产生下一片 work 的当前映射也必须承接最新供养；不能靠空活动表留下旧率。
    for(const auto& pair:movement.prefixes.rows()){
      world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.full.prefixes", &common.state(), nullptr, {"fact", "travel", "movement_prefix", pair.first, 1}, {}, -1});
      const auto& prefix=pair.second;const auto* execution=std::get_if<GroupMovementExecution>(&prefix.execution);
      if(!execution||!std::holds_alternative<ArmyCarrierRef>(execution->carrier))continue;
      const GroupMilitarySupplyRebindSource* latest=nullptr;std::size_t latest_cut=0;
      for(const auto& change:executions.military_supply_rebinds.rows())if(change.second.unit_ref==prefix.unit_ref){
        const auto cut=take(read_actual_common_fact_position(common,{"travel","group_military_supply_rebind",change.first}));
        if(!latest||cut>latest_cut){latest=&change.second;latest_cut=cut;}}
      if(!latest)continue;
      const auto* unit=movement.units.find(prefix.unit_ref);
      if(!unit||latest->prefix_ref!=prefix.id||latest->basis_ref!=basis_id(prefix.basis))fail("MilitarySupplyCurrentMappingRequired",prefix.id);
      // Field 胜方原直接抵达不建新单位：它以真实 arrival/terminal 换掉最后的 map。
      // 只承接这个已验证终局，不把任意 Closed 或今日旅程状态当成跳过证明。
      if(const auto* terminal=carriers.terminals.find(execution->journey_id)){
        const auto* arrival=terminal->actual_terminal_source.domain=="travel"&&terminal->actual_terminal_source.kind=="movement_arrival"?movement.arrivals.find(terminal->actual_terminal_source.id):nullptr;
        if(arrival&&is_field_group_contact_arrival(movement,*arrival,budget)){
          take(read_actual_group_journey_terminal(common,carriers,movement,executions,execution->journey_id,budget));
          const auto* held=std::get_if<HeldMovementMap>(&prefix.activity_world_map);
          const auto* ledger=std::get_if<TerrainUnitBudget>(&unit->budget);
          const auto* old_run=std::get_if<RunningMovementMap>(&latest->original_mapping);
          const auto* old_hold=std::get_if<HeldMovementMap>(&latest->original_mapping);
          const auto rate=latest->disposition==GroupSupplyRateDisposition::closed_unit?(old_run?old_run->rate:old_hold->resume_rate):
             (ledger?add(integer(latest->after_budget,budget),ledger->opening_credit.value,budget):integer(1,budget));
          const auto funding=latest->disposition==GroupSupplyRateDisposition::closed_unit?(old_run?old_run->budget_rate:old_hold->resume_budget_rate):integer(latest->after_budget,budget);
          if(!held||arrival->unit_ref!=unit->id||arrival->prefix_id!=prefix.id||take(read_actual_common_fact_position(common,terminal->actual_terminal_source))<=latest_cut||
             take(compare_world_moments(terminal->occurred_at,latest->occurred_at,budget))<0||!same_moment(held->effective_at,terminal->occurred_at,budget)||
             !same_source(held->actual_hold_source,terminal->actual_terminal_source)||!same_source(held->resume_rate_source,terminal->actual_terminal_source)||
             compare(held->held_cursor,arrival->cursor,budget)||compare(held->resume_rate,rate,budget)||compare(held->resume_budget_rate,funding,budget))
            fail("MilitarySupplyTerminalMappingRequired",latest->id);
          continue;
        }
      }
      if(latest->disposition==GroupSupplyRateDisposition::closed_unit){
        if(!std::holds_alternative<MovementUnitFinalized>(unit->status)||!same_supply_mapping(prefix.activity_world_map,latest->original_mapping,budget))
          fail("MilitarySupplyClosedMappingRequired",latest->id);
        continue;
      }
      const auto* run=std::get_if<RunningMovementMap>(&prefix.activity_world_map);
      const auto* held=std::get_if<HeldMovementMap>(&prefix.activity_world_map);
      const auto& actual_source=run?run->rate_source:held->resume_rate_source;
      const auto* ledger=std::get_if<TerrainUnitBudget>(&unit->budget);
      const auto rate=ledger?add(integer(latest->after_budget,budget),ledger->opening_credit.value,budget):integer(1,budget);
      if(compare(run?run->rate:held->resume_rate,rate,budget)||compare(run?run->budget_rate:held->resume_budget_rate,integer(latest->after_budget,budget),budget))
        fail("MilitarySupplyCurrentRateMismatch",latest->id);
      const SourceRef expected{"travel","group_military_supply_rebind",latest->id};
      if(same_source(actual_source,expected)){
        if(run&&(latest->disposition!=GroupSupplyRateDisposition::running||!same_moment(run->world_from,latest->occurred_at,budget)||compare(run->cursor_at_from,latest->original_cursor,budget)))
          fail("MilitarySupplyCurrentMappingRequired",latest->id);
      }else{
        const auto* resume=actual_source.domain=="travel"&&actual_source.kind=="group_movement_control"?executions.controls.find(actual_source.id):nullptr;
        if(!resume||resume->journey_id!=execution->journey_id||resume->operation!=GroupControlOperation::resume||
           take(read_actual_common_fact_position(common,actual_source))<=latest_cut||take(compare_world_moments(resume->occurred_at,latest->occurred_at,budget))<0||
           (run&&!same_moment(run->world_from,resume->occurred_at,budget)))fail("MilitarySupplyCurrentMappingSourceMismatch",latest->id);
      }
      if(held){
        const auto* pause=held->actual_hold_source.domain=="travel"&&held->actual_hold_source.kind=="group_movement_control"?executions.controls.find(held->actual_hold_source.id):nullptr;
        if(!pause||pause->journey_id!=execution->journey_id||pause->operation!=GroupControlOperation::pause||!same_moment(pause->occurred_at,held->effective_at,budget))
          fail("MilitarySupplyOriginalHoldRequired",latest->id);
        take(read_actual_common_fact_position(common,held->actual_hold_source));
        if(latest->disposition==GroupSupplyRateDisposition::held&&same_source(actual_source,expected)){
          const auto* original=std::get_if<HeldMovementMap>(&latest->original_mapping);
          if(!original||!same_source(original->actual_hold_source,held->actual_hold_source)||!same_moment(original->effective_at,held->effective_at,budget)||compare(original->held_cursor,held->held_cursor,budget))
            fail("MilitarySupplyCurrentMappingRequired",latest->id);
        }
      }
    }
    for (const auto &row : executions.openings.rows()) {
      const auto &opening = row.second;
      const auto *journey = carriers.journeys.find(opening.journey_id);
      if (row.first != opening.id || !journey ||
          journey->composition_revision != take(read_group_composition_revision_before(common, executions, journey->journey_id)) ||
          !same_moment(journey->started_at, opening.occurred_at, budget) ||
          !common.state().facts.find(opening.id))
        fail("GroupOpeningSourceMismatch", row.first);
      if (std::holds_alternative<HouseholdPolicyGroupInitiator>(
              journey->initiator)) {
        if (!context.household_transport)
          fail("HouseholdTransportOriginalSourcesRequired",
               journey->journey_id);
        take(validate_household_transport_phase_adoption(
            *context.household_transport, *journey));
      }
    }
    for (const auto &row : executions.unit_terms.rows()) {
      validate_original_group_unit_terms(common,movement,executions,row.second,budget);
      const auto *unit = movement.units.find(row.first);
      const auto *effort =
          unit && unit->effort_basis_refs.size() > 0
              ? movement.exertion_bases.find(unit->effort_basis_refs.front().id)
              : nullptr;
      const auto* group_execution=unit?std::get_if<GroupMovementExecution>(&unit->execution):nullptr;
      if (row.first != row.second.unit_ref || !unit || !effort || !group_execution ||
          (std::holds_alternative<ArmyCarrierRef>(group_execution->carrier)&&!same_source(row.second.actual_start_source,effort->original_rule_source)) ||
          row.second.journey_id!=group_execution->journey_id ||
          effort->original_action ||
          fare(context.travel_catalog, effort->actual_basis,
               row.second.motion_profile) != row.second.fare_amount)
        fail("GroupUnitTermsMismatch", row.first);
    }
    std::set<StableId> charged;
    for (const auto &row : executions.expenses.rows()) {
      const auto &source = row.second;
      const SourceRef ref{"travel", "group_transport_expense", row.first};
      const auto basis = take(read_verified_group_expense_basis(
          movement, carriers, executions, context.travel_catalog, ref, 1));
      const auto *journey = carriers.journeys.find(source.journey_id);
      if (!journey || !charged.insert(source.unit_ref).second)
        fail("GroupExpensePaymentCoverage", row.first);
      Integer amount = 0;
      StableId payer;
      if (const auto *military = std::get_if<MilitaryMarchExpenseAuthority>(
              &journey->expense_authority)) {
        if (!context.military_expenses || source.original_payment_receipt)
          fail("MilitaryMarchExpenseSourceReaderRequired", row.first);
        const auto actual =
            take(context.military_expenses->read_actual_military_march_expense(
                ref, 1, *military));
        const auto debit =
            take(context.military_expenses->read_recorded_military_march_debit(
                ref, 1));
        if (!debit || debit->amount != actual.amount ||
            debit->payer_wallet_id != actual.payer_wallet_id)
          fail("MilitaryMarchDebitCoverage", row.first);
        amount = actual.amount;
        payer = actual.payer_wallet_id;
      } else {
        auto actual = take(expenses.read_actual_transport_expense(ref, 1));
        if (!source.original_payment_receipt)
          fail("GroupExpensePaymentCoverage", row.first);
        auto receipt = take(read_transport_payment(payments, ref, 1));
        if (!receipt ||
            !same_source(
                *source.original_payment_receipt,
                {"local_economy", "transport_expense_paid", receipt->id}) ||
            receipt->expense.amount != actual.amount)
          fail("GroupExpensePaymentMismatch", row.first);
        amount = actual.amount;
        payer = receipt->payer_wallet_id;
      }
      const auto *unit = movement.units.find(source.unit_ref);
      if (const auto *first =
              std::get_if<GroupExpensePrefixCause>(&source.actual_cause)) {
        if (!unit || unit->activity_spans.empty() ||
            !same_moment(unit->activity_spans.front().span.from,
                         first->first_actual_span.from, budget) ||
            take(compare_world_moments(unit->activity_spans.front().span.to,
                                       first->first_actual_span.to, budget)) <
                0)
          fail("GroupAdoptedPositiveSpanNotConsumed", row.first);
      }
      const auto *paid =
          unit ? std::get_if<MovementPaymentConsumed>(&unit->payment) : nullptr;
      if (!paid || paid->amount != amount || paid->payer_wallet_id != payer)
        fail("GroupUnitPaymentMismatch", row.first);
    }
    for (const auto &row : executions.unit_terms.rows()) {
      const auto *unit = movement.units.find(row.first);
      if (std::holds_alternative<MovementPaymentConsumed>(unit->payment) !=
          bool(charged.count(row.first)))
        fail("GroupExpensePaymentCoverage", row.first);
    }
    for (const auto &row : carriers.crossings.rows())
      if (std::holds_alternative<GroupMovementExecution>(
              row.second.execution)) {
        take(validate_group_boundary_crossing_source(
            common, executions, carriers, movement, context.travel_catalog, row.second, budget));
        take(context.passage.validate_crossing_decision(row.second));
        take(context.passage.validate_after_positive_crossing(row.second));
        take(context.passage.validate_after_boundary_completion(row.second));
      }
  });
}
namespace {
void require_actual_boundary_anchor(const TravelCatalog &catalog,
                                    const PassageBoundaryRef &boundary,
                                    const MovementBasis &basis,
                                    const MovementAnchor &anchor,
                                    ExactArithmeticBudget budget) {
  if (boundary.id.empty() || boundary.from_site_id.empty() ||
      boundary.to_site_id.empty())
    fail("PassageBoundarySourceRequired");
  if (boundary.kind == PassageBoundaryKind::road_edge) {
    const auto *road = std::get_if<RoadMovementBasis>(&basis);
    if (!road || road->original_inputs.edge_id != boundary.id ||
        compare(road->exact_start_progress, {}, budget) != 0 ||
        !take(
            same_actual_movement_position(anchor, road->start_anchor, budget)))
      fail("PassageNotAtRoadEntry", boundary.id);
    const auto *original = catalog.roads.find(boundary.id);
    if (!original || road->original_inputs.origin != boundary.from_site_id ||
        boundary.map_id != (original->world_map_id.empty()
                                ? catalog.world_map_id
                                : original->world_map_id) ||
        !((original->origin == boundary.from_site_id &&
           original->destination == boundary.to_site_id) ||
          (original->destination == boundary.from_site_id &&
           original->origin == boundary.to_site_id)))
      fail("PassageRoadBoundaryMismatch", boundary.id);
    return;
  }
  const auto *city = catalog.settlements.find(boundary.from_site_id);
  if (!city || catalog.world_map_id != boundary.map_id)
    fail("PassageSiteBoundaryMismatch", boundary.id);
  OriginalMovementAnchor expected{
      WildernessLocation{boundary.map_id, boundary.from_site_id, city->map_xy},
      {"travel", "catalog_settlement", boundary.from_site_id}};
  if (!take(same_spatial_anchor(anchor, expected, budget)))
    fail("PassageNotAtActualBoundary", boundary.id);
}
} // namespace
Outcome<ActualGroupPassageEntry> read_actual_group_passage_entry(
    const SpatialCarrierState &carriers, const MovementExactState &movement,
    const TravelCatalog &catalog, const GroupMovementExecution &execution,
    const PassageBoundaryRef &boundary, const WorldMoment &at,
    ExactArithmeticBudget budget) {
  return guard<ActualGroupPassageEntry>([&] {
    const auto *journey = carriers.journeys.find(execution.journey_id);
    const auto *prefix =
        journey ? movement.prefixes.find(journey->movement_prefix_ref)
                : nullptr;
    const auto *unit = prefix ? movement.units.find(prefix->unit_ref) : nullptr;
    const auto *position =
        journey ? carriers.positions.find(journey->position_ref) : nullptr;
    if (!journey || !prefix || !unit || !position ||
        execution_key(prefix->execution) !=
            execution_key(MovementExecutionRef{execution}) ||
        spatial_carrier_key(journey->carrier) !=
            spatial_carrier_key(execution.carrier) ||
        !same_moment(prefix->last_settled_at, at, budget) ||
        compare(unit->active_elapsed.value, {}, budget) != 0 ||
        unit->ordinal == std::numeric_limits<Integer>::max())
      fail("ActualPassageEntrySourceRequired", execution.journey_id);
    auto anchor = take(read_movement_position(*prefix, budget));
    require_actual_boundary_anchor(catalog, boundary, prefix->basis, anchor,
                                   budget);
    ActualGroupPassageEntry result{execution,
                                   prefix->id,
                                   unit->id,
                                   basis_id(prefix->basis),
                                   unit->ordinal + 1,
                                   journey->composition_revision,
                                   position->revision,
                                   at,
                                   anchor,
                                   {}};
    for (const auto &row : carriers.crossings.rows()) {
      const auto *group =
          std::get_if<GroupMovementExecution>(&row.second.execution);
      if (group && group->journey_id == execution.journey_id &&
          row.second.entry_ordinal == result.entry_ordinal &&
          row.second.boundary.kind == boundary.kind &&
          row.second.boundary.id == boundary.id) {
        if (result.existing_crossing_id)
          fail("DuplicateActualPassageEntry");
        result.existing_crossing_id = row.first;
      }
    }
    return result;
  });
}
Status validate_group_boundary_crossing_source(
    const CommonStore& common,const GroupMovementExecutionState& executions,
    const SpatialCarrierState &carriers, const MovementExactState &movement,
    const TravelCatalog &catalog, const BoundaryCrossingReceipt &crossing,
    ExactArithmeticBudget budget) {
  return status([&] {
    const auto *execution =
        std::get_if<GroupMovementExecution>(&crossing.execution);
    const auto *journey =
        execution ? carriers.journeys.find(execution->journey_id) : nullptr;
    const SourceRef actual_cut{"travel","boundary_crossing",crossing.receipt_id};
    if (!journey || crossing.entry_ordinal < 1 ||
        crossing.composition_revision != take(read_group_composition_revision_before(common,executions,journey->journey_id,&actual_cut)) ||
        spatial_carrier_key(crossing.carrier) !=
            spatial_carrier_key(journey->carrier))
      fail("GroupCrossingSourceMismatch", crossing.receipt_id);
    const MovementUnitState *actual = nullptr;
    for (const auto &row : movement.units.rows()) {
      const auto *group =
          std::get_if<GroupMovementExecution>(&row.second.execution);
      if (group && group->journey_id == journey->journey_id &&
          row.second.ordinal == crossing.entry_ordinal - 1) {
        if (actual)
          fail("DuplicateGroupUnitOrdinal");
        actual = &row.second;
      }
    }
    const auto *effort =
        actual && actual->effort_basis_refs.size() > 0
            ? movement.exertion_bases.find(actual->effort_basis_refs.front().id)
            : nullptr;
    if (!actual || !effort || actual->activity_spans.empty() ||
        !same_moment(actual->activity_spans.front().span.from,
                     crossing.entry_at, budget))
      fail("GroupCrossingActivityRequired", crossing.receipt_id);
    MovementPrefixState start;
    start.basis = effort->actual_basis;
    start.cursor = effort->opened_cursor;
    start.lifecycle = MovementActive{};
    require_actual_boundary_anchor(
        catalog, crossing.boundary, effort->actual_basis,
        take(read_movement_position(start, budget)), budget);
  });
}

Outcome<WorldMoment> read_group_source_moment(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const GroupMovementExecutionState &executions,
    const TransportRegistrationState &registrations, const SourceRef &ref,
    Integer schema, ExactArithmeticBudget budget) {
  return guard<WorldMoment>([&] {
    if (schema != 1 || ref.domain != "travel")
      fail("UnsupportedGroupSource", ref.id);
    const auto *head = common.state().facts.find(ref.id);
    if (!head || head->kind != ref.kind || head->domain != ref.domain)
      fail("GroupSourceHeadRequired", ref.id);
    if (ref.kind == "group_movement_opening") {
      const auto *source = executions.openings.find(ref.id);
      const auto *journey =
          source ? carriers.journeys.find(source->journey_id) : nullptr;
      if (!source || !journey ||
          journey->composition_revision != take(read_group_composition_revision_before(common,executions,journey->journey_id)) ||
          !same_moment(source->occurred_at, journey->started_at, budget))
        fail("GroupOpeningSourceMismatch", ref.id);
      return source->occurred_at;
    }
    if(ref.kind=="group_military_supply_rebind")
      return take(read_group_military_supply_rebind_source(common,carriers,movement,executions,ref,schema,budget))->occurred_at;
    if (ref.kind == "group_journey")
      fail("MutableGroupJourneyHasNoDynamicProjection", ref.id);
    if (ref.kind == "group_journey_step") {
      const auto *step = carriers.steps.find(ref.id);
      const MovementUnitCloseSource *close = nullptr;
      for (const auto &row : movement.unit_closes.rows()) {
        const auto *group =
            std::get_if<GroupMovementExecution>(&row.second.execution);
        if (group && row.second.unit_ref + ":group_step" == ref.id && step &&
            group->journey_id == step->journey_id)
          close = &row.second;
      }
      if (!step || !close ||
          !same_moment(close->occurred_at, step->span.to, budget))
        fail("GroupStepCloseRequired", ref.id);
      return close->occurred_at;
    }
    if (ref.kind == "group_movement_control") {
      const auto *source = executions.controls.find(ref.id);
      if (!source || !carriers.journeys.find(source->journey_id) ||
          !source_valid(source->authority_source))
        fail("GroupControlSourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "transport_registration") {
      const auto *source = registrations.registrations.find(ref.id);
      const auto *unit =
          source ? carriers.transport_units.find(source->transport_unit_id)
                 : nullptr;
      if (!source || !unit ||
          !same_source(unit->registration_source_ref, ref) ||
          !source_valid(source->accepted_scope.business_source))
        fail("TransportRegistrationSourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "transport_release") {
      const auto *source = registrations.releases.find(ref.id);
      if (!source ||
          !carriers.transport_units.find(source->transport_unit_id) ||
          source->released_composition.transport_unit_id !=
              source->transport_unit_id ||
          !source_valid(source->accepted_handover_source))
        fail("TransportReleaseSourceRequired", ref.id);
      return source->occurred_at;
    }
    fail("UnknownGroupSourceKind", ref.kind);
  });
}

Outcome<SourceRef> resume_exact_group_field_boundary_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    FieldBattleWorldState &field, const GroupMovementContext &context,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &field_contact,
    CheckpointMemoryCharge &retained, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &carriers != &x.carriers ||
        &movement != &x.movement || &executions != &x.groups ||
        &field != &x.field || &context.travel_catalog != &x.travel ||
        x.group_context != &context ||
        x.travel_services != &context.travel_services)
      fail("GroupBoundaryContinuationSameWorldRequired", field_contact.id);
    for (const auto &item : movement.boundary_continuations.rows())
      if (same_source(item.second.field_contact, field_contact)) {
        const SourceRef source{"travel", "movement_boundary_continuation",
                               item.first};
        take(read_movement_boundary_continuation_source(x, relations, catalog,
                                                        source, b));
        return source;
      }
    const auto ready = take(read_field_boundary_continuation_source(
        x, relations, catalog, field_contact, b));
    if (!same_moment(ready.actual_now, take(context.carriers.current_moment()),
                     b))
      fail("GroupBoundaryContinuationClockMismatch", field_contact.id);
    const SourceRef contact{"travel", "movement_boundary_contact",
                            ready.original.contact->id};
    const auto suffix =
        take(read_movement_boundary_original_suffix(x, catalog, contact, b));
    const auto &old = *suffix.original.prefix;
    const auto *execution = std::get_if<GroupMovementExecution>(&old.execution);
    auto *journey =
        execution ? carriers.journeys.find(execution->journey_id) : nullptr;
    const auto *closed = std::get_if<MovementClosed>(&old.lifecycle);
    const auto *binding = journey ? movement.spatial_bindings.find(
                                        spatial_carrier_key(journey->carrier))
                                  : nullptr;
    const auto *stable =
        binding ? std::get_if<StableMovementWriter>(&binding->writer) : nullptr;
    if (!execution || !journey ||
        journey->state != GroupJourneyLifecycle::paused ||
        carriers.terminals.find(execution->journey_id) ||
        !journey->stop_reason || !same_source(*journey->stop_reason, contact) ||
        !closed || !stable || old.unit_ref != suffix.original.unit->id ||
        !same_source(closed->original_result_ref, contact) ||
        !same_source(binding->binding_source, contact) ||
        !take(same_spatial_anchor(stable->anchor, suffix.closed->actual_anchor,
                                  b)) ||
        suffix.original.unit->ordinal == std::numeric_limits<Integer>::max())
      fail("GroupBoundaryOriginalWriterRequired", field_contact.id);
    const auto composition =
        live_group(common, carriers, movement, executions, context, *journey);
    const auto manifest = take(read_transport_manifest(
        common, carriers, movement, context.carriers, journey->carrier, b));
    const auto profile = take(context.actual_sources.read_motion_profile(
        manifest, journey->route_profile, ready.actual_now));
    take(context.actual_sources.validate_motion_profile(manifest, profile,
                                                        ready.actual_now));
    if (profile.original_profile.payer_wallet_id != journey->payer_wallet_id)
      fail("GroupPayerSourceChanged");


    (retained).grow(0);
    auto prefix = old;
    const auto id = take(common.next_id("movement_boundary_continuation"));
    const auto unit_id = take(common.next_id("movement_unit"));
    const auto basis_id_value = take(common.next_id("movement_basis"));
    const SourceRef source{"travel", "movement_boundary_continuation", id};
    MovementUnitState next;
    next.id = unit_id;
    next.execution = prefix.execution;
    next.ordinal = suffix.original.unit->ordinal + 1;
    CanonicalFraction rate = integer(1, b), funding;
    if (suffix.terrain) {
      auto params = suffix.terrain->original_inputs;
      const auto *map = context.travel_catalog.maps.find(params.map_id);
      if (!map || params.mode != profile.original_profile.mode ||
          suffix.consumed_points >= Integer(params.points.size()))
        fail("GroupBoundaryOriginalTerrainSuffixRequired", field_contact.id);
      params.points.erase(params.points.begin(),
                          params.points.begin() + suffix.consumed_points);
      if (params.route_index >
          std::numeric_limits<Integer>::max() - suffix.consumed_points)
        fail("GroupBoundaryRouteIndexOverflow");
      params.route_index += suffix.consumed_points;
      params.travel_credit = 0;
      params.paused = false;
      params.encounter_id.clear();
      params.budget = rate_budget(*map, profile);
      params.money_per_tick =
          checked_sum(map->money_per_tick, profile.additional_cost_per_unit);
      funding = integer(params.budget, b);
      rate = funding;
      next.budget = TerrainUnitBudget{};
      prefix.basis = take(make_continued_terrain_cost_basis(
          context.travel_catalog, context.travel_services,
          suffix.closed->actual_anchor, params, rate, basis_id_value, unit_id,
          source, b, &retained));
    } else if (suffix.road) {
      auto road = *suffix.road;
      const auto *edge =
          context.travel_catalog.roads.find(road.original_inputs.edge_id);
      if (!edge || edge->duration_ticks < 1 ||
          edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
        fail("GroupBoundaryOriginalRoadRequired", field_contact.id);
      auto &params = road.original_inputs.params;
      params.paused = false;
      params.has_paused = true;
      params.encounter_id.clear();
      if (!params.returning &&
          !road_is_open(*edge, ready.actual_now.half_day_tick))
        fail("RoadBlocked", edge->id);
      CanonicalFraction start;
      if (const auto *a = std::get_if<RoadFractionalAnchor>(
              &suffix.closed->actual_anchor)) {
        if (a->edge_id != edge->id || a->origin != road.original_inputs.origin)
          fail("GroupBoundaryRoadAnchorMismatch");
        start = a->progress;
      } else if (const auto *a = std::get_if<OriginalMovementAnchor>(
                     &suffix.closed->actual_anchor)) {
        const auto *p = std::get_if<OnRouteLocation>(&a->location);
        if (!p || p->edge_id != edge->id ||
            p->origin != road.original_inputs.origin)
          fail("GroupBoundaryRoadAnchorMismatch");
        start = integer(p->progress, b);
      } else
        fail("GroupBoundaryRoadAnchorRequired");
      const auto maximum = integer(edge->duration_ticks * 100, b),
                 step = integer(100, b);
      if (compare(start, maximum, b) > 0)
        fail("GroupBoundaryRoadProgressInvalid");
      auto end = params.returning
                     ? (compare(start, step, b) < 0 ? CanonicalFraction{}
                                                    : sub(start, step, b))
                     : add(start, step, b);
      if (compare(end, maximum, b) > 0)
        end = maximum;
      if (compare(start, end, b) == 0)
        fail("GroupBoundaryArrivalRequired");
      road.basis_ref = basis_id_value;
      road.original_unit_ref = unit_id;
      road.continued = true;
      road.exact_start_progress = start;
      road.exact_end_progress = end;
      road.start_anchor = suffix.closed->actual_anchor;
      road.end_anchor = RoadFractionalAnchor{
          edge->id, road.original_inputs.origin, end, source};
      road.original_inputs.start = take(floor_fraction(start, b));
      road.original_inputs.end = take(floor_fraction(end, b));
      road.original_inputs.moving = true;
      prefix.basis = std::move(road);
    } else
      fail("GroupBoundaryOriginalBasisRequired");
    prefix.cursor = {};
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = ready.actual_now;
    prefix.lifecycle = MovementActive{};
    prefix.activity_world_map =
        RunningMovementMap{ready.actual_now, {}, rate, funding, source};
    MovementBoundaryContinuationSource continued{
        id,
        prefix.id,
        suffix.original.unit->id,
        unit_id,
        contact,
        field_contact,
        {"military", "field_battle_segment", ready.terminal->fact_id},
        ready.actual_now,
        false};
    movement.boundary_continuations.set(id, std::move(continued));
    take(common.append_fact({id, "movement_boundary_continuation", "travel"}));
    install_unit(common, movement, executions, context, prefix, std::move(next),
                 profile, composition.named_participants, ready.actual_now,
                 source, &retained);
    const auto prefix_id = prefix.id;
    movement.prefixes.set(prefix_id, std::move(prefix));
    movement.spatial_bindings.set(spatial_carrier_key(journey->carrier),
                                  {composition.subject_kind,
                                   composition.transport_unit_id,
                                   WorldMovementWriter{prefix_id}, source});
    journey->state = GroupJourneyLifecycle::travelling;
    journey->stop_reason.reset();
    auto &boundary = field.boundary_contacts.at(field_contact.id);
    boundary.status = FieldBoundaryStatus::Released;
    boundary.exact_continuation_source = source;
    if (boundary.entered_binding) {
      // 原 entered 分流已证明完整入场胜军与真实 Opening；同候选先卸在场占用，
      // 再用原整军 manifest 投影原路停止锚，不能假借初始两军 endpoint 或另发军令。
      if (!boundary.army_id || !ready.original.army ||
          *boundary.army_id != ready.original.army->id)
        fail("FieldBoundaryWinnerOriginalArmyRequired", field_contact.id);
      const auto active = field.army_bindings.find(*boundary.army_id);
      if (active == field.army_bindings.end() || active->second.battle_id != boundary.battle_id)
        fail("FieldBoundaryWinnerOriginalActiveBindingRequired", field_contact.id);
      field.army_bindings.erase(active);
      std::vector<BoundSpatialLocationWrite> no_actor_writes;
      const auto *adopted = movement.prefixes.find(prefix_id);
      if (!adopted) fail("GroupBoundaryAdoptedPrefixRequired", prefix_id);
      project_group(common, carriers, movement, executions, context, *journey,
                    take(read_movement_position(*adopted, b)), source,
                    no_actor_writes, b);
    }
    take(read_movement_boundary_continuation_source(x, relations, catalog,
                                                    source, b));
    return source;
  });
}
Outcome<SourceRef> resume_exact_group_field_winner_in_candidate(
    CommonStore &common, SpatialCarrierState &carriers,
    MovementExactState &movement, GroupMovementExecutionState &executions,
    FieldBattleWorldState &field, const GroupMovementContext &context,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const StableId &battle, const StableId &army,
    CheckpointMemoryCharge &retained, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &carriers != &x.carriers ||
        &movement != &x.movement || &executions != &x.groups ||
        &field != &x.field || &context.travel_catalog != &x.travel ||
        x.group_context != &context ||
        x.travel_services != &context.travel_services)
      fail("FieldWinnerContinuationSameWorldRequired", army);
    for (const auto &item : movement.contact_continuations.rows()) {
      const auto &row = item.second;
      if (row.actual_resolution_source.domain != "military" ||
          row.actual_resolution_source.kind != "field_battle_segment")
        continue;
      const auto unit = movement.units.find(row.closed_unit_ref);
      const auto group =
          unit ? std::get_if<GroupMovementExecution>(&unit->execution)
               : nullptr;
      const auto carrier =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!carrier || carrier->army_id != army)
        continue;
      const auto terminal = take(read_field_segment_settlement_source(
          common, x.military, field, row.actual_resolution_source, &x.clock));
      if (terminal.session->battle_id != battle)
        continue;
      const SourceRef source{"travel", "movement_contact_continuation", row.id};
      take(
          read_field_winner_continuation(x, catalog, battle, army, &source, b));
      return source;
    }
    for (const auto &item : movement.arrivals.rows()) {
      const auto *anchor =
          std::get_if<OriginalMovementAnchor>(&item.second.actual_anchor);
      const auto *group =
          std::get_if<GroupMovementExecution>(&item.second.execution);
      const auto *carrier =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!anchor || anchor->actual_source.domain != "military" ||
          anchor->actual_source.kind != "field_battle_segment" || !carrier ||
          carrier->army_id != army)
        continue;
      const auto terminal = take(read_field_segment_settlement_source(
          common, x.military, field, anchor->actual_source, &x.clock));
      if (terminal.session->battle_id != battle)
        continue;
      const SourceRef actual{"travel", "movement_arrival", item.first};
      take(
          read_field_winner_continuation(x, catalog, battle, army, &actual, b));
      return actual;
    }
    const auto ready = take(
        read_field_winner_continuation(x, catalog, battle, army, nullptr, b));
    const auto &endpoint = ready.original.endpoints[ready.endpoint];
    const auto &old = *ready.prefix;
    const SourceRef contact{"travel", "movement_contact",
                            ready.original.contact->id};
    const auto execution = std::get_if<GroupMovementExecution>(&old.execution);
    auto *journey =
        execution ? carriers.journeys.find(execution->journey_id) : nullptr;
    const auto closed = std::get_if<MovementClosed>(&old.lifecycle);
    const auto binding = journey ? movement.spatial_bindings.find(
                                       spatial_carrier_key(journey->carrier))
                                 : nullptr;
    const auto stable =
        binding ? std::get_if<StableMovementWriter>(&binding->writer) : nullptr;
    if (!journey || journey->state != GroupJourneyLifecycle::paused ||
        carriers.terminals.find(journey->journey_id) || !journey->stop_reason ||
        !same_source(*journey->stop_reason, contact) || !closed || !stable ||
        !same_source(closed->original_result_ref, contact) ||
        !same_source(binding->binding_source, contact) ||
        old.unit_ref != ready.closed_unit->id ||
        !take(same_spatial_anchor(stable->anchor,
                                  endpoint.closed->actual_anchor, b)) ||
        !same_moment(ready.opening_at, take(context.carriers.current_moment()),
                     b))
      fail("FieldWinnerOriginalPausedWriterRequired", army);
    const auto composition =
        live_group(common, carriers, movement, executions, context, *journey);


    (retained).grow(0);
    auto prefix = old;
    auto road = *endpoint.original_basis;
    auto edge = context.travel_catalog.roads.find(road.original_inputs.edge_id);
    if (!edge || edge->duration_ticks < 1 ||
        edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
      fail("FieldWinnerOriginalRoadRequired", army);
    auto &params = road.original_inputs.params;
    params.paused = false;
    params.has_paused = true;
    params.encounter_id.clear();
    CanonicalFraction start;
    if (const auto a = std::get_if<RoadFractionalAnchor>(
            &endpoint.closed->actual_anchor)) {
      if (a->edge_id != edge->id || a->origin != road.original_inputs.origin)
        fail("FieldWinnerRoadAnchorMismatch", army);
      start = a->progress;
    } else if (const auto a = std::get_if<OriginalMovementAnchor>(
                   &endpoint.closed->actual_anchor)) {
      const auto at = std::get_if<OnRouteLocation>(&a->location);
      if (!at || at->edge_id != edge->id ||
          at->origin != road.original_inputs.origin)
        fail("FieldWinnerRoadAnchorMismatch", army);
      start = integer(at->progress, b);
    } else
      fail("FieldWinnerRoadAnchorRequired", army);
    auto maximum = integer(edge->duration_ticks * 100, b);
    const auto step = integer(100, b);
    MovementAnchor resumed_anchor = endpoint.closed->actual_anchor;
    bool advanced_edge = false;
    if (compare(start, params.returning ? CanonicalFraction{} : maximum, b) ==
        0) {
      const auto reached_site =
          params.returning ? edge->origin : edge->destination;
      Location reached = OnRouteLocation{edge->id, road.original_inputs.origin,
                                         take(floor_fraction(start, b))};
      if (take(finish_road_leg(context.travel_catalog,
                               ready.opening_at.half_day_tick, params,
                               reached))) {
        const auto arrival_id = take(common.next_id("movement_arrival"));
        const SourceRef arrival_source{"travel", "movement_arrival",
                                       arrival_id};
        const MovementAnchor arrival_anchor = OriginalMovementAnchor{
            reached,
            {"military", "field_battle_segment", ready.terminal->fact_id}};
        MovementArrivalSource arrival{
            arrival_id,
            old.id,
            ready.closed_unit->id,
            old.execution,
            ready.opening_at,
            arrival_anchor,
            endpoint.original_basis->basis_ref,
            endpoint.closed->cursor,
            0,
            0,
            std::get<SettlementLocation>(reached).settlement_id};
        movement.arrivals.set(arrival_id, std::move(arrival));
        take(common.append_fact({arrival_id, "movement_arrival", "travel"}));
        std::vector<BoundSpatialLocationWrite> no_actor_writes;
        project_group(common, carriers, movement, executions, context, *journey,
                      arrival_anchor, arrival_source, no_actor_writes, b);
        prefix.lifecycle = MovementClosed{arrival_source, arrival_anchor};
        const auto *running =
            std::get_if<RunningMovementMap>(&prefix.activity_world_map);
        const auto rate =
            running ? running->rate
                    : std::get<HeldMovementMap>(prefix.activity_world_map)
                          .resume_rate;
        const auto funding =
            running ? running->budget_rate
                    : std::get<HeldMovementMap>(prefix.activity_world_map)
                          .resume_budget_rate;
        // 已终结而非新增等待业务；不把战场停留时间积分成道路 cursor。
        prefix.activity_world_map =
            HeldMovementMap{prefix.cursor, arrival_source, ready.opening_at,
                            rate,          funding,        arrival_source};
        prefix.last_settled_at = ready.opening_at;
        const auto actual_prefix_id = prefix.id;
        movement.prefixes.set(actual_prefix_id, std::move(prefix));
        journey->state = GroupJourneyLifecycle::arrived;
        journey->last_step_at = ready.opening_at;
        journey->stop_reason = arrival_source;
        carriers.terminals.set(journey->journey_id,
                               {journey->journey_id, arrival_source, 1,
                                ready.opening_at,
                                GroupJourneyTerminalOutcome::arrived});
        release_active_group(carriers, *journey);
        movement.spatial_bindings.set(
            spatial_carrier_key(journey->carrier),
            {composition.subject_kind, composition.transport_unit_id,
             StableMovementWriter{arrival_anchor}, arrival_source});
        field.army_bindings.erase(army);
        take(read_field_winner_continuation(x, catalog, battle, army,
                                            &arrival_source, b));
        return arrival_source;
      }
      const auto *next_location = std::get_if<OnRouteLocation>(&reached);
      const auto *next_edge =
          next_location
              ? context.travel_catalog.roads.find(next_location->edge_id)
              : nullptr;
      if (!next_edge || next_edge->origin != reached_site ||
          next_edge->id == edge->id ||
          !road_is_open(*next_edge, ready.opening_at.half_day_tick))
        fail("RoadBlocked", next_edge ? next_edge->id : edge->id);
      if (next_edge->duration_ticks < 1 ||
          next_edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
        fail("FieldWinnerOriginalRoadRequired", next_edge->id);
      edge = next_edge;
      road.original_inputs.edge_id = edge->id;
      road.original_inputs.origin = next_location->origin;
      start = integer(next_location->progress, b);
      maximum = integer(edge->duration_ticks * 100, b);
      advanced_edge = true;
      resumed_anchor = OriginalMovementAnchor{
          reached,
          {"military", "field_battle_segment", ready.terminal->fact_id}};
    }
    if (!params.returning &&
        !road_is_open(*edge, ready.opening_at.half_day_tick))
      fail("RoadBlocked", edge->id);
    if (compare(start, maximum, b) > 0)
      fail("FieldWinnerRoadProgressInvalid", army);
    auto end = params.returning
                   ? (compare(start, step, b) < 0 ? CanonicalFraction{}
                                                  : sub(start, step, b))
                   : add(start, step, b);
    if (compare(end, maximum, b) > 0)
      end = maximum;
    if (compare(start, end, b) == 0)
      fail("FieldWinnerOriginalArrivalRequired", army);
    if (ready.closed_unit->ordinal == std::numeric_limits<Integer>::max())
      fail("MovementUnitOrdinalOverflow", ready.closed_unit->id);
    const auto manifest = take(read_transport_manifest(
        common, carriers, movement, context.carriers, journey->carrier, b));
    const auto profile = take(context.actual_sources.read_motion_profile(
        manifest, journey->route_profile, ready.opening_at));
    take(context.actual_sources.validate_motion_profile(manifest, profile,
                                                        ready.opening_at));
    if (profile.original_profile.payer_wallet_id != journey->payer_wallet_id)
      fail("GroupPayerSourceChanged", army);
    const auto id = take(common.next_id("movement_contact_continuation"));
    const auto unit_id = take(common.next_id("movement_unit"));
    const auto basis_id_value = take(common.next_id("movement_basis"));
    const SourceRef source{"travel", "movement_contact_continuation", id};
    if (advanced_edge)
      std::get<OriginalMovementAnchor>(resumed_anchor).actual_source = source;
    road.basis_ref = basis_id_value;
    road.original_unit_ref = unit_id;
    road.continued = true;
    road.exact_start_progress = start;
    road.exact_end_progress = end;
    road.start_anchor = resumed_anchor;
    road.end_anchor = RoadFractionalAnchor{
        edge->id, road.original_inputs.origin, end, source};
    road.original_inputs.start = take(floor_fraction(start, b));
    road.original_inputs.end = take(floor_fraction(end, b));
    road.original_inputs.moving = true;
    prefix.basis = std::move(road);
    prefix.cursor = {};
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = ready.opening_at;
    prefix.lifecycle = MovementActive{};
    prefix.activity_world_map =
        RunningMovementMap{ready.opening_at, {}, integer(1, b), {}, source};
    MovementUnitState next;
    next.id = unit_id;
    next.execution = prefix.execution;
    next.ordinal = ready.closed_unit->ordinal + 1;
    MovementContactContinuationSource continued{
        id,
        prefix.id,
        ready.original.contact->id,
        ready.closed_unit->id,
        unit_id,
        {"military", "field_battle_segment", ready.terminal->fact_id},
        1,
        ready.opening_at,
        false};
    movement.contact_continuations.set(id, std::move(continued));
    take(common.append_fact({id, "movement_contact_continuation", "travel"}));
    install_unit(common, movement, executions, context, prefix, std::move(next),
                 profile, composition.named_participants, ready.opening_at,
                 source, &retained);
    const auto prefix_id = prefix.id;
    movement.prefixes.set(prefix_id, std::move(prefix));
    movement.spatial_bindings.set(spatial_carrier_key(journey->carrier),
                                  {composition.subject_kind,
                                   composition.transport_unit_id,
                                   WorldMovementWriter{prefix_id}, source});
    journey->state = GroupJourneyLifecycle::travelling;
    journey->stop_reason.reset();
    std::vector<BoundSpatialLocationWrite> no_actor_writes;
    const auto *adopted_prefix = movement.prefixes.find(prefix_id);
    project_group(common, carriers, movement, executions, context, *journey,
                  take(read_movement_position(*adopted_prefix, b)), source,
                  no_actor_writes, b);
    // Retained contact.original_bindings remains the historical authority.
    field.army_bindings.erase(army);
    take(read_field_winner_continuation(x, catalog, battle, army, &source, b));
    return source;
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 按 Common 原追加顺序折叠，不按 ID 字串或今日表排序推历史。
std::size_t supply_rebind_cut(const CommonStore& c,const SourceRef* before){
 return before?take(read_actual_common_fact_position(c,*before)):c.state().facts.size();
}
const GroupMovementOpeningSource& supply_rebind_opening(const CommonStore& c,
 const GroupMovementExecutionState& g,const StableId& journey){
 const GroupMovementOpeningSource* found=nullptr;
 for(const auto& row:g.openings.rows())if(row.second.journey_id==journey){
  if(found||row.first!=row.second.id)fail("MilitarySupplyRebindOpeningRequired",journey);found=&row.second;
 }
 if(!found)fail("MilitarySupplyRebindOpeningRequired",journey);
 take(read_actual_common_fact_position(c,{"travel","group_journey",journey}));
 take(read_actual_common_fact_position(c,{"travel","group_movement_opening",found->id}));
 return *found;
}
// 所有映射成员都与原 witness 比较；不只比较速率或最后一个来源头。
bool same_supply_mapping(const ActivityWorldMap& a,const ActivityWorldMap& z,ExactArithmeticBudget b){
 if(a.index()!=z.index())return false;
 if(const auto* x=std::get_if<RunningMovementMap>(&a)){const auto& y=std::get<RunningMovementMap>(z);
  return same_moment(x->world_from,y.world_from,b)&&compare(x->cursor_at_from,y.cursor_at_from,b)==0&&
   compare(x->rate,y.rate,b)==0&&compare(x->budget_rate,y.budget_rate,b)==0&&same_source(x->rate_source,y.rate_source);}
 const auto& x=std::get<HeldMovementMap>(a);const auto& y=std::get<HeldMovementMap>(z);
 return compare(x.held_cursor,y.held_cursor,b)==0&&same_source(x.actual_hold_source,y.actual_hold_source)&&
  same_moment(x.effective_at,y.effective_at,b)&&compare(x.resume_rate,y.resume_rate,b)==0&&
  compare(x.resume_budget_rate,y.resume_budget_rate,b)==0&&same_source(x.resume_rate_source,y.resume_rate_source);
}
// 已关闭接触段只能借该真实 contact 原 motion witness，不借今日 prefix 或另一场接触。
void validate_closed_supply_mapping(const CommonStore& c,const MovementExactState& m,
 const MovementUnitCloseSource& close,const GroupMilitarySupplyRebindSource& row,ExactArithmeticBudget b){
 const auto& cause=close.actual_cause;const ExactContactMotionWitness* witness=nullptr;const WorldMoment* at=nullptr;
 if(cause.domain=="travel"&&cause.kind=="movement_contact"){
  const auto* contact=m.contacts.find(cause.id);
  if(contact){if(contact->first_prefix_id==row.prefix_ref&&contact->first_motion_source)witness=&*contact->first_motion_source;
   if(contact->second_prefix_id==row.prefix_ref&&contact->second_motion_source){if(witness)fail("MilitarySupplyClosedMappingRequired",row.id);witness=&*contact->second_motion_source;}
   at=&contact->occurred_at;}
 }else if(cause.domain=="travel"&&cause.kind=="movement_boundary_contact"){
  const auto* contact=m.boundary_contacts.find(cause.id);if(contact&&contact->prefix_id==row.prefix_ref){witness=&contact->motion_source;at=&contact->occurred_at;}
 }
 if(!witness||!at||witness->unit_ref!=row.unit_ref||witness->basis_ref!=row.basis_ref||
    !same_moment(*at,close.occurred_at,b)||!same_supply_mapping(witness->original_mapping,row.original_mapping,b)||
    take(read_actual_common_fact_position(c,cause))>=take(read_actual_common_fact_position(c,{"travel","movement_unit_close",close.id})))
  fail("MilitarySupplyClosedMappingRequired",row.id);
}
// 原 unit terms 必须接真实开程/上一单位关闭/Field continuation，不能两份自造快照互证。
void validate_original_group_unit_terms(const CommonStore& c,const MovementExactState& m,
 const GroupMovementExecutionState& g,const GroupMovementUnitTerms& terms,ExactArithmeticBudget b){
 const auto* unit=m.units.find(terms.unit_ref);const auto* execution=unit?std::get_if<GroupMovementExecution>(&unit->execution):nullptr;
 const auto* effort=unit&&!unit->effort_basis_refs.empty()?m.exertion_bases.find(unit->effort_basis_refs.front().id):nullptr;
 if(execution&&!std::holds_alternative<ArmyCarrierRef>(execution->carrier))return;
 if(g.unit_terms.find(terms.unit_ref)!=&terms||!unit||!execution||!effort||execution->journey_id!=terms.journey_id||
    !same_source(terms.actual_start_source,effort->original_rule_source)||effort->unit_ref!=unit->id||
    execution_key(effort->execution)!=execution_key(unit->execution))fail("GroupUnitOriginalStartRequired",terms.unit_ref);
 const auto& source=terms.actual_start_source;
 const auto source_cut=take(read_actual_common_fact_position(c,source));
 if(source.domain!="travel"||source_cut>=take(read_actual_common_fact_position(c,{"travel","movement_unit",unit->id}))||
    source_cut>=take(read_actual_common_fact_position(c,{"travel","travel_exertion_basis",effort->id})))fail("GroupUnitOriginalStartOrder",unit->id);
 if(source.kind=="group_movement_opening"){
  const auto* opening=g.openings.find(source.id);
  if(!opening||opening->journey_id!=terms.journey_id||unit->ordinal!=0||!same_moment(opening->occurred_at,effort->opened_at,b)||
     !same_group_motion_profile(opening->motion_profile,terms.motion_profile))fail("GroupUnitOriginalOpeningMismatch",unit->id);
 }else if(source.kind=="movement_unit_close"){
  const auto* close=m.unit_closes.find(source.id);const auto* previous=close?m.units.find(close->unit_ref):nullptr;
  const auto* old=previous?g.unit_terms.find(previous->id):nullptr;
  if(!close||!previous||!old||close->kind!=MovementCloseKind::carry||previous->ordinal==std::numeric_limits<Integer>::max()||unit->ordinal!=previous->ordinal+1||
     execution_key(previous->execution)!=execution_key(unit->execution)||!same_moment(close->occurred_at,effort->opened_at,b)||
     !same_group_motion_profile(take(read_group_effective_motion_profile(c,g,*old,&source)),terms.motion_profile))fail("GroupUnitOriginalCarryMismatch",unit->id);
 }else if(source.kind=="movement_contact_continuation"){
  const auto* original=m.contact_continuations.find(source.id);
  if(!original||original->new_unit_ref!=unit->id||!same_moment(original->occurred_at,effort->opened_at,b))fail("GroupUnitOriginalContinuationMismatch",unit->id);
 }else if(source.kind=="movement_boundary_continuation"){
  const auto* original=m.boundary_continuations.find(source.id);
  if(!original||original->new_unit_ref!=unit->id||!same_moment(original->occurred_at,effort->opened_at,b))fail("GroupUnitOriginalContinuationMismatch",unit->id);
 }else fail("GroupUnitOriginalStartRequired",unit->id);
}

}
bool same_group_motion_profile(const GroupMotionProfile& a,const GroupMotionProfile& z){
 return same_source(a.actual_rule_source,z.actual_rule_source)&&a.source_schema==z.source_schema&&
 a.original_profile.mode==z.original_profile.mode&&a.original_profile.payer_wallet_id==z.original_profile.payer_wallet_id&&
 a.original_profile.load_weight==z.original_profile.load_weight&&a.original_profile.naval_budget==z.original_profile.naval_budget&&
 a.additional_cost_per_unit==z.additional_cost_per_unit&&a.fare_payee_wallet_id==z.fare_payee_wallet_id&&
 a.fare_destination_id==z.fare_destination_id&&a.original_load_total==z.original_load_total&&a.original_load_people==z.original_load_people;
}
Outcome<Integer> read_group_composition_revision_before(const CommonStore& c,
 const GroupMovementExecutionState& g,const StableId& journey,const SourceRef* before){
 return guard<Integer>([&]{
  const auto& opening=supply_rebind_opening(c,g,journey);
  Integer revision=opening.composition_revision;const auto end=supply_rebind_cut(c,before);
  const auto opening_cut=take(read_actual_common_fact_position(c,{"travel","group_movement_opening",opening.id}));
  if(opening_cut>=end)fail("MilitarySupplyRebindBeforeOpening",journey);
  for(std::size_t i=0;i<end;++i){const auto& id=c.state().facts.order()[i];world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.history.common_candidates", &c.state(), nullptr, {"common_order_entry", "", "", id, 0}, {}, world_stage_observation::observed_cut(end)});const auto* row=g.military_supply_rebinds.find(id);if(!row||row->journey_id!=journey)continue;
   const auto* head=c.state().facts.find(id);
   if(!head||head->domain!="travel"||head->kind!="group_military_supply_rebind"||row->id!=id||i<=opening_cut||
      row->before_composition_revision!=revision||row->after_composition_revision<=revision||row->cause_fact_ids.size()!=1)
    fail("MilitarySupplyRebindRevisionChainMismatch",id);
   const SourceRef supply{"military","military_supply",row->cause_fact_ids.front()};
   world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "group.revision.supply_cause", &c.state(), nullptr, {"fact", "travel", "group_military_supply_rebind", row->id, 1}, {"fact", supply.domain, supply.kind, supply.id, 1}, world_stage_observation::observed_cut(end)});
   const auto source_cut=take(read_actual_common_fact_position(c,supply));
   if(source_cut<=opening_cut||source_cut>=i)fail("MilitarySupplyRebindSourceCutMismatch",id);
   revision=row->after_composition_revision;
  }
  return revision;
 });
}
Outcome<GroupMotionProfile> read_group_effective_motion_profile(const CommonStore& c,
 const GroupMovementExecutionState& g,const GroupMovementUnitTerms& terms,const SourceRef* before){
 return guard<GroupMotionProfile>([&]{
  const auto* actual=g.unit_terms.find(terms.unit_ref);
  if(actual!=&terms)fail("MilitarySupplyRebindActualTermsRequired",terms.unit_ref);
  auto profile=terms.motion_profile;const auto end=supply_rebind_cut(c,before);
  for(std::size_t i=0;i<end;++i){const auto& id=c.state().facts.order()[i];world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "group.history.common_candidates", &c.state(), nullptr, {"common_order_entry", "", "", id, 0}, {}, world_stage_observation::observed_cut(end)});const auto* row=g.military_supply_rebinds.find(id);if(!row||row->unit_ref!=terms.unit_ref)continue;
   take(read_actual_common_fact_position(c,{"travel","group_military_supply_rebind",id}));
   if(row->journey_id!=terms.journey_id||!same_group_motion_profile(profile,row->before_profile))
    fail("MilitarySupplyRebindProfileChainMismatch",id);
   profile=row->after_profile;
  }
  return profile;
 });
}
// 只按不可变单位、真实 Common 前缀和原映射出处验历史；不反用今日 load。
Outcome<const GroupMilitarySupplyRebindSource*> read_group_military_supply_rebind_source(
 const CommonStore& c,const SpatialCarrierState& carriers,const MovementExactState& movement,
 const GroupMovementExecutionState& groups,const SourceRef& source,Integer schema,ExactArithmeticBudget b){
 world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "group.rebind.reader", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {}, -1});
 return guard<const GroupMilitarySupplyRebindSource*>([&]{
  if(schema!=1||source.domain!="travel"||source.kind!="group_military_supply_rebind")fail("MilitarySupplyRebindSourceRequired",source.id);
  const auto cut=take(read_actual_common_fact_position(c,source));const auto* row=groups.military_supply_rebinds.find(source.id);
  const auto* journey=row?carriers.journeys.find(row->journey_id):nullptr;
  const auto* prefix=row?movement.prefixes.find(row->prefix_ref):nullptr;
  const auto* unit=row?movement.units.find(row->unit_ref):nullptr;
  const auto* execution=unit?std::get_if<GroupMovementExecution>(&unit->execution):nullptr;
  const auto* terms=row?groups.unit_terms.find(row->unit_ref):nullptr;
  if(!row||row->id!=source.id||!journey||!std::holds_alternative<ArmyCarrierRef>(journey->carrier)||!prefix||!unit||!terms||!execution||
     execution->journey_id!=row->journey_id||spatial_carrier_key(execution->carrier)!=spatial_carrier_key(journey->carrier)||
     execution_key(prefix->execution)!=execution_key(unit->execution)||row->cause_fact_ids.size()!=1||row->before_budget<0||row->after_budget<0)
   fail("MilitarySupplyRebindIdentityMismatch",source.id);
  validate_original_group_unit_terms(c,movement,groups,*terms,b);
  const SourceRef supply{"military","military_supply",row->cause_fact_ids.front()};
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "group.rebind.supply_cause", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {"fact", supply.domain, supply.kind, supply.id, 1}, world_stage_observation::observed_cut(cut)});
  const auto supply_cut=take(read_actual_common_fact_position(c,supply));
  if(supply_cut>=cut||take(read_group_composition_revision_before(c,groups,row->journey_id,&source))!=row->before_composition_revision||
     !same_group_motion_profile(take(read_group_effective_motion_profile(c,groups,*terms,&source)),row->before_profile))
   fail("MilitarySupplyRebindOriginalPrefixMismatch",source.id);
  const TravelExertionBasis* effort=nullptr;
  for(const auto& ref:unit->effort_basis_refs){const auto* e=movement.exertion_bases.find(ref.id);if(e&&basis_id(e->actual_basis)==row->basis_ref){if(effort)fail("MilitarySupplyRebindBasisRepeated",source.id);effort=e;}}
  if(!effort||take(read_actual_common_fact_position(c,{"travel","travel_exertion_basis",effort->id}))>=supply_cut||
     take(compare_world_moments(effort->opened_at,row->occurred_at,b))>0)
   fail("MilitarySupplyRebindOriginalMappingRequired",source.id);
  const MovementUnitCloseSource* close=nullptr;
  if(const auto* finalized=std::get_if<MovementUnitFinalized>(&unit->status)){
   const auto* original_close=movement.unit_closes.find(unit->id+":close");
   if(!original_close||!same_source(original_close->actual_cause,finalized->original_close_source))fail("MilitarySupplyOriginalCloseRequired",source.id);
   const auto pos=take(read_actual_common_fact_position(c,{"travel","movement_unit_close",original_close->id}));
   if(pos<supply_cut){close=original_close;if(close->unit_ref!=unit->id||close->prefix_id!=prefix->id||close->kind!=MovementCloseKind::contact)fail("MilitarySupplyJourneyAlreadyClosed",source.id);}
  }
  if(bool(close)!=(row->disposition==GroupSupplyRateDisposition::closed_unit))fail("MilitarySupplyRebindDispositionMismatch",source.id);
  if(const auto* terminal=carriers.terminals.find(row->journey_id))
   if(take(read_actual_common_fact_position(c,terminal->actual_terminal_source))<supply_cut)fail("MilitarySupplyJourneyAlreadyTerminal",source.id);
  auto cursor=effort->opened_cursor;CanonicalFraction elapsed;
  for(const auto& work:unit->activity_spans){
   if(take(compare_world_moments(work.span.from,row->occurred_at,b))>=0)break;
   if(take(compare_world_moments(work.span.to,row->occurred_at,b))>0)fail("MilitarySupplyRebindCrossesActivity",source.id);
   elapsed=add(elapsed,take(elapsed_world_duration(work.span.from,work.span.to,b)).value,b);
   if(work.basis_ref==row->basis_ref)cursor=work.cursor_to;
  }
  if(compare(cursor,row->original_cursor,b)||compare(elapsed,row->original_active_elapsed,b))fail("MilitarySupplyRebindActualWorkMismatch",source.id);
  // 接触已关单位仅拥有输入/责任推进；旧关段的 mapping、工期和收费完全不改。
  if(close){if(compare(close->cursor,cursor,b)||take(compare_world_moments(close->occurred_at,row->occurred_at,b))>0)fail("MilitarySupplyClosedUnitCauseMismatch",source.id);validate_closed_supply_mapping(c,movement,*close,*row,b);world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "group.rebind.validated", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {}, world_stage_observation::observed_cut(cut), row->occurred_at.half_day_tick, true});return row;}
  const auto* run=std::get_if<RunningMovementMap>(&row->original_mapping);
  const auto* held=std::get_if<HeldMovementMap>(&row->original_mapping);
  if((row->disposition==GroupSupplyRateDisposition::running)!=bool(run)||
     (row->disposition==GroupSupplyRateDisposition::held)!=bool(held))fail("MilitarySupplyRebindDispositionMismatch",source.id);
  const auto& rate=run?run->rate:held->resume_rate;
  const auto& funding=run?run->budget_rate:held->resume_budget_rate;
  const auto& rate_source=run?run->rate_source:held->resume_rate_source;
  const auto* terrain=std::get_if<TerrainUnitBudget>(&unit->budget);
  const auto expected_rate=terrain?add(integer(row->before_budget,b),terrain->opening_credit.value,b):integer(1,b);
  if(compare(funding,integer(row->before_budget,b),b)||compare(rate,expected_rate,b))fail("MilitarySupplyRebindOriginalRateMismatch",source.id);
  if(run){if(take(compare_world_moments(run->world_from,row->occurred_at,b))>0||
      compare(add(run->cursor_at_from,mul(run->rate,take(elapsed_world_duration(run->world_from,row->occurred_at,b)).value,b),b),cursor,b))fail("MilitarySupplyRebindActualWorkMismatch",source.id);}
  else{
   if(compare(held->held_cursor,cursor,b)||take(compare_world_moments(held->effective_at,row->occurred_at,b))>0)fail("MilitarySupplyHeldCursorMismatch",source.id);
   const auto* pause=held->actual_hold_source.domain=="travel"&&held->actual_hold_source.kind=="group_movement_control"?groups.controls.find(held->actual_hold_source.id):nullptr;
   if(!pause||pause->journey_id!=row->journey_id||pause->operation!=GroupControlOperation::pause||
      !same_moment(pause->occurred_at,held->effective_at,b)||take(read_actual_common_fact_position(c,held->actual_hold_source))>=supply_cut)
    fail("MilitarySupplyOriginalHoldRequired",source.id);
  }
  bool original=same_source(rate_source,effort->original_rule_source);
  if(original&&run)original=same_moment(run->world_from,effort->opened_at,b)&&compare(run->cursor_at_from,effort->opened_cursor,b)==0;
  if(rate_source.domain=="travel"&&rate_source.kind=="group_military_supply_rebind"){
   const auto* previous=groups.military_supply_rebinds.find(rate_source.id);
   original=previous&&previous->unit_ref==row->unit_ref&&previous->prefix_ref==row->prefix_ref&&previous->basis_ref==row->basis_ref&&
    previous->journey_id==row->journey_id&&previous->disposition!=GroupSupplyRateDisposition::closed_unit&&
    take(read_actual_common_fact_position(c,rate_source))<supply_cut&&previous->after_budget==row->before_budget&&
    same_group_motion_profile(previous->after_profile,row->before_profile);
   if(original&&run)original=same_moment(run->world_from,previous->occurred_at,b)&&compare(run->cursor_at_from,previous->original_cursor,b)==0;
  }else if(rate_source.domain=="travel"&&rate_source.kind=="group_movement_control"){
   const auto* resumed=groups.controls.find(rate_source.id);
   original=resumed&&resumed->journey_id==row->journey_id&&resumed->operation==GroupControlOperation::resume&&
     take(read_actual_common_fact_position(c,rate_source))<supply_cut;
   if(original&&run)original=same_moment(resumed->occurred_at,run->world_from,b);
  }
  if(!original)fail("MilitarySupplyOriginalRateSourceRequired",source.id);
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "group.rebind.validated", &c.state(), nullptr, {"fact", source.domain, source.kind, source.id, schema}, {}, world_stage_observation::observed_cut(cut), row->occurred_at.half_day_tick, true});return row;
 });
}
// 真实未终结旅程保有行军责任；运行、原暂停和接触已关单位分开，不把等候算工期。
Outcome<GroupMilitarySupplyRebindSource> prepare_group_military_supply_rebind(
 const CommonStore& common,const SpatialCarrierState& carriers,const MovementExactState& movement,
 const GroupMovementExecutionState& groups,const GroupMovementContext& context,const StableId& id,
 const WorldMoment& at,ExactArithmeticBudget b){
 return guard<GroupMilitarySupplyRebindSource>([&]{
  const auto* journey=carriers.journeys.find(id);const auto* army=journey?std::get_if<ArmyCarrierRef>(&journey->carrier):nullptr;
  const auto* prefix=journey?movement.prefixes.find(journey->movement_prefix_ref):nullptr;
  const auto* unit=prefix?movement.units.find(prefix->unit_ref):nullptr;
  const auto* terms=unit?groups.unit_terms.find(unit->id):nullptr;
  const auto* binding=army?movement.spatial_bindings.find(spatial_carrier_key(journey->carrier)):nullptr;
  if(!army||!prefix||!unit||!terms||!binding||carriers.terminals.find(id)||
     (journey->state!=GroupJourneyLifecycle::travelling&&journey->state!=GroupJourneyLifecycle::paused)||
     take(compare_world_moments(prefix->last_settled_at,at,b))>0||!same_moment(take(context.carriers.current_moment()),at,b))
   fail("MilitarySupplyActualJourneyRequired",id);
  GroupSupplyRateDisposition disposition;
  if(std::holds_alternative<MovementUnitOpen>(unit->status)){
   const auto* writer=std::get_if<WorldMovementWriter>(&binding->writer);
   if(!writer||writer->prefix_id!=prefix->id)fail("MilitarySupplyActualWorldWriterRequired",id);
   if(std::holds_alternative<MovementActive>(prefix->lifecycle)&&std::holds_alternative<RunningMovementMap>(prefix->activity_world_map)&&journey->state==GroupJourneyLifecycle::travelling){
    if(!same_moment(prefix->last_settled_at,at,b))fail("MilitarySupplyUnsettledJourney",id);disposition=GroupSupplyRateDisposition::running;
   }else if(std::holds_alternative<MovementHeld>(prefix->lifecycle)&&std::holds_alternative<HeldMovementMap>(prefix->activity_world_map)&&journey->state==GroupJourneyLifecycle::paused)disposition=GroupSupplyRateDisposition::held;
   else fail("MilitarySupplyActualJourneyStateRequired",id);
  }else{
   const auto* finalized=std::get_if<MovementUnitFinalized>(&unit->status);
   const auto* close=finalized?movement.unit_closes.find(unit->id+":close"):nullptr;
   const auto* closed=std::get_if<MovementClosed>(&prefix->lifecycle);
   const auto* stable=std::get_if<StableMovementWriter>(&binding->writer);
   if(!close||!closed||!stable||!same_source(close->actual_cause,finalized->original_close_source)||close->kind!=MovementCloseKind::contact||journey->state!=GroupJourneyLifecycle::paused||
      !same_source(close->actual_cause,closed->original_result_ref)||!same_source(binding->binding_source,close->actual_cause)||
      !take(same_spatial_anchor(stable->anchor,close->actual_anchor,b)))fail("MilitarySupplyOriginalContactHoldRequired",id);
   disposition=GroupSupplyRateDisposition::closed_unit;
  }
  (void)live_group(common,carriers,movement,groups,context,*journey);
  const auto manifest=take(read_transport_manifest(common,carriers,movement,context.carriers,journey->carrier,b));
  const auto expected=take(read_group_effective_motion_profile(common,groups,*terms));
  if(manifest.composition_revision!=journey->composition_revision)fail("ArmyActualMovementSpanMismatch",id);
  take(context.actual_sources.validate_motion_profile(manifest,expected,at));
  const auto profile=take(context.actual_sources.read_motion_profile(manifest,journey->route_profile,at));
  const auto* terrain=std::get_if<TerrainCostBasis>(&prefix->basis);
  const auto* map=terrain?context.travel_catalog.maps.find(terrain->original_inputs.map_id):nullptr;
  if(terrain&&!map)fail("GroupTravelBudgetSourceRequired",id);
  GroupMilitarySupplyRebindSource out;out.journey_id=id;out.army_id=army->army_id;out.prefix_ref=prefix->id;out.unit_ref=unit->id;out.basis_ref=basis_id(prefix->basis);out.occurred_at=at;out.disposition=disposition;
  out.before_composition_revision=journey->composition_revision;out.before_profile=profile;out.before_manifest_sources=manifest.source_refs;
  out.original_mapping=prefix->activity_world_map;out.original_cursor=prefix->cursor;out.original_active_elapsed=unit->active_elapsed.value;out.before_budget=map?rate_budget(*map,profile):0;
  return out;
 });
}
Status apply_group_military_supply_rebind_in_candidate(CommonStore& common,SpatialCarrierState& carriers,
 MovementExactState& movement,GroupMovementExecutionState& groups,const GroupMovementContext& context,
 const GroupMilitarySupplyRebindSource& prepared,const MilitarySupplyFact& supply,const WorldMoment& at,ExactArithmeticBudget b){
 return status([&]{
  const auto* original=carriers.journeys.find(prepared.journey_id);const auto* army=original?std::get_if<ArmyCarrierRef>(&original->carrier):nullptr;
  const auto* prefix=movement.prefixes.find(prepared.prefix_ref);const auto* unit=movement.units.find(prepared.unit_ref);
  if(!army||prepared.army_id!=army->army_id||supply.army_id!=army->army_id||supply.cause_fact_ids!=std::vector<StableId>{prepared.journey_id}||
     !prepared.id.empty()||!prepared.cause_fact_ids.empty()||!prefix||!unit||original->movement_prefix_ref!=prefix->id||prefix->unit_ref!=unit->id||
     original->composition_revision!=prepared.before_composition_revision||!same_moment(prepared.occurred_at,at,b)||
     take(compare_world_moments(prefix->last_settled_at,at,b))>0||compare(prefix->cursor,prepared.original_cursor,b)||compare(unit->active_elapsed.value,prepared.original_active_elapsed,b))
   fail("MilitarySupplyRebindPreparedInputChanged",supply.id);
  const SourceRef cause{"military","military_supply",supply.id};take(read_actual_common_fact_position(common,cause));
  for(const auto& old:groups.military_supply_rebinds.rows())if(old.second.cause_fact_ids==std::vector<StableId>{supply.id})fail("MilitarySupplyRebindRepeated",supply.id);
  const auto manifest=take(read_transport_manifest(common,carriers,movement,context.carriers,original->carrier,b));
  const auto profile=take(context.actual_sources.read_motion_profile(manifest,original->route_profile,at));
  auto expected=prepared.before_profile;const auto* item=context.common_catalog.items.find(supply.item_id);
  if(!item||item->weight<0||supply.consumed<0||expected.original_load_people<1||expected.original_load_total<0)fail("MilitarySupplyOriginalLoadRequired",supply.id);
  const __int128 after=static_cast<__int128>(expected.original_load_total)-static_cast<__int128>(item->weight)*supply.consumed;
  if(after<0||after>std::numeric_limits<Integer>::max())fail("MilitarySupplyLoadDeltaMismatch",supply.id);
  expected.original_load_total=static_cast<Integer>(after);
  expected.original_profile.load_weight=expected.original_load_total/expected.original_load_people+(expected.original_load_total%expected.original_load_people!=0);
  if(!same_group_motion_profile(expected,profile))fail("ArmyOriginalMovementProfileChanged",supply.id);
  auto refs=prepared.before_manifest_sources;refs.push_back(cause);
  auto keys=[](const std::vector<SourceRef>& values){std::set<StableId> out;for(const auto& value:values)if(!out.insert(value.domain+":"+value.kind+":"+value.id).second)fail("MilitarySupplyManifestSourceRepeated",value.id);return out;};
  if(keys(refs)!=keys(manifest.source_refs)||manifest.composition_revision<=prepared.before_composition_revision)fail("MilitarySupplyRebindForeignRevision",supply.id);
  const auto* terrain=std::get_if<TerrainCostBasis>(&prefix->basis);
  const auto* map=terrain?context.travel_catalog.maps.find(terrain->original_inputs.map_id):nullptr;if(terrain&&!map)fail("GroupTravelBudgetSourceRequired",supply.id);
  auto row=prepared;row.id=take(common.next_id("group_military_supply_rebind"));row.cause_fact_ids={supply.id};row.after_profile=profile;row.after_manifest_sources=manifest.source_refs;
  row.after_composition_revision=manifest.composition_revision;row.after_budget=map?rate_budget(*map,profile):0;
  const SourceRef source{"travel","group_military_supply_rebind",row.id};
  take(common.append_fact({row.id,source.kind,source.domain}));const auto id=row.id;groups.military_supply_rebinds.set(id,std::move(row));
  if(prepared.disposition==GroupSupplyRateDisposition::running){
   if(terrain)take(rebind_movement_rate(movement,prepared.prefix_ref,at,groups.military_supply_rebinds.find(id)->after_budget,source,b));
   else movement.prefixes.find(prepared.prefix_ref)->activity_world_map=RunningMovementMap{at,prepared.original_cursor,integer(1,b),{},source};
  }else if(prepared.disposition==GroupSupplyRateDisposition::held){
   auto* held=std::get_if<HeldMovementMap>(&movement.prefixes.find(prepared.prefix_ref)->activity_world_map);
   if(!held)fail("MilitarySupplyOriginalHoldRequired",id);
   held->resume_budget_rate=integer(groups.military_supply_rebinds.find(id)->after_budget,b);
   held->resume_rate=terrain?add(held->resume_budget_rate,std::get<TerrainUnitBudget>(unit->budget).opening_credit.value,b):integer(1,b);
   held->resume_rate_source=source;
  } // closed_unit 只推进责任和输入；不触碰已关段 mapping、cursor、工期、支付。

  carriers.journeys.find(prepared.journey_id)->composition_revision=manifest.composition_revision;
  take(context.actual_sources.validate_military_supply_rebind(*groups.military_supply_rebinds.find(id)));
 });
}
} // namespace hanmo::resident
