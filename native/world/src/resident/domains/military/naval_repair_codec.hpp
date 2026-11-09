// Naval3 专属工单边界；字段、Owner及锚点重名均拒绝，旧段绝不追加空工单表。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "naval_repair_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {class ScopedCheckpointJsonDecode;}
namespace hanmo::resident::naval_repair_codec {
Status emit_state_fields(ProjectionWriter&,const NavalRepairState&,bool charges=false);
Status emit_action_payload_fields(ProjectionWriter&,const CommonRead&,const NavalRepairState&,bool charges=false);
Status emit_receipt_fields(ProjectionWriter&,const NavalRepairReceipt&);

Status emit_terms_fields(ProjectionWriter&,const NavalRepairTerms&);
Outcome<NavalRepairTerms> decode_terms(const BoundaryValue&);
Outcome<NavalRepairTerms> decode_terms(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status visit_terms(CodecVisitor&,NavalRepairTerms&);

Status validate(const NavalRepairState&,bool charges=false);
BoundaryValue encode(const NavalRepairState&,bool charges=false);
// 有活动Action时显式导出原开工载荷；冷JSON须逐字比对此表，不能吞掉业务params。
Outcome<BoundaryValue> action_payloads(const CommonRead&,const NavalRepairState&,bool charges=false);
Outcome<NavalRepairState> decode(const BoundaryValue&,bool charges=false);
Status visit(CodecVisitor&,NavalRepairState&,bool charges=false);
Outcome<NavalRepairCommand> decode_command(const StableId& kind,const BoundaryValue&);
BoundaryValue encode_quote(const NavalRepairQuote&);
// 新回执独占 Naval3 标签；原命令kind在外层再与start/cancel语义交叉核对。
Status validate_receipt(const NavalRepairReceipt&,const StableId& command_kind);
BoundaryValue encode_receipt(const NavalRepairReceipt&);
Outcome<NavalRepairReceipt> decode_receipt(const BoundaryValue&,const StableId& command_kind);
Status visit_receipt(CodecVisitor&,NavalRepairReceipt&);
}
