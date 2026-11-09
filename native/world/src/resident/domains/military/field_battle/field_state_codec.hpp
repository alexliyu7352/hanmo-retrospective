// 连续会战完整 typed 字段的冷读写；不保路径缓存，不借旧 MilitaryBattle codec。
#pragma once
#include "field_world.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
class CommonStore;
// Module field_battle/1 与 root schema1
// 不变；本修订另完整保存10-31整军出口选择和真实汇合步。新字段不能猜补：旧
// section1/2/3/4/5 必须在 typed payload 前由原有限封套目录门拒绝。
inline constexpr Integer field_battle_checkpoint_codec_revision = 6;
// 原世界外层核 field_battle/1 模块与上述 section revision
// 后才调用；仍须真实来源 validator。
Status visit_field_battle_world(CodecVisitor &, FieldBattleWorldState &);
// 冷 JSON 借原 Boundary 树并走相同字段 callback；不另造另一套字段格式。
Outcome<FieldBattleWorldState> decode_field_battle_world(const BoundaryValue &);
// 诊断导出借原对象；不以导出 JSON 作为领域权威或冷读替代。
Status emit_field_battle_world(ProjectionWriter &, std::string_view,
                               const FieldBattleWorldState &);
// 冷边界只核 schema/真实拥有头及 Common 双向覆盖；不得替代同 World 完整业务校验。
bool owns_field_battle_fact_kind(std::string_view) noexcept;
Status validate_field_battle_fact_headers(const CommonStore &,
                                          const FieldBattleWorldState &);
// null 只证明未选根没有本域残留头；不创建空状态或替代选中根。
Status validate_field_battle_fact_headers(const CommonStore &,
                                          const FieldBattleWorldState *);
// Common 事实头投影到同一原会话、段或军议载荷，不制造另一份事件载荷。
Status emit_field_battle_fact_payloads_fields(ProjectionWriter &,
                                              const FieldBattleWorldState &);
} // namespace hanmo::resident
