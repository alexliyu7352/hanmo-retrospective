// 原清场深值的存读适配；不调用执行/付款/身体迁移，也不复制条件拥有模型。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_clearance_history_values.hpp"
#include "care_clearance_history_values_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
// 本地完整期间的 binary/state visitor，只保存既有字段。
Status visit_facility_clearance_period2(CodecVisitor &visitor,
                                        FacilityClearanceWorkPeriod2 &value) {
  return care_clearance_history_codec::period(visitor, value);
}
// 投影保持原 FieldOrder；未知字段或嵌套形状错误不被吞掉。
Status emit_facility_clearance_period2_fields(
    ProjectionWriter &writer, const FacilityClearanceWorkPeriod2 &value) {
  ActorFoundationProjectionArchive archive(writer);
  auto status = care_clearance_history_codec::period(
      archive, const_cast<FacilityClearanceWorkPeriod2 &>(value));
  return status.ok ? writer.order_fields(value.field_order) : status;
}
// 边界序列化只承载原深值，不把编码成功当原服务/清场成功。
Outcome<BoundaryValue>
encode_facility_clearance_period2(const FacilityClearanceWorkPeriod2 &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_facility_clearance_period2_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}
// 全字段解码拒绝漏项/未知项；不补工作期间或终局。
Outcome<FacilityClearanceWorkPeriod2>
decode_facility_clearance_period2(const BoundaryValue &input) {
  FacilityClearanceWorkPeriod2 value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_clearance_history_codec::period(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
// 本地真实状态边界的 binary/state visitor，只保存既有字段。
Status
visit_facility_clearance_boundary1(CodecVisitor &visitor,
                                   FacilityClearanceExecutionBoundary1 &value) {
  return care_clearance_history_codec::boundary(visitor, value);
}
// 投影保持原 FieldOrder；未知字段或嵌套形状错误不被吞掉。
Status emit_facility_clearance_boundary1_fields(
    ProjectionWriter &writer,
    const FacilityClearanceExecutionBoundary1 &value) {
  ActorFoundationProjectionArchive archive(writer);
  auto status = care_clearance_history_codec::boundary(
      archive, const_cast<FacilityClearanceExecutionBoundary1 &>(value));
  return status.ok ? writer.order_fields(value.field_order) : status;
}
// 边界序列化只承载原深值，不把编码成功当原服务/清场成功。
Outcome<BoundaryValue> encode_facility_clearance_boundary1(
    const FacilityClearanceExecutionBoundary1 &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_facility_clearance_boundary1_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}
// 全字段解码拒绝漏项/未知项；不补工作期间或终局。
Outcome<FacilityClearanceExecutionBoundary1>
decode_facility_clearance_boundary1(const BoundaryValue &input) {
  FacilityClearanceExecutionBoundary1 value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_clearance_history_codec::boundary(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
// 原本地/交接期间联合的 binary/state visitor，只保存既有字段。
Status visit_facility_clearance_work_gate(CodecVisitor &visitor,
                                          FacilityClearanceWorkGate &value) {
  return care_clearance_history_codec::gate(visitor, value);
}
// 投影保持原 FieldOrder；未知字段或嵌套形状错误不被吞掉。
Status emit_facility_clearance_work_gate_fields(
    ProjectionWriter &writer, const FacilityClearanceWorkGate &value) {
  ActorFoundationProjectionArchive archive(writer);
  auto status = care_clearance_history_codec::gate(
      archive, const_cast<FacilityClearanceWorkGate &>(value));
  return status.ok ? writer.order_fields(value.field_order) : status;
}
// 边界序列化只承载原深值，不把编码成功当原服务/清场成功。
Outcome<BoundaryValue>
encode_facility_clearance_work_gate(const FacilityClearanceWorkGate &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_facility_clearance_work_gate_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}
// 全字段解码拒绝漏项/未知项；不补工作期间或终局。
Outcome<FacilityClearanceWorkGate>
decode_facility_clearance_work_gate(const BoundaryValue &input) {
  FacilityClearanceWorkGate value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_clearance_history_codec::gate(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident
