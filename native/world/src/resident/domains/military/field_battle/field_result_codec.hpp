// 公开结果与 Common 缓存共用一个有限 typed 回执，不用 Done 伪装缺数据。
#pragma once
#include "field_deployment_command_model.hpp"
#include "field_view_model.hpp"
#include "field_world.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status validate_field_battle_receipt(const StableId &kind,
                                     const FieldBattleReceipt &);
Status visit_field_battle_receipt(CodecVisitor &, FieldBattleReceipt &);
Status emit_field_battle_receipt_fields(ProjectionWriter &,
                                        const FieldBattleReceipt &);
Outcome<FieldBattleReceipt> decode_field_battle_receipt(const BoundaryValue &);
Outcome<BoundaryValue>
encode_field_battle_read_quote(const FieldBattleReadQuote &);
// 从刚应用的真实会话取得本次结果；只支持已有四条 writer，不给后继空结果。
Outcome<FieldBattleReceipt>
read_applied_field_command_result(const FieldBattleWorldState &,
                                  const FieldDeploymentCommand &,
                                  const StableId &actor);
} // namespace hanmo::resident
