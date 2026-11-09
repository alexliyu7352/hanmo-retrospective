// M-CONTINUITY.lifecycle_decisions 原叶字段表；不是另一物理 section。
#pragma once
#include "political_foundation_decisions.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
// 只 decode 私有值，原 World 安装前还须完整真实源校验，不准 schema/shape
// 当成立。
Status visit_foundation_lifecycle_decision(CodecVisitor &,
                                           PoliticalLifecycleDecisionRecord &);
Status emit_foundation_lifecycle_decision_projection(
    ProjectionWriter &, std::string_view,
    const PoliticalLifecycleDecisionRecord &);
Outcome<BoundaryValue>
encode_foundation_lifecycle_decision(const PoliticalLifecycleDecisionRecord &);
Outcome<PoliticalLifecycleDecisionRecord>
decode_foundation_lifecycle_decision(const BoundaryValue &);
} // namespace hanmo::resident
