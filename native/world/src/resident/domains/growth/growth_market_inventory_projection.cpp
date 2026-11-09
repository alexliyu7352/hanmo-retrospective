// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 稀疏差量仅能替换其完整原前驱；先预留确切最终拥有足迹，禁止copy全库后再补预算。
#include "growth_market_inventory_projection.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 返回借用地址，不创建第二个lot索引或字段副本。
const MarketLotChange *change(const MarketInventoryChange &delta,
                              const StableId &id) {
  for (const auto &row : delta.lots)
    if (row.lot_id == id)
      return &row;
  return nullptr;
}
const LotView *original(const InventoryView &before, const StableId &id) {
  for (const auto &row : before.lots)
    if (row.id == id)
      return &row;
  return nullptr;
}
// 两遍相同选择：第一遍算目标容量/字段租约，第二遍实际fresh-copy。
template <class Sink>
void projected(const InventoryView &before, const MarketInventoryChange &delta,
               Sink sink) {
  for (const auto &row : before.lots) {
    const auto *diff = change(delta, row.id);
    if (!diff)
      sink(row);
    else if (diff->after)
      sink(*diff->after);
  }
  for (const auto &diff : delta.lots)
    if (!diff.before)
      sink(*diff.after);
}
Status predecessor(const InventoryView &before,
                   const MarketInventoryChange &delta,
                   const CommonResourceCodecContext &resources) {
  if (before.id.empty() || delta.inventory_id != before.id ||
      before.revision < 0 || delta.revision_before != before.revision ||
      delta.revision_after <= delta.revision_before || delta.lots.empty() ||
      before.lots.size() > CodecVisitor::max_items ||
      delta.lots.size() > CodecVisitor::max_items)
    return common_failure("MarketInventoryProjectionBoundaryMismatch",
                          before.id);
  for (std::size_t i = 0; i < before.lots.size(); ++i) {
    const auto &lot = before.lots[i];
    if (lot.id.empty() || lot.quantity <= 0)
      return common_failure("MarketInventoryProjectionOriginalInvalid", lot.id);
    for (std::size_t j = 0; j < i; ++j)
      if (before.lots[j].id == lot.id)
        return common_failure("MarketInventoryProjectionOriginalRepeated",
                              lot.id);
  }
  std::size_t changed_ordinal = 0;
  bool new_phase = false;
  for (std::size_t i = 0; i < delta.lots.size(); ++i) {
    const auto &row = delta.lots[i];
    if (row.lot_id.empty() || (!row.before && !row.after) ||
        (row.before &&
         (row.before->id != row.lot_id || row.before->quantity <= 0)) ||
        (row.after &&
         (row.after->id != row.lot_id || row.after->quantity <= 0)))
      return common_failure("MarketInventoryProjectionDeltaInvalid",
                            row.lot_id);
    for (std::size_t j = 0; j < i; ++j)
      if (delta.lots[j].lot_id == row.lot_id)
        return common_failure("MarketInventoryProjectionDeltaRepeated",
                              row.lot_id);
    const auto *old = original(before, row.lot_id);
    if (row.before) {
      if (new_phase || !old ||
          !same_resource_lot_value_cold(*old, *row.before, resources) ||
          (row.after &&
           same_resource_lot_value_cold(*row.before, *row.after, resources)))
        return common_failure("MarketInventoryProjectionPredecessorMismatch",
                              row.lot_id);
      while (changed_ordinal < before.lots.size() &&
             before.lots[changed_ordinal].id != row.lot_id)
        ++changed_ordinal;
      if (changed_ordinal == before.lots.size())
        return common_failure("MarketInventoryProjectionDeltaOrderMismatch",
                              row.lot_id);
      ++changed_ordinal;
    } else {
      new_phase = true;
      if (old)
        return common_failure("MarketInventoryProjectionLotCollision",
                              row.lot_id);
    }
  }
  return common_success();
}
} // namespace
Outcome<GrowthMarketInventoryProjection> derive_growth_market_inventory_after(
    const InventoryView &before, const MarketInventoryChange &delta,
    const CommonResourceCodecContext &resources) {
  auto valid = predecessor(before, delta, resources);
  if (!valid.ok)
    return {false, {}, valid.error};
  try {


    std::size_t count = 0;
    projected(before, delta, [&](const LotView &lot) {
      ++count;

    });
    if (count > CodecVisitor::max_items)
      return {false,
              {},
              common_failure("CheckpointCollectionTooLarge", before.id).error};

    GrowthMarketInventoryProjection result(CheckpointMemoryCharge(0));
    auto &after = *result.value_;
    after.id = checkpoint_fresh_string_copy(before.id);
    after.owner = Owner{before.owner};
    after.capacity = before.capacity;
    after.revision = delta.revision_after;
    after.field_order = FieldOrder{before.field_order};
    after.lots.reserve(count);
    projected(before, delta,
              [&](const LotView &lot) { after.lots.push_back(lot); });
    return {true, std::move(result), {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("MarketInventoryProjectionMemoryExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace hanmo::resident
