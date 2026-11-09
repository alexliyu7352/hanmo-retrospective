// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 冷阶段只复制本笔完整足迹，预收费后派生；未选批与只读库都必须逐字段相等。
#include "growth_economic_fifo_resource_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/original_inventory_mutation.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
void require(Status s) {
  if (!s.ok)
    throw Failure{std::move(s.error)};
}
Integer lot_ordinal(const StableId &id) {
  if (id.size() < 5 || id.compare(0, 4, "lot:") || id[4] < '1' || id[4] > '9')
    fail("GrowthEconomicOriginalLotIdentityRequired", id);
  Integer n = 0;
  auto parsed = std::from_chars(id.data() + 4, id.data() + id.size(), n);
  if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || n < 1)
    fail("GrowthEconomicOriginalLotIdentityRequired", id);
  return n;
}
const LotView *lot(const InventoryView &inventory, const StableId &id) {
  const LotView *found = nullptr;
  for (const auto &value : inventory.lots)
    if (value.id == id) {
      if (found)
        fail("GrowthEconomicOriginalLotRepeated", id);
      found = &value;
    }
  return found;
}
const SelectedInventoryEvidenceBorrow &
frame(const MarketOriginalStageReadView &stage, const StableId &id) {
  const SelectedInventoryEvidenceBorrow *found = nullptr;
  for (const auto &value : stage.inventories)
    if (value.inventory_id == id) {
      if (found)
        fail("GrowthEconomicOriginalFrameRepeated", id);
      found = &value;
    }
  if (!found || !found->before || found->before->id != id ||
      found->after.id != id)
    fail("GrowthEconomicOriginalFrameRequired", id);
  return *found;
}
std::size_t additions(const MarketOriginalStageReadView &stage,
                      const StableId &id) {
  std::size_t count = 0;
  for (const auto &op : stage.stage.item_operations)
    if (std::holds_alternative<MarketOriginalPlainAdd>(op.output_basis)) {
      if (op.lot_flows.size() != 1)
        fail("GrowthEconomicOriginalPlainAddRequired", id);
      if (op.lot_flows[0].destination_inventory_id == id) {
        if (count == SIZE_MAX)
          throw std::bad_alloc();
        ++count;
      }
    }
  return count;
}
// 首成员在任何深值和vector构造前取得租约；不会先copy后补收费。
struct StageProjection {
  CheckpointMemoryCharge copied_fields;
  CheckpointVector<InventoryView> inventories;
  explicit StageProjection(CheckpointMemoryCharge memory)
      : copied_fields(std::move(memory)) {}
  InventoryView &inventory(const StableId &id) {
    for (auto &row : inventories)
      if (row.id == id)
        return row;
    fail("GrowthEconomicOperationOutsideReadFootprint", id);
  }
};
// 回调只对照原七字段，不拥有新flow或把新lot临时ID引用留到调用栈之外。
struct FlowCheck {
  std::span<const MarketOriginalLotFlow> expected;
  std::size_t cursor = 0;
  bool valid = true;
  static void append(void *opaque, ResourceLotFlowDraft::Kind kind,
                     const StableId &si, const StableId &sl, const StableId &di,
                     const StableId &dl, const StableId &item,
                     Integer quantity) {
    auto &self = *static_cast<FlowCheck *>(opaque);
    if (self.cursor >= self.expected.size()) {
      self.valid = false;
      return;
    }
    const auto &f = self.expected[self.cursor++];
    self.valid &= f.kind == kind && f.source_inventory_id == si &&
                  f.source_lot_id == sl && f.destination_inventory_id == di &&
                  f.destination_lot_id == dl && f.item_id == item &&
                  f.quantity == quantity;
  }
  OriginalResourceFlowOutput output() { return {this, append}; }
  bool complete() const { return valid && cursor == expected.size(); }
};
void stage(const CommonCatalog &catalog,
           const CommonResourceCodecContext &resources,
           const MarketOriginalStageReadView &actual) {
  const auto &s = actual.stage;
  if (s.stage == MarketCaptureStage::trade || !s.wallet_postings.empty() ||
      s.next_instance_before < 1 ||
      s.next_instance_after < s.next_instance_before ||
      actual.inventories.size() != s.inventory_frames.size())
    fail("GrowthEconomicOriginalResourceStageRequired");

  for (std::size_t i = 0; i < actual.inventories.size(); ++i) {
    const auto &f = actual.inventories[i];
    const auto &original = frame(actual, f.inventory_id);
    if (&f != &original || s.inventory_frames[i].inventory_id != f.inventory_id)
      fail("GrowthEconomicOriginalFrameOrderMismatch", f.inventory_id);
    const auto &before = *f.before;
    const auto added = additions(actual, before.id);
    if (added > SIZE_MAX - before.lots.size())
      throw std::bad_alloc();


    for (const auto &value : before.lots) {
      if (lot_ordinal(value.id) >= s.next_instance_before ||
          value.quantity <= 0)
        fail("GrowthEconomicOriginalAllocatorBoundaryMismatch", value.id);

      // 同一本笔完整读取足迹不能有重复批ID；外部世界仍归原Common全局门。
      for (std::size_t j = 0; j <= i; ++j)
        for (const auto &other : actual.inventories[j].before->lots)
          if (&other != &value && other.id == value.id)
            fail("GrowthEconomicOriginalLotRepeated", value.id);
    }
  }
  Integer planned_next = s.next_instance_before;
  for (const auto &op : s.item_operations)
    if (std::holds_alternative<MarketOriginalPlainAdd>(op.output_basis)) {
      if (op.lot_flows.size() != 1)
        fail("GrowthEconomicOriginalPlainAddRequired");
      const auto &f = op.lot_flows[0];
      // 先核真实分配序号再按字符串长度收费，畸形短ID不能压低即将构造的created预算。
      if (planned_next == INT64_MAX || f.quantity <= 0 ||
          lot_ordinal(f.destination_lot_id) != planned_next)
        fail("GrowthEconomicOriginalAllocatorMismatch", f.destination_lot_id);
      ++planned_next;
      const auto &destination = frame(actual, f.destination_inventory_id);
      const auto *added = lot(destination.after, f.destination_lot_id);
      if (!added)
        fail("GrowthEconomicOriginalOutputMissing", f.destination_lot_id);
      // 新lot的真实目标字符串及helper的栈期created字符串分别收费；无FieldOrder副本。
      (void)0;


    }
  if (planned_next != s.next_instance_after)
    fail("GrowthEconomicOriginalAllocatorMismatch");
  StageProjection projected(CheckpointMemoryCharge(0));
  projected.inventories.reserve(actual.inventories.size());
  for (const auto &f : actual.inventories) {
    const auto &before = *f.before;
    projected.inventories.emplace_back();
    auto &copy = projected.inventories.back();
    copy.id = checkpoint_fresh_string_copy(before.id);
    copy.owner = Owner{before.owner};
    copy.capacity = before.capacity;
    copy.revision = before.revision;
    copy.field_order = FieldOrder{before.field_order};
    copy.lots.reserve(before.lots.size() + additions(actual, before.id));
    for (const auto &value : before.lots)
      copy.lots.push_back(value);
  }
  Integer next = s.next_instance_before;
  for (std::size_t index = 0; index < s.item_operations.size(); ++index) {
    const auto &op = s.item_operations[index];
    if (op.operation_index != index)
      fail("GrowthEconomicOriginalOperationOrderMismatch");
    FlowCheck flows{op.lot_flows};
    if (std::holds_alternative<MarketOriginalPlainAdd>(op.output_basis)) {
      if (!op.selections.empty() || op.lot_flows.size() != 1 ||
          !std::holds_alternative<MarketNotConsumption>(op.material_need))
        fail("GrowthEconomicOriginalPlainAddRequired");
      const auto &f = op.lot_flows[0];
      auto &destination = projected.inventory(f.destination_inventory_id);
      const auto *added =
          lot(frame(actual, destination.id).after, f.destination_lot_id);
      if (f.quantity <= 0 || !added || added->item_id != f.item_id ||
          added->quantity != f.quantity || added->item_instance_id ||
          added->lot_properties_ref || added->extra || added->owner.extra ||
          added->reserved_by != ReservationRef{})
        fail("GrowthEconomicOriginalPlainAddRequired", f.destination_lot_id);
      for (const auto &inventory : projected.inventories)
        if (lot(inventory, f.destination_lot_id))
          fail("DuplicateLotId", f.destination_lot_id);
      require(apply_original_plain_lot_add(destination, &catalog, f.item_id,
                                           f.quantity, added->owner, {}, next,
                                           flows.output()));
    } else {
      if (!std::holds_alternative<MarketNotOutput>(op.output_basis) ||
          !std::holds_alternative<MarketOriginalMaterialNeed>(
              op.material_need) ||
          op.selections.empty() || op.selections.size() != op.lot_flows.size())
        fail("GrowthEconomicOriginalSelectedConsumeRequired");
      auto &source = projected.inventory(op.selections[0].inventory_id);
      CheckpointVector<OriginalConsumedLotBorrow> selected;
      selected.reserve(op.selections.size());
      for (std::size_t n = 0; n < op.selections.size(); ++n) {
        const auto &choice = op.selections[n];
        const auto &flow = op.lot_flows[n];
        const auto *before = lot(source, choice.lot_id);
        if (choice.inventory_id != source.id || !before ||
            before->item_id != flow.item_id || choice.quantity <= 0 ||
            choice.quantity > before->quantity || before->item_instance_id ||
            before->extra || before->owner.extra ||
            before->reserved_by != ReservationRef{} ||
            !catalog.items.find(before->item_id))
          fail("GrowthEconomicOriginalSelectedConsumeRequired", choice.lot_id);
        // ID借自持久原选择/flow，不借可被erase的工作帧元素。
        selected.push_back(
            {choice.lot_id, flow.item_id, before->quantity, choice.quantity});
      }
      require(apply_original_selected_consume_mutation(source, selected, next,
                                                       flows.output()));
    }
    if (!flows.complete())
      fail("GrowthEconomicOriginalResourceFlowMismatch");
  }
  if (next != s.next_instance_after)
    fail("GrowthEconomicOriginalAllocatorMismatch");
  for (const auto &f : actual.inventories)
    if (!same_original_inventory_frame_cold(projected.inventory(f.inventory_id),
                                            f.after, resources))
      fail("GrowthEconomicOriginalFullAfterMismatch", f.inventory_id);
}
} // namespace
Status validate_growth_economic_fifo_resource_stages(
    const CommonCatalog &catalog, const CommonResourceCodecContext &resources,
    std::span<const MarketOriginalStageReadView> stages) {
  try {
    for (std::size_t i = 0; i < stages.size(); ++i) {
      if (i && stages[i].stage.next_instance_before !=
                   stages[i - 1].stage.next_instance_after)
        fail("GrowthEconomicOriginalStageAllocatorGap");
      stage(catalog, resources, stages[i]);
    }
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("GrowthEconomicResourceReadMemoryExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace hanmo::resident
