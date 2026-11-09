// 请求回执冷边界是域内候选；外层军事版本与特性位只由中央注册。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "battle_request_transition.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::military_request_codec {
Status emit_fields(ProjectionWriter&,const MilitaryRequestReceipt&,bool apply_order=true);
Status emit_answer_fields(ProjectionWriter&,const MilitaryRequestAnswer&);

// 中央预留军事codec5的唯一特性位；旧3/4不得置位或写额外存在位。
inline constexpr std::uint32_t checkpoint_feature=128u;
// JSON与typed检查点均只读取明确字段，不接受额外隐藏计划或热BoundaryValue。
BoundaryValue encode(const MilitaryRequestReceipt&);
void decode(const BoundaryValue&,MilitaryRequestReceipt&,const StableId&);
Status visit(CodecVisitor&,MilitaryRequestReceipt&);
Status validate(const MilitaryRequestReceipt&);
// 公开回覆不包含采纳前AI计划；共同回执仅在128特性位下调用这些冷口。
MilitaryRequestAnswer answer(const MilitaryRequestReceipt&);
BoundaryValue encode_answer(const MilitaryRequestAnswer&);
void decode_answer(const BoundaryValue&,MilitaryRequestAnswer&,const StableId&);
Status visit_answer(CodecVisitor&,MilitaryRequestAnswer&);
Status validate_answer(const MilitaryRequestAnswer&);
}
