// FH §5.5 承运接受唯一字段表；完整根各存读路径共用，不另造接受账。
#pragma once
#include "care_body_scope_codec_fields.hpp"
#include "care_clearance_origin_codec_fields.hpp"
#include "care_facility_transport_acceptance.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_facility_use_codec_fields.hpp"
#include "care_handover_execution_codec_fields.hpp"
#include "care_transport_carrier_codec_fields.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"

namespace hanmo::resident::care_facility_transport_codec {
// 原 kind 不占第二可变成员；codec 必须精确读取，不接受任意命令/来源 kind。
template <class C>
Status fixed_text(C &c, const char *name, std::string_view expected) {
  StableId actual;
  if (!c.reading())
    actual = expected;
  c.field(name, actual);
  if (c.status().ok && actual != expected)
    return c.reject("CareTransportFixedKindMismatch", name);
  return c.status();
}
// 目的地分支由 receiving_kind 唯一选中，business_ref 中不重加 selector。
template <class C>
Status receiving_business(C &c, CareReceivingBusinessRef &business_ref,
                          const char *wire_field) {
  Integer kind = static_cast<Integer>(business_ref.index());
  c.field("receiving_kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind < 0 || kind > 2)
    return c.reject("InvalidCareTransportReceivingKind");
  if (c.reading()) {
    auto ready =
        kind == 0   ? care_clearance_origin_codec::select<0>(c, business_ref)
        : kind == 1 ? care_clearance_origin_codec::select<1>(c, business_ref)
                    : care_clearance_origin_codec::select<2>(c, business_ref);
    if (!ready.ok)
      return ready;
  }
  c.object(wire_field, [&](auto &nested) {
    return std::visit(
        [&](auto &ref) -> Status {
          using T = std::decay_t<decltype(ref)>;
          if constexpr (std::is_same_v<T, CareArrangementRef>)
            return care_requirement_codec::text(
                nested, "accepted_arrangement_id", ref.accepted_arrangement_id);
          else if constexpr (std::is_same_v<T, CareMedicalReceivingRef>)
            return care_requirement_codec::text(nested, "case_id", ref.case_id);
          else {
            care_requirement_codec::text(nested, "agreement_id",
                                         ref.agreement_id);
            return care_requirement_codec::text(nested, "line_id", ref.line_id);
          }
        },
        business_ref);
  });
  return c.status();
}
// 接受记录追加自己的原受领来源与place，不复制清场完成位置。
template <class C>
Status local_receiving(C &c, CareTransportLocalReceiving &v) {
  receiving_business(c, v.business_ref, "business_ref");
  c.object("accepted_receiving_ref", [&](auto &nested) {
    return care_requirement_codec::moment_source(nested,
                                                 v.accepted_receiving_ref);
  });
  return care_requirement_codec::text(c, "place_ref", v.place_ref);
}
// 所有原已成立责任地址完整持值；这里不把后发接受判为知识来源。
template <class C> Status knowledge(C &c, CareTransportKnowledgeBasis &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidCareTransportKnowledgeTag");
  if (c.reading()) {
    auto ready = tag == 0   ? care_clearance_origin_codec::select<0>(c, v)
                 : tag == 1 ? care_clearance_origin_codec::select<1>(c, v)
                            : care_clearance_origin_codec::select<2>(c, v);
    if (!ready.ok)
      return ready;
  }
  return std::visit(
      [&](auto &value) -> Status {
        using T = std::decay_t<decltype(value)>;
        const auto source = [&](const char *name, auto &ref) {
          return c.object(name, [&](auto &nested) {
            return care_requirement_codec::moment_source(nested, ref);
          });
        };
        if constexpr (std::is_same_v<T, CareTransportMedicalKnowledge>) {
          care_requirement_codec::text(c, "case_id", value.case_id);
          care_requirement_codec::text(c, "referral_id", value.referral_id);
          source("authorized_viewer_ref", value.authorized_viewer_ref);
          return source("original_offer_source_ref",
                        value.original_offer_source_ref);
        } else if constexpr (std::is_same_v<T,
                                            CareTransportResidenceKnowledge>) {
          care_requirement_codec::text(c, "agreement_id", value.agreement_id);
          care_requirement_codec::text(c, "line_id", value.line_id);
          return source("actual_provider_or_departure_mandate_ref",
                        value.actual_provider_or_departure_mandate_ref);
        } else {
          care_requirement_codec::text(c, "accepted_care_terms_ref",
                                       value.accepted_care_terms_ref);
          return care_requirement_codec::text(c, "accepted_caregiver_ref",
                                              value.accepted_caregiver_ref);
        }
      },
      v);
}
// 原费用权源始终委托 S1 唯一 codec，不复制其公共预算/私约/自资等 wire。
inline Status authority(CodecVisitor &c, TransportExpenseAuthority &v) {
  return visit_transport_expense_authority(c, v);
}
template <class C> Status authority(C &c, TransportExpenseAuthority &v) {
  return c.foreign_record(
      v,
      [](const BoundaryValue &raw, ScopedCheckpointJsonDecode &budget) {
        return decode_transport_expense_authority(raw, budget);
      },
      emit_transport_expense_authority_fields);
}
template <class C> Status expense(C &c, CareTransportExpenseBasis &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCareTransportExpenseTag");
  if (c.reading()) {
    auto ready = tag == 0 ? care_clearance_origin_codec::select<0>(c, v)
                          : care_clearance_origin_codec::select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return c.status();
  auto &value = std::get<CareTransportOriginalExpense>(v);
  c.object("authority",
           [&](auto &nested) { return authority(nested, value.authority); });
  return c.object("accepted_budget_or_fee_terms_ref", [&](auto &nested) {
    return care_requirement_codec::moment_source(
        nested, value.accepted_budget_or_fee_terms_ref);
  });
}
// 本人/路由由外层唯一 operator 绑定，精确保存接受时的年龄与身体来源。
template <class C>
Status decision(C &c, CareTransportActorDecision &v, const StableId &actor) {
  fixed_text(c, "command_kind", "AcceptFacilityClearanceTransport");
  c.field("request_seq", v.request_seq);
  care_requirement_codec::text(c, "result_id", v.result_id);
  c.object("route_basis", [&](auto &nested) {
    return care_handover_codec::actor_route(nested, v.route_basis, actor);
  });
  c.object("age_basis_ref", [&](auto &nested) {
    return life_age_basis_fields(nested, v.age_basis_ref);
  });
  c.sequence("condition_source_refs", v.condition_source_refs,
             [](auto &nested, auto &ref) {
               return care_body_scope_codec::condition(nested, ref);
             });
  c.object("expressed_at", [&](auto &nested) {
    return family_life4_codec::moment(nested, v.expressed_at);
  });
  care_handover_codec::order(c, v.field_order);
  if (c.status().ok && (v.request_seq < 0 || v.condition_source_refs.empty()))
    return c.reject("InvalidCareTransportActorDecision");
  return c.status();
}
// 真实撤回是原接受内的终点；不能使用旧接受身体 frame 或不同命令。
template <class C>
Status withdrawal(C &c, CareTransportWithdrawal &v, const StableId &actor) {
  care_requirement_codec::text(c, "withdrawal_id", v.withdrawal_id);
  fixed_text(c, "kind", "facility_transport_withdrawn");
  c.object("occurred_at", [&](auto &nested) {
    return family_life4_codec::moment(nested, v.occurred_at);
  });
  c.object("command_binding", [&](auto &nested) {
    return care_handover_codec::command_binding(nested, v.command_binding);
  });
  c.object("route_basis", [&](auto &nested) {
    return care_handover_codec::actor_route(nested, v.route_basis, actor);
  });
  Integer reason = static_cast<Integer>(v.reason);
  c.field("reason", reason);
  if (!c.status().ok)
    return c.status();
  if (reason < 0 || reason > 1)
    return c.reject("InvalidCareTransportWithdrawalReason");
  if (c.reading())
    v.reason = static_cast<CareTransportWithdrawalReason>(reason);
  c.object("actor_capability_inputs", [&](auto &nested) {
    return care_handover_codec::capability(nested, v.actor_capability_inputs);
  });
  return care_handover_codec::order(c, v.field_order);
}
// 十七字段按 FH 唯一原序；所有 exporter/decoder 直接复用此表。
template <class C> Status acceptance(C &c, FacilityTransportAcceptance &v) {
  care_requirement_codec::text(c, "acceptance_id", v.acceptance_id);
  c.field("source_schema", v.source_schema);
  c.object("obligation", [&](auto &nested) {
    return care_clearance_origin_codec::obligation(nested, v.obligation);
  });
  c.object("clearance_rule_ref", [&](auto &nested) {
    return care_requirement_codec::rule(nested, v.clearance_rule_ref);
  });
  c.object("scope", [&](auto &nested) {
    return care_body_scope_codec::scope(nested, v.scope);
  });
  c.object("carrier", [&](auto &nested) {
    return care_transport_carrier_codec::carrier(nested, v.carrier);
  });
  c.object("destination", [&](auto &nested) {
    Integer tag = 0;
    nested.field("tag", tag);
    if (nested.status().ok && tag != 0)
      return nested.reject("CareTransportLocalReceivingRequired");
    return local_receiving(nested, v.destination);
  });
  care_requirement_codec::text(c, "operator_actor_id", v.operator_actor_id);
  c.object("operator_authority_ref", [&](auto &nested) {
    return care_requirement_codec::moment_source(nested,
                                                 v.operator_authority_ref);
  });
  c.object("knowledge_basis",
           [&](auto &nested) { return knowledge(nested, v.knowledge_basis); });
  care_arrangement_codec::moment_refs(c, "patient_or_care_consent_refs",
                                      v.patient_or_care_consent_refs);
  c.object("expense_basis",
           [&](auto &nested) { return expense(nested, v.expense_basis); });
  c.object("accepted_at", [&](auto &nested) {
    return family_life4_codec::moment(nested, v.accepted_at);
  });
  c.object("actor_decision", [&](auto &nested) {
    return decision(nested, v.actor_decision, v.operator_actor_id);
  });
  c.object("actor_capability_inputs", [&](auto &nested) {
    return care_handover_codec::capability(nested, v.actor_capability_inputs);
  });
  c.optional("withdrawal", v.withdrawal, [&](auto &nested, auto &value) {
    return withdrawal(nested, value, v.operator_actor_id);
  });
  care_handover_codec::order(c, v.field_order);
  if (!c.status().ok)
    return c.status();
  const auto valid = validate_care_facility_transport_acceptance_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_facility_transport_codec
