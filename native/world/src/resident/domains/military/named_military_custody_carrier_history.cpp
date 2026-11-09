// 直接重放原军团形成、行军段与返营，不向人物旧静置历史借看管位置。
#include "named_military_custody_carrier_history.hpp"
#include "field_battle/field_segment_settlement.hpp"
#include "field_battle/field_reinforcement_queue.hpp"
#include "field_battle/field_world.hpp"
#include "military_formation_source.hpp"
#include "military_spatial_handoff.hpp"
#include "named_military_custody_terms.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/domains/travel/group_member_loss_handoff.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *out = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (out)
        fail("NamedMilitaryCustodyDuplicatePayload", id);
      out = &row;
    }
  return out;
}
bool army_execution(const MovementExecutionRef &execution,
                    const StableId &army) {
  const auto *group = std::get_if<GroupMovementExecution>(&execution);
  const auto *carrier =
      group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
  return carrier && carrier->army_id == army;
}
MovementAnchor basis_point(const MovementBasis &basis,
                           const CanonicalFraction &cursor,
                           ExactArithmeticBudget budget) {
  MovementPrefixState point;
  point.basis = basis;
  point.cursor = cursor;
  point.lifecycle = MovementActive{};
  return take(read_movement_position(point, budget));
}
OriginalNamedMilitaryCustodyCarrierPoint
read_point(const NamedMilitaryCustodyCarrierHistoryContext &context,
           const NamedMilitaryCustodyTerm &term, const ArmyRecord &keeper,
           const GarrisonDefinition *garrison, const SourceRef &custody_source,
           const WorldMoment &at, std::size_t cut,
           const FieldActorMemberLossPlacementRead *captured = nullptr,
           const FieldBattleSession *capture_battle = nullptr) {
  const auto &x = context.original;
  const auto &c = x.custody.common;
  if (!x.custody.clock ||
      unique(x.custody.military.armies, keeper.id) != &keeper)
    fail("NamedMilitaryCustodyActualCarrierRequired", keeper.id);
  const auto now = take(x.custody.clock->read_current_moment());
  if (take(compare_world_moments(at, now, x.budget)) > 0)
    fail("NamedMilitaryCustodyCarrierFutureObservation", term.id);
  const auto order = [&](const SourceRef &source) {
    return take(read_actual_common_fact_position(c, source));
  };
  const auto visible = [&](const WorldMoment &when, const SourceRef &source) {
    const auto ordinal = order(source);
    const auto cmp = take(compare_world_moments(when, at, x.budget));
    if (ordinal < cut) {
      if (cmp > 0)
        fail("NamedMilitaryCustodyCarrierCausalMismatch", source.id);
      return true;
    }
    if (cmp < 0)
      fail("NamedMilitaryCustodyCarrierOmittedEarlierSource", source.id);
    return false;
  };
  const SourceRef term_source{"military", "military_named_custody_term",
                              term.id};
  if (!visible(term.adopted_at, term_source) || order(custody_source) >= cut)
    fail("NamedMilitaryCustodyCarrierCaptureRequired", term.id);
  OriginalNamedMilitaryCustodyCarrierPoint out;
  out.original_sources.push_back({term_source, 1});
  if (!same_source(custody_source, term.capture_source))
    out.original_sources.push_back({custody_source, 1});
  if (garrison) {
    // 真实返营的原应用 M 才能证明原营地位置，旧粗 Tick 不能补作零分数。
    if (!context.returns || !context.returns->bound_to(c, x.custody.military))
      fail("NamedMilitaryCustodyExactReturnPositionRequired", keeper.id);
    const auto returned =
        take(context.returns->read_exact_army_return(keeper.id));
    if (!returned || !returned->original ||
        returned->actual_application.source_schema != 2 ||
        returned->original->garrison_id != garrison->id ||
        std::count(returned->original->prisoner_ids.begin(),
                   returned->original->prisoner_ids.end(),
                   term.actor_id) != 1 ||
        !visible(returned->original->completed_at,
                 returned->actual_application.source_ref))
      fail("NamedMilitaryCustodyExactReturnPositionRequired", keeper.id);
    const auto *place = x.common_catalog.places.find(garrison->place_id);
    if (!place)
      fail("NamedMilitaryCustodyOriginalGarrisonPlaceRequired", garrison->id);
    out.anchor = OriginalMovementAnchor{
        SettlementLocation{place->settlement_id, place->id},
        returned->actual_application.source_ref};
    out.original_sources.push_back(returned->actual_application);
    return out;
  }
  // Field 的局部终局并非 S1 世界离场应用；原离场正文未齐前绝不回退形成时位置。
  if (x.custody.field)
    for (const auto &[id, session] : x.custody.field->sessions) {
      bool involved = false;
      for (const auto &[formation_id, formation] : session.formations) {
        if (formation.army_id != keeper.id)
          continue;
        // A later admission must not be projected back to the session's
        // creation head. Each reinforcement owns its own real entry M/head;
        // same-M observations are ordered by the original Common prefix.
        bool initial = false;
        for (const auto &[contact_id, contact] : x.custody.field->contacts)
          if (contact.battle_id == session.battle_id)
            for (const auto &binding : contact.original_bindings)
              if (binding.army_id == keeper.id)
                for (const auto &member : binding.members)
                  if (member.formation_id == formation_id)
                    initial = true;
        // Absence of an admission is not proof of initial membership. The
        // append-only initial-contact binding classifies that branch; every
        // other formation must retain its actual admission source.
        if (!initial) {
          const auto *entry = take(read_field_reinforcement_admission_identity(
              c, *x.custody.field, session, formation));
          if (!entry)
            fail("NamedMilitaryCustodyActualFieldEntryRequired", formation_id);
          if (visible(entry->applied_at, entry->application_source))
            involved = true;
        } else {
          // Initial-contact formations retain the original session branch.
          // This narrow change does not certify their later world exit.
          involved = true;
        }
      }
      if (involved && order({"military", "field_battle_session", id}) < cut) {
        if (!captured || capture_battle != &session ||
            !captured->original_loss || !captured->carried_army_id ||
            *captured->carried_army_id != keeper.id)
          fail("NamedMilitaryCustodyFieldWorldExitSourceRequired", id);
        const SourceRef loss_source{"travel", "group_member_loss_handoff",
                                    captured->original_loss->id};
        if (!visible(captured->original_loss->occurred_at, loss_source))
          fail("NamedMilitaryCustodyFieldLossOutsideObservation", id);
      }
    }
  if (x.military_handoffs)
    for (const auto &[id, handoff] : x.military_handoffs->handoffs.rows()) {
      const auto *carrier = std::get_if<ArmyCarrierRef>(&handoff.carrier_ref);
      if (carrier && carrier->army_id == keeper.id &&
          visible(handoff.at, {"military", "military_spatial_handoff", id}))
        fail("NamedMilitaryCustodyLocalCarrierExitSourceRequired", id);
    }
  if (!x.military_handoffs)
    for (const auto &[id, head] : c.state().facts.rows())
      if (head.domain == "military" && head.kind == "military_spatial_handoff")
        fail("NamedMilitaryCustodyCarrierHandoffReadRequired", id);
  const auto formed = take(
      read_original_military_formation(c.state(), x.custody.military, keeper,
                                       now, x.custody.formations, x.budget));
  if (formed.completed_source.source_schema != 2 || !formed.original_terms ||
      !visible(formed.completed_at, formed.completed_source.source_ref))
    fail("NamedMilitaryCustodyExactFormationPositionRequired", keeper.id);
  const auto *place =
      x.common_catalog.places.find(formed.original_terms->place_id);
  if (!place)
    fail("NamedMilitaryCustodyOriginalFormationPlaceRequired", keeper.id);
  out.anchor = OriginalMovementAnchor{
      SettlementLocation{place->settlement_id, place->id},
      formed.completed_source.source_ref};
  WorldMoment latest = formed.completed_at;
  std::size_t latest_order = order(formed.completed_source.source_ref);
  MomentOwnerSource latest_source = formed.completed_source;
  const auto formation_order = latest_order;
  if (captured) {
    const auto &loss = *captured->original_loss;
    const SourceRef source{"travel", "group_member_loss_handoff", loss.id};
    if (!visible(loss.occurred_at, source) ||
        order(source) <= formation_order ||
        take(compare_world_moments(loss.occurred_at, formed.completed_at,
                                   x.budget)) < 0)
      fail("NamedMilitaryCustodyFieldLossOrderMismatch", term.id);
    out.anchor = captured->actual_placement.actual_anchor;
    latest = loss.occurred_at;
    latest_order = order(source);
    latest_source = {source, 1};
    out.original_sources.push_back({source, 1});
  }

  auto adopt = [&](const WorldMoment &when, const MovementAnchor &anchor,
                   const SourceRef &source) {
    if (!visible(when, source))
      return;
    const auto cmp = take(compare_world_moments(when, latest, x.budget));
    const auto ordinal = order(source);
    if (ordinal <= formation_order ||
        take(compare_world_moments(when, formed.completed_at, x.budget)) < 0)
      fail("NamedMilitaryCustodyCarrierBeforeFormation", source.id);
    if (cmp > 0 || (cmp == 0 && ordinal > latest_order)) {
      out.anchor = anchor;
      latest = when;
      latest_order = ordinal;
      latest_source = {source, 1};
    } else if (cmp == 0 && ordinal == latest_order &&
               !take(
                   same_actual_movement_position(out.anchor, anchor, x.budget)))
      fail("NamedMilitaryCustodyCarrierPositionAmbiguous", source.id);
  };
  take(validate_movement_exact_values(x.movement, x.budget));
  // 本叶采用的真实头必须仍有原正文；删掉旧段不能让军团退回形成时位置。
  for (const auto &[id, head] : c.state().facts.rows()) {
    bool recognized = true, present = false;
    if (head.kind == "group_movement_opening")
      present = context.group_executions.openings.find(id);
    else if (head.kind == "group_movement_control")
      present = context.group_executions.controls.find(id);
    else if (head.kind == "travel_exertion_basis")
      present = x.movement.exertion_bases.find(id);
    else if (head.kind == "movement_unit_close")
      present = x.movement.unit_closes.find(id);
    else if (head.kind == "movement_arrival")
      present = x.movement.arrivals.find(id);
    else
      recognized = false;
    if (recognized && (head.domain != "travel" || !present))
      fail("NamedMilitaryCustodyCarrierOriginalPayloadRequired", id);
  }
  for (const auto &[id, opening] : context.group_executions.openings.rows()) {
    const auto *carrier = std::get_if<ArmyCarrierRef>(&opening.carrier_origin);
    if (!carrier || carrier->army_id != keeper.id)
      continue;
    const auto *journey = x.carriers.journeys.find(opening.journey_id);
    const auto *actual =
        journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
    const SourceRef source{"travel", "group_movement_opening", id};
    if (opening.id != id || !actual || actual->army_id != keeper.id ||
        !opening.military_order)
      fail("NamedMilitaryCustodyCarrierOpeningRequired", id);
    take(read_group_source_moment(c, x.carriers, x.movement,
                                  context.group_executions, x.registrations,
                                  source, 1, x.budget));
    adopt(opening.occurred_at, opening.original_anchor, source);
  }
  for (const auto &[id, unit] : x.movement.units.rows())
    if (army_execution(unit.execution, keeper.id)) {
      const auto &execution = std::get<GroupMovementExecution>(unit.execution);
      const auto *journey = x.carriers.journeys.find(execution.journey_id);
      const auto *carrier =
          journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
      if (!carrier || carrier->army_id != keeper.id)
        fail("NamedMilitaryCustodyCarrierJourneyRequired", id);
      for (const auto &source : unit.effort_basis_refs) {
        const auto *basis = x.movement.exertion_bases.find(source.id);
        if (source.domain != "travel" ||
            source.kind != "travel_exertion_basis" || !basis ||
            basis->unit_ref != id ||
            !army_execution(basis->execution, keeper.id))
          fail("NamedMilitaryCustodyCarrierBasisRequired", id);
        take(read_movement_source_moment(x.movement, source, 1, x.budget));
        adopt(basis->opened_at,
              basis_point(basis->actual_basis, basis->opened_cursor, x.budget),
              source);
        for (const auto &work : unit.activity_spans)
          if (work.basis_ref == basis_id(basis->actual_basis)) {
            take(make_moment_span(work.span.from, work.span.to, x.budget));
            if (take(compare_world_moments(work.span.from, work.span.to,
                                           x.budget)) >= 0 ||
                take(compare_world_moments(work.span.from, basis->opened_at,
                                           x.budget)) < 0 ||
                compare(
                    work.cursor_to,
                    add(work.cursor_from,
                        mul(work.rate,
                            take(elapsed_world_duration(work.span.from,
                                                        work.span.to, x.budget))
                                .value,
                            x.budget),
                        x.budget),
                    x.budget))
              fail("NamedMilitaryCustodyCarrierActivityMismatch", id);
            if (visible(basis->opened_at, source) &&
                take(compare_world_moments(work.span.from, at, x.budget)) < 0 &&
                take(compare_world_moments(at, work.span.to, x.budget)) <= 0) {
              const auto cursor = add(
                  work.cursor_from,
                  mul(work.rate,
                      take(elapsed_world_duration(work.span.from, at, x.budget))
                          .value,
                      x.budget),
                  x.budget);
              // 位置由原不可变 basis 与保留的正区间导出，不取今天可变 prefix
              // 的游标。
              adopt(at, basis_point(basis->actual_basis, cursor, x.budget),
                    source);
            }
          }
      }
      if (const auto *close = x.movement.unit_closes.find(id + ":close")) {
        const SourceRef source{"travel", "movement_unit_close", close->id};
        if (close->unit_ref != id ||
            !army_execution(close->execution, keeper.id))
          fail("NamedMilitaryCustodyCarrierCloseRequired", id);
        take(read_movement_source_moment(x.movement, source, 1, x.budget));
        adopt(close->occurred_at, close->actual_anchor, source);
      }
    }
  for (const auto &[id, arrival] : x.movement.arrivals.rows())
    if (army_execution(arrival.execution, keeper.id)) {
      const SourceRef source{"travel", "movement_arrival", id};
      take(read_verified_movement_arrival(x.movement, source, 1, x.budget));
      adopt(arrival.occurred_at, arrival.actual_anchor, source);
    }
  for (const auto &[id, control] : context.group_executions.controls.rows()) {
    const auto *journey = x.carriers.journeys.find(control.journey_id);
    const auto *carrier =
        journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
    if (!carrier || carrier->army_id != keeper.id)
      continue;
    const SourceRef source{"travel", "group_movement_control", id};
    take(read_group_source_moment(c, x.carriers, x.movement,
                                  context.group_executions, x.registrations,
                                  source, 1, x.budget));
    adopt(control.occurred_at, control.actual_anchor, source);
  }
  for (const auto &[id, unit] : x.movement.units.rows())
    if (army_execution(unit.execution, keeper.id))
      for (const auto &work : unit.activity_spans)
        if (take(compare_world_moments(work.span.to, at, x.budget)) <= 0 &&
            take(compare_world_moments(work.span.to, latest, x.budget)) > 0)
          fail("NamedMilitaryCustodyCarrierStationaryCloseRequired", id);
  for (const auto &[id, prefix] : x.movement.prefixes.rows())
    if (army_execution(prefix.execution, keeper.id) &&
        !std::holds_alternative<MovementClosed>(prefix.lifecycle))
      if (const auto *running =
              std::get_if<RunningMovementMap>(&prefix.activity_world_map))
        if (take(compare_world_moments(running->world_from, at, x.budget)) <
                0 &&
            take(compare_world_moments(prefix.last_settled_at, at, x.budget)) <
                0)
          fail("NamedMilitaryCustodyCarrierUnsettledMotion", id);
  out.original_sources.push_back(std::move(latest_source));
  return out;
}
} // namespace
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_current_named_military_custody_carrier_position(
    const NamedMilitaryCustodyCarrierHistoryContext &context,
    const NamedMilitaryCustodyTerm &term) {
  return guard<OriginalNamedMilitaryCustodyCarrierPoint>([&] {
    const auto &x = context.original;
    if (take(read_original_named_military_custody_term(
            x.custody, {"military", "military_named_custody_term", term.id})) !=
            &term ||
        !x.custody.clock)
      fail("NamedMilitaryCustodyActualTermRequired", term.id);
    for (const auto &release : x.custody.military.named_custody.closures)
      if (release.term_id == term.id)
        fail("NamedMilitaryCustodyCurrentTermAlreadyClosed", term.id);
    const auto current =
        take(read_current_named_military_custody(x.custody, term.actor_id));
    if (!current)
      fail("NamedMilitaryCustodyCurrentSourceRequired", term.actor_id);
    const auto *keeper =
        unique(x.custody.military.armies, current->captor_army_id);
    const auto *garrison =
        current->garrison_id
            ? unique(x.custody.catalog.garrisons, *current->garrison_id)
            : nullptr;
    if (!keeper || (current->garrison_id && !garrison))
      fail("NamedMilitaryCustodyCaptorRequired", term.id);
    const auto prefix = take(read_current_common_fact_prefix(x.custody.common));
    auto result = read_point(
        context, term, *keeper, garrison, current->current_custody_source,
        take(x.custody.clock->read_current_moment()), prefix.common_fact_count);
    // 热读再与原当前 S1 唯一 writer
    // 对证；不能仅因历史结果看似同地就创建看管绑定。
    const auto actual = take(read_actual_actor_spatial_location(
        x.custody.common, x.movement, x.spatial, x.battle, term.actor_id,
        x.budget));
    if (!take(same_actual_movement_position(
            result.anchor,
            *take(require_actor_ordinary_movement_anchor(actual)), x.budget)))
      fail("NamedMilitaryCustodyCurrentCarrierPositionMismatch", term.id);
    return result;
  });
}
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_original_named_military_custody_carrier_before_release(
    const NamedMilitaryCustodyCarrierHistoryContext &context,
    const NamedMilitaryCustodyRelease &release) {
  return guard<OriginalNamedMilitaryCustodyCarrierPoint>([&] {
    const auto &x = context.original;
    take(validate_named_military_custody_terms(x.custody));
    const auto prior =
        take(read_named_military_custody_before_release(x.custody, release));
    if (!prior.term || !prior.keeper_army)
      fail("NamedMilitaryCustodyReleaseTermRequired", release.id);
    const auto cut = take(read_actual_common_fact_position(
        x.custody.common,
        SourceRef{"military", "military_named_custody_released", release.id}));
    return read_point(context, *prior.term, *prior.keeper_army, prior.garrison,
                      prior.last_custody_source, release.released_at, cut);
  });
}
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_named_military_custody_carrier_at_original_prefix(
    const NamedMilitaryCustodyCarrierHistoryContext &context,
    const NamedMilitaryCustodyTerm &term, const WorldMoment &at,
    const CommonActualFactPrefix &prefix) {
  return guard<OriginalNamedMilitaryCustodyCarrierPoint>([&] {
    const auto &x = context.original;
    const auto prior = take(read_named_military_custody_at_original_prefix(
        x.custody, term, at, prefix, context.returns));
    const auto cut =
        take(read_actual_common_fact_prefix(x.custody.common, prefix));
    if (prior.term != &term || !prior.keeper_army || !prior.actual_capture)
      fail("NamedMilitaryCustodyActualHistoricalKeeperRequired", term.id);
    const auto *parent =
        x.custody.common.state().facts.find(prior.actual_capture->battle_id);
    const bool field_capture = parent && parent->domain == "military" &&
                               parent->kind == "field_battle_segment";
    if (!field_capture)
      return read_point(context, term, *prior.keeper_army, prior.garrison,
                        prior.last_custody_source, at, cut);
    if (!x.custody.field || !x.custody.field_applications ||
        !context.field_exits ||
        !context.field_exits->bound_to(x.custody.common, x.movement))
      fail("NamedMilitaryCustodyFieldLossSourceRequired", term.id);
    const SourceRef casualty{"military", "military_casualties",
                             prior.actual_capture->id};
    const auto actual = take(read_field_segment_captivity_source(
        x.custody.common, x.custody.military, *x.custody.field, casualty,
        term.actor_id, *x.custody.field_applications));
    const auto losses =
        take(context.field_exits->read_field_member_loss_placement_history(
            term.actor_id));
    const FieldActorMemberLossPlacementRead *loss = nullptr;
    for (const auto &row : losses) {
      if (!row.original_loss ||
          !same_source(row.original_loss->actual_casualty, casualty))
        continue;
      if (loss ||
          context.group_executions.member_loss_handoffs.find(
              row.original_loss->id) != row.original_loss ||
          row.original_loss->actor_id != term.actor_id ||
          !std::holds_alternative<GroupMemberCaptivityApplication>(
              row.original_loss->actual_body) ||
          !row.carried_army_id ||
          *row.carried_army_id != prior.keeper_army->id ||
          !actual.actor_change ||
          actual.actor_change->captor_army_id != *row.carried_army_id ||
          !actual.source.settlement || !actual.source.session ||
          row.actual_placement.actor_id != term.actor_id ||
          row.actual_placement.immutable_source_head.size() != 1 ||
          take(compare_world_moments(row.original_loss->occurred_at,
                                     actual.source.settlement->applied_at,
                                     x.budget)) != 0 ||
          take(compare_world_moments(row.actual_placement.occurred_at,
                                     row.original_loss->occurred_at,
                                     x.budget)) != 0)
        fail("NamedMilitaryCustodyFieldLossMismatch", term.id);
      const SourceRef source{"travel", "group_member_loss_handoff",
                             row.original_loss->id};
      const auto &head = row.actual_placement.immutable_source_head.front();
      const auto term_order = take(read_actual_common_fact_position(
          x.custody.common,
          SourceRefBuildView{"military", "military_named_custody_term",
                             term.id}));
      const auto loss_order =
          take(read_actual_common_fact_position(x.custody.common, source));
      if (loss_order <= term_order || head.schema != 1 ||
          !same_source(head.source, source) ||
          take(read_actual_common_fact_position(x.custody.common, source)) >=
              cut ||
          take(compare_world_moments(row.original_loss->occurred_at, at,
                                     x.budget)) > 0)
        fail("NamedMilitaryCustodyFieldLossOutsideObservation", term.id);
      loss = &row;
    }
    if (!loss)
      fail("NamedMilitaryCustodyFieldLossSourceRequired", term.id);
    return read_point(context, term, *prior.keeper_army, prior.garrison,
                      prior.last_custody_source, at, cut, loss,
                      actual.source.session);
  });
}
} // namespace hanmo::resident
