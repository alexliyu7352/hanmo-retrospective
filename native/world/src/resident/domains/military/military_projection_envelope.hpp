// 原始行校验通过后再添加领域类型，保持字段顺序与失败优先级。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// 独立校验器已建立根对象；直接校验原字段，再输出带类型的同一行。
template <class Emit>
Status
emit_tagged_military_fields(ProjectionWriter &writer, const StableId &kind,
                            const FieldOrder &original_order, Emit emit) {
  auto checked =
      writer.validate_detached([&](auto &row) { return emit(row, true); });
  if (!checked.ok)
    return checked;
  writer.field("kind", kind);
  checked = emit(writer, false);
  if (!checked.ok)
    return checked;
  if (!original_order.empty()) {
    FieldOrder outer;
    outer.reserve(original_order.size() + 1);
    outer.push_back("kind");
    outer.insert(outer.end(), original_order.begin(), original_order.end());
    writer.order_fields(outer);
  }
  return writer.status();
}
} // namespace hanmo::resident
