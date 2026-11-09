// FH §7.3 的唯一十六字段表；各转移保原 enum，不储存另一个 current。
#pragma once
#include "care_clearance_history_values_codec_fields.hpp"
#include "care_clearance_origin_codec_fields.hpp"
#include "care_facility_disposition.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/family/care_facility_disposition.hpp"
#include "care_facility_use_codec_fields.hpp"
#include "care_handover_execution_codec_fields.hpp"

namespace hanmo::resident::care_facility_disposition_codec {
// 有限 enum 按原声明的稳定顺序写；越界值在任何分支构造之前拒绝。
template <class C, class E>
Status enumeration(C &c, const char *name, E &value, Integer last) {
  Integer tag = static_cast<Integer>(value);
  c.field(name, tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > last)
    return c.reject("InvalidCareDispositionEnum", name);
  if (c.reading())
    value = static_cast<E>(tag);
  return c.status();
}
// 实际原 owner 决定分支，绝不从 disposition_kind 推算一个方便的状态。
template <class C> Status transition(C &c, CareFacilityBusinessTransition &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidCareBusinessTransitionTag");
  if (c.reading()) {
    auto ready = tag == 0   ? care_clearance_origin_codec::select<0>(c, v)
                 : tag == 1 ? care_clearance_origin_codec::select<1>(c, v)
                            : care_clearance_origin_codec::select<2>(c, v);
    if (!ready.ok)
      return ready;
  }
  return std::visit(
      [&](auto &x) -> Status {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, CareFacilityHandoverTransition>) {
          care_requirement_codec::text(c, "handover_id", x.handover_id);
          c.object("original_boundary_ref", [&](auto &a) {
            return care_requirement_codec::source(a, x.original_boundary_ref);
          });
          c.field("original_boundary_schema", x.original_boundary_schema);
          enumeration(c, "before", x.before, 5);
          enumeration(c, "after", x.after, 5);
          if (c.status().ok &&
              (x.original_boundary_schema != 1 ||
               x.original_boundary_ref.domain != "care_requirements" ||
               x.original_boundary_ref.kind !=
                   "care_handover_execution_boundary"))
            return c.reject("CareHandoverTransitionSourceMismatch");
        } else {
          if constexpr (std::is_same_v<T, CareFacilityGoalTransition>)
            care_requirement_codec::text(c, "requirement_id", x.requirement_id);
          else
            care_requirement_codec::text(c, "binding_id", x.binding_id);
          c.object("original_transition_ref", [&](auto &a) {
            return care_requirement_codec::source(a, x.original_transition_ref);
          });
          c.field("original_transition_schema", x.original_transition_schema);
          constexpr Integer last =
              std::is_same_v<T, CareFacilityGoalTransition> ? 3 : 2;
          enumeration(c, "before", x.before, last);
          enumeration(c, "after", x.after, last);
          if (c.status().ok &&
              (x.original_transition_schema != 3 ||
               x.original_transition_ref.domain != "care_requirements" ||
               x.original_transition_ref.kind != "care_requirement_changed"))
            return c.reject("CareBusinessTransitionSourceMismatch");
        }
        return c.status();
      },
      v);
}
// 五种 trigger 都保其原实际请求/结果地址，不把 tag 当成原来源已存在的证据。
template <class C> Status trigger(C &c, CareFacilityDispositionTrigger &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 4)
    return c.reject("InvalidCareDispositionTriggerTag");
  if (c.reading()) {
    Status ready = common_success();
    switch (tag) {
    case 0:
      ready = care_clearance_origin_codec::select<0>(c, v);
      break;
    case 1:
      ready = care_clearance_origin_codec::select<1>(c, v);
      break;
    case 2:
      ready = care_clearance_origin_codec::select<2>(c, v);
      break;
    case 3:
      ready = care_clearance_origin_codec::select<3>(c, v);
      break;
    case 4:
      ready = care_clearance_origin_codec::select<4>(c, v);
      break;
    }
    if (!ready.ok)
      return ready;
  }
  return std::visit(
      [&](auto &x) -> Status {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, CareFacilityOriginalBusinessCommand>) {
          return c.object("actual_adoption_ref", [&](auto &a) {
            return care_handover_codec::command_binding(a,
                                                        x.actual_adoption_ref);
          });
        } else if constexpr (std::is_same_v<T, CareFacilityAcceptedWindowDue>) {
          care_requirement_codec::text(c, "previous_claim_id",
                                       x.previous_claim_id);
          return c.object("due_at", [&](auto &a) {
            return family_life4_codec::moment(a, x.due_at);
          });
        } else {
          if constexpr (std::is_same_v<T, CareFacilityProviderChanged>)
            c.object("actual_capacity_or_right_source_ref", [&](auto &a) {
              return care_requirement_codec::source(
                  a, x.actual_capacity_or_right_source_ref);
            });
          else if constexpr (std::is_same_v<T,
                                            CareFacilityOriginalBusinessResult>)
            c.object("actual_result_ref", [&](auto &a) {
              return care_requirement_codec::source(a, x.actual_result_ref);
            });
          else
            c.object("actual_clearance_or_professional_event_ref",
                     [&](auto &a) {
                       return care_requirement_codec::source(
                           a, x.actual_clearance_or_professional_event_ref);
                     });
          c.field("schema", x.schema);
          return c.status().ok && x.schema < 1
                     ? c.reject("InvalidCareDispositionTriggerSchema")
                     : c.status();
        }
      },
      v);
}
// 原外行 tag 固定0；十六字段按 FH 原序，不增加 current 或第二执行键。
template <class C> Status disposition(C &c, CareFacilityDispositionResult &v) {
  Integer tag = 0;
  c.field("tag", tag);
  if (c.status().ok && tag != 0)
    return c.reject("CareDispositionTag0Required");
  care_requirement_codec::text(c, "result_id", v.result_id);
  c.field("source_schema", v.source_schema);
  c.object("owner_business_ref", [&](auto &a) {
    return care_facility_codec::arrangement(a, v.owner_business_ref);
  });
  care_requirement_codec::text(c, "accepted_terms_ref", v.accepted_terms_ref);
  c.object("slot_ref",
           [&](auto &a) { return facility_fields::slot(a, v.slot_ref); });
  c.object("occurred_at", [&](auto &a) {
    return family_life4_codec::moment(a, v.occurred_at);
  });
  enumeration(c, "disposition_kind", v.disposition_kind, 5);
  c.object("trigger", [&](auto &a) { return trigger(a, v.trigger); });
  c.object("original_business_transition", [&](auto &a) {
    return transition(a, v.original_business_transition);
  });
  c.sequence("settled_work_source_refs", v.settled_work_source_refs,
             [](auto &a, auto &x) {
               return care_requirement_codec::moment_source(a, x);
             });
  c.sequence("previous_claim_ids", v.previous_claim_ids, [](auto &a, auto &x) {
    return care_requirement_codec::text(a, "value", x);
  });
  c.sequence("new_claim_ids", v.new_claim_ids, [](auto &a, auto &x) {
    return care_requirement_codec::text(a, "value", x);
  });
  c.optional(
      "physical_occupancy_source_ref", v.physical_occupancy_source_ref,
      [](auto &a, auto &x) { return care_requirement_codec::source(a, x); });
  c.optional("physical_source_schema", v.physical_source_schema,
             [](auto &a, auto &x) {
               a.field("value", x);
               return a.status().ok && x < 1
                          ? a.reject("InvalidCareDispositionPhysicalSchema")
                          : a.status();
             });
  c.object("rule_ref", [&](auto &a) {
    return care_requirement_codec::rule(a, v.rule_ref);
  });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"result_id", "source_schema", "owner_business_ref", "accepted_terms_ref",
       "slot_ref", "occurred_at", "disposition_kind", "trigger",
       "original_business_transition", "settled_work_source_refs",
       "previous_claim_ids", "new_claim_ids", "physical_occupancy_source_ref",
       "physical_source_schema", "rule_ref", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_care_facility_disposition_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_facility_disposition_codec
