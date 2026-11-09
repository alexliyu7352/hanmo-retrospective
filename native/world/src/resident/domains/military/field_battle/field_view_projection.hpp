// 表现层采用合同字段形状；存档仍沿原 typed section，不把展示 DTO 当世界权威。
#pragma once
#include "field_view_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_field_battle_view_fields(ProjectionWriter &,
                                     const FieldBattleView &);
Outcome<BoundaryValue> encode_field_battle_public_view(const FieldBattleView &);
} // namespace hanmo::resident
