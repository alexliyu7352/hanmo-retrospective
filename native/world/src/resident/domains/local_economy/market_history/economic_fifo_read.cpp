// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
#include "economic_fifo_read.hpp"
#include "resident/domains/growth/growth_market_history.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/local_economy/takeover/execution_receipts.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/native_production_cycles.hpp"
#include "resident/storage/common_projection.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
template <class F> Status guard(F &&f) {
  try {
    f();
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
// 所有新临时来源字段在复制前入同job账，租约先于实际字段析构。
class OriginalSource {
  CheckpointCopyLease memory_;
  static CheckpointCopyLease reserve(const FactRecord &h) {


    return CheckpointCopyLease::reserve();
  }
  static const FactRecord &head(const CommonStore &c, const StableId &id) {
    const auto *h = c.state().facts.find(id);
    if (!h)
      fail("EconomicFifoOriginalCauseMissing", id);
    return *h;
  }

public:
  const SourceRef value;
  OriginalSource(const CommonStore &c, const StableId &id)
      : OriginalSource(head(c, id)) {}
  explicit OriginalSource(const FactRecord &h)
      : memory_(reserve(h)), value{h.domain, h.kind, h.id} {}
};
Integer sum(Integer a, Integer b) {
  Integer value = 0;
  if (!local_economy_detail::add(a, b, value))
    fail("EconomyQuantityOverflow");
  return value;
}
Integer product(Integer a, Integer b) {
  Integer value = 0;
  if (!local_economy_detail::multiply(a, b, value))
    fail("EconomyQuantityOverflow");
  return value;
}
Integer amount(Integer value, Integer numerator, Integer denominator,
               Integer &remainder) {
  Integer q = 0, next = 0;
  if (!local_economy_detail::ratio(value, numerator, denominator, remainder, q,
                                   next))
    fail("EconomyQuantityOverflow");
  remainder = next;
  return q;
}
bool module1(const CatalogIdentity &id, const char *name) {
  bool found = false;
  for (const auto &row : id.module_versions)
    if (row.first == name) {
      if (found || row.second != "1")
        fail("EconomicFifoModuleMismatch", row.first);
      found = true;
    }
  return found;
}
const InventoryView &before(const MarketOriginalStageReadView &view,
                            std::size_t index, const StableId &id) {
  if (index >= view.inventories.size() ||
      index >= view.stage.inventory_frames.size())
    fail("EconomicFifoInventoryCoverageMismatch", id);
  const auto &v = view.inventories[index];
  const auto &f = view.stage.inventory_frames[index];
  if (v.inventory_id != id || f.inventory_id != id || !v.before ||
      v.before->id != id || v.after.id != id)
    fail("EconomicFifoOriginalInventoryRequired", id);
  return *v.before;
}
const OriginalBusinessItemOperation &
operation(const MarketOriginalStageReadView &s, std::size_t index) {
  if (index >= s.stage.item_operations.size())
    fail("EconomicFifoOperationMissing");
  const auto &o = s.stage.item_operations[index];
  if (o.operation_index != index || !s.stage.wallet_postings.empty())
    fail("EconomicFifoOperationOrderMismatch");
  return o;
}
void material_ref(const NativeEconomicMaterialRead &materials,
                  const EconomicMaterialNeedKey &key,
                  const MarketOriginalMaterialBasis &basis) {
  const auto *actual = std::get_if<MarketOriginalMaterialNeed>(&basis);
  const auto original = take(materials.read_need(key));
  if (!actual || actual->need_slot_ordinal != original.need_slot_ordinal ||
      actual->rule_ref.id != *original.rule_id ||
      actual->rule_ref.version != original.rule_version ||
      actual->rule_ref.catalog_digest != *original.catalog_digest)
    fail("EconomicFifoOriginalMaterialNeedRequired", key.original_business_key);
}
void consume(const EconomicFifoReadContext &x,
             const EconomicMaterialNeedKey &key, const InventoryView &inventory,
             const Owner &owner, Integer quantity, Integer tick,
             const SourceRef &source, const OriginalBusinessItemOperation &op) {
  material_ref(x.materials, key, op.material_need);
  if (!std::holds_alternative<MarketNotOutput>(op.output_basis))
    fail("EconomicFifoConsumeOutputMismatch", source.id);
  const auto selected =
      take(x.materials.select(key, inventory, owner, quantity, tick, &source));
  if (op.selections.size() != selected.selections.size() ||
      op.lot_flows.size() != selected.selections.size())
    fail("EconomicFifoSelectionCoverageMismatch", source.id);
  for (std::size_t i = 0; i < selected.selections.size(); ++i) {
    const auto &want = selected.selections[i];
    const auto &s = op.selections[i];
    const auto &f = op.lot_flows[i];
    if (s.inventory_id != inventory.id || s.lot_id != want.lot->id ||
        s.quantity != want.quantity ||
        f.kind != ResourceLotFlowDraft::Kind::selected_consume ||
        f.source_inventory_id != s.inventory_id ||
        f.source_lot_id != s.lot_id || !f.destination_inventory_id.empty() ||
        !f.destination_lot_id.empty() || f.item_id != want.lot->item_id ||
        f.quantity != s.quantity)
      fail("EconomicFifoOriginalConsumptionMismatch", source.id);
  }
}
struct Remainders {
  CheckpointVector<Integer> inputs, outputs;
  Integer labor = 0, factor = 0;
};
Remainders initial(const ProductionUnit &u, const ProductionRecipe &r) {
  if ((!u.input_remainders.empty() &&
       u.input_remainders.size() != r.inputs.size()) ||
      (!u.output_remainders.empty() &&
       u.output_remainders.size() != r.outputs.size()))
    fail("EconomicFifoInitialRemainderMismatch", u.id);
  Remainders out;
  out.inputs.reserve(r.inputs.size());
  out.outputs.reserve(r.outputs.size());
  out.labor = u.labor_remainder;
  out.factor = u.factor_remainder;
  for (std::size_t i = 0; i < r.inputs.size(); ++i)
    out.inputs.push_back(u.input_remainders.empty() ? 0
                                                    : u.input_remainders[i]);
  for (std::size_t i = 0; i < r.outputs.size(); ++i)
    out.outputs.push_back(u.output_remainders.empty() ? 0
                                                      : u.output_remainders[i]);
  return out;
}
Integer planned(const EconomicFifoReadContext &x, const ProductionUnit &u,
                const ProductionReceipt &r, Remainders &remainder) {
  const auto &recipe = *x.catalog.recipes.find(u.recipe_id);
  auto context = take(production_context(x.catalog, u, r.due_tick));
  const auto capacity =
      x.work ? take(read_original_production_receipt_capacity(*x.work, r))
             : std::optional<Integer>{};
  Integer raw = 0;
  if (capacity) {
    const auto *s = x.work->sources.settlements.find(r.id);
    if (!s || s->labor_limit < 0)
      fail("EconomicFifoOriginalLaborSettlementRequired", r.id);
    context.capacity = *capacity;
    raw = s->labor_limit;
  } else
    raw =
        amount(r.effective_workers, 1, recipe.labor_per_batch, remainder.labor);
  if (context.capacity < 0 || r.batches < 0)
    fail("EconomicFifoInvalidProductionLimit", r.id);
  raw = std::min(raw, context.capacity);
  const auto factors =
      product(context.region_factor_bp, context.season_factor_bp);
  return std::min(amount(raw, factors, 100000000, remainder.factor),
                  context.capacity);
}
Remainders before_production(const EconomicFifoReadContext &x,
                             const ProductionUnit &u,
                             const ProductionRecipe &recipe,
                             const ProductionReceipt &target) {
  auto value = initial(u, recipe);
  Integer expected = u.next_due_tick;
  bool found = false;
  for (const auto &head : x.common.state().facts.rows()) {
    const auto *r = x.economy.production_receipts.find(head.first);
    if (!r || r->unit_id != u.id)
      continue;
    if (head.second.domain != "local_economy" ||
        head.second.kind != "local_production" || r->id != head.first ||
        r->recipe_id != recipe.id || r->due_tick != expected)
      fail("EconomicFifoOriginalCycleOrderMismatch", head.first);
    if (r->id == target.id) {
      found = true;
      break;
    }
    // 同一受保护原单元的旧接管周期不能拿原Owner/余数冒已核原业务。
    if (x.takeover && x.takeover->production.find(r->id))
      fail("EconomicMarketTakenSourceUnsupported", r->id);
    planned(x, u, *r, value);
    for (std::size_t i = 0; i < recipe.inputs.size(); ++i)
      amount(r->batches, recipe.inputs[i].numerator,
             recipe.inputs[i].denominator, value.inputs[i]);
    for (std::size_t i = 0; i < recipe.outputs.size(); ++i)
      amount(r->batches, recipe.outputs[i].numerator,
             recipe.outputs[i].denominator, value.outputs[i]);
    expected = sum(expected, recipe.period_ticks);
  }
  if (!found)
    fail("EconomicFifoOriginalCycleMissing", target.id);
  return value;
}
Integer inventory_weight(const CommonCatalog &catalog, const InventoryView &i) {
  Integer total = 0;
  for (const auto &lot : i.lots) {
    const auto *item = catalog.items.find(lot.item_id);
    if (!item || item->weight < 0 || lot.quantity <= 0)
      fail("EconomicFifoInvalidHistoricalLot", lot.id);
    total = sum(total, product(lot.quantity, item->weight));
  }
  if (total > i.capacity)
    fail("EconomicFifoHistoricalCapacityMismatch", i.id);
  return total;
}
void production(const EconomicFifoReadContext &x, const MarketMovementRecord &m,
                std::span<const MarketOriginalStageReadView> stages) {
  const auto *r = x.economy.production_receipts.find(m.business_fact_id);
  const auto *u = r ? x.catalog.production_units.find(r->unit_id) : nullptr;
  const auto *recipe = u ? x.catalog.recipes.find(u->recipe_id) : nullptr;
  if (!r || !u || !recipe || r->recipe_id != recipe->id || stages.size() != 2 ||
      stages[0].stage.stage != MarketCaptureStage::production_inputs ||
      stages[1].stage.stage != MarketCaptureStage::production_outputs)
    fail("EconomicFifoOriginalProductionStagesRequired", m.business_fact_id);
  const OriginalSource owned_source(x.common, r->id);
  const auto &source = owned_source.value;
  auto remainder = before_production(x, *u, *recipe, *r);
  const auto maximum = planned(x, *u, *r, remainder);
  const auto &inputs = stages[0];
  if (inputs.inventories.size() != u->input_inventory_ids.size() ||
      inputs.stage.inventory_frames.size() != inputs.inventories.size() ||
      !inputs.stage.wallet_postings.empty())
    fail("EconomicFifoInputReadFootprintMismatch", r->id);
  CheckpointVector<Integer> available;
  available.reserve(recipe->inputs.size());
  for (std::size_t i = 0; i < recipe->inputs.size(); ++i) {
    if (i > UINT32_MAX)
      fail("EconomicMaterialNeedOrdinalOverflow", r->id);
    Integer total = 0;
    for (std::size_t j = 0; j < u->input_inventory_ids.size(); ++j)
      total = sum(total, take(x.materials.count(
                             {EconomicMaterialKind::production_input, u->id,
                              static_cast<std::uint32_t>(i)},
                             before(inputs, j, u->input_inventory_ids[j]),
                             u->owner, r->tick, &source)));
    available.push_back(total);
  }
  auto fits = [&](Integer batches) {
    for (std::size_t i = 0; i < recipe->inputs.size(); ++i) {
      auto saved = remainder.inputs[i];
      if (amount(batches, recipe->inputs[i].numerator,
                 recipe->inputs[i].denominator, saved) > available[i])
        return false;
    }
    return true;
  };
  Integer low = 0, high = maximum;
  while (low < high) {
    const auto middle = low + (high - low) / 2 + (high - low) % 2;
    if (fits(middle))
      low = middle;
    else
      high = middle - 1;
  }
  if (low != r->batches)
    fail("EconomicFifoOriginalBatchLimitMismatch", r->id);
  std::size_t consumed = 0;
  for (std::size_t i = 0; i < recipe->inputs.size(); ++i) {
    const auto &item = recipe->inputs[i];
    Integer needed =
        amount(low, item.numerator, item.denominator, remainder.inputs[i]);
    const EconomicMaterialNeedKey key{EconomicMaterialKind::production_input,
                                      u->id, static_cast<std::uint32_t>(i)};
    for (std::size_t j = 0; j < u->input_inventory_ids.size(); ++j) {
      const auto &inv = before(inputs, j, u->input_inventory_ids[j]);
      const auto quantity = std::min(
          needed,
          take(x.materials.count(key, inv, u->owner, r->tick, &source)));
      if (quantity) {
        if (consumed >= r->inputs.size())
          fail("EconomicFifoReceiptInputMissing", r->id);
        const auto &q = r->inputs[consumed];
        const auto &op = operation(inputs, consumed);
        const auto *slot =
            std::get_if<MarketProductionInputSlot>(&op.source_slot);
        if (q.inventory_id != inv.id || q.item_id != item.item_id ||
            q.quantity != quantity || !slot ||
            slot->receipt_input_ordinal != consumed)
          fail("EconomicFifoReceiptInputMismatch", r->id);
        consume(x, key, inv, u->owner, quantity, r->tick, source, op);
        ++consumed;
        needed -= quantity;
      }
      if (!needed)
        break;
    }
    if (needed)
      fail("EconomicFifoOriginalInputsInsufficient", r->id);
  }
  if (consumed != r->inputs.size() ||
      consumed != inputs.stage.item_operations.size())
    fail("EconomicFifoReceiptInputCoverageMismatch", r->id);
  const auto &outputs = stages[1];
  if (outputs.inventories.size() != 1 ||
      outputs.stage.inventory_frames.size() != 1 ||
      !outputs.stage.wallet_postings.empty())
    fail("EconomicFifoOutputReadFootprintMismatch", r->id);
  const auto &inventory = before(outputs, 0, u->output_inventory_id);
  Integer room = inventory.capacity - inventory_weight(x.shared, inventory);
  Integer factor = 10000;
  if (x.disruptions) {
    const auto quote = take(
        quote_production_disruption(x.common, x.catalog, *x.disruption_catalog,
                                    *x.disruptions, *u, r->tick, r->due_tick));
    factor = quote.remaining_yield_bp;
  }
  std::size_t added = 0, lost = 0;
  for (std::size_t i = 0; i < recipe->outputs.size(); ++i) {
    const auto &item = recipe->outputs[i];
    Integer potential =
        amount(low, item.numerator, item.denominator, remainder.outputs[i]);
    potential = take(disrupted_production_quantity(potential, factor));
    const auto *definition = x.shared.items.find(item.item_id);
    if (!definition || definition->weight < 0)
      fail("EconomicFifoOriginalOutputDefinitionRequired", item.item_id);
    const auto quantity = definition->weight
                              ? std::min(potential, room / definition->weight)
                              : potential;
    room -= product(quantity, definition->weight);
    if (quantity) {
      if (added >= r->outputs.size())
        fail("EconomicFifoReceiptOutputMissing", r->id);
      const auto &q = r->outputs[added];
      const auto &op = operation(outputs, added);
      const auto *slot =
          std::get_if<MarketProductionOutputSlot>(&op.source_slot);
      if (q.inventory_id != inventory.id || q.item_id != item.item_id ||
          q.quantity != quantity || !slot ||
          slot->receipt_output_ordinal != added || !op.selections.empty() ||
          !std::holds_alternative<MarketNotConsumption>(op.material_need) ||
          !std::holds_alternative<MarketOriginalPlainAdd>(op.output_basis) ||
          op.lot_flows.size() != 1)
        fail("EconomicFifoOriginalPlainOutputRequired", r->id);
      const auto &flow = op.lot_flows.front();
      if (flow.kind != ResourceLotFlowDraft::Kind::add ||
          !flow.source_inventory_id.empty() || !flow.source_lot_id.empty() ||
          flow.destination_inventory_id != inventory.id ||
          flow.destination_lot_id.empty() || flow.item_id != item.item_id ||
          flow.quantity != quantity)
        fail("EconomicFifoOriginalOutputFlowMismatch", r->id);
      // 原生产源先证明新批的产权/普通货形，再由共享资源正文核整帧采用数学。
      const LotView *created = nullptr;
      for (const auto &lot : outputs.inventories[0].after.lots)
        if (lot.id == flow.destination_lot_id) {
          if (created)
            fail("EconomicFifoOutputLotNotUnique", r->id);
          created = &lot;
        }
      for (const auto &lot : inventory.lots)
        if (lot.id == flow.destination_lot_id)
          fail("EconomicFifoOutputLotAlreadyExisted", r->id);
      if (!created || created->item_id != q.item_id ||
          created->quantity != q.quantity ||
          !owner_equal(created->owner, u->owner) ||
          !common_owner_field_order_equivalent(created->owner, u->owner) ||
          created->owner.extra ||
          created->reserved_by.kind != ReservationRef::Kind::action ||
          !created->reserved_by.id.empty() ||
          !created->reserved_by.domain_id.empty() ||
          !created->reserved_by.activity_kind.empty() ||
          created->item_instance_id || created->lot_properties_ref ||
          created->extra)
        fail("EconomicFifoOriginalOutputLotMismatch", r->id);
      ++added;
    }
    if (potential > quantity) {
      if (lost >= r->unrealized_output.size())
        fail("EconomicFifoCapacityShortfallMissing", r->id);
      const auto &q = r->unrealized_output[lost++];
      if (q.inventory_id != inventory.id || q.item_id != item.item_id ||
          q.quantity != potential - quantity)
        fail("EconomicFifoCapacityShortfallMismatch", r->id);
    }
  }
  if (added != r->outputs.size() ||
      added != outputs.stage.item_operations.size() ||
      lost != r->unrealized_output.size())
    fail("EconomicFifoOutputCoverageMismatch", r->id);
}
Integer population_before(const EconomicFifoReadContext &x,
                          const RecurringObligation &o,
                          const RationReceipt &r) {
  const OriginalSource owned_source(x.common, r.id);
  const auto &source = owned_source.value;
  const auto limit = take(read_actual_common_fact_position(x.common, source));
  if (o.beneficiary.kind == SubjectKind::actor) {
    if (!x.life4 || !x.life4->bound_to(x.common))
      fail("EconomicFifoOriginalActorLifeRequired", o.beneficiary.id);
    return take(x.life4->read_actor_death_before_original_fact(o.beneficiary.id,
                                                               r.tick, source))
               ? 0
               : 1;
  }
  if (o.beneficiary.kind != SubjectKind::cohort)
    fail("EconomicFifoOriginalPopulationRequired", o.id);
  const auto *initial = x.catalog.cohorts.find(o.beneficiary.id);
  if (!initial)
    fail("EconomicFifoOriginalPopulationRequired", o.id);
  Integer count = initial->headcount;
  std::size_t position = 0;
  for (const auto &head : x.common.state().facts.rows()) {
    if (position++ >= limit)
      break;
    if (const auto *t = x.economy.population_transfers.find(head.first)) {
      if (t->id != head.first || head.second.domain != "local_economy" ||
          head.second.kind != "population_transfer" || t->count < 0)
        fail("EconomicFifoInvalidOriginalPopulation", t->id);
      if (t->from_cohort_id == initial->id)
        count = sum(count, -t->count);
      if (t->to_cohort_id == initial->id)
        count = sum(count, t->count);
    }
    if (const auto *loss = x.economy.population_losses.find(head.first)) {
      if (loss->id != head.first || head.second.domain != "local_economy" ||
          head.second.kind != "population_loss" || loss->count < 0)
        fail("EconomicFifoInvalidOriginalPopulation", loss->id);
      if (loss->cohort_id == initial->id)
        count = sum(count, -loss->count);
    }
  }
  for (const auto &row : x.economy.population_transfers.rows()) {
    const auto &t = row.second;
    if (t.to_cohort_id != initial->id || t.effective_tick != r.due_tick ||
        (t.reason != "population_casualty" &&
         t.reason != "population_recovered" &&
         t.reason != "population_deserted"))
      continue;
    const auto *transfer = x.common.state().facts.find(t.id);
    if (!transfer)
      fail("EconomicFifoOriginalPopulationRequired", t.id);
    if (take(read_actual_common_fact_position(x.common, *transfer)) >= limit)
      continue;
    const auto *head = x.common.state().facts.find(t.source_fact_id);
    if (!head || head->domain != "population_military" ||
        head->kind != t.reason || t.count < 0 || count < t.count)
      fail("InvalidMilitaryRationHandoff", t.id);
    count -= t.count;
  }
  if (count < 0)
    fail("EconomicFifoHistoricalPopulationNegative", o.id);
  return count;
}
void ration(const EconomicFifoReadContext &x, const MarketMovementRecord &m,
            std::span<const MarketOriginalStageReadView> stages) {
  const auto *r = x.economy.rations.find(m.business_fact_id);
  const auto *o = r ? x.catalog.obligations.find(r->obligation_id) : nullptr;
  const auto *rate = o ? x.catalog.rates.find(o->rate_profile) : nullptr;
  if (!r || !o || !rate || stages.size() != 1 ||
      stages[0].stage.stage != MarketCaptureStage::ration ||
      stages[0].inventories.size() != 1 ||
      stages[0].stage.inventory_frames.size() != 1)
    fail("EconomicFifoOriginalRationStageRequired", m.business_fact_id);
  Integer remainder = 0, expected = o->next_due_tick;
  bool found = false;
  for (const auto &head : x.common.state().facts.rows()) {
    const auto *prior = x.economy.rations.find(head.first);
    if (!prior || prior->obligation_id != o->id)
      continue;
    if (head.second.domain != "local_economy" ||
        head.second.kind != "local_ration" || prior->id != head.first ||
        prior->due_tick != expected)
      fail("EconomicFifoOriginalRationOrderMismatch", head.first);
    const auto count = rate->per_capita ? population_before(x, *o, *prior) : 1;
    const auto required =
        amount(count, rate->numerator, rate->denominator, remainder);
    if (prior->required != required)
      fail("EconomicFifoOriginalRationRequirementMismatch", prior->id);
    if (prior->id == r->id) {
      found = true;
      break;
    }
    expected = sum(expected, o->period_ticks);
  }
  if (!found)
    fail("EconomicFifoOriginalRationMissing", r->id);
  const auto &stage = stages[0];
  const auto &inventory = before(stage, 0, r->inventory_id);
  const OriginalSource owned_source(x.common, r->id);
  const auto &source = owned_source.value;
  const EconomicMaterialNeedKey key{EconomicMaterialKind::ration, o->id, 0};
  const auto available = take(
      x.materials.count(key, inventory, inventory.owner, r->tick, &source));
  Integer actual = std::min(r->required, available);
  if (x.household_management)
    actual = std::min(actual, take(household_provision_consumption_limit(
                                  x.economy, x.catalog, *x.household_management,
                                  *o, r->due_tick, r->required)));
  if (actual != r->consumed || r->shortage != r->required - actual ||
      actual <= 0 || stage.stage.item_operations.size() != 1)
    fail("EconomicFifoOriginalRationConsumptionMismatch", r->id);
  const auto &op = operation(stage, 0);
  if (!std::holds_alternative<MarketRationConsumptionSlot>(op.source_slot))
    fail("EconomicFifoOriginalRationSlotRequired", r->id);
  consume(x, key, inventory, inventory.owner, actual, r->tick, source, op);
}
} // namespace
Status NativeEconomicFifoRead::validate_bindings() const {
  return guard([&] {
    take(x_.materials.validate_bindings());
    if (!x_.materials.bound_to(x_.common, x_.shared, x_.catalog) ||
        !x_.catalog.enabled ||
        x_.common.state().metadata.content_id != x_.identity.content_id ||
        x_.common.state().metadata.content_version !=
            x_.identity.content_version ||
        x_.common.state().metadata.content_digest !=
            x_.identity.content_digest ||
        x_.common.state().metadata.ruleset_id != x_.identity.ruleset_id)
      fail("EconomicFifoWorldMismatch");
    if (module1(x_.identity, "production_disruptions") !=
            bool(x_.disruption_catalog) ||
        bool(x_.disruption_catalog) != bool(x_.disruptions) ||
        module1(x_.identity, "household_provision_management") !=
            bool(x_.household_management))
      fail("EconomicFifoSelectedSourcesRequired");
    if (bool(take(selected_native_production_cycles_version(x_.identity))) !=
        bool(x_.work))
      fail("EconomicFifoOriginalWorkSelectionMismatch");
    if (!x_.work)
      for (const auto &row : x_.common.state().facts.rows())
        if (is_native_production_cycle_fact(row.second))
          fail("EconomicFifoUnselectedWorkSource", row.first);
    for (const auto &row : x_.common.state().facts.rows())
      if (row.second.domain == "local_economy") {
        const auto &kind = row.second.kind;
        if (!x_.disruptions && (kind == "production_disruption_registered" ||
                                kind == "production_disruption_settled"))
          fail("EconomicFifoUnselectedDisruptionSource", row.first);
        if (!x_.household_management &&
            (kind == "household_provision_configured" ||
             kind == "household_provision_configuration_cancelled" ||
             kind == "household_provision_configuration_superseded"))
          fail("EconomicFifoUnselectedHouseholdManagement", row.first);
      }
    if (x_.life4 && !x_.life4->bound_to(x_.common))
      fail("EconomicFifoWorldMismatch");
  });
}
Status NativeEconomicFifoRead::visit_original_fifo_sources(
    std::span<const InventoryView *const> inventories,
    MarketOriginalFifoSourceSink &sink) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  return visit_economic_market_cause_sources(x_.common, x_.shared, x_.economy,
                                             x_.catalog, inventories, sink,
                                             x_.takeover, x_.phase);
}
Status NativeEconomicFifoRead::validate_original_fifo_business(
    const MarketMovementRecord &m,
    std::span<const MarketOriginalStageReadView> stages) const {
  return guard([&] {
    take(validate_bindings());
    if (m.business_source_schema != 1 || !m.original_business_execution ||
        m.original_business_execution->schema_version != 1 ||
        !std::holds_alternative<MarketOriginalTick>(
            m.original_business_execution->time_basis) ||
        stages.size() != m.original_business_execution->stages.size())
      fail("EconomicFifoOriginalExecutionRequired", m.business_fact_id);
    const auto cause = take(read_economic_market_cause_borrow(
        x_.common, x_.shared, x_.economy, x_.catalog, m.business_fact_id,
        x_.takeover, x_.phase));
    if (cause.kind != m.business_kind || cause.source_tick != m.source_tick)
      fail("EconomicFifoOriginalCauseMismatch", m.business_fact_id);
    const auto &rule = m.original_business_execution->adapter_rule_ref;
    const char *id = m.business_kind == MarketCauseKind::local_production
                         ? "local_production_original_fifo"
                     : m.business_kind == MarketCauseKind::local_ration
                         ? "local_ration_original_fifo"
                         : nullptr;
    if (!id || rule.id != id || rule.version != 1 ||
        rule.catalog_digest != x_.identity.content_digest)
      fail("EconomicFifoAdapterRuleMismatch", m.business_fact_id);
    for (std::size_t i = 0; i < stages.size(); ++i)
      if (&stages[i].stage != &m.original_business_execution->stages[i])
        fail("EconomicFifoStageBorrowMismatch", m.business_fact_id);
    if (m.business_kind == MarketCauseKind::local_production)
      production(x_, m, stages);
    else
      ration(x_, m, stages);
  });
}
Status NativeEconomicFifoRead::visit_original_material_pins(
    const MarketMovementRecord &m,
    std::span<const MarketOriginalStageReadView> stages,
    LotPropertiesSourcePinSink &sink) const {
  return guard([&] {
    take(validate_original_fifo_business(m, stages));
    const OriginalSource owned_source(x_.common, m.business_fact_id);
    const auto &source = owned_source.value;
    if (m.business_kind == MarketCauseKind::local_production) {
      const auto &r = *x_.economy.production_receipts.find(m.business_fact_id);
      const auto &u = *x_.catalog.production_units.find(r.unit_id);
      const auto &recipe = *x_.catalog.recipes.find(r.recipe_id);
      for (std::size_t i = 0; i < recipe.inputs.size(); ++i)
        for (std::size_t j = 0; j < u.input_inventory_ids.size(); ++j)
          take(x_.materials.visit_candidate_pins(
              {EconomicMaterialKind::production_input, u.id,
               static_cast<std::uint32_t>(i)},
              before(stages[0], j, u.input_inventory_ids[j]), u.owner, r.tick,
              sink, &source));
    } else {
      const auto &r = *x_.economy.rations.find(m.business_fact_id);
      const auto &inventory = before(stages[0], 0, r.inventory_id);
      take(x_.materials.visit_candidate_pins(
          {EconomicMaterialKind::ration, r.obligation_id, 0}, inventory,
          inventory.owner, r.tick, sink, &source));
    }
  });
}
} // namespace hanmo::resident
