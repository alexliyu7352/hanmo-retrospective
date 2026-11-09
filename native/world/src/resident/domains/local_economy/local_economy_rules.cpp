#include "population_labor_change.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
// 到期责任、真实支付、供养和生产按固定顺序执行；缺钱缺粮是可提交的结果。
#include "../estate/employment/estate_employment_internal.hpp"
#include "disruptions/production_disruptions.hpp"
#include "economic_original_payments.hpp"
#include "economic_work_sources.hpp"
#include "household/provision_management.hpp"
#include "local_economy_internal.hpp"
#include "market_history/economic_material_read.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/growth/growth_market_capture.hpp"
#include "takeover/execution_configurations.hpp"
#include "takeover/execution_phase.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
struct PopulationTransferLaborCaptureWriter {
  static void publish(PopulationTransferLaborCapture &out,
                      const StableId &transfer, const StableId &cohort,
                      std::vector<PopulationLaborChange> changes) {
    out.transfer_id_ = transfer;
    out.cohort_id_ = cohort;
    out.changes_ = std::move(changes);
  }
};

namespace {
using namespace local_economy_detail;
// 新事实头与本域收据同候选写入，任何失败由阶段保存点一起还原。
Outcome<StableId> fact_id(CommonStore &c, const StableId &prefix,
                          const StableId &kind) {
  auto id = c.next_id(prefix);
  if (!id.ok)
    return id;
  auto status = c.append_fact({id.value, kind, "local_economy"});
  if (!status.ok)
    return {false, {}, status.error};
  return id;
}
// 义务的整数舍入余数只影响尚未到期账期，旧欠额保持原应付不变。
Outcome<Integer> due_amount(const CommonStore &c, LocalEconomyState &state,
                            const LocalEconomyCatalog &catalog,
                            const RecurringObligation &obligation) {
  const auto *rate = catalog.rates.find(obligation.rate_profile);
  if (!rate)
    return {false, 0, failure("UnknownObligationRate", obligation.id).error};
  Integer count = 1;
  if (rate->per_capita) {
    auto people = population(c, state, obligation.beneficiary);
    if (!people.ok)
      return people;
    count = people.value;
    // 军粮先于战损/恢复和宏观供养：本边界已供给的人沿具名军史交接，不因转群体再吃一份。
    if (obligation.kind == ObligationKind::ration &&
        obligation.beneficiary.kind == SubjectKind::cohort)
      for (const auto &row : state.population_transfers.rows()) {
        const auto &transfer = row.second;
        if (transfer.to_cohort_id != obligation.beneficiary.id ||
            transfer.effective_tick != obligation.next_due_tick ||
            (transfer.reason != "population_casualty" &&
             transfer.reason != "population_recovered" &&
             transfer.reason != "population_deserted"))
          continue;
        const auto *header = c.state().facts.find(transfer.source_fact_id);
        if (!header || header->domain != "population_military" ||
            header->kind != transfer.reason || transfer.count < 0 ||
            count < transfer.count)
          return {false, 0,
                  failure("InvalidMilitaryRationHandoff", transfer.id).error};
        count -= transfer.count;
      }
  }
  const auto *saved = state.obligation_remainders.find(obligation.id);
  Integer amount = 0, remainder = 0;
  if (!ratio(count, rate->numerator, rate->denominator, saved ? *saved : 0,
             amount, remainder))
    return {false, 0, failure("EconomyQuantityOverflow", obligation.id).error};
  if (auto *binding =
          state.membership_wage_bindings.find(obligation.authority_ref)) {
    if (binding->ended_tick || binding->obligation_id != obligation.id ||
        obligation.next_due_tick < binding->first_due_tick ||
        binding->credited_periods == INT64_MAX)
      return {false, 0,
              failure("InvalidMembershipWagePeriod", obligation.id).error};
    ++binding->credited_periods;
    binding->last_credited_due_tick = obligation.next_due_tick;
  }
  state.obligation_remainders.set(obligation.id, remainder);
  return {true, amount, {}};
}
// 只结清本义务最早的未付账期，双方钱包走一个原子 recipe。
Status pay_arrears(CommonStore &c, LocalEconomyState &state,
                   const OrganizationRead &organizations,
                   const RecurringObligation &obligation, Integer tick,
                   LocalEconomyEffects &effects,
                   EconomicTakeoverPhaseContext *takeover = nullptr) {
  if (obligation.period_ticks == 0)
    return common_failure("EconomicSourceOnlyPaymentWriterRequired",
                          obligation.id, ErrorKind::unsupported_capability);
  std::vector<StableId> ids;
  for (const auto &row : state.arrears.rows())
    if (row.second.obligation_id == obligation.id &&
        row.second.paid_amount < row.second.original_due)
      ids.push_back(row.first);
  std::sort(ids.begin(), ids.end(), [&](const auto &a, const auto &b) {
    const auto &x = *state.arrears.find(a);
    const auto &y = *state.arrears.find(b);
    bool xt = false, yt = false;
    for (const auto &terminal : state.membership_terminal_fractions.rows()) {
      xt |= terminal.second.arrear_id == x.id;
      yt |= terminal.second.arrear_id == y.id;
    }
    return std::tie(x.due_tick, xt, x.id) < std::tie(y.due_tick, yt, y.id);
  });
  for (const auto &id : ids) {
    auto &debt = *state.arrears.find(id);
    if (takeover) {
      auto paid = settle_taken_economic_arrear_in_candidate(
          c, state, takeover->state, takeover->read, id, tick);
      if (!paid.ok)
        return {false, {}, paid.error};
      if (paid.value.handled) {
        if (paid.value.receipt_id) {
          effects.receipt_ids.push_back(*paid.value.receipt_id);
          effects.fact_ids.push_back(*paid.value.receipt_id);
        } else
          break;
        continue;
      }
    }
    auto funds = c.spendable(obligation.source_wallet_or_inventory);
    if (!funds.ok)
      return {false, {}, funds.error};
    Integer amount =
        std::min(funds.value, debt.original_due - debt.paid_amount);
    if (amount <= 0)
      break;
    if (!organizations.institutional_obligation_allowed(
            obligation.authority_ref, obligation.id, obligation.principal.id,
            obligation.source_wallet_or_inventory, debt.original_due, tick))
      return failure("InstitutionalAuthorityDenied", obligation.id);
    ResourceRecipe recipe;
    auto assembled = resource_recipe::append_money_transfer(
        recipe, obligation.source_wallet_or_inventory, obligation.destination,
        amount, resource_recipe::MoneyTransferPolicy::spendable);
    if (!assembled.ok)
      return assembled;
    auto paid = c.apply_resources(recipe);
    if (!paid.ok)
      return paid;
    auto receipt = fact_id(c, "local_payment", "obligation_payment");
    if (!receipt.ok)
      return {false, {}, receipt.error};
    state.payments.insert(receipt.value,
                          {receipt.value, id, obligation.id,
                           obligation.source_wallet_or_inventory,
                           obligation.destination, tick, debt.due_tick, amount,
                           obligation.authority_ref});
    debt.paid_amount += amount;
    state.obligations.find(obligation.id)->last_receipt_id = receipt.value;
    effects.receipt_ids.push_back(receipt.value);
    effects.fact_ids.push_back(receipt.value);
  }
  return common_success();
}
// 每个宏观供养对象必须有覆盖到期 tick 的唯一绑定，旧庄园与军事供养不重复消费。
const ProvisionBinding *provision_for(const LocalEconomyState &state,
                                      const RecurringObligation &obligation,
                                      Integer tick) {
  const ProvisionBinding *result = nullptr;
  for (const auto &row : state.provision_bindings.rows()) {
    const auto &binding = row.second;
    if (same_subject(binding.subject, obligation.beneficiary) &&
        binding.from_tick <= tick && tick <= binding.through_tick) {
      if (result)
        return nullptr;
      result = &binding;
    }
  }
  return result && result->policy_owner == ProvisionOwner::macro &&
                 result->obligation_id == obligation.id &&
                 result->payer_inventory ==
                     obligation.source_wallet_or_inventory
             ? result
             : nullptr;
}
// 有效劳力按最近真实供养收据同比下降，不凭不足暗加死亡或迁徙。
Outcome<Integer> effective_workers(const LocalEconomyState &state,
                                   const LocalEconomyCatalog &catalog,
                                   const LaborAllocation &allocation,
                                   Integer due, std::vector<StableId> &causes) {
  auto household =
      household_provision_workers(state, catalog, allocation, due, causes);
  if (!household.ok)
    return {false, 0, household.error};
  if (household.value)
    return {true, *household.value, {}};
  const RationReceipt *ration = nullptr;
  for (const auto &row : state.rations.rows()) {
    const auto &r = row.second;
    if (r.beneficiary.kind == SubjectKind::cohort &&
        r.beneficiary.id == allocation.cohort_id && r.due_tick <= due &&
        (!ration || r.due_tick > ration->due_tick))
      ration = &r;
  }
  bool macro = false;
  for (const auto &row : state.provision_bindings.rows())
    if (row.second.subject.kind == SubjectKind::cohort &&
        row.second.subject.id == allocation.cohort_id &&
        row.second.from_tick <= due && due <= row.second.through_tick)
      macro = row.second.policy_owner == ProvisionOwner::macro;
  if (!ration)
    return {true, macro ? 0 : allocation.workers, {}};
  causes.push_back(ration->id);
  if (ration->required == 0)
    return {true, allocation.workers, {}};
  Integer amount = 0, unused = 0;
  if (!ratio(allocation.workers, ration->consumed, ration->required, 0, amount,
             unused))
    return {false, 0, failure("EconomyQuantityOverflow", allocation.id).error};
  return {true, amount, {}};
}
// 单周期生产先限制实际投入，再记产出与容积造成的未实现量。
Status produce(CommonStore &c, LocalEconomyState &state,
               const LocalEconomyCatalog &catalog, ProductionUnit &unit,
               Integer tick, Integer step_from, LocalEconomyEffects &effects,
               ProductionDisruptionContext *disruption,
               EconomicTakeoverPhaseContext *takeover,
               MarketBusinessCaptureServices *market,
               EconomicProductionContext *exact = nullptr) {
  const auto *binding = state.production_bindings.find(unit.id);
  if (!binding || binding->policy_owner != ProvisionOwner::macro)
    return failure("ProductionBindingMismatch", unit.id);
  const auto *recipe = catalog.recipes.find(unit.recipe_id);
  if (!recipe)
    return failure("UnknownProductionRecipe", unit.id);
  auto context = production_context(catalog, unit, unit.next_due_tick);
  if (!context.ok)
    return {false, {}, context.error};
  std::optional<EconomicProductionLimits> exact_limits;
  if (exact && exact->catalog.production_profiles.find(unit.id)) {
    const auto *cycle_id = exact->state.active_cycles_by_unit.find(unit.id);
    if (!cycle_id)
      return failure("ActiveProductionCycleRequired", unit.id);
    auto closed = close_production_input_segment_in_candidate(
        c, state, catalog, exact->catalog, exact->state, exact->inputs, unit.id,
        {unit.next_due_tick, {}},
        {"local_economy", "production_cycle_due", *cycle_id},
        exact->units_per_half_day);
    if (!closed.ok)
      return {false, {}, closed.error};
    auto limits = read_accumulated_production_limits(
        state, catalog, exact->catalog, exact->state, unit.id,
        exact->units_per_half_day);
    if (!limits.ok)
      return {false, {}, limits.error};
    exact_limits = std::move(limits.value);
    context.value.capacity = exact_limits->capacity_limit;
  }
  ProductionReceipt result;
  if (catalog.production_management &&
      catalog.production_management->plans.find(unit.id))
    result.production_context = context.value;
  result.unit_id = unit.id;
  result.recipe_id = recipe->id;
  result.tick = tick;
  result.due_tick = unit.next_due_tick;
  std::optional<ProductionDisruptionQuote> disruption_quote;
  std::vector<ProductionQuantity> original_potential;
  if (disruption) {
    auto quoted = quote_production_disruption(c, catalog, disruption->catalog,
                                              disruption->state, unit, tick,
                                              unit.next_due_tick);
    if (!quoted.ok)
      return {false, {}, quoted.error};
    disruption_quote = std::move(quoted.value);
    result.cause_fact_ids = disruption_quote->registration_fact_ids;
  }
  std::vector<EconomicTakeoverExecutionPlan> operating_plans;
  Owner operating_owner = unit.owner;
  bool operation_allowed = true;
  if (takeover) {
    auto permission =
        read_economic_production_execution(takeover->read, unit.id);
    if (!permission.ok)
      return {false, {}, permission.error};
    if (permission.value) {
      operation_allowed =
          permission.value->status == EconomicTakeoverExecutionStatus::approved;
      if (permission.value->executing_owner)
        operating_owner = *permission.value->executing_owner;
      auto actual_unit = read_economic_takeover_execution(
          takeover->read,
          {EconomicResponsibilityKind::production_unit, unit.id, {}});
      if (!actual_unit.ok)
        return {false, {}, actual_unit.error};
      operating_plans.push_back(*actual_unit.value);
      for (const auto &asset : actual_unit.value->original.production_assets) {
        auto asset_plan = read_economic_takeover_execution(
            takeover->read, {EconomicResponsibilityKind::production_asset_use,
                             asset.id, unit.id});
        if (!asset_plan.ok)
          return {false, {}, asset_plan.error};
        if (!asset_plan.value)
          operation_allowed = false;
        else
          operating_plans.push_back(*asset_plan.value);
      }
      for (const auto &plan : operating_plans) {
        result.cause_fact_ids.push_back(plan.registration.registration_fact_id);
        if (plan.decision)
          result.cause_fact_ids.push_back(plan.decision->decision_fact_id);
      }
    }
  }
  const DynamicStepInput market_phase{step_from, tick};
  std::unique_ptr<MarketBusinessCaptureScope> capture;
  if (market) {
    auto opened = market->open_economic_business(
        c, MarketCauseKind::local_production, unit.id, unit.next_due_tick,
        market_phase);
    if (!opened.ok)
      return {false, {}, opened.error};
    capture = std::move(opened.value);
  }
  auto finish_cycle = [&]() -> Status {
    auto id = fact_id(c, "local_production", "local_production");
    if (!id.ok)
      return {false, {}, id.error};
    result.id = id.value;
    state.production_receipts.insert(id.value, result);
    unit.last_settlement_id = id.value;
    if (exact_limits) {
      auto settled = settle_accumulated_production_in_candidate(
          c, state, exact->state, *exact_limits, id.value);
      if (!settled.ok)
        return settled;
    }
    const Integer settled_due = unit.next_due_tick;
    if (!add(unit.next_due_tick, recipe->period_ticks, unit.next_due_tick))
      return failure("EconomyTickOverflow", unit.id);
    effects.receipt_ids.push_back(id.value);
    effects.fact_ids.push_back(id.value);
    if (takeover && !operating_plans.empty()) {
      auto recorded = record_economic_taken_production_source(
          takeover->read, takeover->state, operating_plans, id.value);
      if (!recorded.ok)
        return recorded;
    }
    if (capture) {
      auto captured = capture->commit_economy(state, market_phase, id.value);
      if (!captured.ok)
        return captured;
    }
    if (disruption_quote) {
      auto reduced = record_production_disruption(
          c, disruption->state, *disruption_quote, result, original_potential);
      if (!reduced.ok)
        return {false, {}, reduced.error};
      if (!reduced.value.empty())
        effects.fact_ids.push_back(reduced.value);
    }
    auto configured =
        takeover ? activate_taken_production_configuration_in_candidate(
                       c, state, catalog, *takeover, id.value, effects)
                 : activate_production_configuration(c, state, unit.id,
                                                     settled_due, effects);
    if (!configured.ok)
      return configured;
    if (exact_limits) {
      auto opened = open_production_cycle_in_candidate(
          c, state, catalog, exact->catalog, exact->state, exact->inputs,
          unit.id, {settled_due, {}}, exact->clock);
      if (!opened.ok)
        return {false, {}, opened.error};
    }
    return common_success();
  };
  if (!operation_allowed) {
    if (disruption_quote)
      for (const auto &item : recipe->outputs)
        original_potential.push_back(
            {unit.output_inventory_id, item.item_id, 0});
    return finish_cycle();
  }
  for (const auto &id : unit.labor_allocation_ids) {
    const auto *a = state.labor_allocations.find(id);
    if (!a || a->activity_or_production_id != unit.id)
      return failure("InvalidLaborAllocation", id);
    const auto *p = state.cohorts.find(a->cohort_id);
    if (!p)
      return failure("UnknownPopulationCohort", a->cohort_id);
    if (a->from_tick > step_from || a->through_tick < unit.next_due_tick ||
        p->status == PopulationStatus::training ||
        p->status == PopulationStatus::serving ||
        p->status == PopulationStatus::captive ||
        p->status == PopulationStatus::wounded || p->site_id != unit.site_id)
      continue;
    auto workers = effective_workers(state, catalog, *a, unit.next_due_tick,
                                     result.cause_fact_ids);
    if (!workers.ok)
      return {false, {}, workers.error};
    if (!add(result.allocated_workers, a->workers, result.allocated_workers) ||
        !add(result.effective_workers, workers.value, result.effective_workers))
      return failure("EconomyQuantityOverflow", unit.id);
  }
  Integer raw = 0, labor_remainder = unit.labor_remainder;
  if (exact_limits)
    raw = exact_limits->labor_limit;
  else if (!ratio(result.effective_workers, 1, recipe->labor_per_batch,
                  unit.labor_remainder, raw, labor_remainder))
    return failure("EconomyQuantityOverflow", unit.id);
  raw = std::min(raw, context.value.capacity);
  Integer factors = 0, planned = 0, factor_remainder = 0;
  if (!multiply(context.value.region_factor_bp, context.value.season_factor_bp,
                factors) ||
      !ratio(raw, factors, 100000000, unit.factor_remainder, planned,
             factor_remainder))
    return failure("EconomyQuantityOverflow", unit.id);
  planned = std::min(planned, context.value.capacity);
  const NativeEconomicMaterialRead *materials = nullptr;
  if (capture && capture->original_economic_fifo_selected()) {
    materials = market->original_economic_material_read();
    if (!materials || !materials->bound_to(c, catalog))
      return failure("EconomicOriginalMaterialReadRequired", unit.id);
    auto valid = materials->validate_bindings();
    if (!valid.ok)
      return valid;
  }
  auto input_count = [&](std::size_t index,
                         const StableId &inv) -> Outcome<Integer> {
    if (!materials)
      return c.count(inv, recipe->inputs[index].item_id, {operating_owner});
    if (index > UINT32_MAX)
      return {false,
              {},
              failure("EconomicMaterialNeedOrdinalOverflow", unit.id).error};
    const auto *inventory = c.inventory(inv);
    if (!inventory)
      return {false, {}, failure("UnknownInventory", inv).error};
    return materials->count({EconomicMaterialKind::production_input, unit.id,
                             static_cast<std::uint32_t>(index)},
                            *inventory, operating_owner, tick);
  };
  std::vector<Integer> available;
  for (std::size_t index = 0; index < recipe->inputs.size(); ++index) {
    Integer total = 0;
    for (const auto &inv : unit.input_inventory_ids) {
      auto count = input_count(index, inv);
      if (!count.ok)
        return {false, {}, count.error};
      if (!add(total, count.value, total))
        return failure("EconomyQuantityOverflow", unit.id);
    }
    available.push_back(total);
  }
  auto input_fits = [&](Integer batches) -> Outcome<bool> {
    for (std::size_t i = 0; i < recipe->inputs.size(); ++i) {
      Integer quantity = 0, remainder = 0;
      if (!ratio(batches, recipe->inputs[i].numerator,
                 recipe->inputs[i].denominator, unit.input_remainders[i],
                 quantity, remainder))
        return {false, false,
                failure("EconomyQuantityOverflow", unit.id).error};
      if (quantity > available[i])
        return {true, false, {}};
    }
    return {true, true, {}};
  };
  Integer low = 0, high = planned;
  while (low < high) {
    Integer middle = low + (high - low) / 2 + (high - low) % 2;
    auto fits = input_fits(middle);
    if (!fits.ok)
      return {false, {}, fits.error};
    if (fits.value)
      low = middle;
    else
      high = middle - 1;
  }
  result.batches = low;
  ResourceRecipe assets;
  CheckpointVector<EconomicMaterialConsumption> selected_inputs;
  CheckpointVector<EconomicMarketItemOperationBorrow> input_operations;
  for (std::size_t i = 0; i < recipe->inputs.size(); ++i) {
    const auto &input = recipe->inputs[i];
    Integer needed = 0, remainder = 0;
    if (!ratio(low, input.numerator, input.denominator,
               unit.input_remainders[i], needed, remainder))
      return failure("EconomyQuantityOverflow", unit.id);
    unit.input_remainders[i] = remainder;
    for (const auto &inv : unit.input_inventory_ids) {
      auto count = input_count(i, inv);
      if (!count.ok)
        return {false, {}, count.error};
      auto consumed = std::min(needed, count.value);
      if (consumed) {
        if (materials) {
          if (result.inputs.size() > UINT32_MAX || i > UINT32_MAX)
            return failure("EconomicMarketOperationOrdinalOverflow", unit.id);
          input_operations.push_back(
              {selected_inputs.size(),
               static_cast<std::uint32_t>(result.inputs.size()),
               static_cast<std::uint32_t>(i)});
        }
        if (materials)
          selected_inputs.push_back({{EconomicMaterialKind::production_input,
                                      unit.id, static_cast<std::uint32_t>(i)},
                                     *c.inventory(inv),
                                     operating_owner,
                                     consumed});
        else
          assets.operations.push_back(
              ConsumeLots{inv, input.item_id, consumed, {operating_owner}, {}});
        result.inputs.push_back({inv, input.item_id, consumed});
        needed -= consumed;
      }
      if (!needed)
        break;
    }
    if (needed)
      return failure("ProductionInputsChanged", unit.id);
  }
  std::vector<MarketQuantityLeg> input_legs;
  for (const auto &q : result.inputs)
    input_legs.push_back(
        {q.inventory_id, q.item_id, operating_owner, -q.quantity});
  std::optional<EconomicMaterialConsumptionRecipe> selected_recipe;
  if (materials) {
    auto prepared = prepare_economic_material_consumption(
        *materials, selected_inputs, tick);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    selected_recipe.emplace(std::move(prepared.value));
  }
  const auto &consumption =
      selected_recipe ? selected_recipe->recipe() : assets;
  auto consumed = capture ? capture->apply_original_economic_stage(
                                MarketCaptureStage::production_inputs,
                                consumption, input_legs, input_operations)
                          : c.apply_resources(consumption);
  if (!consumed.ok)
    return consumed;
  auto weight = c.inventory_weight(unit.output_inventory_id);
  if (!weight.ok)
    return {false, {}, weight.error};
  const auto *output = c.inventory(unit.output_inventory_id);
  if (!output)
    return failure("UnknownInventory", unit.output_inventory_id);
  Integer room = output->capacity - weight.value;
  ResourceRecipe produced;
  CheckpointVector<EconomicMarketItemOperationBorrow> output_operations;
  for (std::size_t i = 0; i < recipe->outputs.size(); ++i) {
    const auto &item = recipe->outputs[i];
    Integer potential = 0, next = 0;
    if (!ratio(low, item.numerator, item.denominator, unit.output_remainders[i],
               potential, next))
      return failure("EconomyQuantityOverflow", unit.id);
    unit.output_remainders[i] = next;
    if (disruption_quote) {
      original_potential.push_back(
          {unit.output_inventory_id, item.item_id, potential});
      auto adjusted = disrupted_production_quantity(
          potential, disruption_quote->remaining_yield_bp);
      if (!adjusted.ok)
        return {false, {}, adjusted.error};
      potential = adjusted.value;
    }
    const auto *definition = c.item_definition(item.item_id);
    if (!definition || definition->weight < 0)
      return failure("UnknownItem", item.item_id);
    Integer amount = potential;
    if (definition->weight > 0)
      amount = std::min(amount, room / definition->weight);
    Integer added_weight = 0;
    if (!multiply(amount, definition->weight, added_weight))
      return failure("EconomyQuantityOverflow", unit.id);
    room -= added_weight;
    if (amount) {
      if (materials) {
        if (result.outputs.size() > UINT32_MAX)
          return failure("EconomicMarketOperationOrdinalOverflow", unit.id);
        output_operations.push_back(
            {produced.operations.size(),
             static_cast<std::uint32_t>(result.outputs.size()), 0});
      }
      produced.operations.push_back(AddLot{
          unit.output_inventory_id, item.item_id, amount, operating_owner, {}});
      result.outputs.push_back(
          {unit.output_inventory_id, item.item_id, amount});
    }
    if (potential > amount)
      result.unrealized_output.push_back(
          {unit.output_inventory_id, item.item_id, potential - amount});
  }
  std::vector<MarketQuantityLeg> output_legs;
  for (const auto &q : result.outputs)
    output_legs.push_back(
        {q.inventory_id, q.item_id, operating_owner, q.quantity});
  auto added = capture ? capture->apply_original_economic_stage(
                             MarketCaptureStage::production_outputs, produced,
                             output_legs, output_operations)
                       : c.apply_resources(produced);
  if (!added.ok)
    return added;
  unit.labor_remainder = labor_remainder;
  unit.factor_remainder = factor_remainder;
  return finish_cycle();
}
struct PreparedRationProductionInput {
  StableId unit_id;
  PreparedProductionInputClose close;
};
Outcome<std::vector<PreparedRationProductionInput>>
prepare_ration_production_inputs(const CommonStore &common,
                                 const LocalEconomyState &state,
                                 const LocalEconomyCatalog &catalog,
                                 EconomicProductionContext *exact,
                                 const RecurringObligation &obligation,
                                 Integer tick) {
  std::vector<PreparedRationProductionInput> out;
  if (!exact || obligation.beneficiary.kind != SubjectKind::cohort)
    return {true, std::move(out), {}};
  for (const auto &row : exact->catalog.production_profiles.rows()) {
    const auto *unit = catalog.production_units.find(row.first);
    if (!unit)
      return {false,
              {},
              failure("ProductionOriginalUnitRequired", row.first).error};
    bool affected = false;
    for (const auto &id : unit->labor_allocation_ids) {
      const auto *a = catalog.labor_allocations.find(id);
      if (!a)
        return {false,
                {},
                failure("ProductionOriginalAllocationRequired", id).error};
      affected |= a->cohort_id == obligation.beneficiary.id;
    }
    if (!affected)
      continue;
    auto prepared = prepare_production_input_close(
        common, state, catalog, *exact, unit->id, {tick, {}});
    if (!prepared.ok)
      return {false, {}, prepared.error};
    out.push_back({unit->id, std::move(prepared.value)});
  }
  return {true, std::move(out), {}};
}
Status apply_ration_production_inputs(
    CommonStore &common, EconomicProductionContext *exact,
    std::vector<PreparedRationProductionInput> &prepared,
    const StableId &receipt, Integer tick, LocalEconomyEffects &effects) {
  if (!exact)
    return prepared.empty()
               ? common_success()
               : failure("ProductionRationInputContextRequired", receipt);
  const MomentOwnerSource source{{"local_economy", "local_ration", receipt}, 1};
  for (auto &entry : prepared) {
    auto closed = apply_prepared_production_input_close_in_candidate(
        common, *exact, std::move(entry.close), source.source_ref);
    if (!closed.ok)
      return {false, {}, closed.error};
    if (closed.value)
      effects.fact_ids.push_back(*closed.value);
    const auto *id = exact->state.active_cycles_by_unit.find(entry.unit_id);
    const auto *cycle = id ? exact->state.cycles.find(*id) : nullptr;
    if (!cycle)
      return failure("ProductionRationCycleRequired", entry.unit_id);
    if (tick < cycle->due_tick) {
      auto installed = install_changed_production_input_source_in_candidate(
          exact->state, exact->inputs, entry.unit_id, {tick, {}}, source);
      if (!installed.ok)
        return installed;
    }
  }
  return common_success();
}
// 阶段正文只作用于已复制的本域候选，失败不留下应付款或舍入余数。
Outcome<LocalEconomyEffects>
step_body(CommonStore &c, LocalEconomyState &s,
          const LocalEconomyCatalog &catalog,
          const OrganizationRead &organizations, const ActivityRead &,
          const DynamicStepInput &input,
          const HouseholdProvisionManagementContext *household_management,
          ProductionDisruptionContext *disruption,
          EconomicTakeoverPhaseContext *takeover,
          const EstateEmploymentContext *employment,
          MarketBusinessCaptureServices *market,
          const std::set<StableId> &private_obligations,
          EconomicProductionContext *exact = nullptr) {
  LocalEconomyEffects effects;
  auto closed =
      close_expired_household_provision(c, s, catalog, input.to_tick, effects);
  if (!closed.ok)
    return {false, {}, closed.error};
  // 先建立到期款项，随后按义务最早欠期支付，口粮和生产不得抢先改写这些责任。
  for (auto &row : s.obligations.rows()) {
    auto &obligation = row.second;
    if (private_obligations.count(row.first))
      continue;
    if (employment && estate_employment_detail::obligation_contract(
                          employment->state, row.first))
      continue;
    if (obligation.kind == ObligationKind::ration)
      continue;
    if (obligation.active &&
        !s.membership_wage_bindings.find(obligation.authority_ref) &&
        obligation.kind == ObligationKind::wage &&
        obligation.beneficiary.kind == SubjectKind::actor) {
      auto member = organizations.member_view(obligation.beneficiary.id,
                                              obligation.principal.id);
      if (member && member->response == "departed")
        obligation.active = false;
    }
    while (obligation.active && obligation.next_due_tick <= input.to_tick) {
      auto amount = due_amount(c, s, catalog, obligation);
      if (!amount.ok)
        return {false, {}, amount.error};
      std::optional<EconomicTakeoverExecutionPlan> responsibility;
      if (takeover) {
        auto plan = read_economic_takeover_execution(
            takeover->read,
            {EconomicResponsibilityKind::obligation, obligation.id, {}});
        if (!plan.ok)
          return {false, {}, plan.error};
        responsibility = std::move(plan.value);
      }
      if (responsibility) {
        auto accrual = validate_taken_obligation_accrual(
            takeover->read, obligation, amount.value, obligation.next_due_tick);
        if (!accrual.ok)
          return {false, {}, accrual.error};
      } else if (!organizations.institutional_obligation_allowed(
                     obligation.authority_ref, obligation.id,
                     obligation.principal.id,
                     obligation.source_wallet_or_inventory, amount.value,
                     obligation.next_due_tick))
        return {false,
                {},
                failure("InstitutionalAuthorityDenied", obligation.id).error};
      auto id = fact_id(c, "local_due",
                        obligation.kind == ObligationKind::wage
                            ? "wage_due"
                            : "contract_payment_due");
      if (!id.ok)
        return {false, {}, id.error};
      s.arrears.insert(id.value, {id.value, obligation.id,
                                  obligation.next_due_tick, amount.value, 0});
      obligation.last_receipt_id = id.value;
      effects.fact_ids.push_back(id.value);
      effects.receipt_ids.push_back(id.value);
      if (!add(obligation.next_due_tick, obligation.period_ticks,
               obligation.next_due_tick))
        return {false, {}, failure("EconomyTickOverflow", obligation.id).error};
    }
    auto paid = pay_arrears(c, s, organizations, obligation, input.to_tick,
                            effects, takeover);
    if (!paid.ok)
      return {false, {}, paid.error};
  }
  for (auto &row : s.obligations.rows()) {
    auto &obligation = row.second;
    if (!obligation.active || obligation.kind != ObligationKind::ration)
      continue;
    while (obligation.next_due_tick <= input.to_tick) {
      if (!provision_for(s, obligation, obligation.next_due_tick))
        return {false,
                {},
                failure("ProvisionBindingMismatch", obligation.id).error};
      auto prepared_production = prepare_ration_production_inputs(
          c, s, catalog, exact, obligation, input.to_tick);
      if (!prepared_production.ok)
        return {false, {}, prepared_production.error};
      auto amount = due_amount(c, s, catalog, obligation);
      if (!amount.ok)
        return {false, {}, amount.error};
      std::optional<EconomicTakeoverExecutionPlan> ration_plan;
      if (takeover) {
        auto plan = read_economic_takeover_execution(
            takeover->read,
            {EconomicResponsibilityKind::obligation, obligation.id, {}});
        if (!plan.ok)
          return {false, {}, plan.error};
        ration_plan = std::move(plan.value);
      }
      if (ration_plan) {
        auto accrual = validate_taken_obligation_accrual(
            takeover->read, obligation, amount.value, obligation.next_due_tick);
        if (!accrual.ok)
          return {false, {}, accrual.error};
      } else if (obligation.principal.kind == SubjectKind::cohort) {
        if (!catalog.household_provisioning || !s.household_provisioning)
          return {
              false,
              {},
              failure("HouseholdProvisionUnavailable", obligation.id).error};
        auto allowed = check_household_ration_authority(
            c, s, catalog, *catalog.household_provisioning,
            *s.household_provisioning, obligation, obligation.next_due_tick);
        if (!allowed.ok)
          return {false, {}, allowed.error};
      } else if (!organizations.institutional_obligation_allowed(
                     obligation.authority_ref, obligation.id,
                     obligation.principal.id,
                     obligation.source_wallet_or_inventory, amount.value,
                     obligation.next_due_tick))
        return {false,
                {},
                failure("InstitutionalAuthorityDenied", obligation.id).error};
      const auto *inventory =
          c.inventory(obligation.source_wallet_or_inventory);
      if (!inventory)
        return {
            false,
            {},
            failure("UnknownInventory", obligation.source_wallet_or_inventory)
                .error};
      std::unique_ptr<MarketBusinessCaptureScope> capture;
      if (market) {
        auto opened = market->open_economic_business(
            c, MarketCauseKind::local_ration, obligation.id,
            obligation.next_due_tick, input);
        if (!opened.ok)
          return {false, {}, opened.error};
        capture = std::move(opened.value);
      }
      const NativeEconomicMaterialRead *materials = nullptr;
      if (capture && capture->original_economic_fifo_selected()) {
        materials = market->original_economic_material_read();
        if (!materials || !materials->bound_to(c, catalog))
          return {false,
                  {},
                  failure("EconomicOriginalMaterialReadRequired", obligation.id)
                      .error};
        auto valid = materials->validate_bindings();
        if (!valid.ok)
          return {false, {}, valid.error};
      }
      const EconomicMaterialNeedKey need{EconomicMaterialKind::ration,
                                         obligation.id, 0};
      auto stock = materials ? materials->count(need, *inventory,
                                                inventory->owner, input.to_tick)
                             : c.count(inventory->id, obligation.item_id,
                                       {inventory->owner});
      if (!stock.ok)
        return {false, {}, stock.error};
      Integer consumed =
          ration_plan && ration_plan->status !=
                             EconomicTakeoverExecutionStatus::approved
              ? 0
              : std::min(amount.value, stock.value);
      if (household_management) {
        auto limit = household_provision_consumption_limit(
            s, catalog, *household_management, obligation,
            obligation.next_due_tick, amount.value);
        if (!limit.ok)
          return {false, {}, limit.error};
        consumed = std::min(consumed, limit.value);
      }
      if (consumed) {
        ResourceRecipe recipe;
        std::optional<EconomicMaterialConsumptionRecipe> selected_recipe;
        if (materials) {
          const EconomicMaterialConsumption request{need, *inventory,
                                                    inventory->owner, consumed};
          auto prepared = prepare_economic_material_consumption(
              *materials,
              std::span<const EconomicMaterialConsumption>{&request, 1},
              input.to_tick);
          if (!prepared.ok)
            return {false, {}, prepared.error};
          selected_recipe.emplace(std::move(prepared.value));
        } else
          recipe.operations.push_back(ConsumeLots{inventory->id,
                                                  obligation.item_id,
                                                  consumed,
                                                  {inventory->owner},
                                                  {}});
        const auto &consumption =
            selected_recipe ? selected_recipe->recipe() : recipe;
        const std::vector<MarketQuantityLeg> legs{
            {inventory->id, obligation.item_id, inventory->owner, -consumed}};
        const EconomicMarketItemOperationBorrow operation{0, 0, 0};
        const auto metadata =
            materials
                ? std::span<const EconomicMarketItemOperationBorrow>{&operation,
                                                                     1}
                : std::span<const EconomicMarketItemOperationBorrow>{};
        auto status = capture ? capture->apply_original_economic_stage(
                                    MarketCaptureStage::ration, consumption,
                                    legs, metadata)
                              : c.apply_resources(consumption);
        if (!status.ok)
          return {false, {}, status.error};
      }
      auto id = fact_id(c, "local_ration", "local_ration");
      if (!id.ok)
        return {false, {}, id.error};
      s.rations.insert(id.value,
                       {id.value, obligation.id,
                        obligation.source_wallet_or_inventory,
                        obligation.item_id, obligation.beneficiary,
                        input.to_tick, obligation.next_due_tick, amount.value,
                        consumed, amount.value - consumed});
      if (ration_plan) {
        auto recorded = record_economic_taken_ration_source(
            takeover->read, takeover->state, *ration_plan, id.value);
        if (!recorded.ok)
          return {false, {}, recorded.error};
      }
      if (capture) {
        auto captured = capture->commit_economy(s, input, id.value);
        if (!captured.ok)
          return {false, {}, captured.error};
      }
      obligation.last_receipt_id = id.value;
      effects.fact_ids.push_back(id.value);
      effects.receipt_ids.push_back(id.value);
      if (!add(obligation.next_due_tick, obligation.period_ticks,
               obligation.next_due_tick))
        return {false, {}, failure("EconomyTickOverflow", obligation.id).error};
      auto input_changed =
          apply_ration_production_inputs(c, exact, prepared_production.value,
                                         id.value, input.to_tick, effects);
      if (!input_changed.ok)
        return {false, {}, input_changed.error};
    }
  }
  for (auto &row : s.production_units.rows()) {
    auto &unit = row.second;
    const auto *binding = s.production_bindings.find(unit.id);
    if (!binding || binding->policy_owner != ProvisionOwner::macro)
      continue;
    while (unit.next_due_tick <= input.to_tick) {
      auto status = produce(c, s, catalog, unit, input.to_tick, input.from_tick,
                            effects, disruption, takeover, market, exact);
      if (!status.ok)
        return {false, {}, status.error};
    }
  }
  s.last_step_tick = input.to_tick;
  return {true, std::move(effects), {}};
}
} // namespace
// 任职终态沿相同旧欠期排序和原子钱包recipe结账，绝不新建支付总账。
Status local_economy_detail::settle_obligation_arrears(
    CommonStore &c, LocalEconomyState &s, const OrganizationRead &access,
    const RecurringObligation &obligation, Integer tick,
    LocalEconomyEffects &effects) {
  return pay_arrears(c, s, access, obligation, tick, effects);
}
// 原生初始化只复制已声明责任；所有钱货容器必须已经由共同工厂建立。
Outcome<LocalEconomyState>
make_initial_local_economy(CommonStore &c, const LocalEconomyCatalog &catalog,
                           Integer initial_tick) {
  if (initial_tick < 0 || initial_tick != c.tick())
    return {false, {}, common_failure("EconomyInitialTickMismatch").error};
  LocalEconomyState s;
  s.last_step_tick = initial_tick;
  if (!catalog.enabled)
    return {true, std::move(s), {}};
  if (catalog.production_management)
    s.production_management = ProductionManagementState{};
  s.households = catalog.households;
  s.cohorts = catalog.cohorts;
  s.labor_allocations = catalog.labor_allocations;
  s.production_units = catalog.production_units;
  s.obligations = catalog.obligations;
  s.provision_bindings = catalog.provision_bindings;
  s.production_bindings = catalog.production_bindings;
  for (auto &row : s.production_units.rows()) {
    auto &unit = row.second;
    const auto *recipe = catalog.recipes.find(unit.recipe_id);
    if (!recipe || unit.next_due_tick <= initial_tick)
      return {false,
              {},
              common_failure("InvalidProductionInitialDue", row.first).error};
    if (unit.input_remainders.empty())
      unit.input_remainders.resize(recipe->inputs.size());
    if (unit.output_remainders.empty())
      unit.output_remainders.resize(recipe->outputs.size());
  }
  for (const auto &row : s.obligations.rows()) {
    if (row.second.next_due_tick <= initial_tick)
      return {false,
              {},
              common_failure("InvalidObligationInitialDue", row.first).error};
    s.obligation_remainders.insert(row.first, 0);
  }
  if (catalog.household_provisioning) {
    auto household = make_initial_household_provision(
        c, catalog, *catalog.household_provisioning);
    if (!household.ok)
      return {false, {}, household.error};
    s.household_provisioning = std::move(household.value);
  }
  for (const auto &row : catalog.supply_sources.rows()) {
    const auto &source = row.second;
    if (!c.inventory(source.inventory_id) || !c.item_definition(source.item_id))
      return {false,
              {},
              common_failure("InvalidLocalSupplySource", row.first).error};
    auto id = fact_id(c, "local_supply", "local_supply_source");
    if (!id.ok)
      return {false, {}, id.error};
    s.supply_registrations.insert(
        id.value,
        {id.value, source.source_key, source.organization_id, source.site_id,
         source.inventory_id, source.item_id, initial_tick});
  }
  auto observed =
      refresh_local_supply_observations(c, s, catalog, initial_tick);
  if (!observed.ok)
    return {false, {}, observed.error};
  return {true, std::move(s), {}};
}
// 同半日重试保持所有收据和支付不变；技术失败恢复本阶段全部共同写入。
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input) {
  return step_local_economy(c, s, catalog, organizations, activity, input,
                            nullptr);
}
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management) {
  return step_local_economy(c, s, catalog, organizations, activity, input,
                            household_management, nullptr);
}
// 独立减产段和原经济表共享同一候选；晚失败一起撤回真实库存、事实及实例序。
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption) {
  return step_local_economy(c, s, catalog, organizations, activity, input,
                            household_management, disruption, nullptr);
}
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover) {
  return step_local_economy(c, s, catalog, organizations, activity, input,
                            household_management, disruption, takeover,
                            nullptr);
}
namespace {
Outcome<LocalEconomyEffects> step_with_market(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover,
    const EstateEmploymentContext *employment,
    MarketBusinessCaptureServices *market, bool own_savepoint,
    const PrivateShopResponsibilitySourceContext *shops,
    EconomicProductionContext *exact = nullptr) {
  std::set<StableId> private_obligations;
  if (shops) {
    if (&shops->common != &c || &shops->economy != &s ||
        &shops->economy_catalog != &catalog || shops->phase != &input)
      return {false,
              {},
              common_failure("PrivateShopEconomyPhaseContextMismatch").error};
    auto source = read_private_shop_economy_source_ids(*shops);
    if (!source.ok)
      return {false, {}, source.error};
    private_obligations.insert(source.value.obligation_ids.begin(),
                               source.value.obligation_ids.end());
  }
  if (exact && exact->original_payments) {
    auto owned = exact->original_payments->read_original_payment_source_ids();
    if (!owned.ok)
      return {false, {}, owned.error};
    for (const auto &id : owned.value.obligation_ids)
      if (!private_obligations.insert(id).second)
        return {
            false,
            {},
            common_failure("DuplicateEconomicResponsibilityOwner", id).error};
  }
  if (employment) {
    if (&employment->common != &c || &employment->economy != &s ||
        &employment->economy_catalog != &catalog || employment->phase != &input)
      return {false,
              {},
              common_failure("EstateEmploymentPhaseContextMismatch").error};
    auto valid = validate_estate_employment(*employment);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  if (!catalog.enabled) {
    if (disruption || takeover || shops)
      return {false, {}, common_failure("EconomyPhaseProfileDisabled").error};
    return {true, {}, {}};
  }
  if (input.to_tick == s.last_step_tick && input.from_tick + 1 == input.to_tick)
    return {true, {}, {}};
  if (input.from_tick != s.last_step_tick ||
      input.to_tick != input.from_tick + 1)
    return {false, {}, common_failure("EconomyStepTickMismatch").error};
  std::unique_ptr<local_economy_detail::PhaseSavepoint> guard;
  if (own_savepoint) {
    guard = std::make_unique<local_economy_detail::PhaseSavepoint>(c);
    if (!guard->token().ok)
      return {false, {}, guard->token().error};
  }
  CandidateStateGuard<LocalEconomyState> state_guard(s);
  auto &candidate = s;
  std::optional<ProductionDisruptionState> disruption_candidate;
  std::optional<ProductionDisruptionContext> context;
  if (disruption) {
    disruption_candidate = disruption->state;
    context.emplace(ProductionDisruptionContext{disruption->catalog,
                                                *disruption_candidate});
  }
  std::optional<CandidateStateGuard<EconomicTakeoverExecutionState>>
      execution_guard;
  std::optional<EconomicTakeoverPhaseContext> execution_context;
  if (takeover) {
    const auto &original = takeover->read;
    if (&original.common != &c || &original.economy != &s ||
        &original.catalog != &catalog || takeover->state.version != 1)
      return {false, {}, failure("EconomicTakeoverPhaseContextMismatch").error};
    execution_guard.emplace(takeover->state);
    EconomicTakeoverReadContext read{c,
                                     candidate,
                                     catalog,
                                     original.fiscal,
                                     original.fiscal_catalog,
                                     original.relief,
                                     original.politics,
                                     original.organizations,
                                     original.organization_catalog,
                                     &input,
                                     original.occupation_fiscal,
                                     original.taxpayer_assets};
    execution_context.emplace(
        EconomicTakeoverPhaseContext{read, takeover->state});
  }
  auto result = step_body(c, candidate, catalog, organizations, activity, input,
                          household_management, context ? &*context : nullptr,
                          execution_context ? &*execution_context : nullptr,
                          employment, market, private_obligations, exact);
  if (!result.ok)
    return result;
  if (context) {
    auto valid = validate_production_disruptions(
        c, candidate, catalog, context->catalog, context->state);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  if (guard) {
    auto committed = guard->commit();
    if (!committed.ok)
      return {false, {}, committed.error};
  }
  if (disruption)
    disruption->state = std::move(*disruption_candidate);
  state_guard.commit();
  if (execution_guard)
    execution_guard->commit();
  return result;
}
} // namespace
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover,
    const EstateEmploymentContext *employment) {
  return step_with_market(c, s, catalog, organizations, activity, input,
                          household_management, disruption, takeover,
                          employment, nullptr, true, nullptr);
}
Outcome<LocalEconomyEffects> step_local_economy_in_candidate(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover,
    const EstateEmploymentContext *employment,
    MarketBusinessCaptureServices *market) {
  return step_local_economy_in_candidate(
      c, s, catalog, organizations, activity, input, household_management,
      disruption, takeover, employment, market, nullptr);
}
Outcome<LocalEconomyEffects> step_local_economy_in_candidate(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover,
    const EstateEmploymentContext *employment,
    MarketBusinessCaptureServices *market,
    const PrivateShopResponsibilitySourceContext *shops) {
  if (c.tick() == INT64_MAX || input.from_tick != c.tick() ||
      input.to_tick != c.tick() + 1)
    return {false, {}, common_failure("EconomyStepTickMismatch").error};
  return step_with_market(c, s, catalog, organizations, activity, input,
                          household_management, disruption, takeover,
                          employment, market, false, shops);
}
Outcome<LocalEconomyEffects> step_local_economy_exact_in_candidate(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    const OrganizationRead &organizations, const ActivityRead &activity,
    const DynamicStepInput &input,
    const HouseholdProvisionManagementContext *household_management,
    ProductionDisruptionContext *disruption,
    EconomicTakeoverPhaseContext *takeover,
    const EstateEmploymentContext *employment,
    MarketBusinessCaptureServices *market,
    const PrivateShopResponsibilitySourceContext *shops,
    EconomicProductionContext &exact) {
  auto current = exact.clock.read_current_moment();
  if (!current.ok)
    return {false, {}, current.error};
  auto at_boundary =
      compare_world_moments(current.value, WorldMoment{input.to_tick, {}});
  if (!at_boundary.ok)
    return {false, {}, at_boundary.error};
  if (exact.units_per_half_day <= 0 || input.from_tick < 0 ||
      input.from_tick == INT64_MAX || input.to_tick != input.from_tick + 1 ||
      input.from_tick < c.tick() || at_boundary.value != 0)
    return {false, {}, common_failure("EconomyExactPhaseMismatch").error};
  CandidateStateGuard<EconomicWorkSourcesState> work_guard(exact.state);
  auto result = step_with_market(c, s, catalog, organizations, activity, input,
                                 household_management, disruption, takeover,
                                 employment, market, false, shops, &exact);
  if (result.ok)
    work_guard.commit();
  return result;
}
namespace {
// 协调边界先完整核暂停来源，再写任何新快照；不把新公库库存报给旧政策。
Outcome<std::set<StableId>>
supply_paused_keys(const CommonStore &c, const LocalEconomyState &s,
                   const LocalEconomyCatalog &catalog,
                   const EconomicTakeoverReadContext *takeover) {
  std::set<StableId> keys;
  if (takeover) {
    if (&takeover->common != &c || &takeover->economy != &s ||
        &takeover->catalog != &catalog)
      return {false, {}, common_failure("EconomicSupplyContextMismatch").error};
    auto paused = read_economic_supply_suspensions(*takeover);
    if (!paused.ok)
      return {false, {}, paused.error};
    for (const auto &row : paused.value)
      keys.insert(row.source_key);
  }
  for (const auto &row : catalog.supply_sources.rows()) {
    const auto *inventory = c.inventory(row.second.inventory_id);
    if (!inventory)
      return {
          false,
          {},
          common_failure("UnknownInventory", row.second.inventory_id).error};
    if (!keys.count(row.second.source_key) &&
        !owner_equal(
            inventory->owner,
            Owner{Owner::Kind::organization, row.second.organization_id, {}}))
      return {false,
              {},
              common_failure("LocalSupplyOwnerMismatch", row.first).error};
  }
  return {true, std::move(keys), {}};
}
} // namespace
// 观察只读已冻结且仍匹配实际库存的快照，不给旧事实附上新的数量。
Outcome<std::vector<SupplyObservation>>
observe_local_supply(const CommonStore &c, const LocalEconomyState &s,
                     const LocalEconomyCatalog &catalog,
                     const EconomicTakeoverReadContext *takeover) {
  auto paused = supply_paused_keys(c, s, catalog, takeover);
  if (!paused.ok)
    return {false, {}, paused.error};
  std::vector<SupplyObservation> out;
  for (const auto &row : catalog.supply_sources.rows()) {
    const auto &source = row.second;
    if (paused.value.count(source.source_key))
      continue;
    const auto *inventory = c.inventory(source.inventory_id);
    if (!inventory)
      return {false,
              {},
              common_failure("UnknownInventory", source.inventory_id).error};
    auto quantity = count_inventory_physical_items(
        c, source.inventory_id, source.item_id, inventory->owner);
    if (!quantity.ok) {
      if (quantity.error.original.code == "ProjectionOverflow")
        return {
            false,
            {},
            common_failure("EconomyQuantityOverflow", source.source_key).error};
      return {false, {}, quantity.error};
    }
    const Integer physical = quantity.value;
    const LocalSupplyObservation *latest = nullptr;
    for (const auto &observation : s.supply_observations.rows())
      if (observation.second.observation.source_key == source.source_key)
        latest = &observation.second;
    if (!latest || latest->observation.source_revision != inventory->revision ||
        latest->observation.physical_amount != physical)
      return {false,
              {},
              common_failure("LocalSupplyObservationStale", source.source_key)
                  .error};
    out.push_back(latest->observation);
  }
  return {true, std::move(out), {}};
}
// 正式协调阶段仅为有变化的来源登记一次不可变快照，玩家交易也经过同一个入口。
Outcome<std::vector<SupplyObservation>> refresh_local_supply_observations(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &catalog,
    Integer tick, const EconomicTakeoverReadContext *takeover) {
  auto paused = supply_paused_keys(c, s, catalog, takeover);
  if (!paused.ok)
    return {false, {}, paused.error};
  if (tick < 0 || tick < std::min(c.tick(), s.last_step_tick))
    return {false, {}, common_failure("InvalidSupplyObservationTick").error};
  std::vector<SupplyObservation> out;
  for (const auto &row : catalog.supply_sources.rows()) {
    const auto &source = row.second;
    if (paused.value.count(source.source_key))
      continue;
    const auto *inventory = c.inventory(source.inventory_id);
    if (!inventory)
      return {false,
              {},
              common_failure("UnknownInventory", source.inventory_id).error};
    auto quantity = count_inventory_physical_items(
        c, source.inventory_id, source.item_id, inventory->owner);
    if (!quantity.ok) {
      if (quantity.error.original.code == "ProjectionOverflow")
        return {
            false,
            {},
            common_failure("EconomyQuantityOverflow", source.source_key).error};
      return {false, {}, quantity.error};
    }
    const Integer physical = quantity.value;
    const LocalSupplyObservation *latest = nullptr;
    for (const auto &observation : s.supply_observations.rows())
      if (observation.second.observation.source_key == source.source_key)
        latest = &observation.second;
    if (latest && latest->observation.source_revision == inventory->revision &&
        latest->observation.physical_amount == physical &&
        latest->observation.target_amount == source.target_amount) {
      out.push_back(latest->observation);
      continue;
    }
    auto id =
        fact_id(c, "local_supply_observation", "local_supply_observation");
    if (!id.ok)
      return {false, {}, id.error};
    SupplyObservation snapshot{source.source_key,    source.organization_id,
                               source.site_id,       source.inventory_id,
                               source.item_id,       id.value,
                               source.target_amount, physical,
                               inventory->revision};
    s.supply_observations.insert(
        id.value, {id.value, snapshot, tick, source.public_report});
    out.push_back(snapshot);
  }
  return {true, std::move(out), {}};
}
// 原Profile接口仍严格要求原机构拥有供给库，不能隐式解锁跨Owner观察。
Outcome<std::vector<SupplyObservation>>
observe_local_supply(const CommonStore &c, const LocalEconomyState &s,
                     const LocalEconomyCatalog &catalog) {
  return observe_local_supply(c, s, catalog, nullptr);
}
Outcome<std::vector<SupplyObservation>>
refresh_local_supply_observations(CommonStore &c, LocalEconomyState &s,
                                  const LocalEconomyCatalog &catalog,
                                  Integer tick) {
  return refresh_local_supply_observations(c, s, catalog, tick, nullptr);
}
// 债务汇总无独立写口，原应付与累计实付之差始终是唯一余额依据。
Outcome<Integer> outstanding_arrears(const LocalEconomyState &s,
                                     const StableId &obligation_id) {
  Integer amount = 0;
  for (const auto &row : s.arrears.rows())
    if (row.second.obligation_id == obligation_id) {
      if (row.second.original_due < row.second.paid_amount ||
          !local_economy_detail::add(
              amount, row.second.original_due - row.second.paid_amount, amount))
        return {false, 0,
                common_failure("InvalidArrearAmount", row.first).error};
    }
  return {true, amount, {}};
}
// 分配先检查同群体所有重叠区间，行政引用不能凭空增加可用劳力。
Status allocate_labor(const CommonStore &c, LocalEconomyState &s,
                      const LaborAllocation &allocation) {
  const auto *cohort = s.cohorts.find(allocation.cohort_id);
  if (!cohort || allocation.id.empty() || allocation.workers < 0 ||
      allocation.from_tick < c.tick() ||
      allocation.through_tick < allocation.from_tick ||
      s.labor_allocations.find(allocation.id))
    return common_failure("InvalidLaborAllocation", allocation.id);
  std::set<Integer> points{allocation.from_tick};
  for (const auto &row : s.labor_allocations.rows())
    if (row.second.cohort_id == allocation.cohort_id &&
        local_economy_detail::interval(
            row.second.from_tick, row.second.through_tick, allocation.from_tick,
            allocation.through_tick))
      points.insert(std::max(allocation.from_tick, row.second.from_tick));
  for (auto point : points) {
    Integer used = allocation.workers;
    for (const auto &row : s.labor_allocations.rows())
      if (row.second.cohort_id == allocation.cohort_id &&
          row.second.from_tick <= point && point <= row.second.through_tick)
        if (!local_economy_detail::add(used, row.second.workers, used))
          return common_failure("EconomyQuantityOverflow", allocation.id);
    if (used > cohort->labor_capacity)
      return common_failure("InsufficientPopulationLabor",
                            allocation.cohort_id);
  }
  s.labor_allocations.insert(allocation.id, allocation);
  return common_success();
}
Status local_economy_detail::reduce_original_population_labor_release(
    OrderedTable<LaborAllocation> &allocations, const StableId &cohort,
    Integer remaining, Integer effective,
    const std::vector<StableId> &releases) {
  using namespace local_economy_detail;
  if (remaining < 0 || effective < 0)
    return failure("InvalidPopulationLaborRelease", cohort);
  std::set<StableId> seen;
  for (const auto &id : releases) {
    const auto *row = allocations.find(id);
    if (!seen.insert(id).second || !row || row->cohort_id != cohort)
      return failure("InvalidLaborRelease", id);
  }
  std::set<Integer> boundaries{effective};
  for (const auto &row : allocations.rows())
    if (row.second.cohort_id == cohort && row.second.through_tick >= effective)
      boundaries.insert(std::max(effective, row.second.from_tick));
  for (auto point : boundaries) {
    Integer used = 0;
    for (const auto &row : allocations.rows())
      if (row.second.cohort_id == cohort && row.second.from_tick <= point &&
          point <= row.second.through_tick)
        if (!add(used, row.second.workers, used))
          return failure("EconomyQuantityOverflow", cohort);
    Integer release = std::max(Integer(0), used - remaining);
    for (const auto &id : releases) {
      auto *row = allocations.find(id);
      if (row->from_tick <= point && point <= row->through_tick) {
        Integer take = std::min(release, row->workers);
        row->workers -= take;
        release -= take;
      }
    }
    if (release)
      return failure("PopulationLaborCommitted", cohort);
  }
  return common_success();
}
// 人数与劳力分别守恒；家庭容器只复用，未显式供养的新目的群体拒绝成立。
Outcome<StableId> local_economy_detail::transfer_population_counts(
    CommonStore &c, LocalEconomyState &s,
    const PopulationTransferRequest &request,
    PopulationTransferLaborCapture *capture) {
  using namespace local_economy_detail;
  const auto *from = s.cohorts.find(request.from_cohort_id);
  const auto *destination = s.cohorts.find(request.to_cohort_id);
  if (!from || request.from_cohort_id == request.to_cohort_id ||
      request.to_cohort_id.empty() || request.count <= 0 ||
      request.labor_eligible_count < 0 ||
      request.labor_eligible_count > request.count ||
      request.count > from->headcount ||
      request.labor_eligible_count > from->labor_capacity ||
      from->labor_capacity - request.labor_eligible_count >
          from->headcount - request.count ||
      request.effective_tick < c.tick() ||
      !c.state().facts.find(request.source_fact_id) || request.reason.empty())
    return {false,
            {},
            failure("InvalidPopulationTransfer", request.from_cohort_id).error};
  if (destination &&
      (destination->household_group_id != from->household_group_id ||
       destination->site_id != from->site_id ||
       destination->status != request.destination_status ||
       destination->assignment_id != request.assignment_id))
    return {
        false,
        {},
        failure("PopulationDestinationMismatch", request.to_cohort_id).error};
  if (capture && !capture->transfer_id().empty())
    return {false,
            {},
            failure("PopulationLaborCaptureAlreadyUsed", capture->transfer_id())
                .error};
  auto candidate = s;
  auto &source = *candidate.cohorts.find(request.from_cohort_id);
  std::set<StableId> seen;
  for (const auto &id : request.release_allocation_ids) {
    const auto *allocation = candidate.labor_allocations.find(id);
    if (!seen.insert(id).second || !allocation ||
        allocation->cohort_id != source.id)
      return {false, {}, failure("InvalidLaborRelease", id).error};
  }
  auto reduced = reduce_original_population_labor_release(
      candidate.labor_allocations, source.id,
      source.labor_capacity - request.labor_eligible_count,
      request.effective_tick, request.release_allocation_ids);
  if (!reduced.ok)
    return {false, {}, reduced.error};
  PopulationCohort target = destination ? *destination : source;
  target.id = request.to_cohort_id;
  target.status = request.destination_status;
  target.assignment_id = request.assignment_id;
  if (!destination) {
    target.headcount = 0;
    target.labor_capacity = 0;
  }
  if (!add(target.headcount, request.count, target.headcount) ||
      !add(target.labor_capacity, request.labor_eligible_count,
           target.labor_capacity))
    return {false, {}, failure("EconomyQuantityOverflow", target.id).error};
  source.headcount -= request.count;
  source.labor_capacity -= request.labor_eligible_count;
  bool source_fed = false, target_fed = false;
  for (const auto &row : candidate.provision_bindings.rows())
    if (row.second.subject.kind == SubjectKind::cohort &&
        row.second.from_tick <= request.effective_tick &&
        request.effective_tick <= row.second.through_tick) {
      source_fed |= row.second.subject.id == source.id;
      target_fed |= row.second.subject.id == target.id;
    }
  if (source_fed && !target_fed)
    return {false, {}, failure("PopulationProvisionRequired", target.id).error};
  auto id = fact_id(c, "population_transfer", "population_transfer");
  if (!id.ok)
    return id;
  candidate.cohorts.set(target.id, target);
  candidate.population_transfers.insert(
      id.value, {id.value, request.from_cohort_id, request.to_cohort_id,
                 request.count, request.labor_eligible_count, request.reason,
                 request.source_fact_id, request.effective_tick});
  std::vector<PopulationLaborChange> actual_changes;
  if (capture)
    for (const auto &allocation_id : request.release_allocation_ids) {
      const auto *before = s.labor_allocations.find(allocation_id);
      const auto *after = candidate.labor_allocations.find(allocation_id);
      if (before->workers != after->workers ||
          before->through_tick != after->through_tick)
        actual_changes.push_back({allocation_id, before->workers,
                                  after->workers, before->through_tick,
                                  after->through_tick});
    }
  s = std::move(candidate);
  if (capture)
    PopulationTransferLaborCaptureWriter::publish(
        *capture, id.value, request.from_cohort_id, std::move(actual_changes));
  return id;
}
// 普通调用没有军籍来源证明，不得把现役转出或创建现役人口。
Outcome<StableId> transfer_population(CommonStore &c, LocalEconomyState &s,
                                      const PopulationTransferRequest &request,
                                      PopulationTransferLaborCapture *capture) {
  const auto *from = s.cohorts.find(request.from_cohort_id);
  if (request.destination_status == PopulationStatus::serving ||
      (from && from->status == PopulationStatus::serving))
    return {false,
            {},
            common_failure("AggregateMilitaryAdapterUnavailable",
                           request.to_cohort_id,
                           ErrorKind::unsupported_capability)
                .error};
  return local_economy_detail::transfer_population_counts(c, s, request,
                                                          capture);
}
// 报告读取真实收据；它不再次付款，也不泄露未关联的组织私账。
std::optional<DynamicFactView>
local_economy_dynamic_fact(const LocalEconomyState &s, const StableId &id) {
  DynamicFactView out;
  out.id = id;
  out.rule_version = "local_economy_v1";
  if (s.household_provisioning) {
    const auto &household = *s.household_provisioning;
    const auto *registered = household.registrations.find(id);
    const auto *closure = household.closures.find(id);
    if (closure)
      registered = household.registrations.find(closure->registration_id);
    if (registered) {
      out.rule_version = "household_provisioning_v1";
      out.kind = closure ? "household_provision_ended"
                         : "household_provision_registered";
      out.tick = closure ? closure->tick : registered->tick;
      out.subject_id = registered->authorization_id;
      out.outcome =
          closure ? "future_self_provision_ended" : "self_provision_registered";
      if (closure)
        out.cause_fact_ids = {registered->id};
      return out;
    }
  }
  if (s.production_management) {
    const auto *record = s.production_management->configurations.find(id);
    const auto *resolution = s.production_management->results.find(id);
    if (resolution)
      record = s.production_management->configurations.find(
          resolution->configuration_id);
    if (record) {
      out.rule_version = "production_management_v1";
      out.kind = resolution ? "production_configuration_" + resolution->kind
                            : "production_configuration_requested";
      out.tick = resolution ? resolution->tick : record->requested_tick;
      out.subject_id = record->unit_id;
      out.participants = {
          {{SubjectKind::actor, record->actor_id}, "requester"}};
      if (record->authority)
        out.participants.push_back(
            {{SubjectKind::organization, record->authority->organization_id},
             "principal"});
      if (resolution)
        out.cause_fact_ids.push_back(record->id);
      out.outcome =
          resolution ? resolution->kind : "scheduled_for_next_complete_cycle";
      return out;
    }
  }
  if (const auto *terminal = s.membership_terminal_fractions.find(id)) {
    const auto *binding =
        s.membership_wage_bindings.find(terminal->wage_binding_id);
    if (!binding)
      return {};
    out.kind = "membership_wage_ended";
    out.rule_version = "named_membership_v1";
    out.tick = terminal->tick;
    out.subject_id = binding->actor_id;
    out.participants = {
        {{SubjectKind::organization, binding->organization_id}, "principal"},
        {{SubjectKind::actor, binding->actor_id}, "beneficiary"}};
    out.cause_fact_ids = {terminal->departure_fact_id};
    if (!terminal->arrear_id.empty())
      out.cause_fact_ids.push_back(terminal->arrear_id);
    out.outcome = terminal->integer_due ? "terminal_fraction_compensated"
                                        : "membership_wage_stopped";
    return out;
  }
  const ArrearRecord *arrear = s.arrears.find(id);
  const auto *payment = s.payments.find(id);
  if (payment)
    arrear = s.arrears.find(payment->arrear_id);
  if (arrear) {
    const auto *obligation = s.obligations.find(arrear->obligation_id);
    if (!obligation)
      return {};
    out.kind = payment ? "obligation_payment"
                       : (obligation->kind == ObligationKind::wage
                              ? "wage_due"
                              : "contract_payment_due");
    out.tick = payment ? payment->tick : arrear->due_tick;
    out.subject_id = obligation->beneficiary.id;
    out.participants = {{obligation->principal, "principal"},
                        {obligation->beneficiary, "beneficiary"}};
    Integer paid_at_receipt = 0;
    if (payment)
      for (const auto &row : s.payments.rows()) {
        if (row.second.arrear_id == arrear->id &&
            !local_economy_detail::add(paid_at_receipt, row.second.amount,
                                       paid_at_receipt))
          return {};
        if (row.first == id)
          break;
      }
    out.obligation =
        ObligationObservation{obligation->id,
                              obligation->principal.id,
                              obligation->beneficiary.id,
                              id,
                              arrear->due_tick,
                              arrear->original_due,
                              paid_at_receipt,
                              arrear->original_due - paid_at_receipt,
                              arrear->id};
    if (payment)
      out.cause_fact_ids.push_back(payment->arrear_id);
    return out;
  }
  if (const auto *observation = s.supply_observations.find(id)) {
    out.kind = "local_supply_observation";
    out.tick = observation->tick;
    out.subject_id = observation->observation.organization_id;
    out.participants = {
        {{SubjectKind::organization, observation->observation.organization_id},
         "principal"}};
    out.supply = observation->observation;
    out.publicly_visible = observation->publicly_visible;
    out.outcome = "supply_observed";
    return out;
  }
  if (const auto *registration = s.supply_registrations.find(id)) {
    out.kind = "local_supply_source";
    out.tick = registration->tick;
    out.subject_id = registration->organization_id;
    out.participants = {
        {{SubjectKind::organization, registration->organization_id},
         "principal"}};
    out.outcome = "supply_source_registered";
    return out;
  }
  if (const auto *ration = s.rations.find(id)) {
    out.kind = "local_ration";
    out.tick = ration->tick;
    out.subject_id = ration->beneficiary.id;
    out.participants = {{ration->beneficiary, "beneficiary"}};
    out.outcome = ration->shortage ? "ration_shortage" : "ration_satisfied";
    return out;
  }
  if (const auto *production = s.production_receipts.find(id)) {
    out.kind = "local_production";
    out.tick = production->tick;
    out.subject_id = production->unit_id;
    out.cause_fact_ids = production->cause_fact_ids;
    out.outcome = production->outputs.empty() ? "production_stopped"
                                              : "production_completed";
    return out;
  }
  if (const auto *loss = s.population_losses.find(id)) {
    out.kind = "population_loss";
    out.tick = loss->effective_tick;
    out.subject_id = loss->cohort_id;
    out.cause_fact_ids = {loss->source_fact_id, loss->casualty_fact_id};
    out.participants = {{{SubjectKind::cohort, loss->cohort_id}, "source"}};
    out.outcome = "confirmed_military_death";
    return out;
  }
  if (const auto *transfer = s.population_transfers.find(id)) {
    out.kind = "population_transfer";
    out.tick = transfer->effective_tick;
    out.subject_id = transfer->to_cohort_id;
    out.cause_fact_ids = {transfer->source_fact_id};
    out.participants = {
        {{SubjectKind::cohort, transfer->from_cohort_id}, "source"},
        {{SubjectKind::cohort, transfer->to_cohort_id}, "destination"}};
    return out;
  }
  return {};
}
} // namespace hanmo::resident
