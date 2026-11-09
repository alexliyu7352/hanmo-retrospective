// 原 FH 清场期间/边界字段序；共享的 body/route 字段仍调用原 owner codec。
#pragma once
#include "care_clearance_history_values.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_handover_execution_codec_fields.hpp"
#include <initializer_list>
#include <string_view>

namespace hanmo::resident::care_clearance_history_codec {
// 本卡局部值明确持 field_order 字段；非空顺序须涵盖本 payload 的所有真实键。
// 和根的外部顺序元数据分开，不能只去重而让未知键进入冷读状态。
template <class C>
Status exact_order(C &c, FieldOrder &value,
                   std::initializer_list<std::string_view> expected) {
  care_handover_codec::order(c, value);
  if (!c.status().ok || value.empty())
    return c.status();
  if (value.size() != expected.size())
    return c.reject("CareValueFieldOrderMismatch");
  for (const auto &name : value) {
    bool found = false;
    for (const auto key : expected)
      if (name == key)
        found = true;
    if (!found)
      return c.reject("CareValueFieldOrderMismatch", name);
  }
  return c.status();
}

// 保留原期间/来源类型，不把清场执行装进 handover record 骗过其 validator。
template <class C> Status period(C &c, FacilityClearanceWorkPeriod2 &value) {
  care_requirement_codec::text(c, "source_id", value.source_id);
  c.field("source_schema", value.source_schema);
  care_requirement_codec::text(c, "execution_ref", value.execution_ref);
  c.object("work_period_ref", [&](auto &x) {
    return fixed_period_fields::ref(x, value.work_period_ref);
  });
  c.object("provided_at", [&](auto &x) {
    return family_life4_codec::moment(x, value.provided_at);
  });
  c.sequence("worker_capability_inputs", value.worker_capability_inputs,
             [](auto &x, auto &frame) {
               return care_handover_codec::capability(x, frame);
             });
  c.sequence("worker_condition_receipt_refs",
             value.worker_condition_receipt_refs, [](auto &x, auto &ref) {
               return care_requirement_codec::text(x, "result_id",
                                                   ref.result_id);
             });
  exact_order(c, value.field_order,
              {"source_id", "source_schema", "execution_ref", "work_period_ref",
               "provided_at", "worker_capability_inputs",
               "worker_condition_receipt_refs", "field_order"});
  if (!c.status().ok)
    return c.status();
  const auto *binding =
      std::get_if<ActorActionWorkBinding>(&value.work_period_ref.binding);
  const auto at =
      compare_world_moments(value.provided_at, value.work_period_ref.until);
  if (value.source_schema != 2 || !binding ||
      binding->action_kind != "facility_clearance" || !at.ok || at.value != 0 ||
      value.worker_capability_inputs.empty())
    return c.reject("InvalidFacilityClearancePeriod2");
  return c.status();
}

// 真边界的 ordinal/前驱和 cause tag 必须相容，不从当前 Action 补缺历史。
template <class C>
Status boundary(C &c, FacilityClearanceExecutionBoundary1 &value) {
  care_requirement_codec::text(c, "boundary_id", value.boundary_id);
  c.field("source_schema", value.source_schema);
  care_requirement_codec::text(c, "execution_ref", value.execution_ref);
  c.field("boundary_ordinal", value.boundary_ordinal);
  c.optional("previous_boundary_ref", value.previous_boundary_ref,
             [](auto &x, auto &id) {
               return care_requirement_codec::text(x, "value", id);
             });
  Integer kind = static_cast<Integer>(value.kind);
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind < 0 || kind > 5)
    return c.reject("InvalidFacilityClearanceBoundaryKind");
  if (c.reading())
    value.kind = static_cast<CareHandoverBoundaryKind>(kind);
  c.object("occurred_at", [&](auto &x) {
    return family_life4_codec::moment(x, value.occurred_at);
  });
  c.object("transition_rule_ref", [&](auto &x) {
    return care_requirement_codec::rule(x, value.transition_rule_ref);
  });
  c.object("cause", [&](auto &x) {
    return care_handover_codec::cause(x, value.cause);
  });
  c.sequence("worker_capability_inputs", value.worker_capability_inputs,
             [](auto &x, auto &frame) {
               return care_handover_codec::capability(x, frame);
             });
  care_arrangement_codec::moment_refs(c, "actual_participation_source_refs",
                                      value.actual_participation_source_refs);
  care_arrangement_codec::moment_refs(c, "facility_use_binding_refs",
                                      value.facility_use_binding_refs);
  exact_order(c, value.field_order,
              {"boundary_id", "source_schema", "execution_ref",
               "boundary_ordinal", "previous_boundary_ref", "kind",
               "occurred_at", "transition_rule_ref", "cause",
               "worker_capability_inputs", "actual_participation_source_refs",
               "facility_use_binding_refs", "field_order"});
  if (!c.status().ok)
    return c.status();
  if (value.source_schema != 1 || value.boundary_ordinal < 0 ||
      (value.boundary_ordinal == 0) != !value.previous_boundary_ref ||
      (kind == 0) != (value.boundary_ordinal == 0) ||
      (value.previous_boundary_ref &&
       *value.previous_boundary_ref == value.boundary_id) ||
      (kind == 0 && value.cause.index() > 1) ||
      (kind > 0 && kind < 4 && value.cause.index() != 2) ||
      (kind >= 4 && value.cause.index() != 3) ||
      (kind < 4 && value.worker_capability_inputs.empty()))
    return c.reject("InvalidFacilityClearanceBoundary1");
  return c.status();
}

// wire0 实际本地期间、wire1 原 handover source2；未知 tag 不默认本地工作。
template <class C> Status gate(C &c, FacilityClearanceWorkGate &value) {
  Integer tag = static_cast<Integer>(value.payload.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidFacilityClearanceWorkGateTag");
  if (c.reading()) {
    auto prepared = tag == 0 ? c.template prepare_default_values<
                                   FacilityClearanceLocalProvided>()
                             : c.template prepare_default_values<
                                   FacilityClearanceOriginalCarePeriod>();
    if (!prepared.ok)
      return prepared;
    value.payload =
        tag == 0
            ? decltype(value.payload){FacilityClearanceLocalProvided{}}
            : decltype(value.payload){FacilityClearanceOriginalCarePeriod{}};
  }
  c.object("payload", [&](auto &x) {
    if (tag == 0)
      return x.object("period", [&](auto &y) {
        return period(
            y, std::get<FacilityClearanceLocalProvided>(value.payload).period);
      });
    auto &original =
        std::get<FacilityClearanceOriginalCarePeriod>(value.payload);
    x.object("source_ref", [&](auto &y) {
      return care_requirement_codec::source(y, original.source_ref);
    });
    x.field("source_schema", original.source_schema);
    if (x.status().ok && (original.source_schema != 2 ||
                          original.source_ref.domain != "care_requirements" ||
                          original.source_ref.kind != "care_handover_period"))
      return x.reject("OriginalCareHandoverPeriod2Required");
    return x.status();
  });
  return exact_order(c, value.field_order, {"tag", "payload", "field_order"});
}
} // namespace hanmo::resident::care_clearance_history_codec
