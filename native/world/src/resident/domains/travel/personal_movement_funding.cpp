#include "personal_movement_funding.hpp"
#include "movement_action_time.hpp"
#include "movement_exact_detail.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
const TravelExertionBasis &first_basis(const MovementExactState &state,
                                       const MovementUnitState &unit) {
  if (unit.effort_basis_refs.empty())
    fail("PersonalMovementCostBasisRequired", unit.id);
  const auto &ref = unit.effort_basis_refs.front();
  const auto *basis = state.exertion_bases.find(ref.id);
  if (ref.domain != "travel" || ref.kind != "travel_exertion_basis" || !basis ||
      basis->unit_ref != unit.id || !basis->original_action ||
      execution_key(basis->execution) != execution_key(unit.execution))
    fail("PersonalMovementCostBasisRequired", unit.id);
  return *basis;
}
Integer sum(Integer a, Integer b) {
  if (a < 0 || b < 0 || a > std::numeric_limits<Integer>::max() - b)
    fail("PersonalMovementCostOverflow");
  return a + b;
}
} // namespace
Outcome<PersonalMovementUnitCost> read_unconsumed_personal_unit_cost(
    const CommonRead &common, const TravelState &travel,
    const MovementExactState &state, const TravelCatalog &catalog,
    const TravelServices &services, const PersonalMovementCostRequest &request,
    const CurrentMomentRead &clock, ExactArithmeticBudget budget) {
  return guard<PersonalMovementUnitCost>([&] {
    const auto *prefix = state.prefixes.find(request.prefix_id);
    const auto *unit = prefix ? state.units.find(prefix->unit_ref) : nullptr;
    const auto *execution =
        prefix ? std::get_if<PersonalMovementExecution>(&prefix->execution)
               : nullptr;
    const auto *action =
        execution ? common.action(execution->action_id) : nullptr;
    if (!prefix || !unit || !action || action->kind != "travel" ||
        !std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) ||
        !std::holds_alternative<MovementUnitOpen>(unit->status) ||
        !unit->activity_spans.empty() ||
        compare(unit->active_elapsed.value, {}, budget) != 0)
      fail("UnconsumedPersonalMovementCostRequired", request.prefix_id);
    const auto &first = first_basis(state, *unit);
    if (first.original_action->id != action->id ||
        first.original_action->actor_id != action->actor_id)
      fail("PersonalMovementCostActionMismatch", request.prefix_id);
    const auto now = take(clock.read_current_moment());
    PersonalMovementUnitCost out;
    out.action_id = action->id;
    out.actor_id = action->actor_id;
    out.prefix_id = prefix->id;
    out.unit_ref = unit->id;
    out.unit_basis = {{"travel", "travel_exertion_basis", first.id}, 1};
    out.occurred_at = now;
    out.actual_start = request.actual_start;
    if (const auto *positive = std::get_if<PersonalMovementFirstPositivePrefix>(
            &request.actual_start)) {
      if (!same_moment(now, positive->selected_span.from, budget))
        fail("PersonalMovementCostNotAtActualFrom", request.prefix_id);
      const auto prepared = take(prepare_movement_prefix(
          state, request.prefix_id, positive->selected_span, budget));
      if (!prepared.first_payment || prepared.unit_ref != unit->id)
        fail("PersonalMovementCostSelectedSpanMismatch", request.prefix_id);
    } else {
      const auto &source =
          std::get<PersonalMovementZeroContact>(request.actual_start)
              .actual_contact;
      if (source.domain == "travel" &&
          source.kind == "movement_boundary_contact") {
        const auto *contact = state.boundary_contacts.find(source.id);
        if (!contact || contact->prefix_id != prefix->id ||
            contact->motion_source.unit_ref != unit->id ||
            !std::holds_alternative<RunningMovementMap>(
                contact->motion_source.original_mapping) ||
            !same_moment(contact->occurred_at, now, budget) ||
            !same_moment(prefix->last_settled_at, now, budget) ||
            compare(contact->entry_fraction, {}, budget) != 0)
          fail("PersonalMovementCostBoundaryContactRequired", source.id);
        const auto *store = dynamic_cast<const CommonStore *>(&common);
        const auto *header =
            store ? store->state().facts.find(source.id) : nullptr;
        if (!header || header->domain != source.domain ||
            header->kind != source.kind)
          fail("PersonalMovementCostBoundaryHeadRequired", source.id);
      } else {
        const auto *contact =
            source.domain == "travel" && source.kind == "movement_contact"
                ? state.contacts.find(source.id)
                : nullptr;
        if (!contact || !same_moment(contact->occurred_at, now, budget) ||
            !same_moment(prefix->last_settled_at, now, budget) ||
            compare(take(read_exact_contact_fraction(*contact, budget)), {},
                    budget) != 0)
          fail("PersonalMovementCostActualContactRequired", source.id);
        const auto *witness = contact->first_prefix_id == prefix->id
                                  ? &contact->first_motion_source
                              : contact->second_prefix_id == prefix->id
                                  ? &contact->second_motion_source
                                  : nullptr;
        if (!witness || !(*witness) || (**witness).unit_ref != unit->id ||
            !std::holds_alternative<RunningMovementMap>(
                (**witness).original_mapping))
          fail("PersonalMovementCostContactParticipantMismatch", source.id);
        const auto *store = dynamic_cast<const CommonStore *>(&common);
        const auto *header =
            store ? store->state().facts.find(source.id) : nullptr;
        if (!header || header->domain != "travel" ||
            header->kind != source.kind)
          fail("PersonalMovementCostContactHeadRequired", source.id);
      }
    }
    out.original_cost_rule_ref = std::visit(
        [](const auto &b) { return b.catalog_rule_ref; }, first.actual_basis);
    if (const auto *terrain =
            std::get_if<TerrainCostBasis>(&first.actual_basis)) {
      const auto *current = travel.actions.find(action->id);
      const auto *map = catalog.maps.find(terrain->original_inputs.map_id);
      if (!current || !map ||
          current->payer_wallet_id !=
              terrain->original_inputs.payer_wallet_id ||
          current->money_per_tick != terrain->original_inputs.money_per_tick)
        fail("PersonalTerrainCostOriginalMismatch", action->id);
      out.payer_wallet_id = terrain->original_inputs.payer_wallet_id;
      out.amount = terrain->original_inputs.money_per_tick;
    } else {
      const auto &road = std::get<RoadMovementBasis>(first.actual_basis);
      const auto *edge = catalog.roads.find(road.original_inputs.edge_id);
      const auto *current = travel.road_actions.find(action->id);
      if (!edge || !current ||
          current->payer_wallet_id !=
              road.original_inputs.params.payer_wallet_id)
        fail("PersonalRoadCostOriginalMismatch", action->id);
      out.payer_wallet_id = road.original_inputs.params.payer_wallet_id;
      out.amount =
          sum(edge->cost_per_tick, services.party_extra_cost(action->actor_id));
    }
    if (out.amount < 0 || out.payer_wallet_id.empty() ||
        !common.wallet(out.payer_wallet_id))
      fail("PersonalMovementWalletRequired", action->id);
    return out;
  });
}
Status validate_personal_movement_funding_sources(
    const CommonStore &common, const MovementExactState &state,
    const NativeMovementActionTimeRead &actions, Integer travel_version,
    Integer funding_version, const PersonalMovementFundingRead *funding) {
  return status([&] {
    if ((travel_version != 4 && travel_version != 5) ||
        (funding_version != 0 && funding_version != 1) ||
        !actions.bound_to(common, state))
      fail("PersonalFundingSelectedProfileMismatch");
    if (travel_version == 5 && funding_version != 1)
      fail("PersonalFundingSelectedProfileMismatch");
    if (funding_version == 1 && (!funding || !funding->bound_to(common, state)))
      fail("PersonalMovementFundingProviderRequired");
    std::set<StableId> actions_seen;
    for (const auto &row : state.exertion_bases.rows()) {
      const auto &basis = row.second;
      const auto *execution =
          std::get_if<PersonalMovementExecution>(&basis.execution);
      if (execution)
        actions_seen.insert(execution->action_id);
      if (!basis.accepted_funding_plan)
        continue;
      const auto *unit = state.units.find(basis.unit_ref);
      if (travel_version != 5 || funding_version != 1 || !execution ||
          execution->segment_ordinal != 0 || !unit || unit->ordinal != 0 ||
          unit->effort_basis_refs.empty() ||
          !same_source(unit->effort_basis_refs.front(),
                       {"travel", "travel_exertion_basis", basis.id}))
        fail("PersonalFundingOnlyOriginalOpening", basis.id);
    }
    for (const auto &action : actions_seen) {
      const auto plan =
          take(actions.read_original_execution_funding_plan(action));
      if (plan && (travel_version != 5 || funding_version != 1))
        fail("UnselectedPersonalFundingSource", action);
      if (funding_version == 1)
        take(funding->validate_original_personal_execution_funding(action,
                                                                   plan));
    }
  });
}
Status validate_personal_movement_recorded_cost(
    const CommonStore &common, const MovementExactState &state,
    const TravelCatalog &catalog, const MovementExpenseSource &source,
    const PersonalMovementPartyCostHistoryRead *history,
    ExactArithmeticBudget budget) {
  return status([&] {
    const auto *unit = state.units.find(source.unit_ref);
    const auto *execution =
        unit ? std::get_if<PersonalMovementExecution>(&unit->execution)
             : nullptr;
    const auto *head = common.state().facts.find(source.id);
    if (!unit || !execution || !head || head->domain != "travel" ||
        head->kind != "movement_expense" ||
        state.expenses.find(source.id) != &source || source.amount < 0 ||
        static_cast<__int128>(source.balance_before) - source.amount !=
            source.balance_after)
      fail("PersonalMovementDebitSourceMismatch", source.id);
    const auto &first = first_basis(state, *unit);
    if (!first.original_action ||
        first.original_action->id != execution->action_id ||
        source.original_cost_rule_ref !=
            std::visit([](const auto &b) { return b.catalog_rule_ref; },
                       first.actual_basis) ||
        take(compare_world_moments(first.opened_at, source.occurred_at,
                                   budget)) > 0)
      fail("PersonalMovementCostBasisMismatch", source.id);
    StableId payer;
    Integer expected = 0;
    if (const auto *terrain =
            std::get_if<TerrainCostBasis>(&first.actual_basis)) {
      if (!catalog.maps.find(terrain->original_inputs.map_id))
        fail("PersonalTerrainCostOriginalMismatch", source.id);
      payer = terrain->original_inputs.payer_wallet_id;
      expected = terrain->original_inputs.money_per_tick;
    } else {
      const auto &road = std::get<RoadMovementBasis>(first.actual_basis);
      const auto *edge = catalog.roads.find(road.original_inputs.edge_id);
      if (!edge)
        fail("PersonalRoadCostOriginalMismatch", source.id);
      if (!history)
        fail("PersonalPartyCostHistoryRequired", source.id);
      payer = road.original_inputs.params.payer_wallet_id;
      expected = sum(
          edge->cost_per_tick,
          take(history->read_original_personal_party_cost(
              first.original_action->actor_id,
              {{"travel", "travel_exertion_basis", first.id}, 1},
              {"travel", "movement_expense", source.id}, source.occurred_at)));
    }
    if (source.payer_wallet_id != payer || source.amount != expected)
      fail("PersonalMovementOriginalFeeMismatch", source.id);
  });
}
} // namespace hanmo::resident
