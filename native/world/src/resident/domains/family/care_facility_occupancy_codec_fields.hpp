// Care 设施事件唯一字段表；局部源 tag 不接受其它 owner 的设施事件。
#pragma once
#include "care_facility_named_body_codec_fields.hpp"
#include "care_facility_occupancy.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_facility_use_codec_fields.hpp"
#include "care_handover_execution_codec_fields.hpp"
#include "care_physical_position_codec_fields.hpp"

namespace hanmo::resident::care_facility_occupancy_codec {
// Care 独立身体内容只接受原具名 Actor，不将 S7 的匿名内容搬进此表。
template <class C> Status content(C &c, CareFacilityContent &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCareFacilityContentTag");
  if (c.reading()) {
    auto ready = tag == 0 ? care_clearance_origin_codec::select<0>(c, value)
                          : care_clearance_origin_codec::select<1>(c, value);
    if (!ready.ok)
      return ready;
  }
  return tag == 0 ? c.status()
                  : care_facility_named_body_codec::named_body(
                        c, std::get<CareFacilityNamedBody>(value));
}

// 三个分支保 FH §7.2 原字段，不增添自报已完成、工量或当前占位字段。
template <class C> Status operation(C &c, CareFacilityOperation &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidCareFacilityOperationTag");
  if (c.reading()) {
    auto ready = tag == 0   ? care_clearance_origin_codec::select<0>(c, value)
                 : tag == 1 ? care_clearance_origin_codec::select<1>(c, value)
                            : care_clearance_origin_codec::select<2>(c, value);
    if (!ready.ok)
      return ready;
  }
  const auto source = [&](const char *name, SourceRef &ref) {
    return c.object(name, [&](auto &inner) {
      return care_requirement_codec::source(inner, ref);
    });
  };
  return std::visit(
      [&](auto &v) -> Status {
        care_requirement_codec::text(c, "accepted_arrangement_id",
                                     v.accepted_arrangement_id);
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CareFacilityActualPlacement>) {
          care_requirement_codec::text(c, "handover_id", v.handover_id);
          source("actual_care_temporal_event_ref",
                 v.actual_care_temporal_event_ref);
          c.object("placement_rule_ref", [&](auto &inner) {
            return care_requirement_codec::rule(inner, v.placement_rule_ref);
          });
          source("original_receiver_acceptance_ref",
                 v.original_receiver_acceptance_ref);
        } else if constexpr (std::is_same_v<T,
                                            CareFacilityActualContinuation>) {
          source("original_care_transition_ref",
                 v.original_care_transition_ref);
          source("cause_ref", v.cause_ref);
          c.field("cause_schema", v.cause_schema);
        } else {
          source("completed_clearance_source_ref",
                 v.completed_clearance_source_ref);
          c.field("source_schema", v.source_schema);
        }
        return c.status();
      },
      value);
}

// 段内固定源 tag 与完整 Event 原字段只在此列出，三种存读出口共享。
template <class C> Status event(C &c, CareFacilityOccupancyEvent &v) {
  Integer source_tag = 0;
  c.field("event_kind_source_tag", source_tag);
  if (c.status().ok && source_tag != 0)
    return c.reject("CareFacilityOccupancySourceTag0Required");
  care_requirement_codec::text(c, "event_id", v.event_id);
  c.field("source_schema", v.source_schema);
  c.object("owner_business_ref", [&](auto &inner) {
    return care_facility_codec::arrangement(inner, v.owner_business_ref);
  });
  c.object("holder", [&](auto &inner) {
    return facility_fields::holder(inner, v.holder);
  });
  c.object("slot_ref", [&](auto &inner) {
    return facility_fields::slot(inner, v.slot_ref);
  });
  care_requirement_codec::text(c, "placement_claim_id", v.placement_claim_id);
  c.optional("previous_event_ref", v.previous_event_ref,
             [](auto &inner, auto &ref) {
               return care_requirement_codec::source(inner, ref);
             });
  c.object("occurred_at", [&](auto &inner) {
    return family_life4_codec::moment(inner, v.occurred_at);
  });
  Integer kind = static_cast<Integer>(v.event_kind);
  c.field("event_kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind < 0 || kind > 3)
    return c.reject("InvalidCareFacilityEventKind");
  if (c.reading())
    v.event_kind = static_cast<CareFacilityEventKind>(kind);
  c.object("position_basis", [&](auto &inner) {
    return care_physical_position_codec::position(inner, v.position_basis);
  });
  c.object("before_content",
           [&](auto &inner) { return content(inner, v.before_content); });
  c.object("after_content",
           [&](auto &inner) { return content(inner, v.after_content); });
  c.object("operation",
           [&](auto &inner) { return operation(inner, v.operation); });
  c.optional("capability_inputs", v.capability_inputs,
             [](auto &inner, auto &inputs) {
               return care_handover_codec::capability(inner, inputs);
             });
  care_handover_codec::order(c, v.field_order);
  if (!c.status().ok)
    return c.status();
  const auto valid = validate_care_facility_occupancy_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_facility_occupancy_codec
