// FH §5.3B 的原本地辅助清场历史深值；不生产 Action、工量、费用或身体移位。
// work_origin 的实际选择由完整 execution 验证，不能把这些行用于原 Care/Sea
// 双计工。
#pragma once
#include "care_handover_execution.hpp"

namespace hanmo::resident {
// 原 clearance 的完整一期来源，与 CareHandoverWorkPeriod2 是不同强型。
// 唯一工量仍为 S0 WorkPeriodRef2；这里没有 elapsed、remaining 或累计字段。
struct FacilityClearanceWorkPeriod2 {
  StableId source_id;
  Integer source_schema = 2;
  StableId execution_ref;
  WorkPeriodRef2 work_period_ref;
  WorldMoment provided_at;
  std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
  std::vector<ConditionReceiptRefV2> worker_condition_receipt_refs;
  FieldOrder field_order;
};

// 两类原执行共用 FH 明定的局部 cause payload，不共用执行/期间身份。
using FacilityClearanceBoundaryCause = CareHandoverBoundaryCause;

// 仅本地 work_origin0 持有自己的边界；origin1/2 必须反读各自原 owner。
// 本类型无 Common 事实头，完成工作也不自动成为实际清场完成。
struct FacilityClearanceExecutionBoundary1 {
  StableId boundary_id;
  Integer source_schema = 1;
  StableId execution_ref;
  Integer boundary_ordinal = 0;
  std::optional<StableId> previous_boundary_ref;
  CareHandoverBoundaryKind kind = CareHandoverBoundaryKind::started;
  WorldMoment occurred_at;
  ActorFoundationRuleRef transition_rule_ref;
  FacilityClearanceBoundaryCause cause;
  std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
  std::vector<MomentOwnerSource> actual_participation_source_refs;
  std::vector<MomentOwnerSource> facility_use_binding_refs;
  FieldOrder field_order;
};

// 原 work_gate_sources 的本地分支实际持值，不复制另一执行的工作结果。
struct FacilityClearanceLocalProvided {
  FacilityClearanceWorkPeriod2 period;
};
// 原 handover 分支只引用同 handover 的真正 source2，不重复 worker frame
// 或疲劳。
struct FacilityClearanceOriginalCarePeriod {
  SourceRef source_ref;
  Integer source_schema = 2;
};
// FH 固定局部 wire 包装；origin 的准入约束由其唯一 execution 根核验。
struct FacilityClearanceWorkGate {
  std::variant<FacilityClearanceLocalProvided,
               FacilityClearanceOriginalCarePeriod>
      payload;
  FieldOrder field_order;
};

class CodecVisitor;
class ProjectionWriter;
// 三个原值的严格同字段存读，只检查结构，不冒实际服务来源闭合。
Status visit_facility_clearance_period2(CodecVisitor &,
                                        FacilityClearanceWorkPeriod2 &);
Status
emit_facility_clearance_period2_fields(ProjectionWriter &,
                                       const FacilityClearanceWorkPeriod2 &);
Outcome<BoundaryValue>
encode_facility_clearance_period2(const FacilityClearanceWorkPeriod2 &);
Outcome<FacilityClearanceWorkPeriod2>
decode_facility_clearance_period2(const BoundaryValue &);

// 状态边界的实际原因和 worker 输入仅保存，不执行开始/暂停/完成。
Status
visit_facility_clearance_boundary1(CodecVisitor &,
                                   FacilityClearanceExecutionBoundary1 &);
Status emit_facility_clearance_boundary1_fields(
    ProjectionWriter &, const FacilityClearanceExecutionBoundary1 &);
Outcome<BoundaryValue> encode_facility_clearance_boundary1(
    const FacilityClearanceExecutionBoundary1 &);
Outcome<FacilityClearanceExecutionBoundary1>
decode_facility_clearance_boundary1(const BoundaryValue &);

// 局部 gate 联合的 tag/payload/field_order 不按当前工作状态猜补。
Status visit_facility_clearance_work_gate(CodecVisitor &,
                                          FacilityClearanceWorkGate &);
Status
emit_facility_clearance_work_gate_fields(ProjectionWriter &,
                                         const FacilityClearanceWorkGate &);
Outcome<BoundaryValue>
encode_facility_clearance_work_gate(const FacilityClearanceWorkGate &);
Outcome<FacilityClearanceWorkGate>
decode_facility_clearance_work_gate(const BoundaryValue &);
} // namespace hanmo::resident
