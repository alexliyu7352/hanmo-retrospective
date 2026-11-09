// 对话只持不可变的真实行动结果；共同边界提供有限编解码，不把原字典留在热状态。
#pragma once
#include "ordered_value.hpp"
#include "codec_visitor.hpp"
namespace hanmo::resident {
struct ActionCompletion;class ProjectionWriter;
Status emit_action_completion_fields(ProjectionWriter&,const ActionCompletion&);
Outcome<BoundaryValue> encode_action_completion(const ActionCompletion&);
Outcome<std::shared_ptr<const ActionCompletion>> decode_action_completion(const BoundaryValue&);
Status visit_action_completion(CodecVisitor&,std::shared_ptr<const ActionCompletion>&);
bool action_completion_succeeded(const ActionCompletion&);
}
