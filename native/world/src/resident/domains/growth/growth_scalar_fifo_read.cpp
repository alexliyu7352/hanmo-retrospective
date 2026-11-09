// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 原FIFO顺序与核心selected数学各只有一份正文；钱包复用原纯校验，不再次收费。
#include "growth_scalar_fifo_read.hpp"
#include "growth_internal.hpp"
#include "growth_scalar_fifo.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <array>
namespace hanmo::resident {
namespace {
Status wallet(const TradeWalletPosting &posting, const StableId &id,
              const Owner &owner, Integer delta) {
  if (posting.wallet_id != id || !owner_equal(posting.owner, owner) ||
      posting.delta_qian != delta)
    return common_failure("GrowthScalarFifoOriginalWalletMismatch", id);
  // 仅按已有Posting有限字段构造原纯钱包读面；先计两份真实拥有字段及一份操作ID。


  auto memory = CheckpointMemoryCharge(0);
  WalletView before{id,
                    posting.owner,
                    posting.balance_before,
                    posting.reservations_before,
                    {}};
  WalletView after{
      id, posting.owner, posting.balance_after, posting.reservations_after, {}};
  WalletDelta operation{id, delta, true};
  auto available = read_wallet_spendable(before);
  if (!available.ok)
    return {false, {}, available.error};
  if (available.value != posting.spendable_before)
    return common_failure("GrowthScalarFifoSpendableMismatch", id);
  return validate_original_wallet_delta(operation, before, after);
}
} // namespace
Status validate_growth_scalar_fifo_business(
    const GrowthScalarMarketStartView &actual, const CommonCatalog &shared,
    const GrowthCatalog &catalog, const CommonResourceCodecContext &resources,
    std::span<const MarketOriginalStageReadView> stages) {
  const auto *movement = actual.movement;
  const auto *activity = actual.activity;
  if (!movement || !activity || !actual.started_at ||
      movement->business_kind != MarketCauseKind::growth_trade ||
      movement->business_source_schema != 1 ||
      !movement->original_business_execution ||
      activity->selected_trade_settlement || stages.size() != 1)
    return common_failure("GrowthScalarFifoOriginalStartRequired");
  const auto &execution = *movement->original_business_execution;
  const auto *terms = std::get_if<TradeTerms>(&activity->terms);
  const auto *shop = terms ? catalog.shops.find(terms->shop_id) : nullptr;
  const auto *stock =
      shop ? shared.required_inventories.find(shop->inventory_id) : nullptr;
  const auto *merchant =
      shop ? shared.required_wallets.find(shop->wallet_id) : nullptr;
  const auto *at = std::get_if<MarketScalarActualStart>(&execution.time_basis);
  if (!terms || !shop || !stock || !merchant || !at ||
      actual.started_at != &at->started_at || execution.stages.size() != 1 ||
      &stages[0].stage != &execution.stages[0] ||
      (terms->side != "buy" && terms->side != "sell"))
    return common_failure("GrowthScalarFifoOriginalTermsRequired",
                          movement->business_fact_id);
  const auto &stage = stages[0].stage;
  const auto frames = stages[0].inventories;
  if (stage.stage != MarketCaptureStage::trade || frames.size() != 2 ||
      stage.inventory_frames.size() != 2 || stage.item_operations.size() != 1 ||
      stage.wallet_postings.size() != 2)
    return common_failure("GrowthScalarFifoOriginalStageRequired",
                          movement->business_fact_id);
  const auto &op = stage.item_operations[0];
  if (op.operation_index != 0 ||
      !std::holds_alternative<MarketScalarTradeSlot>(op.source_slot) ||
      !std::holds_alternative<MarketNotConsumption>(op.material_need) ||
      !std::holds_alternative<MarketNotOutput>(op.output_basis))
    return common_failure("GrowthScalarFifoOriginalOperationRequired",
                          movement->business_fact_id);
  const auto &view = activity->view;
  const SelectedInventoryEvidenceBorrow *source = nullptr,
                                        *destination = nullptr;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    if (frames[i].inventory_id != stage.inventory_frames[i].inventory_id ||
        !frames[i].before || frames[i].before->id != frames[i].inventory_id ||
        frames[i].after.id != frames[i].inventory_id)
      return common_failure("GrowthScalarFifoOriginalFrameMismatch",
                            frames[i].inventory_id);
    if (frames[i].inventory_id == view.from_inventory_id)
      source = &frames[i];
    if (frames[i].inventory_id == view.to_inventory_id)
      destination = &frames[i];
  }
  if (!source || !destination || source == destination)
    return common_failure("GrowthScalarFifoOriginalFrameMismatch",
                          movement->business_fact_id);
  const bool buy = terms->side == "buy";
  const auto &market = buy ? *source->before : *destination->before;
  if (market.id != shop->inventory_id ||
      !owner_equal(market.owner, stock->owner) ||
      market.capacity != stock->capacity ||
      market.revision != terms->market_revision)
    return common_failure("GrowthScalarFifoOriginalMarketMismatch",
                          movement->business_fact_id);
  const auto *item = shared.items.find(terms->item_id);
  if (!item || item->base_price <= 0 || item->base_price > 2147483647)
    return common_failure("GrowthMarketHistoricalTradeBasisMismatch",
                          movement->business_fact_id);
  Integer quantity = 0;
  for (const auto &lot : market.lots)
    if (lot.item_id == terms->item_id && owner_equal(lot.owner, market.owner) &&
        lot.reserved_by.empty()) {
      if (lot.quantity < 0 || lot.quantity > INT64_MAX - quantity)
        return common_failure("GrowthMarketQuantityOverflow",
                              movement->business_fact_id);
      quantity += lot.quantity;
    }
  if (buy && quantity < terms->quantity)
    return common_failure("GrowthMarketHistoricalStockInsufficient",
                          movement->business_fact_id);
  const auto discount = terms->discount_bp.value_or(0);
  if (discount < 0 || discount > 800)
    return common_failure("GrowthMarketHistoricalDiscountInvalid",
                          movement->business_fact_id);
  // 只调用原唯一边际内核；高库存沿原历史reader的同一平台界限，非新增价则。
  if (growth::trade_price(
          item->base_price,
          std::min<Integer>(quantity, buy ? 200 + terms->quantity : 200),
          terms->quantity, buy, discount) != terms->total)
    return common_failure("GrowthMarketHistoricalTradePriceMismatch",
                          movement->business_fact_id);
  const Owner &seller = buy ? stock->owner : terms->owner;
  const Owner &buyer = buy ? terms->owner : stock->owner;
  auto valid = validate_growth_scalar_fifo_order(*source->before, *terms,
                                                 seller, op.selections);
  if (!valid.ok)
    return valid;
  // 临时视图壳用同job实际allocator；五个id全借原flow，不另造拥有型ResourceDraft。
  if (op.lot_flows.size() != op.selections.size())
    return common_failure("GrowthScalarFifoOriginalFlowCountMismatch",
                          movement->business_fact_id);
  CheckpointVector<SelectedLotFlowEvidenceBorrow> flows;
  flows.reserve(op.lot_flows.size());
  for (const auto &flow : op.lot_flows)
    flows.push_back({flow.kind, flow.source_inventory_id, flow.source_lot_id,
                     flow.destination_inventory_id, flow.destination_lot_id,
                     flow.item_id, flow.quantity});
  const ReservationRef unreserved;
  const SelectedTransferParametersBorrow transfer{
      source->inventory_id, destination->inventory_id,
      op.selections,        std::span<const Owner>{&seller, 1},
      unreserved,           &buyer};
  valid =
      validate_selected_resource_evidence(transfer, {{},
                                                     frames,
                                                     flows,
                                                     stage.next_instance_before,
                                                     stage.next_instance_after,
                                                     &resources});
  if (!valid.ok)
    return valid;
  valid = wallet(stage.wallet_postings[0], view.payer_wallet_id,
                 buy ? terms->owner : merchant->owner, -terms->total);
  if (!valid.ok)
    return valid;
  return wallet(stage.wallet_postings[1], view.payee_wallet_id,
                buy ? merchant->owner : terms->owner, terms->total);
}
} // namespace hanmo::resident
