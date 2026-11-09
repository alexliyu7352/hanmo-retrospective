// v1b §12.4 / v1e §2.2、§7.3：完整 Care 根的三张教育表及其局部表达。
// 只有持值，不创建课程、XP、付款、监护权、知情或 Director 采用许可。
#pragma once
#include "care_maturity_record.hpp"
#include "family_communication_options2.hpp"
#include "family_expression_route.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include "resident/domains/social/knowledge_read.hpp"

namespace hanmo::resident {
// 本域确切计划/选择键；条款版本不由当前 plan revision 代替。
struct EducationPlanRef {
  StableId plan_id;
  Integer terms_version = 1;
};
struct EducationChoiceRef {
  StableId exact_choice_id;
  Integer source_schema = 2;
};
struct EducationActivationRef {
  StableId exact_activation_id;
  Integer source_schema = 2;
};

// 各角色保原拥有者的地址，不复制原条款或把同一整数当成所有域的许可。
// 这些 nominal 包装不提供跨角色转换，实际合法 owner/kind 由各原 reader 核验。
struct EducationFocusRef {
  ActorFoundationRuleRef original_focus_rule_ref;
};
struct EducationGuardianAuthorityRef {
  MomentOwnerSource original_authority_ref;
};
struct EducationOriginalWillingnessRef {
  MomentOwnerSource original_willingness_ref;
};
struct EducationTeacherRef {
  MomentOwnerSource original_teacher_ref;
};
struct EducationMaterialPlanRef {
  MomentOwnerSource original_material_plan_ref;
};
// 原 venue/schedule
// 可定位锁内作者字段或已成立的业务来源；不存当前地点/时间副本。 不接受
// LifeOwnedFieldRef/ScenarioOriginRef 的宽泛替代，也不从 free JSON 反推来源。
struct EducationVenueRef {
  std::variant<LifeCatalogFieldRef, MomentOwnerSource> original_venue_ref;
};
struct EducationScheduleRef {
  std::variant<LifeCatalogFieldRef, MomentOwnerSource> original_schedule_ref;
};
struct EducationBudgetAuthorityRef {
  MomentOwnerSource original_budget_authority_ref;
};
struct EducationCourseOrActivityRef {
  MomentOwnerSource original_course_or_activity_ref;
};
struct EducationTerminalSourceRef {
  MomentOwnerSource original_terminal_source_ref;
};

// 本次实际接触/披露与已知源分开；接触自身不证明收到具体 proposal/terms。
struct FamilyActualContactRef {
  MomentOwnerSource original_contact_ref;
};
struct FamilyActualDisclosureRef {
  MomentOwnerSource original_disclosure_ref;
};
using FamilyKnownBasisRef =
    std::variant<KnowledgeSourceRef, FamilyActualContactRef,
                 FamilyActualDisclosureRef>;
struct FamilyExplicitPlayerDecision {};
// 这里只保存有限策略所实际读取的来源，不保存 score 或当前策略状态。
struct FamilyFiniteNpcDecision {
  ActorFoundationRuleRef family_policy_ref;
  StableId rule_branch;
  std::vector<MomentOwnerSource> source_input_refs;
};
using FamilyExpressionDecision =
    std::variant<FamilyExplicitPlayerDecision, FamilyFiniteNpcDecision>;
// 原十一字段，不保 execution_epoch、第二 command 账或持久 ReadStamp。
struct FamilyPersonalExpression2 {
  Integer source_schema = 2;
  StableId actor_id;
  WorldMoment expressed_at;
  Integer committed_revision = 0;
  LifeActualCommandBinding command_binding;
  FamilyExpressionRoute route_basis;
  std::vector<FamilyKnownBasisRef> known_basis_refs;
  FamilyExpressionDecision decision_basis;
  FamilyCommunicationBudget communication_budget;
  FamilyCommunicationAudience communication_audience =
      FamilyCommunicationAudience::direct_only;
  FieldOrder field_order;
};

enum class EducationPlanState {
  proposed,
  accepted,
  active,
  paused,
  completed,
  stopped
};
// schema1 的完整原十六字段；意愿必有真实原来源，不能零补成 schema2 选择。
struct EducationPlan {
  StableId plan_id, child_actor_id;
  LifeAgeBasisRef age_basis_ref;
  CareAgeStage age_stage = CareAgeStage::early_childhood;
  std::vector<EducationFocusRef> focus_refs;
  EducationGuardianAuthorityRef guardian_authority_ref;
  EducationOriginalWillingnessRef willingness_ref;
  std::optional<EducationTeacherRef> teacher_ref;
  std::vector<EducationMaterialPlanRef> material_plan_refs;
  EducationVenueRef venue_ref;
  EducationScheduleRef schedule_ref;
  EducationBudgetAuthorityRef budget_authority_ref;
  std::vector<EducationCourseOrActivityRef> original_course_or_activity_refs;
  EducationPlanState state = EducationPlanState::proposed;
  Integer revision = 0;
  std::optional<EducationTerminalSourceRef> terminal_source_ref;
};
// schema2 保存其真实形成史；可空 willingness 不是 schema1 缺史的迁移许可。
struct EducationPlan2 {
  StableId plan_id, child_actor_id;
  LifeAgeBasisRef age_basis_ref;
  CareAgeStage age_stage = CareAgeStage::early_childhood;
  std::vector<EducationFocusRef> focus_refs;
  EducationGuardianAuthorityRef guardian_authority_ref;
  std::optional<EducationChoiceRef> willingness_ref;
  std::optional<EducationTeacherRef> teacher_ref;
  std::vector<EducationMaterialPlanRef> material_plan_refs;
  EducationVenueRef venue_ref;
  EducationScheduleRef schedule_ref;
  EducationBudgetAuthorityRef budget_authority_ref;
  std::vector<EducationCourseOrActivityRef> original_course_or_activity_refs;
  EducationPlanState state = EducationPlanState::proposed;
  Integer revision = 0;
  std::optional<EducationTerminalSourceRef> terminal_source_ref;
  Integer terms_version = 1;
  FamilyPersonalExpression2 creation_expression;
  std::optional<EducationActivationRef> activation_ref;
  FieldOrder field_order;
};
// payload_schema 唯一由实际分支写为1/2，不存可与payload冲突的第二版本状态。
struct VersionedEducationPlan {
  std::variant<EducationPlan, EducationPlan2> payload;
  FieldOrder field_order;
};
enum class EducationChoiceDecision { accepted, refused };
// 表 key 取真实 Common 事实键；不向原 payload 添加另一个可改 choice_id。
struct EducationChoice2 {
  EducationPlanRef plan_ref;
  EducationChoiceDecision decision = EducationChoiceDecision::refused;
  FamilyPersonalExpression2 expression;
  FieldOrder field_order;
};
// willingness_source_ref 指同 plan 的真实 accepted
// choice，不复制其表达或老师同意。
struct EducationActivation2 {
  EducationPlanRef plan_ref;
  EducationChoiceRef willingness_source_ref;
  FamilyPersonalExpression2 expression;
  FieldOrder field_order;
};
// 三张表共同的局部关系门；不替代原课程/预算/监护/知识的真实来源闭包。
Status validate_family_personal_expression2_relations(
    const FamilyPersonalExpression2 &);
Status validate_education_plan2_relations(const EducationPlan2 &);
Status validate_education_choice2_relations(const EducationChoice2 &);
Status validate_education_activation2_relations(const EducationActivation2 &);
// 原根调用同一行codec，不能把值roundtrip当来源/权限验收。
Status visit_care_education_plan(CodecVisitor &, VersionedEducationPlan &,
                                 Integer version);
Status emit_care_education_plan_fields(ProjectionWriter &,
                                       const VersionedEducationPlan &,
                                       Integer version);
Outcome<BoundaryValue>
encode_care_education_plan(const VersionedEducationPlan &, Integer version);
Outcome<VersionedEducationPlan>
decode_care_education_plan(const BoundaryValue &, Integer version);
Status visit_care_education_choice(CodecVisitor &, EducationChoice2 &);
Status emit_care_education_choice_fields(ProjectionWriter &,
                                         const EducationChoice2 &);
Outcome<BoundaryValue> encode_care_education_choice(const EducationChoice2 &);
Outcome<EducationChoice2> decode_care_education_choice(const BoundaryValue &);
Status visit_care_education_activation(CodecVisitor &, EducationActivation2 &);
Status emit_care_education_activation_fields(ProjectionWriter &,
                                             const EducationActivation2 &);
Outcome<BoundaryValue>
encode_care_education_activation(const EducationActivation2 &);
Outcome<EducationActivation2>
decode_care_education_activation(const BoundaryValue &);
} // namespace hanmo::resident
