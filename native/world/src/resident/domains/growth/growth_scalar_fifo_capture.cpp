// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
#include "resident/domains/growth/growth_model.hpp"
// 只将原一次真钱货capture落为History2原source1证据；没有第二次资源结算。
#include "growth_scalar_fifo_capture.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
#include <limits>
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
// 构造新普通字符串只走已预留的fresh copy，避免空串赋值的另一capacity增长策略。
StableId copy(const StableId &value) {
  return checkpoint_fresh_string_copy(value);
}
// 只读差量顺序为原before的删除/改变，再原after的新行；未动完整值不复制到delta。
template <class Sink>
void differences(const InventoryView &before, const InventoryView &after,
                 Sink sink) {
  for (const auto &old : before.lots) {
    const LotView *next = nullptr;
    for (const auto &row : after.lots)
      if (row.id == old.id)
        next = &row;
    if (!next || !same_resource_lot_value(old, *next))
      sink(old.id, &old, next);
  }
  for (const auto &next : after.lots)
    if (std::none_of(before.lots.begin(), before.lots.end(),
                     [&](const auto &old) { return old.id == next.id; }))
      sink(next.id, nullptr, &next);
}
// 同一借用足迹驱动预留和真正构造，不拿已复制DTO反算费用。

MarketInventoryChange
build_change(const ResourceInventoryExecutionDraft &frame) {
  MarketInventoryChange result;
  result.inventory_id = copy(frame.inventory_id);
  result.revision_before = frame.before->revision;
  result.revision_after = frame.after.revision;
  std::size_t count = 0;
  differences(*frame.before, frame.after,
              [&](const auto &, const LotView *, const LotView *) { ++count; });
  result.lots.reserve(count);
  differences(*frame.before, frame.after,
              [&](const auto &id, const LotView *before, const LotView *after) {
                result.lots.emplace_back();
                auto &row = result.lots.back();
                row.lot_id = copy(id);
                if (before)
                  row.before.emplace(*before);
                if (after)
                  row.after.emplace(*after);
              });
  return result;
}

void build_wallet(TradeWalletPosting &result,
                  const ResourceWalletExecutionDraft &frame, Integer delta) {
  result.wallet_id = copy(frame.wallet_id);
  result.owner = Owner{frame.before->owner};
  result.delta_qian = delta;
  result.balance_before = frame.before->balance;
  result.balance_after = frame.after.balance;
  result.reservations_before = std::vector<Reservation>{frame.before->reserved};
  result.reservations_after = std::vector<Reservation>{frame.after.reserved};
  Integer held = 0;
  for (const auto &row : frame.before->reserved) {
    if (row.amount <= 0 || row.amount > frame.before->balance - held)
      fail("GrowthScalarFifoWalletReservationsInvalid", frame.wallet_id);
    held += row.amount;
  }
  result.spendable_before = frame.before->balance - held;
}
bool selected(const CatalogIdentity &catalog, std::string_view key,
              std::string_view version) {
  std::size_t found = 0;
  for (const auto &row : catalog.module_versions)
    if (row.first == key) {
      if (row.second != version)
        return false;
      ++found;
    }
  return found == 1;
}
// 真scope必须已选全部对应轴；原目录identity不能因几个新字段被自动升级。
void selection(const CommonStore &common, const GrowthCatalog &growth,
               const GrowthMarketHistoryCatalog &history,
               const GrowthExactStartContext &exact,
               const ActionCoreView &action) {
  const auto &catalog = exact.actor_foundation;
  if (history.selected_version != 2 || !growth.regional_life ||
      !growth.economy || !growth.complete_study_sources ||
      catalog.original_growth.get() != &growth ||
      catalog.identity.content_digest.empty() ||
      !selected(catalog.identity, "growth_market_history", "2") ||
      !selected(catalog.identity, "selected_resource_operations", "1") ||
      !selected(catalog.identity, "world_timeline", "2") ||
      !selected(catalog.identity, "action_elapsed", "2"))
    fail("GrowthScalarFifoSelectedWorldRequired", action.id);
  if (action.start_tick != common.tick() ||
      exact.actual_moment.half_day_tick != action.start_tick)
    fail("GrowthScalarFifoActualStartMismatch", action.id);
  take(encode_world_moment(exact.actual_moment));
}
// 专用市场仍是原dedicated容器及空预约；外端其它合法货权/预约原样保留。
void protected_frame(const ProtectedMarketAccounts &policy,
                     const ResourceInventoryExecutionDraft &frame) {
  const auto *required = policy.inventories.find(frame.inventory_id);
  if (!required)
    return;
  if (!frame.before || frame.before->capacity != required->capacity ||
      frame.after.capacity != required->capacity ||
      !owner_equal(frame.before->owner, required->owner) ||
      !owner_equal(frame.after.owner, required->owner))
    fail("GrowthScalarFifoProtectedContainerMismatch", frame.inventory_id);
  for (const auto *value : {&*frame.before, &frame.after})
    for (const auto &lot : value->lots)
      if (!owner_equal(lot.owner, required->owner) || !lot.reserved_by.empty())
        fail("GrowthScalarFifoProtectedLotMismatch", lot.id);
}
} // namespace
GrowthScalarFifoMovementCapture::GrowthScalarFifoMovementCapture(
    CheckpointMemoryCharge memory)
    : copied_fields_(std::move(memory)), record_(std::in_place) {}
Outcome<GrowthScalarFifoMovementCapture> capture_growth_scalar_fifo_movement(
    const CommonStore &common, const GrowthCatalog &growth,
    const GrowthMarketHistoryCatalog &history,
    const ProtectedMarketAccounts &protected_accounts,
    const ActionCoreView &action, const GrowthQuote &quote,
    const GrowthExactStartContext &exact, std::size_t index) {
  try {
    selection(common, growth, history, exact, action);
    const auto actual = take(
        read_growth_scalar_fifo_capture(common, growth, action, quote, index));
    const bool from = protected_accounts.inventories.find(
                          actual.source->inventory_id) != nullptr;
    const bool to = protected_accounts.inventories.find(
                        actual.destination->inventory_id) != nullptr;
    if (from == to)
      fail("GrowthScalarFifoDedicatedMarketRequired", action.id);
    protected_frame(protected_accounts, *actual.source);
    protected_frame(protected_accounts, *actual.destination);
    const auto &market = from ? *actual.source : *actual.destination;
    const auto &transfer = actual.execution->operations[0];
    const auto &terms = std::get<TradeTerms>(quote.data);
    constexpr std::string_view adapter = "growth_trade_original_fifo";


    for (const auto *frame : {actual.source, actual.destination}) {

      if (!protected_accounts.inventories.find(frame->inventory_id))
        (void)0;

    }


    GrowthScalarFifoMovementCapture result(CheckpointMemoryCharge(0));
    result.common_ = &common;
    result.identity_ = common.resource_execution_capture_identity();
    result.recipe_index_ = index;
    auto &record = *result.record_;
    record.business_fact_id = copy(action.id);
    record.business_source_schema = 1;
    record.business_kind = MarketCauseKind::growth_trade;
    record.source_tick = record.observed_tick = action.start_tick;
    record.steps.reserve(1);
    record.steps.emplace_back();
    auto &step = record.steps.back();
    step.stage = MarketCaptureStage::trade;
    step.next_instance_before = actual.execution->next_instance_before;
    step.next_instance_after = actual.execution->next_instance_after;
    step.inventories.reserve(1);
    step.inventories.emplace_back(build_change(market));
    step.source_binding.emplace<MarketOriginalBusinessFifo>(
        MarketOriginalBusinessFifo{copy(action.id), 0});
    record.original_business_execution.emplace();
    auto &execution = *record.original_business_execution;
    execution.adapter_rule_ref = {
        StableId{adapter}, 1,
        copy(exact.actor_foundation.identity.content_digest)};
    execution.time_basis.emplace<MarketScalarActualStart>(
        MarketScalarActualStart{exact.actual_moment, copy(action.id)});
    execution.stages.reserve(1);
    execution.stages.emplace_back();
    auto &stage = execution.stages.back();
    stage.stage = MarketCaptureStage::trade;
    stage.next_instance_before = step.next_instance_before;
    stage.next_instance_after = step.next_instance_after;
    stage.inventory_frames.reserve(2);
    for (const auto *original : {actual.source, actual.destination}) {
      stage.inventory_frames.emplace_back();
      auto &frame = stage.inventory_frames.back();
      frame.inventory_id = copy(original->inventory_id);
      if (!protected_accounts.inventories.find(original->inventory_id))
        frame.before_basis.emplace<MarketFullCapturedBefore>(
            MarketFullCapturedBefore{*original->before});
      frame.after_delta.emplace(build_change(*original));
    }
    stage.item_operations.reserve(1);
    stage.item_operations.emplace_back();
    auto &item = stage.item_operations.back();
    item.operation_index = 0;
    item.selections = std::vector<CargoSelection>{actual.transfer->selections};
    item.lot_flows.reserve(transfer.lot_flows.size());
    for (const auto &flow : transfer.lot_flows)
      item.lot_flows.push_back(
          {flow.kind, copy(flow.source_inventory_id), copy(flow.source_lot_id),
           copy(flow.destination_inventory_id), copy(flow.destination_lot_id),
           copy(flow.item_id), flow.quantity});
    stage.wallet_postings.resize(2);
    build_wallet(stage.wallet_postings[0], *actual.payer, -terms.total);
    build_wallet(stage.wallet_postings[1], *actual.payee, terms.total);
    return {true, std::move(result), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("GrowthScalarFifoCaptureMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<const MarketMovementRecord *>
GrowthScalarFifoMovementCapture::original_record(
    const CommonStore &common, const GrowthState &growth) const {
  if (!record_ || common_ != &common || !identity_ ||
      identity_ != common.resource_execution_capture_identity() ||
      recipe_index_ >= common.resource_execution_drafts().size())
    return {false, nullptr,
            common_failure("GrowthScalarFifoCaptureIdentityMismatch").error};
  const auto &record = *record_;
  const auto *head = common.state().facts.find(record.business_fact_id);
  const auto *source = growth.life_activities.find(record.business_fact_id);
  const auto *terms =
      source ? std::get_if<TradeTerms>(&source->terms) : nullptr;
  if (!head || head->id != record.business_fact_id ||
      head->domain != "growth" || head->kind != "life_activity" || !source ||
      !terms || source->view.fact_id != record.business_fact_id ||
      source->view.action_id != record.business_fact_id ||
      source->view.kind != LifeActivityKind::trade ||
      source->view.phase != LifeActivityPhase::active ||
      source->view.start_tick != record.source_tick ||
      terms->valid_tick != record.source_tick ||
      source->selected_trade_settlement || source->trade_terminal_time)
    return {false, nullptr,
            common_failure("GrowthScalarFifoOriginalStartRequired",
                           record.business_fact_id)
                .error};
  const auto &stage = record.original_business_execution->stages[0];
  const auto &actual = common.resource_execution_drafts()[recipe_index_];
  const auto *transfer =
      std::get_if<TransferSelectedLots>(&actual.recipe.operations[0]);
  const auto *action = common.action(record.business_fact_id);
  if (!action || action->kind != "trade" ||
      action->actor_id != source->view.actor_id ||
      source->view.activity != terms->side ||
      source->view.item_id != terms->item_id ||
      source->view.settlement_tick != record.source_tick || source->cost_paid ||
      source->wage_paid || !transfer || transfer->owners.size() != 1 ||
      !transfer->new_owner || (terms->side != "buy" && terms->side != "sell") ||
      !owner_equal(terms->owner, terms->side == "buy" ? *transfer->new_owner
                                                      : transfer->owners[0]))
    return {false, nullptr,
            common_failure("GrowthScalarFifoOriginalTermsMismatch",
                           record.business_fact_id)
                .error};
  Integer quantity = 0;
  for (const auto &flow : stage.item_operations[0].lot_flows) {
    if (flow.item_id != terms->item_id || flow.quantity <= 0 ||
        flow.quantity > terms->quantity - quantity)
      return {false, nullptr,
              common_failure("GrowthScalarFifoOriginalQuantityMismatch",
                             record.business_fact_id)
                  .error};
    quantity += flow.quantity;
  }
  if (quantity != terms->quantity)
    return {false, nullptr,
            common_failure("GrowthScalarFifoOriginalQuantityMismatch",
                           record.business_fact_id)
                .error};
  if (!transfer || actual.next_instance_before != stage.next_instance_before ||
      actual.next_instance_after != stage.next_instance_after ||
      source->view.from_inventory_id != transfer->source_id ||
      source->view.to_inventory_id != transfer->destination_id ||
      source->view.payer_wallet_id != stage.wallet_postings[0].wallet_id ||
      source->view.payee_wallet_id != stage.wallet_postings[1].wallet_id ||
      terms->total != stage.wallet_postings[1].delta_qian ||
      source->view.amount != terms->total ||
      source->view.quantity != terms->quantity)
    return {false, nullptr,
            common_failure("GrowthScalarFifoOriginalSourceMismatch",
                           record.business_fact_id)
                .error};
  return {true, &record, {}};
}
} // namespace hanmo::resident
