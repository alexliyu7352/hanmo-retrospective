// Care 的唯一完整十九表 owning 根；只保本域原值，不复制其它原 owner 或
// selector。
#pragma once
#include "care_education.hpp"
#include "care_facility_clearance.hpp"
#include "care_facility_disposition.hpp"
#include "care_facility_occupancy.hpp"
#include "care_period.hpp"
#include "care_root_selected_layout.hpp"

namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
class ProjectionWriter;
// 成员顺序与 care_root_table_names 完全一致，公共封套仍由实际 catalog 选择。
// 所有 borrowed context 都在候选建立后另建，不能被复制/编码到本状态。
struct CareRequirementsState {
  OrderedTable<CareRequirement> requirements;
  OrderedTable<CareRequirementTransition> requirement_transitions;
  OrderedTable<AcceptedCaregiver> accepted_caregivers;
  OrderedTable<CareBinding> care_bindings;
  OrderedTable<CareCoverageSatisfied> coverage_satisfied;
  OrderedTable<CareCoverageLost> coverage_lost;
  OrderedTable<CarePeriod> care_periods;
  OrderedTable<VersionedEducationPlan> education_plans;
  OrderedTable<MaturityResult> maturity_results;
  OrderedTable<CareHandoverExecution> care_handover_executions;
  OrderedTable<CareTemporalEvent> care_temporal_events;
  OrderedTable<CarePeriodSource> care_period_sources;
  OrderedTable<EducationChoice2> education_choices;
  OrderedTable<EducationActivation2> education_activations;
  OrderedTable<CareFacilityUseBinding> facility_use_bindings;
  OrderedTable<CareFacilityOccupancyEvent> facility_occupancy_events;
  OrderedTable<CareFacilityDispositionResult> facility_disposition_results;
  OrderedTable<FacilityClearanceExecution> facility_clearance_executions;
  OrderedTable<FacilityTransportAcceptance> facility_transport_acceptances;
  FieldOrder field_order;

  // 只返回当前 owner 的原三表引用；不存在第二 CareGoal owning 模型。
  CareGoalReadTables goal_read_tables() const noexcept;
  CareGoalWriteTables goal_write_tables() noexcept;
  CareCoverageReadTables coverage_read_tables() const noexcept;
  CareCoverageWriteTables coverage_write_tables() noexcept;
  CarePeriodReadTables period_read_tables() const noexcept;
  CarePeriodWriteTables period_write_tables() noexcept;
};

// 只为真正新局构造本域空初态；已选择table齐全，不安装任何空provider。
// 带入旧档须实际decode，不能调用本口抹去历史或将未选数据默认丢弃。
Outcome<CareRequirementsState>
initialize_care_requirements_state(const PreparedCatalog &,
                                   const StableId &actual_scenario_id);
// 共同选择形状先于任何 table 遍历；真实source/PIN/producer另行联合末验。
Status
validate_care_requirements_state_shape(const CareRequirementsState &,
                                       const PreparedCatalog &,
                                       const StableId &actual_scenario_id);
// 三张原教育表的有限双向引用与必要时序；不代替实际Common头、guardian、
// 知识、老师或预算源验证，不能凭该关系门授课。
Status validate_care_education_table_relations(const CareRequirementsState &);
// 同一实际根binary出口，JSON/projection共用对应字段表。
Status visit_care_requirements_state(CodecVisitor &, CareRequirementsState &,
                                     const PreparedCatalog &,
                                     const StableId &actual_scenario_id);
// 原JSON冷读只接同job scope；Location仍由Common唯一codec及真实目录选择。
Outcome<CareRequirementsState>
decode_care_requirements_state(const BoundaryValue &, const PreparedCatalog &,
                               const StableId &actual_scenario_id,
                               ScopedCheckpointJsonDecode &);
Status emit_care_requirements_state_fields(ProjectionWriter &,
                                           const CareRequirementsState &,
                                           const PreparedCatalog &,
                                           const StableId &actual_scenario_id);
Outcome<BoundaryValue>
encode_care_requirements_state(const CareRequirementsState &,
                               const PreparedCatalog &,
                               const StableId &actual_scenario_id);
} // namespace hanmo::resident
