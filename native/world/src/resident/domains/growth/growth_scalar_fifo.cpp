// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
#include "resident/domains/growth/growth_model.hpp"
// selected只替换原FIFO执行原语，不改变标量贸易的选择政策或开工付款顺序。
#include "growth_scalar_fifo.hpp"
#include "growth_market_history.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
// 失败保原错误框架，外层整候选负责撤销已发生的真实recipe。
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
// 这些地址只借同一原quote及目录，不复制货权或临时世界。
struct TermsView {
  const TradeTerms &terms;
  const InventoryView &source;
  const InventoryView &destination;
  const Owner &seller;
  const Owner &buyer;
  const StableId &payer;
  const StableId &payee;
};
TermsView terms(const CommonStore &common, const GrowthCatalog &catalog,
                const ActionCoreView &action, const GrowthQuote &quote) {
  const auto *trade = std::get_if<TradeTerms>(&quote.data);
  const auto *actor = common.actor(action.actor_id);
  const auto *original = common.action(action.id);
  const auto *shop = trade ? catalog.shops.find(trade->shop_id) : nullptr;
  if (!catalog.regional_life || !catalog.economy || !trade || !actor ||
      !original || !shop || action.kind != "trade" ||
      original->kind != "trade" || original->actor_id != action.actor_id ||
      original->start_tick != action.start_tick ||
      quote.command.kind != "trade" || quote.duration != trade_action_ticks ||
      trade->quantity < 1 || trade->quantity > 500 || trade->total < 0 ||
      (trade->side != "buy" && trade->side != "sell") ||
      trade->valid_tick != action.start_tick ||
      std::find(catalog.tradeable_items.begin(), catalog.tradeable_items.end(),
                trade->item_id) == catalog.tradeable_items.end())
    fail("GrowthScalarFifoOriginalTermsRequired", action.id);
  const auto *stock = common.inventory(shop->inventory_id);
  const auto *bag = common.inventory(actor->inventory_id);
  const bool buy = trade->side == "buy";
  const auto &payer = buy ? trade->payer_wallet_id : shop->wallet_id;
  const auto &payee = buy ? shop->wallet_id : trade->payer_wallet_id;
  if (!stock || !bag || stock->id == bag->id || payer == payee ||
      !common.wallet(payer) || !common.wallet(payee))
    fail("GrowthScalarFifoOriginalAssetsRequired", action.id);
  return {*trade,
          buy ? *stock : *bag,
          buy ? *bag : *stock,
          buy ? stock->owner : trade->owner,
          buy ? trade->owner : stock->owner,
          payer,
          payee};
}
// 同一纯扫描用于热recipe和实际capture复核；没有品质、新鲜度或普通批优先过滤。
template <class Sink>
void fifo(const InventoryView &source, const TradeTerms &trade,
          const Owner &seller, Sink append) {
  Integer remaining = trade.quantity;
  for (const auto &lot : source.lots) {
    if (lot.item_id != trade.item_id || !owner_equal(lot.owner, seller) ||
        !lot.reserved_by.empty())
      continue;
    if (lot.quantity <= 0 || lot.id.empty())
      fail("GrowthScalarFifoInvalidLot", lot.id);
    const auto amount = std::min(remaining, lot.quantity);
    // 独特件若被原可交易物品选中，只能同ID整件转移，不能拆成普通批。
    if (lot.item_instance_id &&
        (lot.quantity != 1 || amount != 1 || lot.lot_properties_ref))
      fail("GrowthScalarFifoUniqueWholeRequired", lot.id);
    append(lot, amount);
    remaining -= amount;
    if (!remaining)
      break;
  }
  if (remaining)
    fail("GrowthScalarFifoInsufficientGoods", source.id);
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
} // namespace
Outcome<GrowthScalarFifoRecipe> prepare_growth_scalar_fifo_recipe(
    const CommonStore &common, const GrowthCatalog &catalog,
    const ActionCoreView &action, const GrowthQuote &quote) {
  try {
    const auto value = terms(common, catalog, action, quote);
    const auto &market =
        value.terms.side == "buy" ? value.source : value.destination;
    if (market.revision != value.terms.market_revision)
      fail("GrowthScalarFifoOriginalMarketRevisionMismatch", action.id);


    std::size_t count = 0;
    fifo(value.source, value.terms, value.seller,
         [&](const LotView &lot, Integer) {
           ++count;


         });


    GrowthScalarFifoRecipe prepared(CheckpointMemoryCharge(0));
    auto &result = prepared.recipe_.emplace();
    TransferSelectedLots transfer;
    transfer.source_id = value.source.id;
    transfer.destination_id = value.destination.id;
    transfer.owners.push_back(value.seller);
    transfer.new_owner = value.buyer;
    transfer.selections.reserve(count);
    fifo(value.source, value.terms, value.seller,
         [&](const LotView &lot, Integer amount) {
           transfer.selections.push_back({value.source.id, lot.id, amount});
         });
    result.operations.reserve(3);
    result.operations.emplace_back(std::move(transfer));
    take(resource_recipe::append_money_transfer(
        result, value.payer, value.payee, value.terms.total,
        resource_recipe::MoneyTransferPolicy::spendable));
    result.expected_versions.reserve(4);
    const auto append_version = [&](ResourceKey::Kind kind,
                                    const StableId &id) {
      // 原read_version在栈参数及版本map查找时各复制一次key，临时租约只随本调用存活。


      auto temporary_fields = CheckpointMemoryCharge(0);
      result.expected_versions.push_back(common.read_version({kind, id}));
    };
    append_version(ResourceKey::Kind::inventory, value.source.id);
    append_version(ResourceKey::Kind::inventory, value.destination.id);
    append_version(ResourceKey::Kind::wallet, value.payer);
    append_version(ResourceKey::Kind::wallet, value.payee);
    return {true, std::move(prepared), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Status
validate_growth_scalar_fifo_order(const InventoryView &before,
                                  const TradeTerms &trade, const Owner &seller,
                                  std::span<const CargoSelection> selections) {
  try {
    if (trade.quantity < 1 || trade.quantity > 500 || trade.item_id.empty())
      fail("GrowthScalarFifoOriginalTermsRequired", before.id);
    std::size_t index = 0;
    fifo(before, trade, seller, [&](const LotView &lot, Integer amount) {
      if (index >= selections.size() ||
          selections[index].inventory_id != before.id ||
          selections[index].lot_id != lot.id ||
          selections[index].quantity != amount)
        fail("GrowthScalarFifoSelectionMismatch", before.id);
      ++index;
    });
    if (index != selections.size())
      fail("GrowthScalarFifoSelectionMismatch", before.id);
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Status inspect_growth_scalar_fifo_market_recipe(
    const CommonStore &common, const GrowthCatalog &catalog,
    const GrowthMarketHistoryCatalog &history,
    const ProtectedMarketAccounts &accounts, const ActionCoreView &action,
    const GrowthQuote &quote, const ResourceRecipe &recipe) {
  try {
    const auto value = terms(common, catalog, action, quote);
    const auto &market =
        value.terms.side == "buy" ? value.source : value.destination;
    const auto *dedicated = accounts.inventories.find(market.id);
    if (history.selected_version != 2 || !dedicated ||
        std::find(history.shop_ids.begin(), history.shop_ids.end(),
                  value.terms.shop_id) == history.shop_ids.end() ||
        (accounts.inventories.find(value.source.id) != nullptr) ==
            (accounts.inventories.find(value.destination.id) != nullptr) ||
        !owner_equal(market.owner, dedicated->owner) ||
        market.capacity != dedicated->capacity ||
        market.revision != value.terms.market_revision)
      fail("GrowthScalarFifoProtectedScopeRequired", action.id);
    if (recipe.operations.size() != 3 || recipe.expected_versions.size() != 4)
      fail("GrowthScalarFifoOriginalRecipeRequired", action.id);
    const auto *transfer =
        std::get_if<TransferSelectedLots>(&recipe.operations[0]);
    const auto *debit = std::get_if<WalletDelta>(&recipe.operations[1]);
    const auto *credit = std::get_if<WalletDelta>(&recipe.operations[2]);
    if (!transfer || transfer->source_id != value.source.id ||
        transfer->destination_id != value.destination.id ||
        transfer->owners.size() != 1 ||
        !owner_equal(transfer->owners[0], value.seller) ||
        !transfer->new_owner ||
        !owner_equal(*transfer->new_owner, value.buyer) ||
        transfer->reservation != ReservationRef{} || !debit || !credit ||
        debit->wallet_id != value.payer || credit->wallet_id != value.payee ||
        debit->delta != -value.terms.total ||
        credit->delta != value.terms.total || !debit->require_spendable ||
        !credit->require_spendable)
      fail("GrowthScalarFifoOriginalRecipeMismatch", action.id);
    const StableId *ids[] = {&value.source.id, &value.destination.id,
                             &value.payer, &value.payee};
    for (std::size_t i = 0; i < 4; ++i)
      if (recipe.expected_versions[i].resource.kind !=
              (i < 2 ? ResourceKey::Kind::inventory
                     : ResourceKey::Kind::wallet) ||
          recipe.expected_versions[i].resource.id != *ids[i])
        fail("GrowthScalarFifoOriginalVersionSetMismatch", action.id);
    // 版本数值仍由Common唯一apply核；本门不自行回写或代替读版本检查。
    std::size_t ordinal = 0;
    fifo(value.source, value.terms, value.seller,
         [&](const LotView &lot, Integer quantity) {
           if (ordinal >= transfer->selections.size())
             fail("GrowthScalarFifoSelectionMismatch", action.id);
           const auto &pick = transfer->selections[ordinal++];
           if (pick.inventory_id != value.source.id || pick.lot_id != lot.id ||
               pick.quantity != quantity)
             fail("GrowthScalarFifoSelectionMismatch", action.id);
         });
    if (ordinal != transfer->selections.size())
      fail("GrowthScalarFifoSelectionMismatch", action.id);
    for (const auto &lot : market.lots)
      if (!owner_equal(lot.owner, dedicated->owner) || !lot.reserved_by.empty())
        fail("GrowthScalarFifoProtectedLotMismatch", lot.id);
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<GrowthScalarFifoCaptureView> read_growth_scalar_fifo_capture(
    const CommonStore &common, const GrowthCatalog &catalog,
    const ActionCoreView &action, const GrowthQuote &quote, std::size_t index) {
  try {
    const auto value = terms(common, catalog, action, quote);
    const auto &drafts = common.resource_execution_drafts();
    if (!common.resource_execution_capture_identity() ||
        index >= drafts.size() || index != drafts.size() - 1)
      fail("GrowthScalarFifoActualCaptureRequired", action.id);
    const auto &draft = drafts[index];
    if (!same_identity(draft.identity, common.state().identity) ||
        draft.recipe.operations.size() != 3 || draft.operations.size() != 3 ||
        draft.next_instance_after != common.state().next_instance_id ||
        draft.next_instance_before !=
            draft.operations.front().next_instance_before ||
        draft.next_instance_after !=
            draft.operations.back().next_instance_after)
      fail("GrowthScalarFifoActualRecipeMismatch", action.id);
    for (std::size_t n = 0; n < draft.operations.size(); ++n)
      if (draft.operations[n].operation_index != n ||
          (n && draft.operations[n - 1].next_instance_after !=
                    draft.operations[n].next_instance_before))
        fail("GrowthScalarFifoActualOperationOrderMismatch", action.id);
    const auto *transfer =
        std::get_if<TransferSelectedLots>(&draft.recipe.operations[0]);
    if (!transfer || transfer->source_id != value.source.id ||
        transfer->destination_id != value.destination.id ||
        transfer->owners.size() != 1 ||
        !owner_equal(transfer->owners.front(), value.seller) ||
        !transfer->new_owner ||
        !owner_equal(*transfer->new_owner, value.buyer) ||
        !transfer->reservation.empty())
      fail("GrowthScalarFifoActualTransferMismatch", action.id);
    take(validate_selected_resource_execution(draft.recipe.operations[0],
                                              draft.operations[0]));
    const ResourceInventoryExecutionDraft *from = nullptr;
    const ResourceInventoryExecutionDraft *to = nullptr;
    for (const auto &frame : draft.operations[0].inventories) {
      if (frame.inventory_id == transfer->source_id)
        from = &frame;
      else if (frame.inventory_id == transfer->destination_id)
        to = &frame;
      else
        fail("GrowthScalarFifoUnexpectedInventory", frame.inventory_id);
    }
    if (!from || !to || !from->before || !to->before ||
        !same_original_inventory_frame(from->after, value.source) ||
        !same_original_inventory_frame(to->after, value.destination))
      fail("GrowthScalarFifoCaptureAfterMismatch", action.id);
    std::size_t selected = 0;
    fifo(*from->before, value.terms, value.seller,
         [&](const LotView &lot, Integer amount) {
           if (selected >= transfer->selections.size())
             fail("GrowthScalarFifoSelectionMismatch", action.id);
           const auto &pick = transfer->selections[selected++];
           if (pick.inventory_id != from->inventory_id ||
               pick.lot_id != lot.id || pick.quantity != amount)
             fail("GrowthScalarFifoSelectionMismatch", action.id);
         });
    if (selected != transfer->selections.size())
      fail("GrowthScalarFifoSelectionMismatch", action.id);
    const auto *debit = std::get_if<WalletDelta>(&draft.recipe.operations[1]);
    const auto *credit = std::get_if<WalletDelta>(&draft.recipe.operations[2]);
    if (!debit || !credit || debit->wallet_id != value.payer ||
        credit->wallet_id != value.payee ||
        debit->delta != -value.terms.total ||
        credit->delta != value.terms.total || !debit->require_spendable ||
        !credit->require_spendable)
      fail("GrowthScalarFifoActualMoneyMismatch", action.id);
    const auto *payer =
        take(read_original_wallet_delta(*debit, draft.operations[1]));
    const auto *payee =
        take(read_original_wallet_delta(*credit, draft.operations[2]));
    if (!same_original_wallet_frame(payer->after,
                                    *common.wallet(value.payer)) ||
        !same_original_wallet_frame(payee->after, *common.wallet(value.payee)))
      fail("GrowthScalarFifoWalletAfterMismatch", action.id);
    return {true, {&draft, transfer, from, to, payer, payee}, {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
