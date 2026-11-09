// Fortification 根的严格 array codec；只承载原六槽，不接旧 /2-/4 section。
#pragma once
#include "military_fortification_nodes.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// 职责：使用原 JSON job 和 dynamic_military 集合预算，拒第二种键表形状。
Outcome<MilitaryFortificationNodes>
decode_military_fortification_nodes(const BoundaryValue &);
// 职责：原序发出完整 array；持值结构不合法即拒，不排序修复。
Outcome<BoundaryValue>
encode_military_fortification_nodes(const MilitaryFortificationNodes &);
// 职责：沿同一六槽 callback 输出，只借记录不复制根。
Status emit_military_fortification_nodes(ProjectionWriter &, std::string_view,
                                         const MilitaryFortificationNodes &);
// 职责：原 binary 字段/tag/边界严格读写，再核 tuple 序；不注册完整 provider。
Status visit_military_fortification_nodes(CodecVisitor &,
                                          MilitaryFortificationNodes &);
}
