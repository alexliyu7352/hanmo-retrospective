// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原设施具名身体的局部存读；不创建占位、条件变化、死亡或清场成功源。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_facility_named_body.hpp"
#include "care_facility_named_body_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"

namespace hanmo::resident {
// 默认构造只含空 actor、原 Seed 首分支及整数，不假定非空字段免费。


// Binary 路径只分派唯一字段表，不从当前世界重建历史来源。
Status visit_care_facility_named_body(CodecVisitor &visitor,
                                      CareFacilityNamedBody &value) {
  return care_facility_named_body_codec::named_body(visitor, value);
}

// 投影只借已拥有值；原 archive 的写模式不修改来源。
Status
emit_care_facility_named_body_fields(ProjectionWriter &writer,
                                     const CareFacilityNamedBody &value) {
  ActorFoundationProjectionArchive archive(writer);
  return care_facility_named_body_codec::named_body(
      archive, const_cast<CareFacilityNamedBody &>(value));
}

// 不向世界发布记录；编码失败完整返回原错误。
Outcome<BoundaryValue>
encode_care_facility_named_body(const CareFacilityNamedBody &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_care_facility_named_body_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}

// 未知或多余字段不可静默丢弃；返回候选值仍不是原来源验证证明。
Outcome<CareFacilityNamedBody>
decode_care_facility_named_body(const BoundaryValue &input) {
  CareFacilityNamedBody value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_facility_named_body_codec::named_body(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident
