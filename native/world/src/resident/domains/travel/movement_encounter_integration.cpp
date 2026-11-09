#include "movement_encounter_integration.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/military/field_battle/field_winner_continuation.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
const EncounterRecord &encounter(const InteractionState &state,
                                 const StableId &id) {
  const EncounterRecord *out = nullptr;
  for (const auto &x : state.encounters)
    if (x.id == id) {
      if (out)
        fail("DuplicateExactEncounter", id);
      out = &x;
    }
  if (!out || !out->exact_origin)
    fail("ExactEncounterRequired", id);
  return *out;
}
const MovementPrefixState &contact_side(const MovementExactState &s,
                                        const ExactMovementContact &contact,
                                        const StableId &action) {
  for (const auto &id : {contact.first_prefix_id, contact.second_prefix_id}) {
    const auto *p = s.prefixes.find(id);
    if (p && execution_action(p->execution) == action)
      return *p;
  }
  fail("ExactContactPersonalSideRequired", action);
}
void personal_sides(const MovementExactState &movement,
                    const ExactMovementContact &contact) {
  for (const auto &id : {contact.first_prefix_id, contact.second_prefix_id}) {
    const auto *p = movement.prefixes.find(id);
    if (!p || !std::holds_alternative<PersonalMovementExecution>(p->execution))
      fail("ContactRequiresMilitaryCarrierConsumer", id);
  }
}
} // namespace
Outcome<MomentOwnerSource>
NativePersonalMotionContactSources::install_actual_contact_in_candidate(
    const ExactMovementContact &contact, ExactPhaseWriteContext &context) {
  return guard<MomentOwnerSource>([&] {
    personal_sides(movement_, contact);
    EncounterInstall request;
    const auto &hit = contact.original_hit;
    request.leaders = {hit.a, hit.b};
    if (hit.road) {
      const auto &ends = *hit.road;
      request.action_ids = {ends.first.trace.action_id,
                            ends.second.trace.action_id};
      request.anchors = {Location{ends.first.anchor},
                         Location{ends.second.anchor}};
      request.resume_paused = {ends.first.trace.params.paused,
                               ends.second.trace.params.paused};
      for (auto &order : request.anchor_field_orders)
        order = {"kind", "edge_id", "origin", "progress"};
      request.anchors_order = {hit.a, hit.b};
      if (ends.first.trace.action_id > ends.second.trace.action_id)
        std::swap(request.anchors_order[0], request.anchors_order[1]);
    } else {
      request.action_ids = {hit.first.trace.action_id,
                            hit.second.trace.action_id};
      request.anchors = {Location{hit.first.anchor},
                         Location{hit.second.anchor}};
      request.resume_paused = {hit.first.trace.params.paused,
                               hit.second.trace.params.paused};
    }
    request.apply_motion = [&](const StableId &id, bool interactive) {
      return status([&] {
        for (const auto &action : request.action_ids) {
          const auto &p = contact_side(movement_, contact, action);
          const auto *closed = std::get_if<MovementClosed>(&p.lifecycle);
          if (!closed ||
              !same_source(closed->original_result_ref,
                           {"travel", "movement_contact", contact.id}))
            fail("ExactContactNotTruncated", action);
          if (auto *terrain = travel_.actions.find(action)) {
            terrain->has_encounter_id = true;
            terrain->encounter_id = interactive ? id : StableId{};
            terrain->paused = interactive || terrain->paused;
          } else if (auto *road = travel_.road_actions.find(action)) {
            road->has_paused = road->has_encounter_id = true;
            road->encounter_id = interactive ? id : StableId{};
            road->paused = interactive || road->paused;
          } else
            fail("ExactContactActionPayloadRequired", action);
        }
      });
    };
    auto id = take(install_exact_encounter(
        common_, interaction_, player_, request, movement_,
        {{"travel", "movement_contact", contact.id}, 1}, contact.occurred_at));
    const auto &original = encounter(interaction_, id);
    for (const auto &prefix_id :
         {contact.first_prefix_id, contact.second_prefix_id}) {
      auto arrival = take(adopt_exact_personal_contact_arrival_in_candidate(
          common_, movement_, contact.id, prefix_id, budget_));
      if (arrival) {
        const auto action_id =
            execution_action(movement_.prefixes.find(prefix_id)->execution);
        take(finish_exact_personal_movement_in_candidate(
            common_, movement_, prefix_id, release_action_, budget_));
        ActionCompletion completion;
        completion.completed = true;
        if (!context.advance.completed_actions.insert(action_id,
                                                      std::move(completion)))
          fail("DuplicateMovementActionCompletion", action_id);
      }
    }
    if (original.status == "resolved")
      take(resume_exact_movement_after_encounter_in_candidate(
          common_, travel_, movement_, interaction_, catalog_, services_, id,
          contact.occurred_at, budget_));
    return MomentOwnerSource{{"interaction", "encounter_created", id}, 1};
  });
}
Status NativePersonalMotionContactRead::validate_actual_contact_installation(
    const ExactMovementContact &contact,
    const MomentOwnerSource &source) const {
  return status([&] {
    if (source.source_ref.domain != "interaction" ||
        source.source_ref.kind != "encounter_created" ||
        source.source_schema != 1)
      fail("ExactEncounterInstallationSourceRequired", source.source_ref.id);
    const auto &original = encounter(interaction_, source.source_ref.id);
    if (!same_source(original.exact_origin->contact_source.source_ref,
                     {"travel", "movement_contact", contact.id}) ||
        original.exact_origin->contact_source.source_schema != 1 ||
        !same_moment(original.exact_origin->created_at, contact.occurred_at,
                     budget_))
      fail("ExactEncounterContactMismatch", contact.id);
    auto at = take(read_exact_encounter_source_moment(common_, interaction_,
                                                      source.source_ref, 1));
    if (!same_moment(at, contact.occurred_at, budget_))
      fail("ExactEncounterMomentMismatch", contact.id);
  });
}
Status NativePersonalMotionContactSources::validate_actual_contact_installation(
    const ExactMovementContact &contact,
    const MomentOwnerSource &source) const {
  return NativePersonalMotionContactRead(common_, movement_, interaction_,
                                         budget_)
      .validate_actual_contact_installation(contact, source);
}
Status resume_exact_movement_after_encounter_in_candidate(
    CommonStore &common, TravelState &travel, MovementExactState &movement,
    const InteractionState &interaction, const TravelCatalog &catalog,
    const TravelServices &services, const StableId &encounter_id,
    const WorldMoment &at, ExactArithmeticBudget b) {
  return status([&] {
    const auto &original = encounter(interaction, encounter_id);
    if (original.status != "resolved" || !original.resolved_at ||
        !same_moment(*original.resolved_at, at, b))
      fail("ActualEncounterResolutionRequired", encounter_id);
    const SourceRef actual_resolution{"interaction", "encounter_resolved",
                                      original.result_id};
    if (!same_moment(take(read_exact_encounter_source_moment(
                         common, interaction, actual_resolution, 1)),
                     at, b))
      fail("ActualEncounterResolutionMomentMismatch", encounter_id);
    const auto *contact = movement.contacts.find(
        original.exact_origin->contact_source.source_ref.id);
    if (!contact)
      fail("MovementContactSourceRequired", encounter_id);
    for (std::size_t side = 0; side < original.action_ids.size(); ++side) {
      const auto &action_id = original.action_ids[side];
      const auto &found = contact_side(movement, *contact, action_id);
      if (!common.action(action_id)) {
        NativeMovementActionTimeRead reader(common, travel, movement, catalog,
                                            services, b);
        auto proof = take(reader.read_action_time_owner(
            movement_action_time_registration(), action_id));
        if (proof.phase != ActionTimeOwnerPhase::terminal ||
            !proof.terminal_result_ref)
          fail("ContactReleasedActionSourceRequired", action_id);
        const auto *end =
            movement.personal_terminals.find(*proof.terminal_result_ref);
        if (!end || end->kind != PersonalMovementTerminalKind::arrived ||
            !same_source(end->actual_close_source,
                         {"travel", "movement_contact", contact->id}))
          fail("ContactReleasedActionSourceRequired", action_id);
        continue;
      }
      bool replay = false;
      for (const auto &row : movement.contact_continuations.rows())
        if (row.second.prefix_id == found.id &&
            row.second.contact_ref == contact->id) {
          if (!same_source(row.second.actual_resolution_source,
                           actual_resolution))
            fail("ContactContinuationResolutionMismatch", found.id);
          replay = true;
        }
      if (replay)
        continue;
      const auto *closed = std::get_if<MovementClosed>(&found.lifecycle);
      const auto *old = movement.units.find(found.unit_ref);
      const auto *old_close =
          old ? movement.unit_closes.find(old->id + ":close") : nullptr;
      const auto *action_read = common.action(action_id);
      const auto *members_read =
          action_read ? common.members(action_read->actor_id) : nullptr;
      if (!closed || !old || !old_close ||
          old_close->kind != MovementCloseKind::contact ||
          !same_source(closed->original_result_ref,
                       {"travel", "movement_contact", contact->id}) ||
          !action_read || !members_read)
        fail("MovementContactContinuationRequired", action_id);
      const auto action = *action_read;
      const auto members = *members_read;
      auto prefix = found;
      const auto closed_unit = old->id;
      const auto old_ordinal = old->ordinal;
      if (old_ordinal == std::numeric_limits<Integer>::max())
        fail("MovementUnitOrdinalOverflow", old->id);
      const auto anchor = closed->actual_anchor;
      const auto id = take(common.next_id("movement_contact_continuation")),
                 unit_id = take(common.next_id("movement_unit")),
                 basis_id_value = take(common.next_id("movement_basis"));
      const SourceRef source{"travel", "movement_contact_continuation", id};
      MovementUnitState next;
      next.id = unit_id;
      next.execution = prefix.execution;
      next.ordinal = old_ordinal + 1;
      Integer fatigue = 0;
      CanonicalFraction rate, funding;
      if (const auto *params_read = travel.actions.find(action_id)) {
        auto params = *params_read;
        params.paused = original.resume_paused[side];
        params.encounter_id.clear();
        params.travel_credit = 0;
        if (params.points.empty())
          fail("ContactArrivalContinuationRequired", action_id);
        const auto *location = std::get_if<OriginalMovementAnchor>(&anchor);
        const auto *point =
            location ? std::get_if<WildernessLocation>(&location->location)
                     : nullptr;
        if (!point)
          fail("OriginalContactTerrainAnchorRequired", action_id);
        auto trace = take(terrain_geometry_motion(catalog, services,
                                                  point->point, params, true,
                                                  action.actor_id, action.id));
        prefix.basis = take(make_original_terrain_cost_basis(
            trace, basis_id_value, "terrain_original/1", unit_id, source, b));
        rate = integer(trace.budget, b);
        funding = integer(params.budget, b);
        fatigue = params.fatigue_per_tick;
        next.budget = TerrainUnitBudget{};
        travel.actions.set(action_id, std::move(params));
      } else if (const auto *params_read =
                     travel.road_actions.find(action_id)) {
        auto params = *params_read;
        params.paused = original.resume_paused[side];
        params.encounter_id.clear();
        const auto *location = std::get_if<OriginalMovementAnchor>(&anchor);
        const auto *here =
            location ? std::get_if<OnRouteLocation>(&location->location)
                     : nullptr;
        const auto *edge = here ? catalog.roads.find(here->edge_id) : nullptr;
        if (!edge || edge->duration_ticks < 1 ||
            edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
          fail("OriginalContactRoadAnchorRequired", action_id);
        const auto end = params.returning
                             ? std::max<Integer>(0, here->progress - 100)
                             : std::min<Integer>(edge->duration_ticks * 100,
                                                 here->progress + 100);
        RoadTravelMotion trace{
            action.actor_id, action_id, edge->id, here->origin,
            here->progress,  end,       true,     params};
        prefix.basis = take(make_original_road_movement_basis(
            trace, basis_id_value, "road_original/1", unit_id, source, b));
        rate = integer(1, b);
        fatigue = edge->fatigue_per_tick;
        travel.road_actions.set(action_id, std::move(params));
      } else
        fail("MovementActionPayloadRequired", action_id);
      prefix.cursor = {};
      prefix.unit_ref = unit_id;
      prefix.last_settled_at = at;
      prefix.lifecycle =
          original.resume_paused[side]
              ? MovementLifecycle{MovementHeld{actual_resolution}}
              : MovementLifecycle{MovementActive{}};
      prefix.activity_world_map =
          original.resume_paused[side]
              ? ActivityWorldMap{HeldMovementMap{
                    {}, actual_resolution, at, rate, funding, source}}
              : ActivityWorldMap{
                    RunningMovementMap{at, {}, rate, funding, source}};
      TravelExertionBasis effort{unit_id + ":effort",
                                 unit_id,
                                 prefix.execution,
                                 action,
                                 at,
                                 members,
                                 fatigue,
                                 source,
                                 prefix.basis,
                                 {}};
      next.effort_basis_refs.push_back(
          {"travel", "travel_exertion_basis", effort.id});
      MovementContactContinuationSource continued{
          id,          prefix.id, contact->id,
          closed_unit, unit_id,   actual_resolution,
          1,           at,        original.resume_paused[side]};
      movement.contact_continuations.set(id, std::move(continued));
      movement.exertion_bases.set(effort.id, effort);
      movement.units.set(unit_id, std::move(next));
      movement.prefixes.set(prefix.id, prefix);
      movement.spatial_bindings.set(
          "actor:" + action.actor_id,
          {"actor", action.actor_id, WorldMovementWriter{prefix.id}, source});
      take(common.append_fact({id, "movement_contact_continuation", "travel"}));
      take(common.append_fact({unit_id, "movement_unit", "travel"}));
      take(common.append_fact(
          {unit_id + ":effort", "travel_exertion_basis", "travel"}));
      take(sync_exact_personal_action_time_projection(
          common, travel, movement, catalog, services, action_id, b));
    }
  });
}
Status validate_movement_encounter_continuations(
    const CommonStore &common, const MovementExactState &movement,
    const InteractionState &interaction, ExactArithmeticBudget b,
    const FieldExactContactReadContext *field,
    const FieldBattleCatalog *field_catalog) {
  return status([&] {
    std::set<std::pair<StableId, StableId>> seen;
    for (const auto &row : movement.contact_continuations.rows()) {
      const auto &source = row.second;
      const auto *contact = movement.contacts.find(source.contact_ref);
      const auto *old = movement.units.find(source.closed_unit_ref);
      const auto *next = movement.units.find(source.new_unit_ref);
      const auto *close =
          old ? movement.unit_closes.find(old->id + ":close") : nullptr;
      const auto *effort =
          next && !next->effort_basis_refs.empty()
              ? movement.exertion_bases.find(next->effort_basis_refs.front().id)
              : nullptr;
      const auto *head = common.state().facts.find(source.id);
      if (row.first != source.id ||
          !seen.emplace(source.prefix_id, source.contact_ref).second ||
          !contact || !old || !next || !close || !effort || !head ||
          head->kind != "movement_contact_continuation" ||
          head->domain != "travel" ||
          close->kind != MovementCloseKind::contact ||
          !same_source(close->actual_cause,
                       {"travel", "movement_contact", contact->id}) ||
          old->ordinal == std::numeric_limits<Integer>::max() ||
          next->ordinal != old->ordinal + 1 ||
          execution_key(old->execution) != execution_key(next->execution) ||
          !same_moment(effort->opened_at, source.occurred_at, b) ||
          source.resolution_schema != 1)
        fail("MovementContactContinuationMismatch", row.first);
      if (source.actual_resolution_source.domain == "military" &&
          source.actual_resolution_source.kind == "field_battle_segment") {
        if (!field || !field_catalog || &field->common != &common ||
            &field->movement != &movement)
          fail("FieldWinnerContinuationOriginalOwnerRequired", source.id);
        const auto terminal = take(read_field_segment_settlement_source(
            common, field->military, field->field,
            source.actual_resolution_source, &field->clock));
        const auto group = std::get_if<GroupMovementExecution>(&old->execution);
        const auto army =
            group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
        if (!army)
          fail("FieldWinnerArmyCarrierRequired", source.id);
        const SourceRef ref{"travel", "movement_contact_continuation",
                            source.id};
        take(read_field_winner_continuation(*field, *field_catalog,
                                            terminal.session->battle_id,
                                            army->army_id, &ref, b));
      } else {
        auto at = take(read_exact_encounter_source_moment(
            common, interaction, source.actual_resolution_source,
            source.resolution_schema));
        if (!same_moment(at, source.occurred_at, b))
          fail("MovementContinuationMomentMismatch", source.id);
      }
      MovementPrefixState opening;
      opening.basis = effort->actual_basis;
      opening.cursor = effort->opened_cursor;
      opening.lifecycle = MovementActive{};
      // Field 的完整原读已核同边余程或原路径相邻边的同一真实端点。
      // Personal 仍保旧同锚门，不能借 Field 源放宽其连续性。
      if (!(source.actual_resolution_source.domain == "military" &&
            source.actual_resolution_source.kind == "field_battle_segment") &&
          !take(same_spatial_anchor(close->actual_anchor,
                                    take(read_movement_position(opening, b)),
                                    b)))
        fail("MovementContactContinuationDiscontinuous", source.id);
    }
  });
}
} // namespace hanmo::resident
