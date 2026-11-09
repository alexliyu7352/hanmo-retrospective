// 精确前缀只写本次候选；技术拆片、暂停、存读不重开原收费单位。
#include "movement_boundary_apply.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 真实身份对应唯一world writer；战场持有者不可继续行旅。
void require_writer(const MovementExactState &state,
                    const MovementPrefixState &prefix, const StableId &actor) {
  const auto *binding = state.spatial_bindings.find("actor:" + actor);
  const auto *world =
      binding ? std::get_if<WorldMovementWriter>(&binding->writer) : nullptr;
  if (!world || world->prefix_id != prefix.id)
    fail("MovementWriterNotWorld", actor);
}
// 同一Action安装一次，恢复不能重新获得开段credit/费用状态。
void unused_action(const MovementExactState &state, const StableId &action) {
  for (const auto &id : state.prefixes.order()) {
    const auto *p = state.prefixes.find(id);
    if (p && execution_action(p->execution) == action &&
        !std::holds_alternative<MovementClosed>(p->lifecycle))
      fail("DuplicateMovementExecution", action);
  }
}
// First positive prefix and actual zero-time contact share the original debit.
void consume_personal_unit_payment(
    CommonWrite &common, const TravelState &travel,
    const TravelCatalog &catalog, const TravelServices &services,
    MovementExactState &state, const MovementPrefixState &prefix,
    MovementUnitState &unit, const PersonalMovementActualStart &actual_start,
    const CurrentMomentRead &clock, const PersonalMovementFundingRead *funding,
    ExactArithmeticBudget budget) {
  if (std::holds_alternative<MovementPaymentConsumed>(unit.payment))
    return;
  const auto cost = take(read_unconsumed_personal_unit_cost(
      common, travel, state, catalog, services, {prefix.id, actual_start},
      clock, budget));
  if (cost.unit_ref != unit.id)
    fail("PersonalMovementCostUnitMismatch", unit.id);
  if (funding) {
    if (!funding->bound_to(common, state))
      fail("PersonalMovementFundingCandidateMismatch", unit.id);
    take(funding->validate_new_personal_unit_cost(cost));
  }
  const auto *wallet = common.wallet(cost.payer_wallet_id);
  if (!wallet)
    fail("MovementWalletRequired", cost.payer_wallet_id);
  const Integer before = wallet->balance;
  ResourceRecipe recipe;
  recipe.operations.push_back(
      WalletDelta{cost.payer_wallet_id, -cost.amount, true});
  take(common.apply_resources(recipe));
  const auto *after = common.wallet(cost.payer_wallet_id);
  if (!after)
    fail("MovementWalletRequired", cost.payer_wallet_id);
  const auto balance_after = after->balance;
  const auto payment_id = take(common.next_id("movement_expense"));
  MovementExpenseSource payment{payment_id,       prefix.id,
                                unit.id,          cost.payer_wallet_id,
                                prefix.execution, cost.occurred_at,
                                cost.amount,      before,
                                balance_after,    cost.original_cost_rule_ref};
  state.expenses.set(payment_id, std::move(payment));
  take(common.append_fact({payment_id, "movement_expense", "travel"}));
  unit.payment =
      MovementPaymentConsumed{{"travel", "movement_expense", payment_id},
                              cost.payer_wallet_id,
                              cost.amount};
}
// 累计余额只从四个已存真实量推导，不另存可独立花费的credit。
CanonicalFraction balance(const TerrainUnitBudget &unit,
                          ExactArithmeticBudget b) {
  return sub(sub(add(unit.opening_credit.value, unit.funded.value, b),
                 unit.spent.value, b),
             unit.discarded.value, b);
}
} // namespace
// 从已真实安装的terrain
// Action取原一次geometry输出，预算和起点均不来自外部请求。
Outcome<StableId> install_exact_personal_terrain_movement(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const StableId &action_id, const WorldMoment &at,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    unused_action(state, action_id);
    const auto *borrowed_action = common.action(action_id);
    const auto *borrowed_params = travel.actions.find(action_id);
    if (!borrowed_action || !borrowed_params)
      fail("ExactTravelInstallationSourceRequired", action_id);
    const auto action_copy = *borrowed_action;
    const auto params_copy = *borrowed_params;
    const auto *action = &action_copy;
    const auto *params = &params_copy;
    if (action->kind != "travel" || params->paused ||
        !params->encounter_id.empty())
      fail("ExactTravelInstallationSourceRequired", action_id);
    auto trace =
        take(terrain_motion(common, catalog, services, *action, *params));
    if (!trace.moving)
      fail("MovementNotAllowed", action_id);
    const auto actor_id = action->actor_id;
    if (take(services.active_army(actor_id)))
      fail("ArmyGroupMovementEntryRequired", actor_id);
    const auto *member_read = common.members(actor_id);
    if (!member_read)
      fail("InvalidParty", actor_id);
    const auto participants = *member_read;
    for (const auto &id : participants) {
      const auto *previous = state.spatial_bindings.find("actor:" + id);
      if (previous &&
          !std::holds_alternative<StableMovementWriter>(previous->writer))
        fail("SpatialWriterAlreadyOwned", id);
    }
    auto prefix_id = take(common.next_id("movement_prefix")),
         unit_id = take(common.next_id("movement_unit")),
         basis_id = take(common.next_id("movement_basis"));
    SourceRef source{"travel", "terrain_journey", params->plan_id};
    auto basis = take(make_original_terrain_cost_basis(
        trace, basis_id, "terrain_original/1", unit_id, source, budget));
    MovementPrefixState prefix;
    prefix.id = prefix_id;
    prefix.execution = PersonalMovementExecution{action_id, params->plan_id, 0};
    prefix.basis = std::move(basis);
    prefix.last_settled_at = at;
    prefix.unit_ref = unit_id;
    prefix.activity_world_map =
        RunningMovementMap{at,
                           {},
                           integer(trace.budget, budget),
                           integer(params->budget, budget),
                           source};
    MovementUnitState unit;
    unit.id = unit_id;
    unit.execution = prefix.execution;
    TerrainUnitBudget ledger;
    ledger.opening_credit.value = integer(params->travel_credit, budget);
    unit.budget = ledger;
    TravelExertionBasis effort{unit_id + ":effort",
                               unit_id,
                               prefix.execution,
                               *action,
                               at,
                               participants,
                               params->fatigue_per_tick,
                               {"travel", "terrain_journey", params->plan_id},
                               prefix.basis,
                               {}};
    unit.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort.id});
    {
      const auto stored_key = effort.id;
      state.exertion_bases.set(stored_key, std::move(effort));
    }
    state.units.set(unit_id, std::move(unit));
    state.prefixes.set(prefix_id, std::move(prefix));
    take(common.append_fact({prefix_id, "movement_prefix", "travel"}));
    take(common.append_fact({unit_id, "movement_unit", "travel"}));
    take(common.append_fact(
        {unit_id + ":effort", "travel_exertion_basis", "travel"}));
    state.spatial_bindings.set(
        "actor:" + actor_id,
        {"actor", actor_id, WorldMovementWriter{prefix_id}, source});
    return prefix_id;
  });
}
// 原道路trace包含真实夹边后的方向与path，不从整数Location单独猜行程。
Outcome<StableId> install_exact_personal_road_movement(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const RoadTravelMotion &trace, const WorldMoment &at,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    unused_action(state, trace.action_id);
    const auto *action = common.action(trace.action_id);
    const auto *params = travel.road_actions.find(trace.action_id);
    const auto *actor = common.actor(trace.actor_id);
    if (!action || !params || !actor || action->kind != "travel" ||
        action->actor_id != trace.actor_id)
      fail("ExactRoadInstallationSourceRequired", trace.action_id);
    if (take(services.active_army(trace.actor_id)))
      fail("ArmyGroupMovementEntryRequired", trace.actor_id);
    take(can_step_road_travel(common, catalog, services, *action, *params));
    const auto action_copy = *action;
    const auto *member_read = common.members(trace.actor_id);
    if (!member_read)
      fail("InvalidParty", trace.actor_id);
    const auto participants = *member_read;
    for (const auto &id : participants) {
      const auto *previous = state.spatial_bindings.find("actor:" + id);
      if (previous &&
          !std::holds_alternative<StableMovementWriter>(previous->writer))
        fail("SpatialWriterAlreadyOwned", id);
    }
    auto id = take(common.next_id("movement_prefix")),
         unit_id = take(common.next_id("movement_unit")),
         basis_id = take(common.next_id("movement_basis"));
    SourceRef source{"travel", "road_action", trace.action_id};
    MovementPrefixState prefix;
    prefix.id = id;
    prefix.execution =
        PersonalMovementExecution{trace.action_id, trace.action_id, 0};
    prefix.basis = take(make_original_road_movement_basis(
        trace, basis_id, "road_original/1", unit_id, source, budget));
    prefix.last_settled_at = at;
    prefix.unit_ref = unit_id;
    prefix.activity_world_map =
        RunningMovementMap{at, {}, integer(1, budget), {}, source};
    MovementUnitState unit;
    unit.id = unit_id;
    unit.execution = prefix.execution;
    const auto *members = common.members(trace.actor_id);
    const auto *edge = catalog.roads.find(trace.edge_id);
    if (!members || !edge)
      fail("RoadExertionSourceRequired");
    TravelExertionBasis effort{unit_id + ":effort",
                               unit_id,
                               prefix.execution,
                               action_copy,
                               at,
                               participants,
                               edge->fatigue_per_tick,
                               source,
                               prefix.basis,
                               {}};
    unit.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort.id});
    {
      const auto stored_key = effort.id;
      state.exertion_bases.set(stored_key, std::move(effort));
    }
    state.units.set(unit_id, std::move(unit));
    state.prefixes.set(id, std::move(prefix));
    take(common.append_fact({id, "movement_prefix", "travel"}));
    take(common.append_fact({unit_id, "movement_unit", "travel"}));
    take(common.append_fact(
        {unit_id + ":effort", "travel_exertion_basis", "travel"}));
    state.spatial_bindings.set(
        "actor:" + trace.actor_id,
        {"actor", trace.actor_id, WorldMovementWriter{id}, source});
    return id;
  });
}
// 只计算真实from→to，遇basis/原单位边界必须交原owner处理，不越过due。
Outcome<PreparedMovementPrefix>
prepare_movement_prefix(const MovementExactState &state, const StableId &id,
                        const MomentSpan &span, ExactArithmeticBudget budget) {
  return guard<PreparedMovementPrefix>([&] {
    const auto *prefix = state.prefixes.find(id);
    const auto *unit = prefix ? state.units.find(prefix->unit_ref) : nullptr;
    const auto *running =
        prefix ? std::get_if<RunningMovementMap>(&prefix->activity_world_map)
               : nullptr;
    if (!prefix || !unit || !running ||
        !std::holds_alternative<MovementActive>(prefix->lifecycle) ||
        !std::holds_alternative<MovementUnitOpen>(unit->status))
      fail("MovementNotRunning", id);
    if (!same_moment(prefix->last_settled_at, span.from, budget))
      fail("MovementPrefixFromMismatch", id);
    take(make_moment_span(span.from, span.to, budget));
    auto elapsed = take(elapsed_world_duration(span.from, span.to, budget));
    if (compare(elapsed.value, CanonicalFraction{}, budget) <= 0)
      fail("PositiveMovementPrefixRequired", id);
    PreparedMovementPrefix out;
    out.prefix_id = id;
    out.unit_ref = unit->id;
    out.span = span;
    out.active_elapsed_after.value =
        add(unit->active_elapsed.value, elapsed.value, budget);
    auto one = integer(1, budget);
    if (compare(out.active_elapsed_after.value, one, budget) > 0)
      fail("MovementSliceCrossesUnitBoundary", id);
    auto map_elapsed =
        take(elapsed_world_duration(running->world_from, span.to, budget));
    out.cursor_after =
        add(running->cursor_at_from,
            mul(running->rate, map_elapsed.value, budget), budget);
    auto expected_start = add(
        running->cursor_at_from,
        mul(running->rate,
            take(elapsed_world_duration(running->world_from, span.from, budget))
                .value,
            budget),
        budget);
    if (compare(expected_start, prefix->cursor, budget) != 0)
      fail("MovementMapCursorMismatch", id);
    auto limit = basis_limit(prefix->basis, budget);
    if (compare(out.cursor_after, limit, budget) > 0)
      fail("MovementSliceCrossesBasisBoundary", id);
    out.at_unit_boundary =
        compare(out.active_elapsed_after.value, one, budget) == 0;
    out.at_basis_end = compare(out.cursor_after, limit, budget) == 0;
    out.first_payment =
        std::holds_alternative<MovementPaymentNotConsumed>(unit->payment);
    if (const auto *basis = std::get_if<TerrainCostBasis>(&prefix->basis)) {
      const auto *old = std::get_if<TerrainUnitBudget>(&unit->budget);
      if (!old)
        fail("TerrainUnitBudgetRequired", id);
      out.terrain_after = *old;
      out.terrain_after->funded.value =
          add(old->funded.value,
              mul(running->budget_rate, elapsed.value, budget), budget);
      auto before =
               take(movement_consumed_cost(*basis, prefix->cursor, budget)),
           after =
               take(movement_consumed_cost(*basis, out.cursor_after, budget));
      out.terrain_after->spent.value =
          add(old->spent.value, sub(after, before, budget), budget);
      balance(*out.terrain_after, budget);
      out.arrived =
          basis->reached_points == basis->original_point_count &&
          compare(after, integer(basis->actual_consumed_total, budget),
                  budget) == 0 &&
          (out.at_basis_end || out.at_unit_boundary);
    }
    auto result = *prefix;
    result.cursor = out.cursor_after;
    out.anchor_after = take(read_movement_position(result, budget));
    return out;
  });
}
// 仅合法已选正片在真实 from 采用；不挪动位置或给工，尾失败由完整候选撤销。
Status adopt_exact_personal_positive_span_from_in_candidate(
    CommonWrite &common, const TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const CurrentMomentRead &clock, const StableId &id, const MomentSpan &span,
    ExactArithmeticBudget budget, const PersonalMovementFundingRead *funding) {
  return status([&] {
    if (!same_moment(take(clock.read_current_moment()), span.from, budget))
      fail("PersonalMovementAdoptionNotAtFrom", id);
    const auto prepared =
        take(prepare_movement_prefix(state, id, span, budget));
    const auto *prefix = state.prefixes.find(id);
    const auto *personal =
        prefix ? std::get_if<PersonalMovementExecution>(&prefix->execution)
               : nullptr;
    const auto *action =
        personal ? common.action(personal->action_id) : nullptr;
    if (!prefix || !personal || !action || action->kind != "travel")
      fail("PersonalMovementActionRequired", id);
    require_writer(state, *prefix, action->actor_id);
    const auto *members = common.members(action->actor_id);
    if (!members)
      fail("InvalidParty", action->actor_id);
    for (const auto &member : *members) {
      const auto *actor = common.actor(member);
      if (!actor || !actor->alive || !actor->available)
        fail("ParticipantBusy", member);
    }
    if (!prepared.first_payment)
      return;
    const auto *old = state.units.find(prepared.unit_ref);
    if (!old || !old->activity_spans.empty() ||
        compare(old->active_elapsed.value, {}, budget) != 0)
      fail("UnpaidPersonalMovementAlreadyAdvanced", id);
    auto unit = *old;
    consume_personal_unit_payment(
        common, travel, catalog, services, state, *prefix, unit,
        PersonalMovementFirstPositivePrefix{span}, clock, funding, budget);
    const auto key = unit.id;
    state.units.set(key, std::move(unit));
  });
}
// 原支付来源须对应实际首正片或真实零时接触；不接受孤立已扣费的稳定发布。
Status validate_personal_movement_expense_sources(
    const CommonStore &common, const MovementExactState &state,
    ExactArithmeticBudget budget, const PersonalMovementFundingRead *funding,
    const TravelCatalog *catalog,
    const PersonalMovementPartyCostHistoryRead *party_history) {
  return status([&] {
    if (funding && !funding->bound_to(common, state))
      fail("PersonalMovementFundingCandidateMismatch");
    for (const auto &row : state.units.rows())
      if (std::holds_alternative<PersonalMovementExecution>(
              row.second.execution) &&
          !row.second.activity_spans.empty()) {
        const auto *paid =
            std::get_if<MovementPaymentConsumed>(&row.second.payment);
        const auto *expense =
            paid ? state.expenses.find(paid->actual_expense_source.id)
                 : nullptr;
        if (!expense || expense->unit_ref != row.first)
          fail("PersonalMovementExpenseCoverageMismatch", row.first);
      }
    for (const auto &row : state.expenses.rows()) {
      const auto &expense = row.second;
      if (!std::holds_alternative<PersonalMovementExecution>(expense.execution))
        continue;
      const auto *unit = state.units.find(expense.unit_ref);
      const auto *paid =
          unit ? std::get_if<MovementPaymentConsumed>(&unit->payment) : nullptr;
      const auto *header = common.state().facts.find(expense.id);
      if (!unit || !paid || !header || header->domain != "travel" ||
          header->kind != "movement_expense" ||
          !same_source(paid->actual_expense_source,
                       {"travel", "movement_expense", expense.id}) ||
          execution_key(unit->execution) != execution_key(expense.execution) ||
          paid->payer_wallet_id != expense.payer_wallet_id ||
          paid->amount != expense.amount)
        fail("PersonalMovementExpenseSourceMismatch", row.first);
      if (catalog)
        take(validate_personal_movement_recorded_cost(
            common, state, *catalog, expense, party_history, budget));
      if (!unit->activity_spans.empty()) {
        const auto &first = unit->activity_spans.front().span;
        if (take(compare_world_moments(first.from, first.to, budget)) >= 0 ||
            !same_moment(expense.occurred_at, first.from, budget))
          fail("PersonalMovementExpenseFirstPrefixMismatch", row.first);
      } else {
        const auto *close = state.unit_closes.find(unit->id + ":close");
        if (close && close->actual_cause.domain == "travel" &&
            close->actual_cause.kind == "movement_boundary_contact") {
          const auto *contact =
              state.boundary_contacts.find(close->actual_cause.id);
          const auto *head = common.state().facts.find(close->actual_cause.id);
          if (!head || head->domain != "travel" ||
              head->kind != "movement_boundary_contact" || !contact ||
              close->kind != MovementCloseKind::contact ||
              contact->prefix_id != close->prefix_id ||
              contact->motion_source.unit_ref != unit->id ||
              !std::holds_alternative<RunningMovementMap>(
                  contact->motion_source.original_mapping) ||
              !same_moment(expense.occurred_at, contact->occurred_at, budget) ||
              compare(unit->active_elapsed.value, {}, budget) != 0 ||
              compare(contact->entry_fraction, {}, budget) != 0)
            fail("PersonalMovementExpenseBoundaryStartRequired", row.first);
        } else {
          const auto *contact =
              close && close->actual_cause.domain == "travel" &&
                      close->actual_cause.kind == "movement_contact"
                  ? state.contacts.find(close->actual_cause.id)
                  : nullptr;
          if (!close || close->kind != MovementCloseKind::contact || !contact ||
              !same_moment(expense.occurred_at, contact->occurred_at, budget) ||
              compare(unit->active_elapsed.value, {}, budget) != 0 ||
              compare(take(read_exact_contact_fraction(*contact, budget)), {},
                      budget) != 0)
            fail("PersonalMovementExpenseActualStartRequired", row.first);
          bool participant = false;
          for (const auto &witness :
               {contact->first_motion_source, contact->second_motion_source})
            if (witness && witness->unit_ref == unit->id &&
                std::holds_alternative<RunningMovementMap>(
                    witness->original_mapping))
              participant = true;
          if (!participant)
            fail("PersonalMovementZeroContactSourceMismatch", row.first);
        }
      }
      if (funding)
        take(funding->validate_recorded_personal_unit_cost(expense));
    }
  });
}
// 已付款单位不重复校验将要再次付款；成员/身体与唯一writer仍每次重核。
Status apply_exact_personal_prefix(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const PreparedMovementPrefix &prepared,
    std::vector<BoundSpatialLocationWrite> *actual_writes,
    ExactArithmeticBudget budget) {
  return status([&] {
    auto *store = dynamic_cast<CommonStore *>(&common);
    if (!store || !actual_writes || !store->actor_location_capture_identity())
      fail("SpatialLocationCaptureRequired");
    auto actual = take(prepare_movement_prefix(state, prepared.prefix_id,
                                               prepared.span, budget));
    if (compare(actual.cursor_after, prepared.cursor_after, budget) != 0 ||
        actual.unit_ref != prepared.unit_ref)
      fail("StaleMovementPrefix", prepared.prefix_id);
    auto prefix = *state.prefixes.find(actual.prefix_id);
    auto unit = *state.units.find(actual.unit_ref);
    const auto action_id = execution_action(prefix.execution);
    const auto *action = common.action(action_id);
    if (!action || action->kind != "travel")
      fail("MovementActionRequired", action_id);
    const auto actor_id = action->actor_id;
    require_writer(state, prefix, actor_id);
    const auto *members = common.members(actor_id);
    if (!members)
      fail("InvalidParty", actor_id);
    for (const auto &id : *members) {
      const auto *actor = common.actor(id);
      if (!actor || !actor->alive || !actor->available)
        fail("ParticipantBusy", id);
    }
    if (actual.first_payment)
      fail("PersonalMovementFromAdoptionRequired", prepared.prefix_id);
    const auto mapping =
        std::get<RunningMovementMap>(prefix.activity_world_map);
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
    ActorMutation projection;
    projection.actor_id = actor_id;
    projection.location =
        take(project_movement_location(actual.anchor_after, budget));
    take(common.apply_actor(projection));
    {
      const auto stored_key = unit.id;
      state.units.set(stored_key, std::move(unit));
    }
    const auto written_prefix_id = prefix.id;
    {
      const auto stored_key = prefix.id;
      state.prefixes.set(stored_key, std::move(prefix));
    }
    actual_writes->push_back(take(bind_actual_spatial_location_write(
        *store, state, actor_id,
        {"travel", "movement_prefix", written_prefix_id}, 1,
        actual.anchor_after, 0, budget)));
  });
}
// 只准已结到M的真实停工，不能倒结过去或抹掉原成本尺。
Status hold_movement_prefix(MovementExactState &state, const StableId &id,
                            const WorldMoment &at, const SourceRef &cause,
                            ExactArithmeticBudget budget) {
  return status([&] {
    auto *p = state.prefixes.find(id);
    if (!p || !source_valid(cause) ||
        !same_moment(p->last_settled_at, at, budget) ||
        std::holds_alternative<MovementClosed>(p->lifecycle))
      fail("InvalidMovementHold", id);
    const auto *running =
        std::get_if<RunningMovementMap>(&p->activity_world_map);
    if (!running)
      fail("MovementAlreadyHeld", id);
    p->activity_world_map = HeldMovementMap{
        p->cursor,           cause, at, running->rate, running->budget_rate,
        running->rate_source};
    p->lifecycle = MovementHeld{cause};
  });
}
// 恢复只重接同cursor，从实际恢复点开始累计，暂停时间不算工作。
Status resume_movement_prefix(MovementExactState &state, const StableId &id,
                              const WorldMoment &at, const SourceRef &source,
                              ExactArithmeticBudget budget) {
  return status([&] {
    auto *p = state.prefixes.find(id);
    const auto *unit = p ? state.units.find(p->unit_ref) : nullptr;
    const auto *held =
        p ? std::get_if<HeldMovementMap>(&p->activity_world_map) : nullptr;
    if (!p || !unit || !held || !source_valid(source) ||
        !std::holds_alternative<MovementHeld>(p->lifecycle) ||
        take(compare_world_moments(at, p->last_settled_at, budget)) < 0)
      fail("InvalidMovementResume", id);
    auto rate = held->resume_rate;
    auto funding = held->resume_budget_rate;
    p->activity_world_map =
        RunningMovementMap{at, p->cursor, rate, funding, source};
    p->last_settled_at = at;
    p->lifecycle = MovementActive{};
  });
}
// 仅有效预算率改变才重接mapping；不把已存余额再次塞入本单位rate。
Status rebind_movement_rate(MovementExactState &state, const StableId &id,
                            const WorldMoment &at, Integer new_budget,
                            const SourceRef &source,
                            ExactArithmeticBudget budget) {
  return status([&] {
    auto *p = state.prefixes.find(id);
    const auto *u = p ? state.units.find(p->unit_ref) : nullptr;
    if (!p || !u || !source_valid(source) || new_budget < 1 ||
        !same_moment(p->last_settled_at, at, budget) ||
        !std::holds_alternative<RunningMovementMap>(p->activity_world_map) ||
        !std::holds_alternative<TerrainCostBasis>(p->basis))
      fail("InvalidMovementRateRebind", id);
    auto funding = integer(new_budget, budget);
    // 同预算也重新锚定真实生效 M/cursor，保留供养 cause 的分段，不能把新头回填旧 mapping 起点。
    auto rate = add(funding,
                    std::get<TerrainUnitBudget>(u->budget).opening_credit.value,
                    budget);
    p->activity_world_map =
        RunningMovementMap{at, p->cursor, rate, funding, source};
  });
}
// 尚未发生任何step的Held身体只接受真实接触关闭，不消费步费、不发用力/导航源。
Outcome<bool> read_unstarted_held_contact_close(const MovementExactState &state,
                                                const MovementUnitState &unit,
                                                const SourceRef &source,
                                                ExactArithmeticBudget b) {
  return guard<bool>([&] {
    if (!std::holds_alternative<MovementPaymentNotConsumed>(unit.payment) ||
        compare(unit.active_elapsed.value, {}, b) != 0 ||
        !unit.activity_spans.empty())
      return false;
    const auto *contact =
        source.domain == "travel" && source.kind == "movement_contact"
            ? state.contacts.find(source.id)
            : nullptr;
    if (!contact)
      return false;
    for (const auto &witness :
         {contact->first_motion_source, contact->second_motion_source})
      if (witness && witness->unit_ref == unit.id &&
          std::holds_alternative<HeldMovementMap>(witness->original_mapping)) {
        const auto &held = std::get<HeldMovementMap>(witness->original_mapping);
        if (!source_valid(held.actual_hold_source) ||
            take(compare_world_moments(held.effective_at, contact->occurred_at,
                                       b)) > 0)
          fail("UnstartedHeldContactSourceMismatch", unit.id);
        return true;
      }
    return false;
  });
}
// 真实unit关闭一次消费余预算，source相同重放只返回原件，不重扣已付费用。
Status close_movement_unit(MovementExactState &state, const StableId &id,
                           MovementCloseKind kind, const SourceRef &source,
                           const WorldMoment &at,
                           ExactArithmeticBudget budget) {
  return status([&] {
    auto *prefix = state.prefixes.find(id);
    auto *unit = prefix ? state.units.find(prefix->unit_ref) : nullptr;
    if (!prefix || !unit || !source_valid(source) ||
        !same_moment(prefix->last_settled_at, at, budget))
      fail("InvalidMovementUnitClose", id);
    if (const auto *done = std::get_if<MovementUnitFinalized>(&unit->status)) {
      if (done->original_close_source.domain == source.domain &&
          done->original_close_source.kind == source.kind &&
          done->original_close_source.id == source.id)
        return;
      fail("MovementUnitAlreadyFinalized", unit->id);
    }
    if (std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) &&
        (kind != MovementCloseKind::contact ||
         !take(
             read_unstarted_held_contact_close(state, *unit, source, budget))))
      fail("UnpaidMovementUnitClose", unit->id);
    if (kind == MovementCloseKind::carry &&
        compare(unit->active_elapsed.value, integer(1, budget), budget) != 0)
      fail("MovementCarryBeforeUnitBoundary", unit->id);
    if (auto *ledger = std::get_if<TerrainUnitBudget>(&unit->budget)) {
      auto before = balance(*ledger, budget),
           discard = kind == MovementCloseKind::arrival ||
                             kind == MovementCloseKind::contact
                         ? before
                         : CanonicalFraction{};
      ledger->discarded.value = add(ledger->discarded.value, discard, budget);
      auto disposition_id = unit->id + ":credit";
      TerrainCreditDispositionSource disposition{
          disposition_id,
          prefix->id,
          unit->id,
          basis_id(prefix->basis),
          prefix->execution,
          prefix->cursor,
          take(movement_consumed_cost(std::get<TerrainCostBasis>(prefix->basis),
                                      prefix->cursor, budget)),
          {before},
          {balance(*ledger, budget)},
          {discard},
          kind,
          source,
          at};
      state.credit_dispositions.set(disposition_id, std::move(disposition));
      ledger->disposition_sources.push_back(
          {"travel", "terrain_credit_disposition", disposition_id});
    }
    MovementUnitCloseSource closed{
        unit->id + ":close",
        prefix->id,
        unit->id,
        prefix->execution,
        kind,
        at,
        prefix->cursor,
        take(read_movement_position(*prefix, budget)),
        source};
    {
      const auto stored_key = closed.id;
      state.unit_closes.set(stored_key, std::move(closed));
    }
    unit->status = MovementUnitFinalized{source};
  });
}
// 原双方锚/suffix一次恢复；时间和费用已结from→M，不执行旧整步或退款。
Outcome<StableId> apply_exact_personal_contact_side(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const StableId &contact_id, const StableId &prefix_id,
    const StableId &encounter, bool interactive,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    ExactArithmeticBudget budget, const PersonalMovementFundingRead *funding,
    const CurrentMomentRead *clock) {
  return guard<StableId>([&] {
    auto *store = dynamic_cast<CommonStore *>(&common);
    if (!store || !store->actor_location_capture_identity() ||
        encounter.empty())
      fail("SpatialLocationCaptureRequired");
    const auto *contact = state.contacts.find(contact_id);
    auto *p = state.prefixes.find(prefix_id);
    const auto *action =
        p ? common.action(execution_action(p->execution)) : nullptr;
    if (!contact || !p || !action || action->kind != "travel" ||
        !same_moment(p->last_settled_at, contact->occurred_at, budget))
      fail("EncounterWithoutJourney", prefix_id);
    const auto actor_id = action->actor_id;
    require_writer(state, *p, actor_id);
    auto *unit = state.units.find(p->unit_ref);
    if (!unit)
      fail("MovementUnitRequired", prefix_id);
    if (std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) &&
        !take(read_unstarted_held_contact_close(
            state, *unit, {"travel", "movement_contact", contact_id},
            budget))) {
      if (compare(take(read_exact_contact_fraction(*contact, budget)), {},
                  budget) != 0 ||
          compare(unit->active_elapsed.value, {}, budget) != 0)
        fail("UnpaidPositiveContact", prefix_id);
      auto copy = *unit;
      if (!clock)
        fail("PersonalContactActualClockRequired", contact_id);
      consume_personal_unit_payment(
          common, travel, catalog, services, state, *p, copy,
          PersonalMovementZeroContact{
              {"travel", "movement_contact", contact_id}},
          *clock, funding, budget);
      {
        const auto key = copy.id;
        state.units.set(key, std::move(copy));
      }
    }
    const SourceRef source{"travel", "movement_contact", contact_id};
    const auto anchor = take(
        read_exact_contact_endpoint_anchor(state, *contact, prefix_id, budget));
    if (contact->original_hit.road) {
      const auto &hit = *contact->original_hit.road;
      const auto *end = hit.first.actor_id == actor_id    ? &hit.first
                        : hit.second.actor_id == actor_id ? &hit.second
                                                          : nullptr;
      if (!end || !travel.road_actions.find(action->id))
        fail("EncounterWithoutJourney", prefix_id);
      auto params = end->trace.params;
      params.has_paused = true;
      params.paused = interactive || params.paused;
      params.has_encounter_id = params.has_met_actions = true;
      params.encounter_id = interactive ? encounter : StableId{};
      params.met_actions.push_back(end == &hit.first
                                       ? hit.second.trace.action_id
                                       : hit.first.trace.action_id);
      travel.road_actions.set(action->id, std::move(params));
    } else {
      const auto &hit = contact->original_hit;
      const auto *end = hit.first.actor_id == actor_id    ? &hit.first
                        : hit.second.actor_id == actor_id ? &hit.second
                                                          : nullptr;
      if (!end || !travel.actions.find(action->id))
        fail("EncounterWithoutJourney", prefix_id);
      auto params = end->trace.params;
      params.paused = interactive || params.paused;
      params.has_encounter_id = params.has_met_actions = true;
      params.encounter_id = interactive ? encounter : StableId{};
      params.met_actions.push_back(end == &hit.first
                                       ? hit.second.trace.action_id
                                       : hit.first.trace.action_id);
      travel.actions.set(action->id, std::move(params));
    }
    const auto exertion = take(finalize_exact_personal_unit(
        common, travel, state, catalog, prefix_id, MovementCloseKind::contact,
        source, contact->occurred_at, &actual_writes, budget));
    p = state.prefixes.find(prefix_id);
    auto *close = state.unit_closes.find(p->unit_ref + ":close");
    if (!close)
      fail("MovementCloseSourceRequired", prefix_id);
    close->actual_anchor = anchor;
    p->lifecycle = MovementClosed{source, anchor};
    state.spatial_bindings.set(
        "actor:" + actor_id,
        {"actor", actor_id, StableMovementWriter{anchor}, source});
    ActorMutation mutation;
    mutation.actor_id = actor_id;
    mutation.location = take(project_movement_location(anchor, budget));
    if (contact->original_hit.road)
      mutation.location_mode = ActorMutation::LocationMode::route_contact;
    take(common.apply_actor(mutation));
    actual_writes.push_back(take(bind_actual_spatial_location_write(
        *store, state, actor_id, source, 1, anchor, 0, budget)));
    return exertion;
  });
}
Outcome<StableId> apply_exact_personal_boundary_in_candidate(
    CommonStore &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const FieldExactContactReadContext &field,
    const FieldBattleCatalog &field_catalog, const SourceRef &source,
    std::vector<BoundSpatialLocationWrite> &actual_writes,
    const PersonalMovementFundingRead *funding, ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    if (&field.common != &common || &field.movement != &state ||
        &field.travel != &catalog || !common.actor_location_capture_identity())
      fail("MovementBoundarySameCandidateRequired", source.id);
    const auto verified = take(read_movement_boundary_contact_source(
        field, field_catalog, source, budget));
    const auto &contact = *verified.contact;
    const auto now = take(field.clock.read_current_moment());
    if (!std::holds_alternative<PersonalMovementExecution>(
            verified.prefix->execution) ||
        !std::holds_alternative<MovementUnitOpen>(verified.unit->status) ||
        !same_moment(now, contact.occurred_at, budget))
      fail("MovementBoundaryActualPersonalRequired", source.id);
    auto *prefix = state.prefixes.find(contact.prefix_id);
    const auto *action = common.action(execution_action(prefix->execution));
    if (!action || action->kind != "travel" ||
        !same_moment(prefix->last_settled_at, now, budget))
      fail("MovementBoundaryPersonalActionRequired", source.id);
    const auto actor_id = action->actor_id;
    const auto action_id = action->id;
    const bool road = std::holds_alternative<RoadMovementBasis>(prefix->basis);
    require_writer(state, *prefix, actor_id);
    auto *unit = state.units.find(prefix->unit_ref);
    if (!unit)
      fail("MovementUnitRequired", contact.prefix_id);
    if (std::holds_alternative<MovementPaymentNotConsumed>(unit->payment)) {
      if (compare(contact.entry_fraction, {}, budget) != 0 ||
          compare(unit->active_elapsed.value, {}, budget) != 0)
        fail("UnpaidPositiveBoundaryContact", source.id);
      auto copy = *unit;
      consume_personal_unit_payment(
          common, travel, catalog, services, state, *prefix, copy,
          PersonalMovementZeroContact{source}, field.clock, funding, budget);
      const auto key = copy.id;
      state.units.set(key, std::move(copy));
    }
    // Do not manufacture a dynamic encounter participant. The original exact
    // basis/cursor and boundary source retain the suffix for finite
    // continuation; pausing the legacy projection does not rebase or consume
    // that suffix.
    if (road) {
      const auto *original = travel.road_actions.find(action_id);
      if (!original)
        fail("MovementBoundaryRoadActionRequired", action_id);
      auto params = *original;
      params.has_paused = true;
      params.paused = true;
      travel.road_actions.set(action_id, std::move(params));
    } else {
      const auto *original = travel.actions.find(action_id);
      if (!original)
        fail("MovementBoundaryTerrainActionRequired", action_id);
      auto params = *original;
      params.paused = true;
      travel.actions.set(action_id, std::move(params));
    }
    const auto exertion = take(finalize_exact_personal_unit(
        common, travel, state, catalog, contact.prefix_id,
        MovementCloseKind::contact, source, now, &actual_writes, budget));
    prefix = state.prefixes.find(contact.prefix_id);
    auto *close = state.unit_closes.find(prefix->unit_ref + ":close");
    if (!close)
      fail("MovementCloseSourceRequired", contact.prefix_id);
    close->actual_anchor = contact.actual_anchor;
    prefix->lifecycle = MovementClosed{source, contact.actual_anchor};
    state.spatial_bindings.set("actor:" + actor_id,
                               {"actor", actor_id,
                                StableMovementWriter{contact.actual_anchor},
                                source});
    ActorMutation mutation;
    mutation.actor_id = actor_id;
    mutation.location =
        take(project_movement_location(contact.actual_anchor, budget));
    if (road)
      mutation.location_mode = ActorMutation::LocationMode::route_contact;
    take(common.apply_actor(mutation));
    actual_writes.push_back(take(bind_actual_spatial_location_write(
        common, state, actor_id, source, 1, contact.actual_anchor, 0, budget)));
    return exertion;
  });
}
Status apply_exact_contact_truncation(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const ExactMovementContact &contact, const StableId &encounter,
    bool interactive, std::vector<BoundSpatialLocationWrite> *actual_writes,
    ExactArithmeticBudget budget, const PersonalMovementFundingRead *funding,
    const CurrentMomentRead *clock) {
  return status([&] {
    if (!actual_writes)
      fail("SpatialLocationCaptureRequired");
    take(install_exact_contact_source(common, state, contact, budget));
    for (const auto &id : {contact.first_prefix_id, contact.second_prefix_id})
      take(apply_exact_personal_contact_side(
          common, travel, state, catalog, services, contact.id, id, encounter,
          interactive, *actual_writes, budget, funding, clock));
  });
}

Outcome<TravelExertionSource>
read_verified_travel_exertion(const MovementExactState &state,
                              const StableId &id, ExactArithmeticBudget b) {
  return guard<TravelExertionSource>([&] {
    const auto *exertion = state.exertions.find(id);
    const auto *unit =
        exertion ? state.units.find(exertion->unit_ref) : nullptr;
    const auto *done =
        unit ? std::get_if<MovementUnitFinalized>(&unit->status) : nullptr;
    if (!exertion || !unit || !done || unit->effort_basis_refs.empty() ||
        std::holds_alternative<MovementPaymentNotConsumed>(unit->payment))
      fail("TravelExertionSourceRequired", id);
    const auto *first =
        state.exertion_bases.find(unit->effort_basis_refs.front().id);
    const auto *closed = state.unit_closes.find(unit->id + ":close");
    if (!first || !closed || closed->unit_ref != unit->id ||
        closed->prefix_id != exertion->prefix_id ||
        first->actual_participants != exertion->actual_participants ||
        first->original_fatigue_per_unit !=
            exertion->original_fatigue_per_unit ||
        !same_moment(first->opened_at, exertion->actual_span.from, b) ||
        !same_moment(closed->occurred_at, exertion->actual_span.to, b) ||
        !same_source(closed->actual_cause, exertion->original_close_source) ||
        !same_source(done->original_close_source,
                     exertion->original_close_source) ||
        execution_key(unit->execution) != execution_key(exertion->execution) ||
        compare(unit->active_elapsed.value, exertion->active_elapsed.value,
                b) != 0)
      fail("TravelExertionCloseMismatch", id);
    CanonicalFraction active, funded, spent;
    WorldMoment previous = first->opened_at;
    std::size_t work_index = 0;
    for (std::size_t i = 0; i < unit->effort_basis_refs.size(); ++i) {
      const auto &ref = unit->effort_basis_refs[i];
      const auto *basis = state.exertion_bases.find(ref.id);
      if (ref.domain != "travel" || ref.kind != "travel_exertion_basis" ||
          !basis || basis->unit_ref != unit->id ||
          execution_key(basis->execution) != execution_key(unit->execution) ||
          basis->actual_participants != first->actual_participants ||
          basis->original_fatigue_per_unit !=
              first->original_fatigue_per_unit ||
          take(compare_world_moments(basis->opened_at, previous, b)) < 0)
        fail("TravelExertionBasisMismatch", id);
      auto cursor = basis->opened_cursor;
      const TravelExertionBasis *next =
          i + 1 < unit->effort_basis_refs.size()
              ? state.exertion_bases.find(unit->effort_basis_refs[i + 1].id)
              : nullptr;
      const auto until = next ? next->opened_at : closed->occurred_at;
      while (work_index < unit->activity_spans.size() &&
             unit->activity_spans[work_index].basis_ref ==
                 basis_id(basis->actual_basis)) {
        const auto &work = unit->activity_spans[work_index++];
        if (!source_valid(work.rate_source) ||
            take(compare_world_moments(work.span.from, previous, b)) < 0 ||
            take(compare_world_moments(work.span.from, basis->opened_at, b)) <
                0 ||
            take(compare_world_moments(work.span.to, until, b)) > 0 ||
            compare(work.cursor_from, cursor, b) != 0)
          fail("TravelActivitySourceMismatch", id);
        auto elapsed =
            take(elapsed_world_duration(work.span.from, work.span.to, b)).value;
        if (compare(elapsed, {}, b) <= 0 ||
            compare(add(cursor, mul(work.rate, elapsed, b), b), work.cursor_to,
                    b) != 0)
          fail("TravelActivityCursorMismatch", id);
        active = add(active, elapsed, b);
        funded = add(funded, mul(work.budget_rate, elapsed, b), b);
        if (const auto *terrain =
                std::get_if<TerrainCostBasis>(&basis->actual_basis))
          spent = add(
              spent,
              sub(take(movement_consumed_cost(*terrain, work.cursor_to, b)),
                  take(movement_consumed_cost(*terrain, work.cursor_from, b)),
                  b),
              b);
        cursor = work.cursor_to;
        previous = work.span.to;
      }
      if (next) {
        const MovementBasisTransitionSource *transition = nullptr;
        for (const auto &row : state.basis_transitions.rows())
          if (row.second.unit_ref == unit->id &&
              row.second.old_basis_ref == basis_id(basis->actual_basis) &&
              row.second.new_basis_ref == basis_id(next->actual_basis)) {
            if (transition)
              fail("DuplicateMovementBasisTransition", id);
            transition = &row.second;
          }
        if (!transition || compare(transition->old_cursor, cursor, b) != 0 ||
            !same_moment(transition->occurred_at, next->opened_at, b))
          fail("TravelBasisTransitionRequired", id);
      } else if (compare(cursor, closed->cursor, b) != 0)
        fail("TravelActivityTotalMismatch", id);
    }
    if (work_index != unit->activity_spans.size() ||
        compare(active, unit->active_elapsed.value, b) != 0 ||
        compare(active, integer(1, b), b) > 0 ||
        compare(active,
                take(elapsed_world_duration(exertion->actual_span.from,
                                            exertion->actual_span.to, b))
                    .value,
                b) > 0)
      fail("TravelActivityTotalMismatch", id);
    if (const auto *ledger = std::get_if<TerrainUnitBudget>(&unit->budget))
      if (compare(ledger->funded.value, funded, b) != 0 ||
          compare(ledger->spent.value, spent, b) != 0)
        fail("TravelExertionOriginalCostMismatch", id);
    return *exertion;
  });
}

Outcome<StableId> finalize_exact_movement_unit_source(
    CommonWrite &common, MovementExactState &state, const StableId &prefix_id,
    MovementCloseKind kind, const SourceRef &source, const WorldMoment &at,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    auto *prefix = state.prefixes.find(prefix_id);
    auto *unit = prefix ? state.units.find(prefix->unit_ref) : nullptr;
    if (!prefix || !unit || unit->effort_basis_refs.empty())
      fail("MovementUnitRequired", prefix_id);
    const auto unit_id = unit->id;
    const auto exertion_id = unit_id + ":exertion";
    if (kind == MovementCloseKind::contact &&
        take(read_unstarted_held_contact_close(state, *unit, source, budget))) {
      const auto *prior = state.unit_closes.find(unit_id + ":close");
      if (prior) {
        if (prior->kind != kind || !same_source(prior->actual_cause, source) ||
            !same_moment(prior->occurred_at, at, budget))
          fail("MovementUnitCloseReplayMismatch", unit_id);
        return StableId{};
      }
      take(close_movement_unit(state, prefix_id, kind, source, at, budget));
      take(common.append_fact(
          {unit_id + ":close", "movement_unit_close", "travel"}));
      if (state.credit_dispositions.find(unit_id + ":credit"))
        take(common.append_fact(
            {unit_id + ":credit", "terrain_credit_disposition", "travel"}));
      return StableId{};
    }
    if (state.exertions.find(exertion_id)) {
      const auto *prior = state.unit_closes.find(unit_id + ":close");
      if (!prior || prior->kind != kind ||
          !same_source(prior->actual_cause, source) ||
          !same_moment(prior->occurred_at, at, budget))
        fail("MovementUnitCloseReplayMismatch", unit_id);
      take(read_verified_travel_exertion(state, exertion_id, budget));
      return exertion_id;
    }
    take(close_movement_unit(state, prefix_id, kind, source, at, budget));
    prefix = state.prefixes.find(prefix_id);
    unit = state.units.find(unit_id);
    const auto *effort =
        state.exertion_bases.find(unit->effort_basis_refs.front().id);
    if (!effort)
      fail("TravelExertionBasisRequired", unit_id);
    TravelExertionSource exertion{exertion_id,
                                  prefix_id,
                                  unit_id,
                                  prefix->execution,
                                  {effort->opened_at, at},
                                  unit->active_elapsed,
                                  effort->original_fatigue_per_unit,
                                  effort->actual_participants,
                                  source};
    state.exertions.set(exertion_id, std::move(exertion));
    take(common.append_fact(
        {unit_id + ":close", "movement_unit_close", "travel"}));
    if (state.credit_dispositions.find(unit_id + ":credit"))
      take(common.append_fact(
          {unit_id + ":credit", "terrain_credit_disposition", "travel"}));
    take(common.append_fact({exertion_id, "travel_exertion", "travel"}));
    take(read_verified_travel_exertion(state, exertion_id, budget));
    return exertion_id;
  });
}
Outcome<StableId> finalize_exact_personal_unit(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const StableId &prefix_id,
    MovementCloseKind kind, const SourceRef &source, const WorldMoment &at,
    std::vector<BoundSpatialLocationWrite> *actual_writes,
    ExactArithmeticBudget budget) {
  return guard<StableId>([&] {
    const auto exertion_id = take(finalize_exact_movement_unit_source(
        common, state, prefix_id, kind, source, at, budget));
    auto *prefix = state.prefixes.find(prefix_id);
    auto *unit = state.units.find(prefix->unit_ref);
    const auto unit_id = unit->id;
    if (kind == MovementCloseKind::arrival && state.arrivals.find(source.id))
      return exertion_id;
    if (kind == MovementCloseKind::arrival) {
      auto *store = dynamic_cast<CommonStore *>(&common);
      if (!store || !actual_writes || !store->actor_location_capture_identity())
        fail("SpatialLocationCaptureRequired");
      MovementArrivalSource arrival;
      arrival.id = source.id;
      arrival.prefix_id = prefix_id;
      arrival.unit_ref = unit_id;
      arrival.execution = prefix->execution;
      arrival.occurred_at = at;
      arrival.actual_anchor = take(read_movement_position(*prefix, budget));
      arrival.basis_ref = basis_id(prefix->basis);
      arrival.cursor = prefix->cursor;
      if (const auto *basis = std::get_if<TerrainCostBasis>(&prefix->basis)) {
        if (basis->reached_points != basis->original_point_count ||
            compare(
                take(movement_consumed_cost(*basis, prefix->cursor, budget)),
                integer(basis->actual_consumed_total, budget), budget) != 0 ||
            (compare(prefix->cursor, basis->parameter_limit.value, budget) !=
                 0 &&
             compare(unit->active_elapsed.value, integer(1, budget), budget) !=
                 0))
          fail("MovementArrivalNotAtRealEnd", prefix_id);
        arrival.reached_points = basis->reached_points;
        arrival.original_point_count = basis->original_point_count;
        if (const auto *destination =
                std::get_if<StableId>(&basis->original_inputs.destination)) {
          const auto *city = catalog.settlements.find(*destination);
          if (!city)
            fail("MovementArrivalDestinationRequired");
          arrival.destination_settlement_id = *destination;
          arrival.actual_anchor = OriginalMovementAnchor{
              SettlementLocation{*destination, city->entry_place}, source};
        }
      } else {
        const auto &road = std::get<RoadMovementBasis>(prefix->basis);
        if (compare(prefix->cursor, integer(1, budget), budget) != 0)
          fail("MovementArrivalNotAtRealEnd", prefix_id);
        auto params = road.original_inputs.params;
        auto location =
            take(project_movement_location(arrival.actual_anchor, budget));
        if (!take(finish_road_leg(catalog, at.half_day_tick, params, location)))
          fail("MovementArrivalNotAtRealEnd", prefix_id);
        const auto *settlement = std::get_if<SettlementLocation>(&location);
        if (!settlement)
          fail("MovementArrivalDestinationRequired");
        arrival.destination_settlement_id = settlement->settlement_id;
        arrival.actual_anchor = OriginalMovementAnchor{location, source};
        travel.road_actions.set(execution_action(prefix->execution),
                                std::move(params));
      }
      state.arrivals.set(arrival.id, arrival);
      take(common.append_fact({arrival.id, "movement_arrival", "travel"}));
      const auto action_id = execution_action(prefix->execution);
      const auto *action = common.action(action_id);
      if (!action)
        fail("MovementActionRequired", action_id);
      auto actor_id = action->actor_id;
      ActorMutation mutation;
      mutation.actor_id = actor_id;
      mutation.location =
          take(project_movement_location(arrival.actual_anchor, budget));
      take(common.apply_actor(mutation));
      prefix->lifecycle = MovementClosed{source, arrival.actual_anchor};
      state.spatial_bindings.set("actor:" + actor_id,
                                 {"actor", actor_id,
                                  StableMovementWriter{arrival.actual_anchor},
                                  source});
      actual_writes->push_back(take(bind_actual_spatial_location_write(
          *store, state, actor_id, source, 1, arrival.actual_anchor, 0,
          budget)));
    }
    return exertion_id;
  });
}
} // namespace hanmo::resident
