// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 完整 Care 根教育行的局部不变量；不产生原同意、课程或来源验证成功。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_education.hpp"
#include "care_education_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"

namespace hanmo::resident {
namespace {
// 原来源地址只核形状；未知 owner/kind 不在此获得任何业务资格。
bool source_shape(const MomentOwnerSource &value) {
  return !value.source_ref.domain.empty() && !value.source_ref.kind.empty() &&
         !value.source_ref.id.empty() && value.source_schema > 0;
}
// 只有对应计划的真正 source2 地址可落这两个新 Optional。
bool choice_shape(const EducationChoiceRef &value) {
  return !value.exact_choice_id.empty() && value.source_schema == 2;
}
// 教育固定不授权付费传播或告知中间信使，成人guardian也不能绕过该叶边界。
Status education_expression(const FamilyPersonalExpression2 &value) {
  auto valid = validate_family_personal_expression2_relations(value);
  if (!valid.ok)
    return valid;
  if (!std::holds_alternative<FamilyNoFeeCommunication>(
          value.communication_budget) ||
      value.communication_audience != FamilyCommunicationAudience::direct_only)
    return common_failure("EducationNoFeeDirectExpressionRequired",
                          value.actor_id);
  return common_success();
}
} // namespace

// 只核有限分支之间的角色、次数和地址关系，不证明原NPC采用或原知情。
Status validate_family_personal_expression2_relations(
    const FamilyPersonalExpression2 &v) {
  const auto reject = [&](const char *code) {
    return common_failure(code, v.actor_id);
  };
  if (v.source_schema != 2 || v.actor_id.empty() || v.committed_revision < 1 ||
      v.command_binding.command_kind.empty() ||
      v.command_binding.result_id.empty() ||
      v.command_binding.request_seq < 1 || v.known_basis_refs.size() > 8)
    return reject("InvalidFamilyPersonalExpressionIdentity");
  if (const auto *npc = std::get_if<FamilyNpcExpressionRoute>(&v.route_basis)) {
    if (npc->subject.kind != SubjectKind::actor ||
        npc->subject.id != v.actor_id || npc->registration_epoch < 1 ||
        npc->decision_seq < 1 || npc->policy_ref.id.empty() ||
        npc->policy_ref.domain != "director" ||
        npc->policy_ref.kind != "actor_admission" ||
        npc->policy_ref.id.size() > 256 ||
        !std::holds_alternative<FamilyFiniteNpcDecision>(v.decision_basis))
      return reject("InvalidFamilyNpcExpressionRoute");
  } else {
    const auto &player = std::get<FamilyPlayerExpressionRoute>(v.route_basis);
    const bool same_actor = std::visit(
        [&](const auto &source) { return source.actor_id == v.actor_id; },
        player.original_control_source_ref);
    if (!same_actor ||
        !std::holds_alternative<FamilyExplicitPlayerDecision>(v.decision_basis))
      return reject("InvalidFamilyPlayerExpressionRoute");
  }
  if (const auto *npc =
          std::get_if<FamilyFiniteNpcDecision>(&v.decision_basis)) {
    if (npc->family_policy_ref.id.empty() ||
        npc->family_policy_ref.version < 1 ||
        npc->family_policy_ref.catalog_digest.empty() ||
        npc->rule_branch.empty() || npc->source_input_refs.size() > 8)
      return reject("InvalidFamilyFiniteDecision");
    for (const auto &source : npc->source_input_refs)
      if (!source_shape(source))
        return reject("InvalidFamilyFiniteDecisionSource");
  }
  if (const auto *budget =
          std::get_if<FamilyOwnWalletCommunicationCap>(&v.communication_budget);
      budget && budget->max_money < 0)
    return reject("InvalidFamilyCommunicationBudget");
  if (v.communication_audience != FamilyCommunicationAudience::direct_only &&
      v.communication_audience !=
          FamilyCommunicationAudience::accepted_carrier_delivery)
    return reject("InvalidFamilyCommunicationAudience");
  return common_success();
}

// 原不可变创建值和后续实际形成引用保持分离，不按state补choice/activation。
Status validate_education_plan2_relations(const EducationPlan2 &v) {
  auto valid = education_expression(v.creation_expression);
  if (!valid.ok)
    return valid;
  if (v.plan_id.empty() || v.child_actor_id.empty() || v.terms_version != 1 ||
      v.revision < 0 || v.creation_expression.actor_id == v.child_actor_id)
    return common_failure("InvalidEducationPlan2Identity", v.plan_id);
  if (v.willingness_ref && !choice_shape(*v.willingness_ref))
    return common_failure("InvalidEducationPlanChoiceRef", v.plan_id);
  if (v.activation_ref &&
      (v.activation_ref->exact_activation_id.empty() ||
       v.activation_ref->source_schema != 2 || !v.willingness_ref))
    return common_failure("InvalidEducationPlanActivationRef", v.plan_id);
  if ((v.state == EducationPlanState::accepted ||
       v.state == EducationPlanState::active ||
       v.state == EducationPlanState::paused ||
       v.state == EducationPlanState::completed) &&
      (!v.willingness_ref || !v.activation_ref))
    return common_failure("EducationActualFormationRefsRequired", v.plan_id);
  if (v.state == EducationPlanState::proposed && v.activation_ref)
    return common_failure("EducationProposedPlanHasActivation", v.plan_id);
  if ((v.state == EducationPlanState::completed ||
       v.state == EducationPlanState::stopped) &&
      !v.terminal_source_ref)
    return common_failure("EducationTerminalSourceRequired", v.plan_id);
  return common_success();
}

// 本人的真实收到/年龄/表达能力必须由原reader验证，局部值不代答复授权。
Status validate_education_choice2_relations(const EducationChoice2 &v) {
  auto valid = education_expression(v.expression);
  if (!valid.ok)
    return valid;
  if (v.plan_ref.plan_id.empty() || v.plan_ref.terms_version != 1 ||
      (v.decision != EducationChoiceDecision::accepted &&
       v.decision != EducationChoiceDecision::refused))
    return common_failure("InvalidEducationChoice2", v.plan_ref.plan_id);
  return common_success();
}

// 同计划真实选择只存一次引用，不能以本表达创造老师或预算的接受。
Status validate_education_activation2_relations(const EducationActivation2 &v) {
  auto valid = education_expression(v.expression);
  if (!valid.ok)
    return valid;
  if (v.plan_ref.plan_id.empty() || v.plan_ref.terms_version != 1 ||
      !choice_shape(v.willingness_source_ref))
    return common_failure("InvalidEducationActivation2", v.plan_ref.plan_id);
  return common_success();
}
// 默认深值沿每个实际成员计，不复制根或保存borrowed source context。


// VersionedEducationPlan 的三个出口复用同一原字段表。
Status visit_care_education_plan(CodecVisitor &c, VersionedEducationPlan &v,
                                 Integer version) {
  return care_education_codec::plan(c, v, version);
}
Status emit_care_education_plan_fields(ProjectionWriter &writer,
                                       const VersionedEducationPlan &v,
                                       Integer version) {
  ActorFoundationProjectionArchive c(writer);
  return care_education_codec::plan(c, const_cast<VersionedEducationPlan &>(v),
                                    version);
}
Outcome<BoundaryValue>
encode_care_education_plan(const VersionedEducationPlan &v, Integer version) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto status = w.object("", [&](ProjectionWriter &c) {
    return emit_care_education_plan_fields(c, v, version);
  });
  if (status.ok)
    status = w.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<VersionedEducationPlan>
decode_care_education_plan(const BoundaryValue &raw, Integer version) {
  if (auto *budget = ScopedCheckpointJsonDecode::current())
    budget->default_values<VersionedEducationPlan>();
  VersionedEducationPlan out;
  ActorFoundationJsonVisitor c(raw);
  auto status = care_education_codec::plan(c, out, version);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(out), status.error};
}
// EducationChoice2 的三个出口复用同一原字段表。
Status visit_care_education_choice(CodecVisitor &c, EducationChoice2 &v) {
  return care_education_codec::choice(c, v);
}
Status emit_care_education_choice_fields(ProjectionWriter &writer,
                                         const EducationChoice2 &v) {
  ActorFoundationProjectionArchive c(writer);
  return care_education_codec::choice(c, const_cast<EducationChoice2 &>(v));
}
Outcome<BoundaryValue> encode_care_education_choice(const EducationChoice2 &v) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto status = w.object("", [&](ProjectionWriter &c) {
    return emit_care_education_choice_fields(c, v);
  });
  if (status.ok)
    status = w.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<EducationChoice2>
decode_care_education_choice(const BoundaryValue &raw) {
  if (auto *budget = ScopedCheckpointJsonDecode::current())
    budget->default_values<EducationChoice2>();
  EducationChoice2 out;
  ActorFoundationJsonVisitor c(raw);
  auto status = care_education_codec::choice(c, out);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(out), status.error};
}
// EducationActivation2 的三个出口复用同一原字段表。
Status visit_care_education_activation(CodecVisitor &c,
                                       EducationActivation2 &v) {
  return care_education_codec::activation(c, v);
}
Status emit_care_education_activation_fields(ProjectionWriter &writer,
                                             const EducationActivation2 &v) {
  ActorFoundationProjectionArchive c(writer);
  return care_education_codec::activation(
      c, const_cast<EducationActivation2 &>(v));
}
Outcome<BoundaryValue>
encode_care_education_activation(const EducationActivation2 &v) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto status = w.object("", [&](ProjectionWriter &c) {
    return emit_care_education_activation_fields(c, v);
  });
  if (status.ok)
    status = w.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<EducationActivation2>
decode_care_education_activation(const BoundaryValue &raw) {
  if (auto *budget = ScopedCheckpointJsonDecode::current())
    budget->default_values<EducationActivation2>();
  EducationActivation2 out;
  ActorFoundationJsonVisitor c(raw);
  auto status = care_education_codec::activation(c, out);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(out), status.error};
}
} // namespace hanmo::resident
