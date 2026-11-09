// FH §3/§7.2 的 Care 独立床位事件；不创建 S7 匿名内容或其他职业实物分支。
#pragma once
#include "care_facility_named_body.hpp"
#include "care_facility_use.hpp"
#include "care_physical_position.hpp"

namespace hanmo::resident {
// 本 CareArrangement 的 subject 是原具名 Actor；其它 owner 保持自己的内容联合。
struct CareFacilityEmptyContent {};
using CareFacilityContent =
    std::variant<CareFacilityEmptyContent, CareFacilityNamedBody>;

// 只有原真正完成交接的同次 placement 能产生初次独立床位。
struct CareFacilityActualPlacement {
  StableId accepted_arrangement_id, handover_id;
  SourceRef actual_care_temporal_event_ref;
  ActorFoundationRuleRef placement_rule_ref;
  SourceRef original_receiver_acceptance_ref;
};
// 覆盖丧失只影响服务资格；此支必须保留仍在位的原身体。
struct CareFacilityActualContinuation {
  StableId accepted_arrangement_id;
  SourceRef original_care_transition_ref, cause_ref;
  Integer cause_schema = 0;
};
// 只接受实际 clearance completed，work_completed 不能冒充身体已经离位。
struct CareFacilityActualClearance {
  StableId accepted_arrangement_id;
  SourceRef completed_clearance_source_ref;
  Integer source_schema = 1;
};
using CareFacilityOperation =
    std::variant<CareFacilityActualPlacement, CareFacilityActualContinuation,
                 CareFacilityActualClearance>;

// 原 event_kind wire 固定 0..3，不将死亡当成 Cleared。
enum class CareFacilityEventKind { placed, continued, partly_cleared, cleared };

// 原 LifeFacilityEvent/1 的完整固定字段；source kind 由本 owner 静态确定。
struct CareFacilityOccupancyEvent {
  StableId event_id;
  Integer source_schema = 1;
  CareArrangementRef owner_business_ref;
  ReservationRef holder;
  FacilitySlotRef slot_ref;
  StableId placement_claim_id;
  std::optional<SourceRef> previous_event_ref;
  WorldMoment occurred_at;
  CareFacilityEventKind event_kind = CareFacilityEventKind::placed;
  CarePhysicalPositionBasis position_basis;
  CareFacilityContent before_content, after_content;
  CareFacilityOperation operation;
  std::optional<FrozenFacilityCapabilityInputs> capability_inputs;
  FieldOrder field_order;
};

// 仅规范原地址；这些函数不注册源、不分配 ID、不产生 Common 头。
SourceRef care_facility_occupancy_source(const StableId &);
Outcome<Integer> care_facility_occupancy_source_schema(const SourceRef &);
// 字段表逐字段检查后核跨字段关系；不是独立来源/完整结构验收入口。
Status
validate_care_facility_occupancy_relations(const CareFacilityOccupancyEvent &);
// 本 Care 段每行先写固定 event_kind_source_tag=0，再按唯一字段表存完整值。
Status visit_care_facility_occupancy(CodecVisitor &,
                                     CareFacilityOccupancyEvent &);
Status emit_care_facility_occupancy_fields(ProjectionWriter &,
                                           const CareFacilityOccupancyEvent &);
Outcome<BoundaryValue>
encode_care_facility_occupancy(const CareFacilityOccupancyEvent &);
Outcome<CareFacilityOccupancyEvent>
decode_care_facility_occupancy(const BoundaryValue &);
} // namespace hanmo::resident
