// 热Common和显式冷历史派生共用原变换；本叶不触及World或发布状态。
#include "original_inventory_mutation.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
void OriginalResourceFlowOutput::emit(ResourceLotFlowDraft::Kind kind,
                                      const StableId &source_inventory,
                                      const StableId &source_lot,
                                      const StableId &destination_inventory,
                                      const StableId &destination_lot,
                                      const StableId &item,
                                      Integer quantity) const {
  if (append)
    append(context, kind, source_inventory, source_lot, destination_inventory,
           destination_lot, item, quantity);
}
namespace {
// 热选择已完整验证；冷借用也必须匹配当前阶段前驱，不能越界减批或重复消耗。
template <class Read>
Status consume(InventoryView &source, std::size_t count, Read read,
               Integer next, const OriginalResourceFlowOutput &output) {
  for (std::size_t n = 0; n < count; ++n) {
    const auto before = read(n);
    const auto at = std::find_if(
        source.lots.begin(), source.lots.end(),
        [&](const LotView &lot) { return lot.id == before.lot_id; });
    if (at == source.lots.end() || at->item_id != before.item_id ||
        at->quantity != before.quantity_before || before.quantity <= 0 ||
        before.quantity > before.quantity_before)
      return common_failure("StaleSelectedLot", before.lot_id);
    for (std::size_t j = 0; j < n; ++j)
      if (read(j).lot_id == before.lot_id)
        return common_failure("InvalidCargoSelection", before.lot_id);
  }
  if (count > static_cast<std::size_t>(INT64_MAX) || source.revision < 0 ||
      source.revision > INT64_MAX - 1 || next < 1)
    return common_failure("ProjectionOverflow", source.id);
  for (std::size_t n = 0; n < count; ++n) {
    const auto before = read(n);
    auto at = std::find_if(
        source.lots.begin(), source.lots.end(),
        [&](const LotView &lot) { return lot.id == before.lot_id; });
    if (before.quantity == before.quantity_before)
      source.lots.erase(at);
    else
      at->quantity -= before.quantity;
    output.emit(ResourceLotFlowDraft::Kind::selected_consume, source.id,
                before.lot_id, {}, {}, before.item_id, before.quantity);
  }
  ++source.revision;
  return common_success();
}
} // namespace
Status apply_original_selected_consume_mutation(
    InventoryView &source, std::span<const OriginalConsumedLotBorrow> selected,
    Integer next, const OriginalResourceFlowOutput &output) {
  return consume(
      source, selected.size(), [&](std::size_t n) { return selected[n]; }, next,
      output);
}
Status apply_original_selected_consume_mutation(
    InventoryView &source, std::span<const LotView> selected,
    std::span<const CargoSelection> selections, Integer next,
    const OriginalResourceFlowOutput &output) {
  if (selected.size() != selections.size())
    return common_failure("InvalidCargoSelection", source.id);
  return consume(
      source, selected.size(),
      [&](std::size_t n) {
        return OriginalConsumedLotBorrow{selected[n].id, selected[n].item_id,
                                         selected[n].quantity,
                                         selections[n].quantity};
      },
      next, output);
}
Status apply_original_plain_lot_add(
    InventoryView &inv, const CommonCatalog *catalog, const StableId &item,
    Integer q, const Owner &owner, const ReservationRef &reserved,
    Integer &next, const OriginalResourceFlowOutput &output,
    const StableId &source_inventory, const StableId &source_lot,
    ResourceLotFlowDraft::Kind kind) {
  if (q <= 0)
    return common_success();
  if (!catalog || !catalog->items.find(item))
    return common_failure("UnknownItem", item);
  if (next < 1 || next == INT64_MAX)
    return common_failure("InstanceIdOverflow");
  // 同一原序号只在这里递增；固定栈字符避免冷派生出现未计划的中间字符串扩容。
  char id[32] = {'l', 'o', 't', ':'};
  const auto digits = std::to_chars(id + 4, id + sizeof(id), next++);
  const StableId created(id, digits.ptr);
  inv.lots.push_back({created, item, reserved, q, owner, {}});
  output.emit(kind, source_inventory, source_lot, inv.id, created, item, q);
  if (inv.revision == INT64_MAX)
    return common_failure("ProjectionOverflow");
  ++inv.revision;
  return common_success();
}
} // namespace hanmo::resident
