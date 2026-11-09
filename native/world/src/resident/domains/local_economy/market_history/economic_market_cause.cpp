// 原实际投入/产出/民耗按收据顺序逐腿导出，未实现产量、短缺和零实际量不产生资源腿。
#include "economic_market_cause.hpp"
#include "resident/domains/growth/growth_market_fifo_read.hpp"
#include "resident/domains/local_economy/takeover/execution_receipts.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
namespace {
template <class Legs, class Metadata>
Status original_cause(const CommonStore &c, const CommonCatalog &common,
                      const LocalEconomyState &state,
                      const LocalEconomyCatalog &catalog, const StableId &id,
                      const EconomicTakeoverExecutionState *takeover,
                      const DynamicStepInput *phase, Legs &&emit,
                      Metadata &&metadata) {
  auto fail = [&](const StableId &code) -> Status {
    return common_failure(code, id);
  };
  Integer limit = c.tick();
  if (phase) {
    if (c.tick() == INT64_MAX || phase->from_tick != c.tick() ||
        phase->to_tick != c.tick() + 1)
      return fail("EconomicMarketPhaseMismatch");
    limit = phase->to_tick;
  }
  if (!catalog.enabled || state.last_step_tick < 0 ||
      state.last_step_tick > limit)
    return fail("EconomicMarketSourceContextMismatch");
  if (!takeover)
    for (const auto &h : c.state().facts.rows())
      if (h.second.domain == "political_takeover")
        return fail("EconomicMarketTakeoverContextRequired");
  const auto *header = c.state().facts.find(id);
  if (!header || header->id != id || header->domain != "local_economy")
    return fail("EconomicMarketExactSourceRequired");
  const auto *production = state.production_receipts.find(id);
  const auto *ration = state.rations.find(id);
  if (bool(production) == bool(ration))
    return fail("EconomicMarketExactSourceRequired");
  auto leg = [&](const StableId &inventory, const StableId &item,
                 const Owner &owner, Integer quantity) -> Status {
    if (!quantity)
      return common_success();
    if (inventory.empty() || item.empty() || owner.id.empty() ||
        !common.items.find(item) || !c.inventory(inventory))
      return common_failure("EconomicMarketQuantitySourceMismatch", id);
    return emit(inventory, item, owner, quantity);
  };
  if (production) {
    const auto &p = *production;
    const auto n = organization_instance_sequence(id, "local_production");
    const auto *original = catalog.production_units.find(p.unit_id);
    const auto *current = state.production_units.find(p.unit_id);
    const auto *recipe = catalog.recipes.find(p.recipe_id);
    if (header->kind != "local_production" || !n ||
        *n >= c.state().next_instance_id || p.id != id || !original ||
        !current || !recipe || !owner_equal(current->owner, original->owner) ||
        p.tick < 0 || p.due_tick < 0 || p.due_tick > p.tick || p.tick > limit ||
        (!phase && p.tick > state.last_step_tick))
      return fail("EconomicMarketProductionSourceMismatch");
    if (takeover && takeover->production.find(id))
      return fail("EconomicMarketTakenSourceUnsupported");
    metadata(MarketCauseKind::local_production, p.tick, p.unit_id, p.due_tick);
    for (const auto &q : p.inputs) {
      if (q.quantity <= 0)
        return fail("EconomicMarketQuantitySourceMismatch");
      auto valid = leg(q.inventory_id, q.item_id, original->owner, -q.quantity);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    for (const auto &q : p.outputs) {
      if (q.quantity <= 0)
        return fail("EconomicMarketQuantitySourceMismatch");
      auto valid = leg(q.inventory_id, q.item_id, original->owner, q.quantity);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  } else {
    const auto &r = *ration;
    const auto n = organization_instance_sequence(id, "local_ration");
    const auto *original = catalog.obligations.find(r.obligation_id);
    const auto *current = state.obligations.find(r.obligation_id);
    Integer total = 0;
    if (header->kind != "local_ration" || !n ||
        *n >= c.state().next_instance_id || r.id != id || !original ||
        !current || original->kind != ObligationKind::ration ||
        current->kind != ObligationKind::ration ||
        current->principal.kind != original->principal.kind ||
        current->principal.id != original->principal.id ||
        r.inventory_id != original->source_wallet_or_inventory ||
        r.item_id != original->item_id ||
        r.beneficiary.kind != original->beneficiary.kind ||
        r.beneficiary.id != original->beneficiary.id || r.tick < 0 ||
        r.due_tick < 0 || r.due_tick > r.tick || r.tick > limit ||
        (!phase && r.tick > state.last_step_tick) || r.consumed < 0 ||
        r.shortage < 0 ||
        __builtin_add_overflow(r.consumed, r.shortage, &total) ||
        total != r.required)
      return fail("EconomicMarketRationSourceMismatch");
    if (takeover && takeover->rations.find(id))
      return fail("EconomicMarketTakenSourceUnsupported");
    const auto *initial = common.required_inventories.find(r.inventory_id);
    if (!initial)
      return fail("EconomicMarketRationOwnerHistoryUnavailable");
    const Owner &owner = initial->owner;
    if (original->principal.kind == SubjectKind::organization) {
      if (owner.kind != Owner::Kind::organization ||
          owner.id != original->principal.id)
        return fail("EconomicMarketRationOwnerHistoryUnavailable");
    } else if (original->principal.kind == SubjectKind::cohort) {
      const auto *cohort = catalog.cohorts.find(original->principal.id);
      const auto *household =
          cohort ? catalog.households.find(cohort->household_group_id)
                 : nullptr;
      if (!household || household->inventory_id != r.inventory_id)
        return fail("EconomicMarketRationOwnerHistoryUnavailable");
      if (owner.kind != Owner::Kind::household || owner.id != household->id)
        return fail("EconomicMarketRationOwnerHistoryUnavailable");
    } else
      return fail("EconomicMarketRationOwnerHistoryUnavailable");
    metadata(MarketCauseKind::local_ration, r.tick, r.obligation_id,
             r.due_tick);
    auto valid = leg(r.inventory_id, r.item_id, owner, -r.consumed);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return common_success();
}
} // namespace
Outcome<MarketBusinessCause> read_economic_market_cause(
    const CommonStore &c, const CommonCatalog &common,
    const LocalEconomyState &state, const LocalEconomyCatalog &catalog,
    const StableId &id, const EconomicTakeoverExecutionState *takeover,
    const DynamicStepInput *phase) {
  MarketBusinessCause result;
  result.source_identity = c.state().identity;
  result.source_fact_id = id;
  auto status = original_cause(
      c, common, state, catalog, id, takeover, phase,
      [&](const StableId &inventory, const StableId &item, const Owner &owner,
          Integer quantity) {
        // 旧口粮返回的Owner无额外字段，原borrow只核相同kind/id，不改旧接口值形状。
        if (result.kind == MarketCauseKind::local_ration)
          result.legs.push_back(
              {inventory, item, {owner.kind, owner.id, {}}, quantity});
        else
          result.legs.push_back({inventory, item, owner, quantity});
        return common_success();
      },
      [&](MarketCauseKind kind, Integer tick, const StableId &key,
          Integer due) {
        result.kind = kind;
        result.source_tick = tick;
        result.original_business_key = key;
        result.business_due_tick = due;
      });
  if (!status.ok)
    return {false, {}, std::move(status.error)};
  return {true, std::move(result), {}};
}
Outcome<std::vector<StableId>> read_economic_market_cause_ids(
    const CommonStore &c, const CommonCatalog &common,
    const LocalEconomyState &state, const LocalEconomyCatalog &catalog,
    const std::vector<StableId> &protected_ids,
    const EconomicTakeoverExecutionState *takeover,
    const DynamicStepInput *phase) {
  auto fail = [&](const StableId &code) -> Outcome<std::vector<StableId>> {
    return {false, {}, common_failure(code).error};
  };
  Integer limit = c.tick();
  if (phase) {
    if (c.tick() == INT64_MAX || phase->from_tick != c.tick() ||
        phase->to_tick != c.tick() + 1)
      return fail("EconomicMarketPhaseMismatch");
    limit = phase->to_tick;
  }
  if (!catalog.enabled || state.last_step_tick < 0 ||
      state.last_step_tick > limit)
    return fail("EconomicMarketSourceContextMismatch");
  std::set<StableId> inventories, selected;
  for (const auto &id : protected_ids)
    if (id.empty() || !c.inventory(id) || !inventories.insert(id).second)
      return fail("EconomicMarketProtectedInventoryMismatch");
  // 从原本账找候选，不能只从仍存在的Common头或市场回执枚举，避免缺头/缺行被跳过。
  for (const auto &row : state.production_receipts.rows()) {
    bool touched = false;
    for (const auto &q : row.second.inputs)
      if (q.quantity != 0 && inventories.count(q.inventory_id))
        touched = true;
    for (const auto &q : row.second.outputs)
      if (q.quantity != 0 && inventories.count(q.inventory_id))
        touched = true;
    if (touched && !selected.insert(row.first).second)
      return fail("EconomicMarketDuplicateSource");
  }
  for (const auto &row : state.rations.rows())
    if (row.second.consumed != 0 && inventories.count(row.second.inventory_id))
      if (!selected.insert(row.first).second)
        return fail("EconomicMarketDuplicateSource");
  for (const auto &id : selected) {
    auto exact = read_economic_market_cause(c, common, state, catalog, id,
                                            takeover, phase);
    if (!exact.ok)
      return {false, {}, exact.error};
    bool touched = false;
    for (const auto &leg : exact.value.legs)
      if (leg.signed_quantity != 0 && inventories.count(leg.inventory_id))
        touched = true;
    if (!touched)
      return fail("EconomicMarketQuantitySourceMismatch");
  }
  std::vector<StableId> out;
  for (const auto &header : c.state().facts.rows())
    if (selected.count(header.first)) {
      out.push_back(header.first);
      selected.erase(header.first);
    }
  if (!selected.empty())
    return fail("EconomicMarketExactSourceRequired");
  return {true, std::move(out), {}};
}
Outcome<EconomicMarketCauseBorrow> read_economic_market_cause_borrow(
    const CommonStore &c, const CommonCatalog &common,
    const LocalEconomyState &state, const LocalEconomyCatalog &catalog,
    const StableId &id, const EconomicTakeoverExecutionState *takeover,
    const DynamicStepInput *phase) {
  EconomicMarketCauseBorrow result;
  auto valid = original_cause(
      c, common, state, catalog, id, takeover, phase,
      [](const StableId &, const StableId &, const Owner &, Integer) {
        return common_success();
      },
      [&](MarketCauseKind kind, Integer tick, const StableId &key,
          Integer due) { result = {kind, tick, due, &key}; });
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, result, {}};
}
Status visit_economic_market_cause_sources(
    const CommonStore &c, const CommonCatalog &common,
    const LocalEconomyState &state, const LocalEconomyCatalog &catalog,
    std::span<const InventoryView *const> protected_initial,
    MarketOriginalFifoSourceSink &sink,
    const EconomicTakeoverExecutionState *takeover,
    const DynamicStepInput *phase) {
  Integer limit = c.tick();
  if (phase) {
    if (c.tick() == INT64_MAX || phase->from_tick != c.tick() ||
        phase->to_tick != c.tick() + 1)
      return common_failure("EconomicMarketPhaseMismatch");
    limit = phase->to_tick;
  }
  if (!catalog.enabled || state.last_step_tick < 0 ||
      state.last_step_tick > limit)
    return common_failure("EconomicMarketSourceContextMismatch");
  for (std::size_t i = 0; i < protected_initial.size(); ++i) {
    const auto *p = protected_initial[i];
    if (!p || p->id.empty() || !c.inventory(p->id))
      return common_failure("EconomicMarketProtectedInventoryMismatch");
    for (std::size_t j = 0; j < i; ++j)
      if (protected_initial[j]->id == p->id)
        return common_failure("EconomicMarketProtectedInventoryMismatch",
                              p->id);
  }
  auto protects = [&](const StableId &id) {
    for (const auto *p : protected_initial)
      if (p->id == id)
        return true;
    return false;
  };
  auto production_selected = [&](const ProductionReceipt &r) {
    for (const auto &q : r.inputs)
      if (q.quantity && protects(q.inventory_id))
        return true;
    for (const auto &q : r.outputs)
      if (q.quantity && protects(q.inventory_id))
        return true;
    return false;
  };
  auto ration_selected = [&](const RationReceipt &r) {
    return r.consumed && protects(r.inventory_id);
  };
  auto validate = [&](const StableId &id) {
    bool touched = false;
    auto valid = original_cause(
        c, common, state, catalog, id, takeover, phase,
        [&](const StableId &inventory, const StableId &, const Owner &,
            Integer quantity) {
          if (quantity && protects(inventory))
            touched = true;
          return common_success();
        },
        [](MarketCauseKind, Integer, const StableId &, Integer) {});
    if (!valid.ok)
      return valid;
    return touched ? common_success()
                   : common_failure("EconomicMarketQuantitySourceMismatch", id);
  };
  // 先从原收据检查缺头/跨类重名，再按真实Common顺序借出，不扫描市场movement自证。
  for (const auto &row : state.production_receipts.rows())
    if (production_selected(row.second)) {
      auto valid = validate(row.first);
      if (!valid.ok)
        return valid;
    }
  for (const auto &row : state.rations.rows())
    if (ration_selected(row.second)) {
      auto valid = validate(row.first);
      if (!valid.ok)
        return valid;
    }
  for (const auto &row : c.state().facts.rows()) {
    const auto *p = state.production_receipts.find(row.first);
    const auto *r = state.rations.find(row.first);
    if (p && production_selected(*p)) {
      auto valid = sink.visit_original_fifo_source(
          MarketCauseKind::local_production, row.first, 1);
      if (!valid.ok)
        return valid;
    } else if (r && ration_selected(*r)) {
      auto valid = sink.visit_original_fifo_source(
          MarketCauseKind::local_ration, row.first, 1);
      if (!valid.ok)
        return valid;
    }
  }
  return common_success();
}

} // namespace hanmo::resident
