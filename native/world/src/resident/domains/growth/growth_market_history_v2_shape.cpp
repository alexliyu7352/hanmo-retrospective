// History2结构门只核声明字段和有限关系；原业务、FIFO与Installed链仍须真实reader反验。
#include "growth_market_history_v2_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <limits>
namespace hanmo::resident {
namespace {
// 不建索引副本；所有检查只借原有序拥有行，集合预算沿原checkpoint上限。
bool bounded(std::size_t size) { return size <= CodecVisitor::max_items; }
bool canonical(const StableId &id, std::string_view prefix) {
  if (id.size() <= prefix.size() + 1 ||
      id.compare(0, prefix.size(), prefix) != 0 || id[prefix.size()] != ':')
    return false;
  const char *first = id.data() + prefix.size() + 1;
  const char *last = id.data() + id.size();
  if (*first < '1' || *first > '9')
    return false;
  Integer value = 0;
  auto parsed = std::from_chars(first, last, value);
  return parsed.ec == std::errc{} && parsed.ptr == last && value > 0;
}
bool ordered(const FieldOrder &order,
             std::initializer_list<std::string_view> keys) {
  if (order.empty())
    return true;
  if (order.size() != keys.size())
    return false;
  for (std::size_t i = 0; i < order.size(); ++i)
    if (std::find(keys.begin(), keys.end(), order[i]) == keys.end() ||
        std::find(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(i),
                  order[i]) != order.begin() + static_cast<std::ptrdiff_t>(i))
      return false;
  return true;
}
template <class Range, class Id> bool unique_ids(const Range &values, Id id) {
  if (!bounded(values.size()))
    return false;
  for (std::size_t i = 0; i < values.size(); ++i)
    for (std::size_t j = 0; j < i; ++j)
      if (id(values[i]) == id(values[j]))
        return false;
  return true;
}
Status owner(const Owner &value) {
  const auto kind = static_cast<Integer>(value.kind);
  return !value.id.empty() && !value.extra && kind >= 0 && kind <= 3 &&
                 ordered(value.field_order, {"kind", "id"})
             ? common_success()
             : common_failure("InvalidGrowthMarketOwner", value.id);
}
Status reservation(const ReservationRef &value,
                   const CommonResourceCodecContext &layout,
                   bool empty_allowed) {
  const auto kind = static_cast<Integer>(value.kind);
  if (kind < 0 || kind > 3 || (!empty_allowed && value.id.empty()) ||
      (value.id.empty() && value.kind != ReservationRef::Kind::action))
    return common_failure("InvalidGrowthMarketReservation", value.id);
  if (value.kind == ReservationRef::Kind::domain_activity)
    return layout.domain_activity_reservation_refs_enabled &&
                   !value.domain_id.empty() && !value.activity_kind.empty()
               ? common_success()
               : common_failure("InvalidGrowthMarketReservation", value.id);
  return value.domain_id.empty() && value.activity_kind.empty()
             ? common_success()
             : common_failure("InvalidGrowthMarketReservation", value.id);
}
// 完整Common叶不删未选批的typed引用；原codec可追加已选I/L字段，仍不能携带未知键。
Status lot(const LotView &value, const CommonResourceCodecContext &layout) {
  if (!canonical(value.id, "lot") || value.item_id.empty() ||
      value.quantity <= 0 || value.extra ||
      (value.item_instance_id && !layout.item_instance_refs_enabled) ||
      (value.lot_properties_ref && !layout.lot_property_refs_enabled) ||
      (value.item_instance_id && value.lot_properties_ref) ||
      (value.item_instance_id && value.item_instance_id->empty()) ||
      (value.lot_properties_ref &&
       value.lot_properties_ref->properties_id.empty()))
    return common_failure("InvalidGrowthMarketLot", value.id);
  auto valid = owner(value.owner);
  if (!valid.ok)
    return valid;
  valid = reservation(value.reserved_by, layout, true);
  if (!valid.ok)
    return valid;
  if (!value.field_order.empty()) {
    constexpr std::string_view base[] = {"id", "item_id", "quantity", "owner",
                                         "reserved_by"};
    for (const auto key : base)
      if (std::find(value.field_order.begin(), value.field_order.end(), key) ==
          value.field_order.end())
        return common_failure("InvalidGrowthMarketLotOrder", value.id);
    for (std::size_t i = 0; i < value.field_order.size(); ++i) {
      const auto &key = value.field_order[i];
      const bool known =
          std::find(std::begin(base), std::end(base), key) != std::end(base) ||
          (layout.item_instance_refs_enabled && key == "item_instance_id") ||
          (layout.lot_property_refs_enabled && key == "lot_properties_ref");
      if (!known ||
          std::find(value.field_order.begin(),
                    value.field_order.begin() + static_cast<std::ptrdiff_t>(i),
                    key) !=
              value.field_order.begin() + static_cast<std::ptrdiff_t>(i))
        return common_failure("InvalidGrowthMarketLotOrder", value.id);
    }
  }
  return common_success();
}
Status inventory(const InventoryView &value,
                 const CommonResourceCodecContext &layout) {
  if (value.id.empty() || value.capacity < 0 || value.revision < 0 ||
      !ordered(value.field_order,
               {"id", "owner", "capacity", "revision", "lots"}) ||
      !unique_ids(
          value.lots, [](const auto &row) -> const auto & { return row.id; }))
    return common_failure("InvalidGrowthMarketInventory", value.id);
  auto valid = owner(value.owner);
  if (!valid.ok)
    return valid;
  for (const auto &row : value.lots) {
    valid = lot(row, layout);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
Status change(const MarketInventoryChange &value,
              const CommonResourceCodecContext &layout) {
  if (value.inventory_id.empty() || value.revision_before < 0 ||
      value.revision_after <= value.revision_before || value.lots.empty() ||
      !ordered(value.field_order,
               {"inventory_id", "revision_before", "revision_after", "lots"}) ||
      !unique_ids(
          value.lots,
          [](const auto &row) -> const auto & { return row.lot_id; }))
    return common_failure("InvalidGrowthMarketInventoryChange",
                          value.inventory_id);
  for (const auto &row : value.lots) {
    if (!canonical(row.lot_id, "lot") || (!row.before && !row.after) ||
        !ordered(row.field_order, {"lot_id", "before", "after"}))
      return common_failure("InvalidGrowthMarketLotChange", row.lot_id);
    for (const auto *item : {&row.before, &row.after})
      if (*item) {
        auto valid = lot(**item, layout);
        if (!valid.ok)
          return valid;
        if ((*item)->id != row.lot_id)
          return common_failure("GrowthMarketLotChangeIdentityMismatch",
                                row.lot_id);
      }
    if (row.before && row.after &&
        same_resource_lot_value(*row.before, *row.after))
      return common_failure("GrowthMarketUnchangedLotDelta", row.lot_id);
  }
  return common_success();
}
// Step与执行frame只能借同一真实稀疏值，不能另保存一套较弱投影。
bool same_change(const MarketInventoryChange &a,
                 const MarketInventoryChange &b) {
  if (a.inventory_id != b.inventory_id ||
      a.revision_before != b.revision_before ||
      a.revision_after != b.revision_after || a.field_order != b.field_order ||
      a.lots.size() != b.lots.size())
    return false;
  for (std::size_t i = 0; i < a.lots.size(); ++i) {
    const auto &x = a.lots[i];
    const auto &y = b.lots[i];
    if (x.lot_id != y.lot_id || x.field_order != y.field_order ||
        bool(x.before) != bool(y.before) || bool(x.after) != bool(y.after) ||
        (x.before && !same_resource_lot_value(*x.before, *y.before)) ||
        (x.after && !same_resource_lot_value(*x.after, *y.after)))
      return false;
  }
  return true;
}
Status rule(const ActorFoundationRuleRef &value) {
  return !value.id.empty() && value.version > 0 && !value.catalog_digest.empty()
             ? common_success()
             : common_failure("InvalidMarketFifoRuleRef", value.id);
}
Status wallet(const TradeWalletPosting &value,
              const CommonResourceCodecContext &layout) {
  if (value.wallet_id.empty() || value.balance_before < 0 ||
      value.balance_after < 0 || value.spendable_before < 0 ||
      !bounded(value.reservations_before.size()) ||
      value.reservations_before.size() != value.reservations_after.size())
    return common_failure("InvalidMarketTradeWallet", value.wallet_id);
  auto valid = owner(value.owner);
  if (!valid.ok)
    return valid;
  if (value.delta_qian < -value.balance_before ||
      value.delta_qian >
          std::numeric_limits<Integer>::max() - value.balance_before ||
      value.balance_after != value.balance_before + value.delta_qian)
    return common_failure("MarketTradeWalletBalanceMismatch", value.wallet_id);
  Integer reserved = 0;
  for (std::size_t i = 0; i < value.reservations_before.size(); ++i) {
    const auto &before = value.reservations_before[i];
    const auto &after = value.reservations_after[i];
    valid = reservation(before.action_id, layout, false);
    if (!valid.ok)
      return valid;
    if (before.action_id != after.action_id || before.amount != after.amount ||
        before.amount <= 0 || before.amount > value.balance_before - reserved)
      return common_failure("MarketTradeWalletReservationMismatch",
                            value.wallet_id);
    for (std::size_t j = 0; j < i; ++j)
      if (value.reservations_before[j].action_id == before.action_id)
        return common_failure("MarketTradeWalletReservationRepeated",
                              value.wallet_id);
    reserved += before.amount;
  }
  if (value.spendable_before != value.balance_before - reserved ||
      value.delta_qian < -value.spendable_before)
    return common_failure("MarketTradeWalletSpendableMismatch",
                          value.wallet_id);
  return common_success();
}
// 来源slot约束原语的有限种类；完整FIFO顺序仍由原source reader以完整before复算。
Status operation(const OriginalBusinessItemOperation &value,
                 MarketCaptureStage stage) {
  const auto slot = static_cast<std::size_t>(stage);
  if (value.source_slot.valueless_by_exception() ||
      value.material_need.valueless_by_exception() ||
      value.output_basis.valueless_by_exception() ||
      value.source_slot.index() != slot ||
      !ordered(value.field_order,
               {"operation_index", "source_slot", "selections", "material_need",
                "output_basis", "lot_flows"}) ||
      !bounded(value.selections.size()) || value.lot_flows.empty() ||
      !bounded(value.lot_flows.size()))
    return common_failure("InvalidMarketFifoItemOperation");
  const bool output = stage == MarketCaptureStage::production_outputs;
  const bool consume = stage == MarketCaptureStage::production_inputs ||
                       stage == MarketCaptureStage::ration;
  if (value.material_need.index() != (consume ? 1u : 0u) ||
      (output ? value.output_basis.index() == 0
              : value.output_basis.index() != 0))
    return common_failure("MarketFifoOperationBasisMismatch");
  if (consume) {
    auto valid = rule(
        std::get<MarketOriginalMaterialNeed>(value.material_need).rule_ref);
    if (!valid.ok)
      return valid;
  }
  if (const auto *registered =
          std::get_if<MarketRegisteredProductionOutput>(&value.output_basis))
    if (registered->properties_ref.properties_id.empty() ||
        registered->output_ordinal !=
            std::get<MarketProductionOutputSlot>(value.source_slot)
                .receipt_output_ordinal)
      return common_failure("MarketFifoOutputBasisMismatch");
  if (output) {
    if (!value.selections.empty() || value.lot_flows.size() != 1)
      return common_failure("MarketFifoOutputFlowMismatch");
  } else if (value.selections.empty() ||
             value.selections.size() != value.lot_flows.size())
    return common_failure("MarketFifoSelectionFlowMismatch");
  for (std::size_t i = 0; i < value.lot_flows.size(); ++i) {
    const auto &flow = value.lot_flows[i];
    if (flow.item_id.empty() || flow.quantity <= 0)
      return common_failure("InvalidMarketFifoFlow");
    if (output) {
      if (flow.kind != ResourceLotFlowDraft::Kind::add ||
          !flow.source_inventory_id.empty() || !flow.source_lot_id.empty() ||
          flow.destination_inventory_id.empty() ||
          !canonical(flow.destination_lot_id, "lot"))
        return common_failure("MarketFifoOutputFlowMismatch");
      continue;
    }
    const auto &selected = value.selections[i];
    if (selected.inventory_id.empty() || !canonical(selected.lot_id, "lot") ||
        selected.quantity <= 0 ||
        flow.source_inventory_id != selected.inventory_id ||
        flow.source_lot_id != selected.lot_id ||
        flow.quantity != selected.quantity)
      return common_failure("MarketFifoSelectionFlowMismatch", selected.lot_id);
    for (std::size_t j = 0; j < i; ++j)
      if (value.selections[j].inventory_id == selected.inventory_id &&
          value.selections[j].lot_id == selected.lot_id)
        return common_failure("MarketFifoSelectionRepeated", selected.lot_id);
    if (consume) {
      if (flow.kind != ResourceLotFlowDraft::Kind::selected_consume ||
          !flow.destination_inventory_id.empty() ||
          !flow.destination_lot_id.empty())
        return common_failure("MarketFifoConsumptionFlowMismatch",
                              selected.lot_id);
    } else {
      const bool whole =
          flow.kind == ResourceLotFlowDraft::Kind::selected_transfer_whole;
      const bool split =
          flow.kind == ResourceLotFlowDraft::Kind::selected_transfer_split;
      if ((!whole && !split) || flow.destination_inventory_id.empty() ||
          flow.destination_inventory_id == flow.source_inventory_id ||
          !canonical(flow.destination_lot_id, "lot") ||
          (whole ? flow.destination_lot_id != flow.source_lot_id
                 : flow.destination_lot_id == flow.source_lot_id))
        return common_failure("MarketFifoTradeFlowMismatch", selected.lot_id);
    }
  }
  return common_success();
}
Status stage(const OriginalBusinessStage &value,
             const CommonResourceCodecContext &layout) {
  const auto number = static_cast<Integer>(value.stage);
  if (number < 0 || number > 3 || value.next_instance_before < 1 ||
      value.next_instance_after < value.next_instance_before ||
      !ordered(value.field_order,
               {"stage", "next_instance_before", "next_instance_after",
                "inventory_frames", "item_operations", "wallet_postings"}) ||
      !unique_ids(
          value.inventory_frames,
          [](const auto &row) -> const auto & { return row.inventory_id; }) ||
      !bounded(value.item_operations.size()))
    return common_failure("InvalidMarketFifoStage");
  for (const auto &frame : value.inventory_frames) {
    if (frame.inventory_id.empty() ||
        frame.before_basis.valueless_by_exception() ||
        !ordered(frame.field_order,
                 {"inventory_id", "before_basis", "after_delta"}))
      return common_failure("InvalidMarketFifoFrame", frame.inventory_id);
    if (const auto *full =
            std::get_if<MarketFullCapturedBefore>(&frame.before_basis)) {
      if (full->inventory.id != frame.inventory_id)
        return common_failure("MarketFifoBeforeIdentityMismatch",
                              frame.inventory_id);
      auto valid = inventory(full->inventory, layout);
      if (!valid.ok)
        return valid;
    }
    if (const auto *prior =
            std::get_if<MarketPriorPinnedStageAfter>(&frame.before_basis))
      if (!canonical(prior->movement_id, "market_history"))
        return common_failure("InvalidMarketFifoPriorStage",
                              prior->movement_id);
    if (frame.after_delta) {
      if (frame.after_delta->inventory_id != frame.inventory_id)
        return common_failure("MarketFifoAfterIdentityMismatch",
                              frame.inventory_id);
      auto valid = change(*frame.after_delta, layout);
      if (!valid.ok)
        return valid;
    }
  }
  for (std::size_t i = 0; i < value.item_operations.size(); ++i) {
    const auto &row = value.item_operations[i];
    if (i &&
        value.item_operations[i - 1].operation_index >= row.operation_index)
      return common_failure("MarketFifoOperationOrderMismatch");
    auto valid = operation(row, value.stage);
    if (!valid.ok)
      return valid;
  }
  for (const auto &item : value.item_operations)
    for (const auto &flow : item.lot_flows) {
      for (const auto *endpoint :
           {&flow.source_inventory_id, &flow.destination_inventory_id})
        if (!endpoint->empty() &&
            std::none_of(value.inventory_frames.begin(),
                         value.inventory_frames.end(), [&](const auto &frame) {
                           return frame.inventory_id == *endpoint;
                         }))
          return common_failure("MarketFifoFlowEndpointMissing", *endpoint);
    }
  if (value.stage == MarketCaptureStage::trade) {
    if (value.inventory_frames.size() != 2 ||
        value.item_operations.size() != 1 ||
        value.item_operations[0].operation_index != 0 ||
        value.wallet_postings.size() != 2)
      return common_failure("MarketFifoTradeStageMismatch");
    const auto &payer = value.wallet_postings[0];
    const auto &payee = value.wallet_postings[1];
    if (payer.wallet_id == payee.wallet_id || payer.delta_qian > 0 ||
        payee.delta_qian < 0 || payer.delta_qian != -payee.delta_qian)
      return common_failure("MarketFifoTradeMoneyLegMismatch");
    for (const auto &posting : value.wallet_postings) {
      auto valid = wallet(posting, layout);
      if (!valid.ok)
        return valid;
    }
  } else if (!value.wallet_postings.empty())
    return common_failure("UnexpectedMarketFifoWalletPosting");
  if (value.stage == MarketCaptureStage::ration &&
      (value.item_operations.size() != 1 ||
       value.item_operations[0].operation_index != 0))
    return common_failure("MarketFifoRationStageMismatch");
  if (value.item_operations.empty()) {
    if (value.next_instance_before != value.next_instance_after)
      return common_failure("MarketFifoReadOnlyAllocatorChanged");
    for (const auto &frame : value.inventory_frames)
      if (frame.after_delta)
        return common_failure("MarketFifoReadOnlyDelta");
  }
  return common_success();
}
Status movement(const MarketMovementRecord &value,
                const CommonResourceCodecContext &layout) {
  const auto kind = static_cast<Integer>(value.business_kind);
  if (value.kind != "growth_market_movement" ||
      value.business_fact_id.empty() || kind < 0 || kind > 2 ||
      (value.business_source_schema != 1 &&
       value.business_source_schema != 2) ||
      value.source_tick < 0 || value.observed_tick < 0 || value.steps.empty() ||
      value.steps.size() > 2 ||
      !ordered(value.field_order,
               {"kind", "business_fact_id", "business_kind",
                "business_source_schema", "source_tick", "observed_tick",
                "steps", "original_business_execution"}))
    return common_failure("InvalidGrowthMarketMovement");
  if (value.business_source_schema == 2 &&
      (kind != 0 || value.original_business_execution))
    return common_failure("MarketBasketExecutionShapeMismatch",
                          value.business_fact_id);
  if (value.business_source_schema == 1 && !value.original_business_execution)
    return common_failure("MarketOriginalFifoExecutionRequired",
                          value.business_fact_id);
  const auto *execution = value.original_business_execution
                              ? &*value.original_business_execution
                              : nullptr;
  if (execution) {
    constexpr std::string_view adapters[] = {"growth_trade_original_fifo",
                                             "local_production_original_fifo",
                                             "local_ration_original_fifo"};
    if (execution->schema_version != 1 ||
        execution->adapter_rule_ref.id != adapters[kind] ||
        execution->adapter_rule_ref.version != 1 ||
        execution->time_basis.valueless_by_exception() ||
        execution->stages.empty() || execution->stages.size() > 2 ||
        !ordered(execution->field_order, {"schema_version", "adapter_rule_ref",
                                          "time_basis", "stages"}))
      return common_failure("InvalidMarketOriginalFifoExecution",
                            value.business_fact_id);
    auto valid = rule(execution->adapter_rule_ref);
    if (!valid.ok)
      return valid;
    if (const auto *exact =
            std::get_if<MarketScalarActualStart>(&execution->time_basis)) {
      if (kind != 0 || exact->action_ref != value.business_fact_id ||
          exact->started_at.half_day_tick != value.source_tick ||
          value.source_tick != value.observed_tick)
        return common_failure("MarketScalarStartShapeMismatch",
                              value.business_fact_id);
      auto encoded = encode_world_moment(exact->started_at);
      if (!encoded.ok)
        return {false, {}, encoded.error};
    }
    Integer previous = -1;
    for (const auto &row : execution->stages) {
      valid = stage(row, layout);
      if (!valid.ok)
        return valid;
      const auto number = static_cast<Integer>(row.stage);
      if (number <= previous || (kind == 0 && number != 0) ||
          (kind == 1 && (number < 1 || number > 2)) ||
          (kind == 2 && number != 3))
        return common_failure("MarketOriginalStageOrderMismatch",
                              value.business_fact_id);
      previous = number;
    }
  }
  Integer previous = -1;
  for (std::size_t i = 0; i < value.steps.size(); ++i) {
    const auto &step = value.steps[i];
    const auto number = static_cast<Integer>(step.stage);
    if (number < 0 || number > 3 || number <= previous ||
        (kind == 0 && number != 0) ||
        (kind == 1 && (number < 1 || number > 2)) ||
        (kind == 2 && number != 3) || step.next_instance_before < 1 ||
        step.next_instance_after < step.next_instance_before ||
        step.inventories.empty() ||
        step.source_binding.valueless_by_exception() ||
        !ordered(step.field_order,
                 {"stage", "next_instance_before", "next_instance_after",
                  "inventories", "source_binding"}) ||
        !unique_ids(
            step.inventories,
            [](const auto &row) -> const auto & { return row.inventory_id; }))
      return common_failure("InvalidGrowthMarketCaptureStep",
                            value.business_fact_id);
    previous = number;
    for (const auto &delta : step.inventories) {
      auto valid = change(delta, layout);
      if (!valid.ok)
        return valid;
    }
    if (execution) {
      const auto *binding =
          std::get_if<MarketOriginalBusinessFifo>(&step.source_binding);
      if (!binding || binding->business_fact_id != value.business_fact_id ||
          binding->execution_stage_ordinal >= execution->stages.size())
        return common_failure("MarketOriginalStageBindingMismatch",
                              value.business_fact_id);
      const auto &actual = execution->stages[binding->execution_stage_ordinal];
      if (actual.stage != step.stage ||
          actual.next_instance_before != step.next_instance_before ||
          actual.next_instance_after != step.next_instance_after)
        return common_failure("MarketOriginalStageBindingMismatch",
                              value.business_fact_id);
      for (const auto &delta : step.inventories) {
        const OriginalBusinessInventoryFrame *frame = nullptr;
        for (const auto &candidate : actual.inventory_frames)
          if (candidate.inventory_id == delta.inventory_id)
            frame = &candidate;
        if (!frame ||
            !std::holds_alternative<MarketProtectedHistoryBefore>(
                frame->before_basis) ||
            !frame->after_delta || !same_change(delta, *frame->after_delta))
          return common_failure("MarketOriginalStageDeltaMismatch",
                                delta.inventory_id);
      }
    } else {
      const auto *binding =
          std::get_if<MarketSelectedTradeRows>(&step.source_binding);
      if (!binding || binding->life_activity_id != value.business_fact_id ||
          binding->source_schema != 2 || binding->row_ordinals.empty() ||
          !bounded(binding->row_ordinals.size()))
        return common_failure("MarketSelectedRowsBindingMismatch",
                              value.business_fact_id);
      for (std::size_t row = 1; row < binding->row_ordinals.size(); ++row)
        if (binding->row_ordinals[row - 1] >= binding->row_ordinals[row])
          return common_failure("MarketSelectedRowsOrderMismatch",
                                value.business_fact_id);
    }
  }
  if (execution)
    for (std::size_t ordinal = 0; ordinal < execution->stages.size();
         ++ordinal) {
      const auto &actual = execution->stages[ordinal];
      const MarketCaptureStep *matched = nullptr;
      for (const auto &step : value.steps)
        if (std::get<MarketOriginalBusinessFifo>(step.source_binding)
                .execution_stage_ordinal == ordinal)
          matched = &step;
      bool protected_write = false;
      for (const auto &frame : actual.inventory_frames)
        if (std::holds_alternative<MarketProtectedHistoryBefore>(
                frame.before_basis) &&
            frame.after_delta) {
          protected_write = true;
          if (!matched ||
              std::none_of(matched->inventories.begin(),
                           matched->inventories.end(), [&](const auto &delta) {
                             return delta.inventory_id == frame.inventory_id;
                           }))
            return common_failure("MarketOriginalProtectedWriteMissing",
                                  frame.inventory_id);
        }
      if (bool(matched) != protected_write)
        return common_failure("MarketOriginalProtectedStepMismatch",
                              value.business_fact_id);
    }
  return common_success();
}
// 前驱只能引用本有序段已出现的source1完整stage；同笔严格向前，完整值连续性留给冷重建。
Status prior_links(const GrowthMarketHistoryState &state, std::size_t current) {
  const auto &row = state.movements.rows()[current];
  if (!row.second.original_business_execution)
    return common_success();
  const auto &execution = *row.second.original_business_execution;
  for (std::size_t ordinal = 0; ordinal < execution.stages.size(); ++ordinal)
    for (const auto &frame : execution.stages[ordinal].inventory_frames)
      if (const auto *ref =
              std::get_if<MarketPriorPinnedStageAfter>(&frame.before_basis)) {
        const MarketMovementRecord *target = nullptr;
        for (std::size_t prior = 0; prior <= current; ++prior)
          if (state.movements.rows()[prior].first == ref->movement_id) {
            if (prior == current && ref->execution_stage_ordinal >= ordinal)
              return common_failure("MarketFifoPriorStageCycle",
                                    ref->movement_id);
            target = &state.movements.rows()[prior].second;
            break;
          }
        if (!target || target->business_source_schema != 1 ||
            !target->original_business_execution ||
            ref->execution_stage_ordinal >=
                target->original_business_execution->stages.size())
          return common_failure("MarketFifoPriorStageMissing",
                                ref->movement_id);
        const auto &frames = target->original_business_execution
                                 ->stages[ref->execution_stage_ordinal]
                                 .inventory_frames;
        const OriginalBusinessInventoryFrame *previous = nullptr;
        for (const auto &candidate : frames)
          if (candidate.inventory_id == frame.inventory_id)
            previous = &candidate;
        if (!previous || std::holds_alternative<MarketProtectedHistoryBefore>(
                             previous->before_basis))
          return common_failure("MarketFifoPriorInventoryMismatch",
                                frame.inventory_id);
      }
  return common_success();
}
} // namespace
Status validate_growth_market_history_v2_shape(
    const GrowthMarketHistoryState &state,
    const CommonResourceCodecContext &layout) {
  if (state.version != 2 || !bounded(state.movements.size()) ||
      !ordered(state.field_order, {"version", "movements"}))
    return common_failure("InvalidGrowthMarketHistory2State");
  for (std::size_t i = 0; i < state.movements.size(); ++i) {
    const auto &row = state.movements.rows()[i];
    if (!canonical(row.first, "market_history"))
      return common_failure("InvalidGrowthMarketMovementId", row.first);
    auto valid = movement(row.second, layout);
    if (!valid.ok)
      return valid;
    valid = prior_links(state, i);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
} // namespace hanmo::resident
