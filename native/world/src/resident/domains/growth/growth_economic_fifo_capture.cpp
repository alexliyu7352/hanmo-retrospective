// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 原 LE 物料循环给槽地址；此处只保存同次 Common 执行已经产生的完整证据。
#include "growth_economic_fifo_capture.hpp"
#include "resident/domains/local_economy/market_history/economic_fifo_read.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &path = {}) {
  throw Failure{common_failure(code, path).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
StableId copy(const StableId &x) { return checkpoint_fresh_string_copy(x); }
// 差量只是原完整值的有序比较；不重跑扣货、产出或 allocator 数学。
template <class Sink>
void differences(const InventoryView &a, const InventoryView &b, Sink sink) {
  for (const auto &old : a.lots) {
    const LotView *next = nullptr;
    for (const auto &row : b.lots)
      if (row.id == old.id)
        next = &row;
    if (!next || !same_resource_lot_value(old, *next))
      sink(old.id, &old, next);
  }
  for (const auto &next : b.lots)
    if (std::none_of(a.lots.begin(), a.lots.end(),
                     [&](const auto &old) { return old.id == next.id; }))
      sink(next.id, nullptr, &next);
}

MarketInventoryChange change(const InventoryView &a, const InventoryView &b) {
  MarketInventoryChange out;
  out.inventory_id = copy(a.id);
  out.revision_before = a.revision;
  out.revision_after = b.revision;
  std::size_t count = 0;
  differences(a, b,
              [&](const auto &, const LotView *, const LotView *) { ++count; });
  out.lots.reserve(count);
  differences(a, b,
              [&](const auto &id, const LotView *old, const LotView *next) {
                out.lots.emplace_back();
                auto &row = out.lots.back();
                row.lot_id = copy(id);
                if (old)
                  row.before.emplace(*old);
                if (next)
                  row.after.emplace(*next);
              });
  return out;
}
const InventoryView &after(const ResourceExecutionDraft &draft,
                           const InventoryView &before) {
  const InventoryView *result = &before;
  for (const auto &op : draft.operations)
    for (const auto &frame : op.inventories)
      if (frame.inventory_id == before.id)
        result = &frame.after;
  return *result;
}
bool touched(const ResourceExecutionDraft &draft, const StableId &id) {
  for (const auto &op : draft.operations)
    for (const auto &frame : op.inventories)
      if (frame.inventory_id == id)
        return true;
  return false;
}
void protected_inventory(const ProtectedMarketAccounts &policy,
                         const InventoryView &value) {
  const auto *required = policy.inventories.find(value.id);
  if (!required)
    return;
  if (value.capacity != required->capacity ||
      !owner_equal(value.owner, required->owner))
    fail("GrowthEconomicProtectedContainerMismatch", value.id);
  for (const auto &lot : value.lots)
    if (!owner_equal(lot.owner, required->owner) || !lot.reserved_by.empty())
      fail("GrowthEconomicProtectedLotMismatch", lot.id);
}
const StableId &operation_inventory(const ResourceOperation &op,
                                    MarketCaptureStage stage) {
  if (stage == MarketCaptureStage::production_outputs) {
    const auto *v = std::get_if<AddLot>(&op);
    if (!v || v->quantity <= 0 || !v->reservation.empty() ||
        v->item_instance_id || v->lot_properties_ref)
      fail("GrowthEconomicPlainAddRequired");
    return v->inventory_id;
  }
  const auto *v = std::get_if<ConsumeSelectedLots>(&op);
  if (!v || v->selections.empty() || v->owners.size() != 1 ||
      !v->reservation.empty())
    fail("GrowthEconomicSelectedConsumeRequired");
  return v->inventory_id;
}

void build_operation(OriginalBusinessItemOperation &out,
                     const ResourceOperation &recipe,
                     const ResourceOperationExecutionDraft &actual,
                     const EconomicMarketItemOperationBorrow &meta,
                     const EconomicMaterialNeedRead &need,
                     MarketCaptureStage stage) {
  out.operation_index = static_cast<std::uint32_t>(meta.operation_index);
  if (stage == MarketCaptureStage::production_inputs)
    out.source_slot.emplace<MarketProductionInputSlot>(
        MarketProductionInputSlot{meta.receipt_ordinal});
  else if (stage == MarketCaptureStage::production_outputs)
    out.source_slot.emplace<MarketProductionOutputSlot>(
        MarketProductionOutputSlot{meta.receipt_ordinal});
  else
    out.source_slot.emplace<MarketRationConsumptionSlot>();
  if (const auto *consume = std::get_if<ConsumeSelectedLots>(&recipe)) {
    out.selections = std::vector<CargoSelection>{consume->selections};
    out.material_need.emplace<MarketOriginalMaterialNeed>(
        MarketOriginalMaterialNeed{{copy(*need.rule_id), need.rule_version,
                                    copy(*need.catalog_digest)},
                                   need.need_slot_ordinal});
  } else
    out.output_basis.emplace<MarketOriginalPlainAdd>();
  out.lot_flows.reserve(actual.lot_flows.size());
  for (const auto &flow : actual.lot_flows)
    out.lot_flows.push_back(
        {flow.kind, copy(flow.source_inventory_id), copy(flow.source_lot_id),
         copy(flow.destination_inventory_id), copy(flow.destination_lot_id),
         copy(flow.item_id), flow.quantity});
}
} // namespace
GrowthEconomicFifoStageCapture::GrowthEconomicFifoStageCapture(
    CheckpointMemoryCharge memory)
    : copied_fields_(std::move(memory)) {}
Outcome<GrowthEconomicFifoStageCapture> prepare_growth_economic_fifo_stage(
    const CommonStore &common, const GrowthMarketHistoryCatalog &history,
    const ProtectedMarketAccounts &policy,
    const NativeEconomicMaterialRead &materials, MarketCauseKind kind,
    const StableId &key, Integer due, MarketCaptureStage stage,
    std::span<const InventoryView *const> full_before,
    const ResourceRecipe &recipe,
    std::span<const EconomicMarketItemOperationBorrow> metadata) {
  try {
    if (history.selected_version != 2 ||
        !common.resource_execution_capture_identity() || key.empty() ||
        due < 0 ||
        !((kind == MarketCauseKind::local_production &&
           (stage == MarketCaptureStage::production_inputs ||
            stage == MarketCaptureStage::production_outputs)) ||
          (kind == MarketCauseKind::local_ration &&
           stage == MarketCaptureStage::ration)) ||
        metadata.size() != recipe.operations.size())
      fail("GrowthEconomicOriginalScopeRequired", key);
    take(materials.validate_bindings());
    for (std::size_t i = 0; i < full_before.size(); ++i) {
      const auto *v = full_before[i];
      if (!v || common.inventory(v->id) != v)
        fail("GrowthEconomicActualBeforeRequired", key);
      for (std::size_t j = 0; j < i; ++j)
        if (full_before[j]->id == v->id)
          fail("GrowthEconomicDuplicateReadInventory", v->id);
      protected_inventory(policy, *v);
    }
    for (std::size_t i = 0; i < metadata.size(); ++i) {
      if (i > UINT32_MAX || metadata[i].operation_index != i ||
          metadata[i].receipt_ordinal != i)
        fail("GrowthEconomicOriginalOperationOrdinalMismatch", key);
      const auto &id = operation_inventory(recipe.operations[i], stage);
      if (std::none_of(full_before.begin(), full_before.end(),
                       [&](auto v) { return v->id == id; }))
        fail("GrowthEconomicOperationOutsideReadFootprint", id);
    }


    GrowthEconomicFifoStageCapture out(CheckpointMemoryCharge(0));
    out.common_ = &common;
    out.policy_ = &policy;
    out.materials_ = &materials;
    out.identity_ = common.resource_execution_capture_identity();
    out.recipe_index_ = common.resource_execution_drafts().size();
    out.observed_tick_ = common.tick();
    out.due_tick_ = due;
    out.next_before_ = common.state().next_instance_id;
    out.kind_ = kind;
    out.stage_kind_ = stage;
    out.original_key_ = copy(key);
    out.before_.reserve(full_before.size());
    for (auto v : full_before)
      out.before_.push_back(*v);
    out.metadata_.reserve(metadata.size());
    out.needs_.reserve(metadata.size());
    for (const auto &meta : metadata) {
      out.metadata_.push_back(meta);
      if (stage == MarketCaptureStage::production_outputs)
        out.needs_.emplace_back();
      else {
        auto need = take(
            materials.read_need({kind == MarketCauseKind::local_ration
                                     ? EconomicMaterialKind::ration
                                     : EconomicMaterialKind::production_input,
                                 key, meta.need_slot_ordinal}));
        if (!need.rule_id || !need.catalog_digest || !need.item_id ||
            need.rule_version != 1 ||
            need.need_slot_ordinal != meta.need_slot_ordinal)
          fail("GrowthEconomicOriginalMaterialNeedRequired", key);
        out.needs_.push_back(need);
      }
    }
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("GrowthEconomicCaptureMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Status capture_growth_economic_fifo_stage(GrowthEconomicFifoStageCapture &out,
                                          const CommonStore &common) {
  try {
    if (out.common_ != &common || !out.identity_ ||
        out.identity_ != common.resource_execution_capture_identity() ||
        out.stage_ || common.tick() != out.observed_tick_ ||
        common.resource_execution_drafts().size() != out.recipe_index_ + 1)
      fail("GrowthEconomicActualDraftRequired");
    const auto &draft = common.resource_execution_drafts()[out.recipe_index_];
    if (draft.next_instance_before != out.next_before_ ||
        draft.operations.size() != out.metadata_.size() ||
        draft.recipe.operations.size() != out.metadata_.size() ||
        draft.next_instance_after != common.state().next_instance_id)
      fail("GrowthEconomicActualDraftShapeMismatch", out.original_key_);
    for (std::size_t i = 0; i < draft.operations.size(); ++i) {
      const auto &op = draft.operations[i];
      if (out.stage_kind_ != MarketCaptureStage::production_outputs)
        take(validate_selected_resource_execution(draft.recipe.operations[i],
                                                  op));
      const auto &id =
          operation_inventory(draft.recipe.operations[i], out.stage_kind_);
      if (op.operation_index != i || !op.wallets.empty() ||
          op.inventories.size() != 1 || op.inventories[0].inventory_id != id ||
          !op.inventories[0].before ||
          std::none_of(out.before_.begin(), out.before_.end(),
                       [&](const auto &v) { return v.id == id; }))
        fail("GrowthEconomicActualOperationMismatch", id);
      for (const auto &flow : op.lot_flows)
        if (flow.kind !=
            (out.stage_kind_ == MarketCaptureStage::production_outputs
                 ? ResourceLotFlowDraft::Kind::add
                 : ResourceLotFlowDraft::Kind::selected_consume))
          fail("GrowthEconomicActualFlowKindMismatch", id);
    }
    std::size_t selected_count = 0;
    for (const auto &before : out.before_) {
      const auto &last = after(draft, before);
      const auto *live = common.inventory(before.id);
      if (!live || !same_original_inventory_frame(*live, last))
        fail("GrowthEconomicActualAfterMismatch", before.id);
      const InventoryView *prior = &before;
      for (const auto &op : draft.operations)
        for (const auto &frame : op.inventories)
          if (frame.inventory_id == before.id) {
            if (!frame.before ||
                !same_original_inventory_frame(*prior, *frame.before))
              fail("GrowthEconomicActualBeforeChainMismatch", before.id);
            prior = &frame.after;
          }
      protected_inventory(*out.policy_, last);
      if (out.policy_->inventories.find(before.id) && touched(draft, before.id))
        ++selected_count;
    }


    for (const auto &before : out.before_) {

      if (!out.policy_->inventories.find(before.id))
        (void)0;
      if (touched(draft, before.id))
        (void)0;
      if (out.policy_->inventories.find(before.id) && touched(draft, before.id))
        (void)0;
    }


    (out.copied_fields_).grow(0);
    out.stage_.emplace();
    auto &stage = *out.stage_;
    stage.stage = out.stage_kind_;
    stage.next_instance_before = draft.next_instance_before;
    stage.next_instance_after = draft.next_instance_after;
    stage.inventory_frames.reserve(out.before_.size());
    if (selected_count) {
      out.protected_step_.emplace();
      auto &step = *out.protected_step_;
      step.stage = stage.stage;
      step.next_instance_before = stage.next_instance_before;
      step.next_instance_after = stage.next_instance_after;
      step.inventories.reserve(selected_count);
    }
    for (const auto &before : out.before_) {
      stage.inventory_frames.emplace_back();
      auto &frame = stage.inventory_frames.back();
      frame.inventory_id = copy(before.id);
      if (!out.policy_->inventories.find(before.id))
        frame.before_basis.emplace<MarketFullCapturedBefore>(
            MarketFullCapturedBefore{before});
      if (touched(draft, before.id))
        frame.after_delta.emplace(change(before, after(draft, before)));
      if (out.policy_->inventories.find(before.id) && touched(draft, before.id))
        out.protected_step_->inventories.push_back(
            change(before, after(draft, before)));
    }
    stage.item_operations.resize(draft.operations.size());
    for (std::size_t i = 0; i < draft.operations.size(); ++i)
      build_operation(stage.item_operations[i], draft.recipe.operations[i],
                      draft.operations[i], out.metadata_[i], out.needs_[i],
                      out.stage_kind_);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("GrowthEconomicCaptureMemoryExhausted", {},
                          ErrorKind::internal);
  }
}
GrowthEconomicFifoMovementCapture::GrowthEconomicFifoMovementCapture(
    CheckpointMemoryCharge memory)
    : copied_fields_(std::move(memory)), record_(std::in_place) {}
Outcome<GrowthEconomicFifoMovementCapture> bind_growth_economic_fifo_movement(
    const CommonStore &common, const NativeEconomicFifoRead &reader,
    const StableId &receipt,
    std::span<const GrowthEconomicFifoStageCapture *const> stages) {
  try {
    take(reader.validate_bindings());
    const auto &x = reader.context();
    if (&x.common != &common || stages.empty() || !stages.front() ||
        !common.resource_execution_capture_identity())
      fail("GrowthEconomicSameWorldSourcesRequired", receipt);
    const auto cause = take(read_economic_market_cause_borrow(
        common, x.shared, x.economy, x.catalog, receipt, x.takeover, x.phase));
    const auto *first = stages.front();
    if (!cause.original_business_key ||
        *cause.original_business_key != first->original_key_ ||
        cause.kind != first->kind_ ||
        cause.source_tick !=
            (x.phase ? x.phase->to_tick : first->observed_tick_) ||
        cause.business_due_tick != first->due_tick_)
      fail("GrowthEconomicOriginalReceiptMismatch", receipt);
    if ((cause.kind == MarketCauseKind::local_production &&
         stages.size() != 2) ||
        (cause.kind == MarketCauseKind::local_ration && stages.size() != 1))
      fail("GrowthEconomicOriginalStagesRequired", receipt);
    std::size_t steps = 0, frames = 0;
    for (std::size_t i = 0; i < stages.size(); ++i) {
      const auto *s = stages[i];
      if (!s || s->common_ != &common ||
          s->identity_ != common.resource_execution_capture_identity() ||
          !s->stage_ || s->materials_ != &x.materials ||
          s->original_key_ != first->original_key_ || s->kind_ != cause.kind ||
          s->observed_tick_ != first->observed_tick_ ||
          s->due_tick_ != cause.business_due_tick ||
          s->recipe_index_ >= common.resource_execution_drafts().size() ||
          (i && s->recipe_index_ <= stages[i - 1]->recipe_index_))
        fail("GrowthEconomicStageIdentityMismatch", receipt);
      if (s->protected_step_)
        ++steps;
      frames += s->before_.size();
    }
    if (!steps)
      fail("GrowthEconomicProtectedWriteRequired", receipt);
    const char *adapter = cause.kind == MarketCauseKind::local_production
                              ? "local_production_original_fifo"
                              : "local_ration_original_fifo";


    GrowthEconomicFifoMovementCapture out(CheckpointMemoryCharge(0));
    out.common_ = &common;
    out.identity_ = common.resource_execution_capture_identity();
    auto &record = *out.record_;
    record.business_fact_id = copy(receipt);
    record.business_kind = cause.kind;
    record.business_source_schema = 1;
    record.source_tick = cause.source_tick;
    record.observed_tick = common.tick();
    record.steps.reserve(steps);
    record.original_business_execution.emplace();
    auto &execution = *record.original_business_execution;
    execution.adapter_rule_ref = {StableId{adapter}, 1,
                                  copy(x.identity.content_digest)};
    execution.stages.reserve(stages.size());
    for (std::size_t i = 0; i < stages.size(); ++i) {
      const auto &s = *stages[i];
      execution.stages.push_back(*s.stage_);
      if (s.protected_step_) {
        record.steps.push_back(*s.protected_step_);
        record.steps.back().source_binding.emplace<MarketOriginalBusinessFifo>(
            MarketOriginalBusinessFifo{copy(receipt),
                                       static_cast<std::uint32_t>(i)});
      }
    }
    CheckpointVector<SelectedInventoryEvidenceBorrow> inventories;
    inventories.reserve(frames);
    CheckpointVector<MarketOriginalStageReadView> views;
    views.reserve(stages.size());
    for (std::size_t i = 0; i < stages.size(); ++i) {
      const auto &s = *stages[i];
      const auto &draft = common.resource_execution_drafts()[s.recipe_index_];
      const auto begin = inventories.size();
      for (const auto &before : s.before_)
        inventories.push_back({before.id, &before, after(draft, before)});
      views.push_back({execution.stages[i],
                       std::span<const SelectedInventoryEvidenceBorrow>{
                           inventories.data() + begin, s.before_.size()}});
    }
    take(reader.validate_original_fifo_business(record, views));
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("GrowthEconomicCaptureMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<const MarketMovementRecord *>
GrowthEconomicFifoMovementCapture::original_record(
    const CommonStore &common) const {
  if (!record_ || common_ != &common || !identity_ ||
      identity_ != common.resource_execution_capture_identity())
    return {false, nullptr,
            common_failure("GrowthEconomicCaptureIdentityMismatch").error};
  const auto *head = common.state().facts.find(record_->business_fact_id);
  if (!head || head->domain != "local_economy" ||
      head->kind != (record_->business_kind == MarketCauseKind::local_production
                         ? "local_production"
                         : "local_ration"))
    return {false, nullptr,
            common_failure("GrowthEconomicOriginalReceiptRequired",
                           record_->business_fact_id)
                .error};
  return {true, &*record_, {}};
}
} // namespace hanmo::resident
