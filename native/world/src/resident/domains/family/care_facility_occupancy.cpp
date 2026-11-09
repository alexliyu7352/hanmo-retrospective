// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// Care 的完整局部事件存读；这里只持原引用，不新增当前床位/身体账。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_facility_occupancy.hpp"
#include "care_facility_occupancy_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <algorithm>
#include <string_view>

namespace hanmo::resident {
namespace {
// 实际成员默认形状求和；未知嵌套型不会被本局部登记默认为已知。

// 地址类别检查只识别原 owner，不因地址正确就声称其实际源存在。
bool source_kind(const SourceRef &ref, std::string_view kind) {
  return ref.domain == "care_requirements" && ref.kind == kind &&
         !ref.id.empty() && ref.id.size() <= 256;
}
// 当前事件能保留同一身体的后续真 basis，但不能以换 actor 偷换占床人。
bool same_actor(const CareFacilityContent &a, const CareFacilityContent &b) {
  const auto *before = std::get_if<CareFacilityNamedBody>(&a);
  const auto *after = std::get_if<CareFacilityNamedBody>(&b);
  return before && after && before->actor_id == after->actor_id;
}
} // namespace

// 真正零字段分支，不代表缺少来源的业务表可以省略。

// 每个操作分支按完整原字段逐项计默认构造。


// 整行默认值沿实际成员，不以字段数估算深值或跳过首个 variant 分支。


// 固定原地址不分配 ID、不写 Common 头，也不安装 registry。
SourceRef care_facility_occupancy_source(const StableId &id) {
  return {"care_requirements", "care_facility_occupancy", id};
}
Outcome<Integer> care_facility_occupancy_source_schema(const SourceRef &ref) {
  if (source_kind(ref, "care_facility_occupancy"))
    return {true, 1, {}};
  return {false,
          {},
          common_failure("UnknownCareFacilityOccupancySource", ref.id,
                         ErrorKind::unsupported_capability)
              .error};
}

// 已知字段的跨字段关系；实际之前/之后来源与操作事实必须由 owner 另核。
Status validate_care_facility_occupancy_relations(
    const CareFacilityOccupancyEvent &v) {
  const auto reject = [&](const char *code) {
    return common_failure(code, v.event_id);
  };
  const auto &owner = v.owner_business_ref.accepted_arrangement_id;
  const ReservationRef expected{ReservationRef::Kind::domain_activity, owner,
                                "care_requirements", "care_arrangement"};
  if (v.source_schema != 1 || v.holder != expected ||
      v.slot_ref.slot_kind != "bed_space")
    return reject("InvalidCareFacilityOccupancyIdentity");
  if (v.previous_event_ref &&
      (!source_kind(*v.previous_event_ref, "care_facility_occupancy") ||
       v.previous_event_ref->id == v.event_id))
    return reject("InvalidCareFacilityOccupancyPredecessor");
  const bool placed = v.event_kind == CareFacilityEventKind::placed;
  const bool cleared = v.event_kind == CareFacilityEventKind::cleared;
  if (placed == v.previous_event_ref.has_value() ||
      (placed &&
       !std::holds_alternative<CareFacilityEmptyContent>(v.before_content)) ||
      (!placed &&
       !std::holds_alternative<CareFacilityNamedBody>(v.before_content)) ||
      (cleared !=
       std::holds_alternative<CareFacilityEmptyContent>(v.after_content)))
    return reject("InvalidCareFacilityOccupancyContentTransition");
  if (!placed && !cleared && !same_actor(v.before_content, v.after_content))
    return reject("CareFacilityOccupancyActorChanged");
  bool position_ok = std::visit(
      [&](const auto &p) {
        const auto &units = [&]() -> const auto & {
          if constexpr (std::is_same_v<std::decay_t<decltype(p)>,
                                       CareAggregatePhysicalPosition>)
            return p.affected_unit_refs;
          else
            return p.occupied_unit_refs;
        }();
        // AggregateOnly 的无名册情况仍允许非 Cleared 空数组；不得据此证明已清。
        if (p.placement_claim_id != v.placement_claim_id ||
            (cleared && !units.empty()))
          return false;
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>,
                                     CareExactPhysicalPosition>)
          return cleared || (!units.empty() && !p.position_source_refs.empty());
        return true;
      },
      v.position_basis);
  if (!position_ok)
    return reject("CareFacilityOccupancyPositionMismatch");
  const bool operation_ok = std::visit(
      [&](const auto &op) {
        if (op.accepted_arrangement_id != owner)
          return false;
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, CareFacilityActualPlacement>)
          return placed &&
                 source_kind(op.actual_care_temporal_event_ref,
                             "care_requirement_changed") &&
                 source_kind(op.original_receiver_acceptance_ref,
                             "care_arrangement_accepted") &&
                 op.original_receiver_acceptance_ref.id == owner;
        else if constexpr (std::is_same_v<T, CareFacilityActualContinuation>)
          return v.event_kind == CareFacilityEventKind::continued &&
                 source_kind(op.original_care_transition_ref,
                             "care_requirement_changed") &&
                 op.cause_schema > 0;
        else
          return (v.event_kind == CareFacilityEventKind::partly_cleared ||
                  cleared) &&
                 op.source_schema == 1 &&
                 source_kind(op.completed_clearance_source_ref,
                             "facility_clearance_completed");
      },
      v.operation);
  if (!operation_ok)
    return reject("CareFacilityOccupancyOperationMismatch");
  constexpr std::string_view fields[]{
      "event_id",   "source_schema",      "owner_business_ref", "holder",
      "slot_ref",   "placement_claim_id", "previous_event_ref", "occurred_at",
      "event_kind", "position_basis",     "before_content",     "after_content",
      "operation",  "capability_inputs",  "field_order"};
  for (std::size_t i = 0; i < v.field_order.size(); ++i) {
    if (std::find(std::begin(fields), std::end(fields), v.field_order[i]) ==
        std::end(fields))
      return reject("UnknownCareFacilityOccupancyField");
    for (std::size_t j = 0; j < i; ++j)
      if (v.field_order[j] == v.field_order[i])
        return reject("DuplicateCareFacilityOccupancyField");
  }
  return common_success();
}

// 二进制和投影共同分派完整局部字段表，不重建任何来源或当前状态。
Status visit_care_facility_occupancy(CodecVisitor &visitor,
                                     CareFacilityOccupancyEvent &value) {
  return care_facility_occupancy_codec::event(visitor, value);
}
Status
emit_care_facility_occupancy_fields(ProjectionWriter &writer,
                                    const CareFacilityOccupancyEvent &value) {
  ActorFoundationProjectionArchive archive(writer);
  return care_facility_occupancy_codec::event(
      archive, const_cast<CareFacilityOccupancyEvent &>(value));
}
// 边界树只是原值投影，不能作为新占位成功记录发布。
Outcome<BoundaryValue>
encode_care_facility_occupancy(const CareFacilityOccupancyEvent &value) {
  BoundaryValue encoded;
  BoundaryProjectionWriter writer(encoded);
  auto status = writer.object("", [&](ProjectionWriter &out) {
    return emit_care_facility_occupancy_fields(out, value);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(encoded), status.error};
}
// 多余/缺失/未知分支严格拒绝；成功仍须走原来源、真实单位和全链验证。
Outcome<CareFacilityOccupancyEvent>
decode_care_facility_occupancy(const BoundaryValue &input) {
  CareFacilityOccupancyEvent value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = care_facility_occupancy_codec::event(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident
