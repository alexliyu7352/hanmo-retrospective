// 独立 named_military_custody/1 附段；不改原 Military 主段字节与俘获旧账。
#pragma once
#include "named_military_custody_history_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct MovementRecordCodecSelection;
inline constexpr Integer named_military_custody_checkpoint_revision = 1;
Status visit_named_military_custody_history(CodecVisitor &, NamedMilitaryCustodyHistory &,
                                          const MovementRecordCodecSelection &);
Outcome<NamedMilitaryCustodyHistory> decode_named_military_custody_history(
    const BoundaryValue &, const MovementRecordCodecSelection &);
Status emit_named_military_custody_history(ProjectionWriter &, std::string_view,
    const NamedMilitaryCustodyHistory &, const MovementRecordCodecSelection &);
// Common 只导出原真实头，正文唯一保留在 Military.named_custody 中。
Status emit_named_military_custody_fact_payloads_fields(ProjectionWriter &,
    const NamedMilitaryCustodyHistory &);
Outcome<NamedMilitaryCustodyPolicy> decode_named_military_custody_policy(const BoundaryValue &);
} // namespace hanmo::resident
