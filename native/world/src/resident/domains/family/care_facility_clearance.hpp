// FH §5.3–5.5：完整 Care 根第十八表的原执行持值。
// 实际工量仍原 S0 / handover / S2，身体与人口仍各原 owner，本值不存替代状态。
#pragma once
#include "care_clearance_history_values.hpp"
#include "care_facility_transport_acceptance.hpp"
#include "care_physical_position.hpp"

namespace hanmo::resident {
enum class CareExecutionScopeSlot { before, cleared, remaining };
// 仅同一次执行的三个原 scope；before 不是已经清出的证明。
struct CareExecutionBodyScopeRef {
  StableId execution_id;
  CareExecutionScopeSlot scope_slot = CareExecutionScopeSlot::before;
};
// S7 后继份额的名义地址，不能与任意结果隐式互转。
// 当前尚缺原 completed-slice producer/reader；此声明不准入任何 source kind。
struct CareCompletedResidenceBodyScopeRef {
  StableId agreement_id, intake_line_id;
  MomentOwnerSource completed_scope_source;
};
using CareClearanceBodyScopeRef =
    std::variant<CareExecutionBodyScopeRef, CareCompletedResidenceBodyScopeRef>;

// 原七字段只保本次真实重放置结果；不补出过去匿名成员各躺在哪一床。
struct CareRepositionedBodyBinding {
  CareClearanceBodyScopeRef body_scope_ref;
  Integer body_count = 0;
  std::vector<FacilityPhysicalUnitRef> actual_unit_refs;
  ActorFoundationRuleRef receiving_safety_rule_ref;
  std::optional<SourceRef> actual_receiver_source_ref;
  std::optional<Integer> actual_receiver_schema;
  FieldOrder field_order;
};
enum class CareRemainingPositionMode { exact_current, unresolved_affected_set };
// 原八字段现场证明；集合由真正操作形成，不是请求方指定的可售空床。
struct CarePhysicalClearanceConfirmation {
  ActorFoundationRuleRef confirmation_policy_ref;
  std::vector<FacilityPhysicalUnitRef> original_affected_unit_refs;
  std::vector<FacilityPhysicalUnitRef> cleared_unit_refs;
  std::vector<FacilityPhysicalUnitRef> remaining_footprint_unit_refs;
  CareRemainingPositionMode remaining_position_mode =
      CareRemainingPositionMode::unresolved_affected_set;
  std::vector<CareRepositionedBodyBinding> actual_repositioned_body_bindings;
  WorldMoment observed_at;
  FieldOrder field_order;
};

// 同地真实受领的原十四字段；业务键的有限三支与接受记录使用同一原类型。
struct CareAcceptedLocalPlacement {
  CareReceivingBusinessRef receiving_business_ref;
  SourceRef receiving_acceptance_ref;
  Integer receiving_acceptance_schema = 0;
  StableId receiving_actor_id;
  MomentOwnerSource receiving_authority_ref;
  StableId source_place_ref, destination_place_ref;
  std::optional<MomentOwnerSource> new_facility_use_binding_ref;
  SourceRef actual_new_placement_source_ref;
  Integer actual_new_placement_source_schema = 0;
  Location receiver_location_before;
  std::vector<MomentOwnerSource> receiver_capability_source_refs;
  FieldOrder field_order;
};
// 原 Ground 装载只保真正 S1 操作前后值，不直接写编组、人口或当前位置。
struct CareActualGroundLoading {
  TransportUnitRef transport_unit_ref;
  Integer composition_revision_before = 0, composition_revision_after = 0;
  SourceRef registration_source_ref;
  Integer registration_source_schema = 0;
  StableId operator_actor_id, receiving_actor_id;
  MomentOwnerSource operator_authority_ref;
  Location source_location_before, receiver_location_before,
      carrier_position_before;
  CareBodyScopeSlice loaded_scope;
  std::vector<MomentOwnerSource> original_population_binding_refs;
  FieldOrder field_order;
};
// 原单船由真实 S2 Completed Result3 和唯一 passenger binding 来源证明。
struct CareActualVesselLoading {
  StableId vessel_id;
  SourceRef actual_s2_load_result_ref;
  Integer actual_s2_load_source_schema = 3;
  std::vector<MomentOwnerSource> passenger_binding_refs;
  FieldOrder field_order;
};
using CareActualTransportLoading =
    std::variant<CareActualGroundLoading, CareActualVesselLoading>;
struct CareOriginalTravelHandover {
  MomentOwnerSource transport_acceptance_ref;
  CareActualTransportLoading actual_loading;
  std::vector<MomentOwnerSource> actual_participation_result_refs;
  std::vector<MomentOwnerSource> actual_location_binding_result_refs;
  FieldOrder field_order;
};
// completed_local_operation 在 codec 固定原 literal，不是可自报的任意完成原因。
struct CareActualRearrangeAndConfirm {
  SourceRef original_placement_source_ref;
  Integer source_schema = 0;
  ActorFoundationRuleRef safe_placement_rule_ref;
  MomentOwnerSource original_use_or_care_authority_ref;
  FieldOrder field_order;
};
struct CareAuthorizedLocalDisposition {
  std::vector<MomentOwnerSource> death_or_population_loss_refs;
  MomentOwnerSource accepted_disposition_clause_ref, executor_authority_ref;
  StableId authorized_site_ref;
  MomentOwnerSource site_use_source_ref;
  StableId source_place_ref, disposition_place_ref;
  ActorFoundationRuleRef disposition_rule_ref;
  FieldOrder field_order;
};
using CareClearanceDestinationBasis =
    std::variant<CareAcceptedLocalPlacement, CareOriginalTravelHandover,
                 CareActualRearrangeAndConfirm, CareAuthorizedLocalDisposition>;

// 以下四种 source kind 由 codec 固定；唯有 Completed 是实际清出源。
struct CareFacilityClearanceStarted {
  StableId source_id;
  WorldMoment occurred_at;
  MomentOwnerSource original_adoption_or_obligation_trigger_ref;
  std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
  FieldOrder field_order;
};
struct CareFacilityClearanceWorkCompleted {
  StableId source_id;
  WorldMoment occurred_at;
  MomentOwnerSource original_action_terminal_ref;
  std::optional<MomentOwnerSource> last_closed_gate_ref;
  std::vector<ConditionReceiptRefV2> worker_condition_receipt_refs;
  FieldOrder field_order;
};
struct CareFacilityClearanceCompleted {
  StableId source_id;
  WorldMoment occurred_at;
  CareClearanceDestinationBasis destination_basis;
  CareBodyScopeSlice scope_cleared, scope_remaining;
  CarePhysicalClearanceConfirmation physical_confirmation;
  std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
  std::vector<MomentOwnerSource> actual_original_holder_result_refs;
  std::vector<MomentOwnerSource> actual_receiver_result_refs;
  FieldOrder field_order;
};
struct CareFacilityClearanceStopped {
  StableId source_id;
  WorldMoment occurred_at;
  SourceRef cause_ref;
  Integer cause_schema = 0;
  ActorFoundationRuleRef original_stop_rule_ref;
  std::optional<MomentOwnerSource> last_completed_gate_ref;
  FieldOrder field_order;
};
enum class FacilityClearanceState {
  working,
  suspended,
  awaiting_actual_placement,
  completed,
  cancelled_or_stopped
};

// 二十一原字段；origin0 才拥有自己期间/边界，origin1/2 只读真实原 owner。
struct FacilityClearanceExecution {
  StableId execution_id;
  FacilityClearanceObligation obligation;
  ActorFoundationRuleRef accepted_rule_ref;
  SourceRef original_holder_source_ref;
  Integer original_holder_source_schema = 0;
  StableId placement_claim_id;
  FacilitySlotRef slot_ref;
  std::vector<StableId> worker_actor_ids;
  std::vector<MomentOwnerSource> worker_acceptance_refs;
  CareBodyScopeSlice scope_before;
  CarePhysicalPositionBasis position_before;
  CareFacilityClearanceStarted started_source;
  FacilityClearanceWorkOrigin work_origin;
  std::optional<WorkExecutionBinding> work_binding;
  std::vector<FacilityClearanceWorkGate> work_gate_sources;
  std::optional<CareFacilityClearanceWorkCompleted> work_completed_source;
  FacilityClearanceState state = FacilityClearanceState::working;
  std::optional<CareFacilityClearanceCompleted> completion;
  std::optional<CareFacilityClearanceStopped> terminal_without_clearance;
  std::vector<FacilityClearanceExecutionBoundary1> execution_boundaries;
  FieldOrder field_order;
};
// 只验证原完整记录字段关系，不推定 source/动作/身体已经合法完成。
Status validate_facility_clearance_execution_relations(
    const FacilityClearanceExecution &);
// 二进制原字段出口；JSON/projection由完整根借同Common字段adapter。
Status visit_care_facility_clearance(CodecVisitor &,
                                     FacilityClearanceExecution &);
} // namespace hanmo::resident
