// 原资源状态变换的唯一正文；调用者仍须先完成真实准入、权源与语义检查。
#pragma once
#include "resource_execution_draft.hpp"
#include <span>
namespace hanmo::resident {
// 同步借用七字段，不复制草稿；回调不得保留临时新lot标识的引用。
struct OriginalResourceFlowOutput {
  void *context = nullptr;
  void (*append)(void *, ResourceLotFlowDraft::Kind, const StableId &,
                 const StableId &, const StableId &, const StableId &,
                 const StableId &, Integer) = nullptr;
  void emit(ResourceLotFlowDraft::Kind kind, const StableId &source_inventory,
            const StableId &source_lot, const StableId &destination_inventory,
            const StableId &destination_lot, const StableId &item,
            Integer quantity) const;
};
// 只借已核选择的原批字段；不是产权、预约或物料资格证明。
struct OriginalConsumedLotBorrow {
  const StableId &lot_id;
  const StableId &item_id;
  Integer quantity_before = 0, quantity = 0;
};
// 保原选择序、消耗后未选集与一次revision递增；不分配ID。
Status apply_original_selected_consume_mutation(
    InventoryView &, std::span<const OriginalConsumedLotBorrow>, Integer next,
    const OriginalResourceFlowOutput &);
// 热原语借原已校验selected副本，避免另造一份字段或拥有型选择表。
Status apply_original_selected_consume_mutation(
    InventoryView &, std::span<const LotView>, std::span<const CargoSelection>,
    Integer next, const OriginalResourceFlowOutput &);
// 原add lambda机械提取；旧FIFO转移/预约继续传原flow种类和原来源，失败序不变。
Status apply_original_plain_lot_add(
    InventoryView &, const CommonCatalog *, const StableId &item,
    Integer quantity, const Owner &, const ReservationRef &, Integer &next,
    const OriginalResourceFlowOutput &, const StableId &source_inventory = {},
    const StableId &source_lot = {},
    ResourceLotFlowDraft::Kind = ResourceLotFlowDraft::Kind::add);
} // namespace hanmo::resident
