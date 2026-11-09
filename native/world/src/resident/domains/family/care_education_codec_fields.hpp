// v1b / v1e 教育三表唯一字段表；已知源、控制源仍保原强型引用。
#pragma once
#include "care_clearance_history_values_codec_fields.hpp"
#include "care_education.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/family/care_education.hpp"
#include "care_facility_transport_acceptance_codec_fields.hpp"
#include "care_maturity_record_codec_fields.hpp"
namespace hanmo::resident::care_education_codec {
using care_clearance_origin_codec::select;
using care_facility_transport_codec::fixed_text;
using care_requirement_codec::moment_source;
using care_requirement_codec::rule;
using care_requirement_codec::text;
// 只有声明过的 enum 值可读；不把未知值转换成默认通过分支。
template <class C, class E>
Status enumeration(C &c, const char *name, E &value, Integer last) {
  Integer tag = static_cast<Integer>(value);
  c.field(name, tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > last)
    return c.reject("InvalidEducationEnum", name);
  if (c.reading())
    value = static_cast<E>(tag);
  return c.status();
}
// SubjectRef 保原 kind 与稳定 ID；NPC 的 actor 限制另由真实表达关系门核验。
template <class C> Status subject(C &c, SubjectRef &v) {
  enumeration(c, "kind", v.kind, 2);
  return text(c, "id", v.id);
}
// CatalogIdentity 调用 Character 原字段表，不复制目录摘要或新增权源内容。
template <class C> Status catalog_field(C &c, LifeCatalogFieldRef &v) {
  c.object("catalog_identity", [&](auto &a) {
    return character_origin_codec::catalog(a, v.catalog_identity);
  });
  text(c, "scenario_id", v.scenario_id);
  text(c, "entity_id", v.entity_id);
  return text(c, "field_selector", v.field_selector);
}
// venue/schedule 只允许原锁字段或原业务source，两支均非当前值快照。
template <class C>
Status
authored_or_source(C &c,
                   std::variant<LifeCatalogFieldRef, MomentOwnerSource> &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidEducationOriginTag");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return catalog_field(c, std::get<0>(v));
  return moment_source(c, std::get<1>(v));
}
// 五种来源是实际 Profile owner 的原键，不创建额外 controlled_actor 布尔量。
template <class C> Status control(C &c, ProfileControllerSource &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 4)
    return c.reject("InvalidFamilyControlSourceTag");
  if (c.reading()) {
    Status ready = common_success();
    switch (tag) {
    case 0:
      ready = select<0>(c, v);
      break;
    case 1:
      ready = select<1>(c, v);
      break;
    case 2:
      ready = select<2>(c, v);
      break;
    case 3:
      ready = select<3>(c, v);
      break;
    case 4:
      ready = select<4>(c, v);
      break;
    }
    if (!ready.ok)
      return ready;
  }
  return std::visit(
      [&](auto &x) -> Status {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, AuthoredProfileController>)
          text(c, "scenario_id", x.scenario_id);
        else if constexpr (std::is_same_v<T, CreatedProfileController>)
          text(c, "creation_fact_id", x.creation_fact_id);
        else if constexpr (std::is_same_v<T, SuccessionProfileController>)
          text(c, "succession_fact_id", x.succession_fact_id);
        else if constexpr (std::is_same_v<T, LegacyProfileController>) {
          text(c, "activation_fact_id", x.activation_fact_id);
          text(c, "consent_fact_id", x.consent_fact_id);
        } else {
          text(c, "activation_id", x.activation_id);
          text(c, "pending_id", x.pending_id);
        }
        text(c, "actor_id", x.actor_id);
        if constexpr (std::is_same_v<T, ContinuationProfileController>) {
          c.field("control_epoch", x.control_epoch);
          if (c.status().ok && x.control_epoch < 1)
            return c.reject("InvalidFamilyControlEpoch");
        }
        return c.status();
      },
      v);
}
// 路由保存原控制来源或原 Director 采用键；这里不发起 Director command。
template <class C> Status route(C &c, FamilyExpressionRoute &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidFamilyExpressionRoute");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return c.object("original_control_source_ref", [&](auto &a) {
      return control(a, std::get<0>(v).original_control_source_ref);
    });
  auto &x = std::get<1>(v);
  c.object("subject", [&](auto &a) { return subject(a, x.subject); });
  c.field("registration_epoch", x.registration_epoch);
  c.field("decision_seq", x.decision_seq);
  return c.object("policy_ref", [&](auto &a) {
    return care_requirement_codec::source(a, x.policy_ref);
  });
}
// KQ1来源不把发送方已发自动转换为接收方已知；contact/disclosure 保各原地址。
template <class C> Status knowledge(C &c, FamilyKnownBasisRef &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidFamilyKnownBasisTag");
  if (c.reading()) {
    auto ready = tag == 0   ? select<0>(c, v)
                 : tag == 1 ? select<1>(c, v)
                            : select<2>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0) {
    auto &x = std::get<0>(v);
    enumeration(c, "kind", x.kind, 3);
    text(c, "source_id", x.source_id);
    return c.optional("source_subject", x.source_subject,
                      [](auto &a, auto &x) { return subject(a, x); });
  }
  if (tag == 1)
    return c.object("original_contact_ref", [&](auto &a) {
      return moment_source(a, std::get<1>(v).original_contact_ref);
    });
  return c.object("original_disclosure_ref", [&](auto &a) {
    return moment_source(a, std::get<2>(v).original_disclosure_ref);
  });
}
// 有限决策只保存实际规则与八个以内的输入，不保存自报分数。
template <class C> Status decision(C &c, FamilyExpressionDecision &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidFamilyExpressionDecision");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return c.status();
  auto &x = std::get<1>(v);
  c.object("family_policy_ref",
           [&](auto &a) { return rule(a, x.family_policy_ref); });
  text(c, "rule_branch", x.rule_branch);
  c.sequence("source_input_refs", x.source_input_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  return c.status().ok && x.source_input_refs.size() > 8
             ? c.reject("FamilyDecisionSourceLimit")
             : c.status();
}
// 预算是本次许可上限而非预付；未成年人/教育的NoFee关系另核。
template <class C> Status budget(C &c, FamilyCommunicationBudget &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidFamilyCommunicationBudget");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return c.status();
  auto &x = std::get<1>(v);
  c.field("max_money", x.max_money);
  return c.status().ok && x.max_money < 0
             ? c.reject("InvalidFamilyCommunicationCap")
             : c.status();
}
// 原十一字段从一个表生成三种出口，含不可替换的真实Common command绑定。
template <class C> Status expression(C &c, FamilyPersonalExpression2 &v) {
  c.field("source_schema", v.source_schema);
  text(c, "actor_id", v.actor_id);
  c.object("expressed_at", [&](auto &a) {
    return family_life4_codec::moment(a, v.expressed_at);
  });
  c.field("committed_revision", v.committed_revision);
  c.object("command_binding", [&](auto &a) {
    return care_handover_codec::command_binding(a, v.command_binding);
  });
  c.object("route_basis", [&](auto &a) { return route(a, v.route_basis); });
  c.sequence("known_basis_refs", v.known_basis_refs,
             [](auto &a, auto &x) { return knowledge(a, x); });
  c.object("decision_basis",
           [&](auto &a) { return decision(a, v.decision_basis); });
  c.object("communication_budget",
           [&](auto &a) { return budget(a, v.communication_budget); });
  c.object("communication_audience", [&](auto &a) {
    return enumeration(a, "tag", v.communication_audience, 1);
  });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"source_schema", "actor_id", "expressed_at", "committed_revision",
       "command_binding", "route_basis", "known_basis_refs", "decision_basis",
       "communication_budget", "communication_audience", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_family_personal_expression2_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
// 原确切计划条款与选择source版本，不以当前revision取代。
template <class C> Status plan_ref(C &c, EducationPlanRef &v) {
  text(c, "plan_id", v.plan_id);
  c.field("terms_version", v.terms_version);
  return c.status().ok && v.terms_version != 1
             ? c.reject("EducationTermsVersion1Required")
             : c.status();
}
template <class C> Status choice_ref(C &c, EducationChoiceRef &v) {
  text(c, "exact_choice_id", v.exact_choice_id);
  c.field("source_schema", v.source_schema);
  return c.status().ok && v.source_schema != 2
             ? c.reject("EducationChoiceSchema2Required")
             : c.status();
}
template <class C> Status activation_ref(C &c, EducationActivationRef &v) {
  text(c, "exact_activation_id", v.exact_activation_id);
  c.field("source_schema", v.source_schema);
  return c.status().ok && v.source_schema != 2
             ? c.reject("EducationActivationSchema2Required")
             : c.status();
}
// EducationFocusRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status focus(C &c, EducationFocusRef &v) {
  return c.object("original_focus_rule_ref",
                  [&](auto &a) { return rule(a, v.original_focus_rule_ref); });
}
// EducationGuardianAuthorityRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status guardian(C &c, EducationGuardianAuthorityRef &v) {
  return c.object("original_authority_ref", [&](auto &a) {
    return moment_source(a, v.original_authority_ref);
  });
}
// EducationOriginalWillingnessRef 的原角色地址，不接受其它角色的隐式互转。
template <class C>
Status willingness(C &c, EducationOriginalWillingnessRef &v) {
  return c.object("original_willingness_ref", [&](auto &a) {
    return moment_source(a, v.original_willingness_ref);
  });
}
// EducationTeacherRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status teacher(C &c, EducationTeacherRef &v) {
  return c.object("original_teacher_ref", [&](auto &a) {
    return moment_source(a, v.original_teacher_ref);
  });
}
// EducationMaterialPlanRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status material(C &c, EducationMaterialPlanRef &v) {
  return c.object("original_material_plan_ref", [&](auto &a) {
    return moment_source(a, v.original_material_plan_ref);
  });
}
// EducationVenueRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status venue(C &c, EducationVenueRef &v) {
  return c.object("original_venue_ref", [&](auto &a) {
    return authored_or_source(a, v.original_venue_ref);
  });
}
// EducationScheduleRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status schedule(C &c, EducationScheduleRef &v) {
  return c.object("original_schedule_ref", [&](auto &a) {
    return authored_or_source(a, v.original_schedule_ref);
  });
}
// EducationBudgetAuthorityRef 的原角色地址，不接受其它角色的隐式互转。
template <class C>
Status budget_authority(C &c, EducationBudgetAuthorityRef &v) {
  return c.object("original_budget_authority_ref", [&](auto &a) {
    return moment_source(a, v.original_budget_authority_ref);
  });
}
// EducationCourseOrActivityRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status course(C &c, EducationCourseOrActivityRef &v) {
  return c.object("original_course_or_activity_ref", [&](auto &a) {
    return moment_source(a, v.original_course_or_activity_ref);
  });
}
// EducationTerminalSourceRef 的原角色地址，不接受其它角色的隐式互转。
template <class C> Status terminal(C &c, EducationTerminalSourceRef &v) {
  return c.object("original_terminal_source_ref", [&](auto &a) {
    return moment_source(a, v.original_terminal_source_ref);
  });
}
// 原十六字段共同前缀；schema1的意愿必有，schema2指真实Choice且可未形成。
template <class C, class T> Status plan_fields(C &c, T &v) {
  text(c, "plan_id", v.plan_id);
  text(c, "child_actor_id", v.child_actor_id);
  c.object("age_basis_ref",
           [&](auto &a) { return life_age_basis_fields(a, v.age_basis_ref); });
  care_maturity_codec::stage(c, "age_stage", v.age_stage);
  c.sequence("focus_refs", v.focus_refs,
             [](auto &a, auto &x) { return focus(a, x); });
  c.object("guardian_authority_ref",
           [&](auto &a) { return guardian(a, v.guardian_authority_ref); });
  if constexpr (std::is_same_v<T, EducationPlan>)
    c.object("willingness_ref",
             [&](auto &a) { return willingness(a, v.willingness_ref); });
  else
    c.optional("willingness_ref", v.willingness_ref,
               [](auto &a, auto &x) { return choice_ref(a, x); });
  c.optional("teacher_ref", v.teacher_ref,
             [](auto &a, auto &x) { return teacher(a, x); });
  c.sequence("material_plan_refs", v.material_plan_refs,
             [](auto &a, auto &x) { return material(a, x); });
  c.object("venue_ref", [&](auto &a) { return venue(a, v.venue_ref); });
  c.object("schedule_ref",
           [&](auto &a) { return schedule(a, v.schedule_ref); });
  c.object("budget_authority_ref", [&](auto &a) {
    return budget_authority(a, v.budget_authority_ref);
  });
  c.sequence("original_course_or_activity_refs",
             v.original_course_or_activity_refs,
             [](auto &a, auto &x) { return course(a, x); });
  enumeration(c, "state", v.state, 5);
  c.field("revision", v.revision);
  c.optional("terminal_source_ref", v.terminal_source_ref,
             [](auto &a, auto &x) { return terminal(a, x); });
  if (c.status().ok && v.revision < 1)
    return c.reject("InvalidEducationRevision");
  return c.status();
}
// schema2追加原形成来源；不允许只看state编造willingness或activation。
template <class C> Status plan2(C &c, EducationPlan2 &v) {
  plan_fields(c, v);
  c.field("terms_version", v.terms_version);
  c.object("creation_expression",
           [&](auto &a) { return expression(a, v.creation_expression); });
  c.optional("activation_ref", v.activation_ref,
             [](auto &a, auto &x) { return activation_ref(a, x); });
  care_clearance_history_codec::exact_order(c, v.field_order,
                                            {"plan_id",
                                             "child_actor_id",
                                             "age_basis_ref",
                                             "age_stage",
                                             "focus_refs",
                                             "guardian_authority_ref",
                                             "willingness_ref",
                                             "teacher_ref",
                                             "material_plan_refs",
                                             "venue_ref",
                                             "schedule_ref",
                                             "budget_authority_ref",
                                             "original_course_or_activity_refs",
                                             "state",
                                             "revision",
                                             "terminal_source_ref",
                                             "terms_version",
                                             "creation_expression",
                                             "activation_ref",
                                             "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_education_plan2_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
// 版本仅由payload实际分支选择；未知或未选版本在构造前拒绝。
template <class C>
Status plan(C &c, VersionedEducationPlan &v, Integer physical_version) {
  if (physical_version == 3) {
    if (c.reading()) {
      auto ready = select<0>(c, v.payload);
      if (!ready.ok)
        return ready;
    }
    if (v.payload.index() != 0 || !v.field_order.empty())
      return c.reject("Care3OriginalEducationPlanRequired");
    return plan_fields(c, std::get<0>(v.payload));
  }
  if (physical_version != 4 && physical_version != 5)
    return c.reject("CareEducationProfileRequired");
  Integer schema = static_cast<Integer>(v.payload.index()) + 1;
  c.field("payload_schema", schema);
  if (!c.status().ok)
    return c.status();
  if (schema < 1 || schema > 2)
    return c.reject("InvalidEducationPayloadSchema");
  if (c.reading()) {
    auto ready =
        schema == 1 ? select<0>(c, v.payload) : select<1>(c, v.payload);
    if (!ready.ok)
      return ready;
  }
  c.object("payload", [&](auto &a) {
    if (schema == 1)
      return plan_fields(a, std::get<0>(v.payload));
    return plan2(a, std::get<1>(v.payload));
  });
  return care_clearance_history_codec::exact_order(
      c, v.field_order, {"payload_schema", "payload", "field_order"});
}
// Choice不携带额外ID；真实Common表key是唯一事实键。
template <class C> Status choice(C &c, EducationChoice2 &v) {
  fixed_text(c, "kind", "family_education_chosen");
  c.object("plan_ref", [&](auto &a) { return plan_ref(a, v.plan_ref); });
  enumeration(c, "decision", v.decision, 1);
  c.object("expression", [&](auto &a) { return expression(a, v.expression); });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"kind", "plan_ref", "decision", "expression", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_education_choice2_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
// Activation只指真实同计划accepted Choice，不复制孩子表达或课程XP。
template <class C> Status activation(C &c, EducationActivation2 &v) {
  fixed_text(c, "kind", "family_education_activated");
  c.object("plan_ref", [&](auto &a) { return plan_ref(a, v.plan_ref); });
  c.object("willingness_source_ref",
           [&](auto &a) { return choice_ref(a, v.willingness_source_ref); });
  c.object("expression", [&](auto &a) { return expression(a, v.expression); });
  care_clearance_history_codec::exact_order(c, v.field_order,
                                            {"kind", "plan_ref",
                                             "willingness_source_ref",
                                             "expression", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_education_activation2_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_education_codec
