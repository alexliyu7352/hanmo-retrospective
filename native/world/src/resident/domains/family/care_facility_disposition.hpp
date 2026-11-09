// FH §7.3：完整 Care 根第十七表只保存真实原业务处置，不持第二 current state。
#pragma once
#include "care_coverage.hpp"
#include "care_facility_use.hpp"
#include "resident/kernel/action_time_validation.hpp"

namespace hanmo::resident {
// 各原执行保其自身枚举与真实转移源；不能按处置kind挑无关状态填入。
struct CareFacilityGoalTransition {
  StableId requirement_id;
  SourceRef original_transition_ref;
  Integer original_transition_schema = 3;
  StateGoalState before = StateGoalState::unsatisfied;
  StateGoalState after = StateGoalState::unsatisfied;
};
struct CareFacilityBindingTransition {
  StableId binding_id;
  SourceRef original_transition_ref;
  Integer original_transition_schema = 3;
  CareBindingPhase before = CareBindingPhase::active;
  CareBindingPhase after = CareBindingPhase::active;
};
struct CareFacilityHandoverTransition {
  StableId handover_id;
  SourceRef original_boundary_ref;
  Integer original_boundary_schema = 1;
  ActionTimeOwnerPhase before = ActionTimeOwnerPhase::working;
  ActionTimeOwnerPhase after = ActionTimeOwnerPhase::working;
};
using CareFacilityBusinessTransition =
    std::variant<CareFacilityGoalTransition, CareFacilityBindingTransition,
                 CareFacilityHandoverTransition>;

// 五种原trigger只存原真实地址，既不执行命令，也不在此发起provider批次变更。
struct CareFacilityProviderChanged {
  SourceRef actual_capacity_or_right_source_ref;
  Integer schema = 0;
};
struct CareFacilityOriginalBusinessCommand {
  LifeActualCommandBinding actual_adoption_ref;
};
struct CareFacilityOriginalBusinessResult {
  SourceRef actual_result_ref;
  Integer schema = 0;
};
struct CareFacilityAcceptedWindowDue {
  StableId previous_claim_id;
  WorldMoment due_at;
};
struct CareFacilityPhysicalClearanceCompleted {
  SourceRef actual_clearance_or_professional_event_ref;
  Integer schema = 0;
};
using CareFacilityDispositionTrigger = std::variant<
    CareFacilityProviderChanged, CareFacilityOriginalBusinessCommand,
    CareFacilityOriginalBusinessResult, CareFacilityAcceptedWindowDue,
    CareFacilityPhysicalClearanceCompleted>;
enum class CareFacilityDispositionKind {
  future_reservation_closed,
  service_suspended,
  resume_applied,
  bounded_continuation,
  service_terminal_released,
  actual_physical_clearance
};

// 原十六字段，外行source tag固定0；所有真实claim仍由S0唯一拥有。
struct CareFacilityDispositionResult {
  StableId result_id;
  Integer source_schema = 1;
  CareArrangementRef owner_business_ref;
  StableId accepted_terms_ref;
  FacilitySlotRef slot_ref;
  WorldMoment occurred_at;
  CareFacilityDispositionKind disposition_kind =
      CareFacilityDispositionKind::future_reservation_closed;
  CareFacilityDispositionTrigger trigger;
  CareFacilityBusinessTransition original_business_transition;
  std::vector<MomentOwnerSource> settled_work_source_refs;
  std::vector<StableId> previous_claim_ids, new_claim_ids;
  std::optional<SourceRef> physical_occupancy_source_ref;
  std::optional<Integer> physical_source_schema;
  ActorFoundationRuleRef rule_ref;
  FieldOrder field_order;
};

// 只由真实失覆盖源构造同arrangement的binding主转移；不读取今日phase补过去。
// Goal的同次变化继续由原coverage reader闭包核验，不复制成第二主状态。
Outcome<CareFacilityBindingTransition>
read_care_facility_binding_loss_transition(
    const CareCoverageSources &, const CareArrangementRef &,
    const SourceRef &actual_care_transition);
// 局部完整字段关系，不替代原writer/源反向覆盖/设施容量核验。
Status validate_care_facility_disposition_relations(
    const CareFacilityDispositionResult &);
// 仅值编码；调用方仍必须以实际 owner reader 联合验证原来源闭包。
Status visit_care_facility_disposition(CodecVisitor &,
                                       CareFacilityDispositionResult &);
Status
emit_care_facility_disposition_fields(ProjectionWriter &,
                                      const CareFacilityDispositionResult &);
Outcome<BoundaryValue>
encode_care_facility_disposition(const CareFacilityDispositionResult &);
Outcome<CareFacilityDispositionResult>
decode_care_facility_disposition(const BoundaryValue &);
} // namespace hanmo::resident
