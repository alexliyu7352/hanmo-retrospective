// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 局部原身体份额的严格存读；不创建人员/损失/占位/运输或任何清场成功源。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_body_scope.hpp"
#include "care_body_scope_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"

namespace hanmo::resident {
// 同一强型字段表用于原 binary visitor。
Status visit_care_body_scope(CodecVisitor &visitor, CareBodyScopeSlice &value) {
  return care_body_scope_codec::scope(visitor, value);
}

// 投影只借现有不可变字段，不从当前世界补出身体来源。
Status emit_care_body_scope_fields(ProjectionWriter &writer,
                                   const CareBodyScopeSlice &value) {
  ActorFoundationProjectionArchive archive(writer);
  return care_body_scope_codec::scope(archive,
                                      const_cast<CareBodyScopeSlice &>(value));
}

// 序列化结果不是新人口/身体拥有账，错误完整向上传递。
Outcome<BoundaryValue> encode_care_body_scope(const CareBodyScopeSlice &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_care_body_scope_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}

// 不接受额外字段或缺省份额；发布前仍必须核完整原来源。
Outcome<CareBodyScopeSlice> decode_care_body_scope(const BoundaryValue &input) {
  CareBodyScopeSlice value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_body_scope_codec::scope(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}

} // namespace hanmo::resident
