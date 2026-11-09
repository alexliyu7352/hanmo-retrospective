// 配置回执的原字段只遍历一次；此叶不写世界、分配事实或改变二进制布局。
#include "production_configuration_projection.hpp"
#include "local_economy_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"

namespace hanmo::resident {

// 与原 economic_world_integration 的边界字面序一致，shape sink 不构造
// BoundaryValue 树。
Status
emit_economic_configuration_effects_fields(ProjectionWriter &writer,
                                           const LocalEconomyEffects &effects) {
  const auto reference = [](auto &out, const StableId &id) {
    return out.field("", id);
  };
  writer.sequence("receipt_ids", effects.receipt_ids, reference);
  return writer.sequence("fact_ids", effects.fact_ids, reference);
}

// 原公开编码只包装同一 emitter，不能与预算投影形成第二份字段定义。
BoundaryValue
encode_economic_configuration_effects(const LocalEconomyEffects &effects) {
  BoundaryValue value;
  BoundaryProjectionWriter writer(value);
  auto status = writer.object("", [&](auto &out) {
    return emit_economic_configuration_effects_fields(out, effects);
  });
  if (status.ok)
    status = writer.finish();
  if (!status.ok)
    throw boundary::DecodeFailure{status.error};
  return value;
}
} // namespace hanmo::resident
