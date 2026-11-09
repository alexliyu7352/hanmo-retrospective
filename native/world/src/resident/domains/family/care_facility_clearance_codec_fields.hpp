// FH §5.3—5.5 完整清场根行唯一字段表；Location由真正Common原codec借入。
#pragma once
#include "care_clearance_history_values_codec_fields.hpp"
#include "care_facility_clearance.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/family/care_facility_clearance.hpp"
#include "care_facility_transport_acceptance_codec_fields.hpp"
#include "care_physical_position_codec_fields.hpp"
namespace hanmo::resident::care_facility_clearance_codec {
using care_clearance_origin_codec::select;
using care_facility_transport_codec::fixed_text;
using care_requirement_codec::moment_source;
using care_requirement_codec::rule;
using care_requirement_codec::source;
using care_requirement_codec::text;
using family_life4_codec::moment;
// 同一强型enum只接原有限tag，拒绝未知值而非默认归Working。
template <class C, class E>
Status enumeration(C &c, const char *name, E &v, Integer last) {
  Integer tag = static_cast<Integer>(v);
  c.field(name, tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > last)
    return c.reject("InvalidCareClearanceEnum", name);
  if (c.reading())
    v = static_cast<E>(tag);
  return c.status();
}
// 只接受同执行slot或真正同S7 line的已完成后继地址，地址存在性不在此伪造。
template <class C> Status body_scope_ref(C &c, CareClearanceBodyScopeRef &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidClearanceBodyScopeRefTag");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0) {
    auto &x = std::get<0>(v);
    text(c, "execution_id", x.execution_id);
    return enumeration(c, "scope_slot", x.scope_slot, 2);
  }
  auto &x = std::get<1>(v);
  text(c, "agreement_id", x.agreement_id);
  text(c, "intake_line_id", x.intake_line_id);
  return c.object("completed_scope_source", [&](auto &a) {
    return moment_source(a, x.completed_scope_source);
  });
}
// CareRepositionedBodyBinding 完整原字段，无当前状态或历史推测值。
template <class C> Status repositioned(C &c, CareRepositionedBodyBinding &v) {

  c.object("body_scope_ref",
           [&](auto &a) { return body_scope_ref(a, v.body_scope_ref); });
  c.field("body_count", v.body_count);
  c.sequence("actual_unit_refs", v.actual_unit_refs, [](auto &a, auto &x) {
    return care_physical_position_codec::unit(a, x);
  });
  c.object("receiving_safety_rule_ref",
           [&](auto &a) { return rule(a, v.receiving_safety_rule_ref); });
  c.optional("actual_receiver_source_ref", v.actual_receiver_source_ref,
             [](auto &a, auto &x) { return source(a, x); });
  c.optional(
      "actual_receiver_schema", v.actual_receiver_schema, [](auto &a, auto &x) {
        a.field("value", x);
        return a.status().ok && x < 1 ? a.reject("InvalidCareClearanceSchema")
                                      : a.status();
      });
  if (c.status().ok &&
      (v.body_count < 1 || v.actual_receiver_source_ref.has_value() !=
                               v.actual_receiver_schema.has_value()))
    return c.reject("InvalidCareRepositionedBodyBinding");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"body_scope_ref", "body_count", "actual_unit_refs",
       "receiving_safety_rule_ref", "actual_receiver_source_ref",
       "actual_receiver_schema", "field_order"});
  return c.status();
}
// CarePhysicalClearanceConfirmation 完整原字段，无当前状态或历史推测值。
template <class C>
Status confirmation(C &c, CarePhysicalClearanceConfirmation &v) {

  c.object("confirmation_policy_ref",
           [&](auto &a) { return rule(a, v.confirmation_policy_ref); });
  c.sequence("original_affected_unit_refs", v.original_affected_unit_refs,
             [](auto &a, auto &x) {
               return care_physical_position_codec::unit(a, x);
             });
  c.sequence("cleared_unit_refs", v.cleared_unit_refs, [](auto &a, auto &x) {
    return care_physical_position_codec::unit(a, x);
  });
  c.sequence("remaining_footprint_unit_refs", v.remaining_footprint_unit_refs,
             [](auto &a, auto &x) {
               return care_physical_position_codec::unit(a, x);
             });
  enumeration(c, "remaining_position_mode", v.remaining_position_mode, 1);
  c.sequence("actual_repositioned_body_bindings",
             v.actual_repositioned_body_bindings,
             [](auto &a, auto &x) { return repositioned(a, x); });
  c.object("observed_at", [&](auto &a) { return moment(a, v.observed_at); });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"confirmation_policy_ref", "original_affected_unit_refs",
       "cleared_unit_refs", "remaining_footprint_unit_refs",
       "remaining_position_mode", "actual_repositioned_body_bindings",
       "observed_at", "field_order"});
  return c.status();
}
// CareAcceptedLocalPlacement 完整原字段，无当前状态或历史推测值。
template <class C, class LocationFields>
Status local_placement(C &c, CareAcceptedLocalPlacement &v,
                       LocationFields &location_fields) {
  care_facility_transport_codec::receiving_business(c, v.receiving_business_ref,
                                                    "receiving_business_ref");
  c.object("receiving_acceptance_ref",
           [&](auto &a) { return source(a, v.receiving_acceptance_ref); });
  c.field("receiving_acceptance_schema", v.receiving_acceptance_schema);
  text(c, "receiving_actor_id", v.receiving_actor_id);
  c.object("receiving_authority_ref", [&](auto &a) {
    return moment_source(a, v.receiving_authority_ref);
  });
  text(c, "source_place_ref", v.source_place_ref);
  text(c, "destination_place_ref", v.destination_place_ref);
  c.optional("new_facility_use_binding_ref", v.new_facility_use_binding_ref,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.object("actual_new_placement_source_ref", [&](auto &a) {
    return source(a, v.actual_new_placement_source_ref);
  });
  c.field("actual_new_placement_source_schema",
          v.actual_new_placement_source_schema);
  c.object("receiver_location_before", [&](auto &a) {
    return location_fields(a, v.receiver_location_before);
  });
  c.sequence("receiver_capability_source_refs",
             v.receiver_capability_source_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  if (c.status().ok && (v.receiving_acceptance_schema < 1 ||
                        v.actual_new_placement_source_schema < 1 ||
                        v.source_place_ref != v.destination_place_ref))
    return c.reject("InvalidCareLocalReceiving");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"receiving_kind", "receiving_business_ref", "receiving_acceptance_ref",
       "receiving_acceptance_schema", "receiving_actor_id",
       "receiving_authority_ref", "source_place_ref", "destination_place_ref",
       "new_facility_use_binding_ref", "actual_new_placement_source_ref",
       "actual_new_placement_source_schema", "receiver_location_before",
       "receiver_capability_source_refs", "field_order"});
  return c.status();
}
// CareActualGroundLoading 完整原字段，无当前状态或历史推测值。
template <class C, class LocationFields>
Status ground_loading(C &c, CareActualGroundLoading &v,
                      LocationFields &location_fields) {

  c.object("transport_unit_ref", [&](auto &a) {
    return spatial_carrier_ref_codec_fields::transport_unit_fields(
        a, v.transport_unit_ref);
  });
  c.field("composition_revision_before", v.composition_revision_before);
  c.field("composition_revision_after", v.composition_revision_after);
  c.object("registration_source_ref",
           [&](auto &a) { return source(a, v.registration_source_ref); });
  c.field("registration_source_schema", v.registration_source_schema);
  text(c, "operator_actor_id", v.operator_actor_id);
  text(c, "receiving_actor_id", v.receiving_actor_id);
  c.object("operator_authority_ref",
           [&](auto &a) { return moment_source(a, v.operator_authority_ref); });
  c.object("source_location_before", [&](auto &a) {
    return location_fields(a, v.source_location_before);
  });
  c.object("receiver_location_before", [&](auto &a) {
    return location_fields(a, v.receiver_location_before);
  });
  c.object("carrier_position_before", [&](auto &a) {
    return location_fields(a, v.carrier_position_before);
  });
  c.object("loaded_scope", [&](auto &a) {
    return care_body_scope_codec::scope(a, v.loaded_scope);
  });
  c.sequence("original_population_binding_refs",
             v.original_population_binding_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  if (c.status().ok &&
      (v.composition_revision_before < 0 ||
       v.composition_revision_after < v.composition_revision_before ||
       v.registration_source_schema < 1 || v.loaded_scope.index() == 0))
    return c.reject("InvalidCareGroundLoad");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"transport_unit_ref", "composition_revision_before",
       "composition_revision_after", "registration_source_ref",
       "registration_source_schema", "operator_actor_id", "receiving_actor_id",
       "operator_authority_ref", "source_location_before",
       "receiver_location_before", "carrier_position_before", "loaded_scope",
       "original_population_binding_refs", "field_order"});
  return c.status();
}
// CareActualVesselLoading 完整原字段，无当前状态或历史推测值。
template <class C> Status vessel_loading(C &c, CareActualVesselLoading &v) {

  text(c, "vessel_id", v.vessel_id);
  c.object("actual_s2_load_result_ref",
           [&](auto &a) { return source(a, v.actual_s2_load_result_ref); });
  c.field("actual_s2_load_source_schema", v.actual_s2_load_source_schema);
  c.sequence("passenger_binding_refs", v.passenger_binding_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  if (c.status().ok &&
      (v.actual_s2_load_source_schema != 3 ||
       v.actual_s2_load_result_ref.domain != "military" ||
       v.actual_s2_load_result_ref.kind != "sea_transfer_result"))
    return c.reject("ActualSeaLoadResult3Required");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"vessel_id", "actual_s2_load_result_ref", "actual_s2_load_source_schema",
       "passenger_binding_refs", "field_order"});
  return c.status();
}
// Ground与Vessel分支由原实际装载选择；未知分支不默认为陆上。
template <class C, class LocationFields>
Status loading(C &c, CareActualTransportLoading &v,
               LocationFields &location_fields) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCareActualLoadingTag");
  if (c.reading()) {
    auto ready = tag == 0 ? select<0>(c, v) : select<1>(c, v);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0)
    return ground_loading(c, std::get<0>(v), location_fields);
  return vessel_loading(c, std::get<1>(v));
}
// 真实运输交接只存已完成的原装载与参与、位置来源。
template <class C, class LocationFields>
Status travel_handover(C &c, CareOriginalTravelHandover &v,
                       LocationFields &location_fields) {
  c.object("transport_acceptance_ref", [&](auto &a) {
    return moment_source(a, v.transport_acceptance_ref);
  });
  c.object("actual_loading", [&](auto &a) {
    return loading(a, v.actual_loading, location_fields);
  });
  c.sequence("actual_participation_result_refs",
             v.actual_participation_result_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.sequence("actual_location_binding_result_refs",
             v.actual_location_binding_result_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  return care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"transport_acceptance_ref", "actual_loading",
       "actual_participation_result_refs",
       "actual_location_binding_result_refs", "field_order"});
}
// CareActualRearrangeAndConfirm 完整原字段，无当前状态或历史推测值。
template <class C> Status rearrange(C &c, CareActualRearrangeAndConfirm &v) {

  c.object("original_placement_source_ref",
           [&](auto &a) { return source(a, v.original_placement_source_ref); });
  c.field("source_schema", v.source_schema);
  c.object("safe_placement_rule_ref",
           [&](auto &a) { return rule(a, v.safe_placement_rule_ref); });
  c.object("original_use_or_care_authority_ref", [&](auto &a) {
    return moment_source(a, v.original_use_or_care_authority_ref);
  });
  fixed_text(c, "completed_local_operation", "rearranged_and_checked");
  if (c.status().ok && v.source_schema < 1)
    return c.reject("InvalidCareRearrangeSourceSchema");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"original_placement_source_ref", "source_schema",
       "safe_placement_rule_ref", "original_use_or_care_authority_ref",
       "completed_local_operation", "field_order"});
  return c.status();
}
// CareAuthorizedLocalDisposition 完整原字段，无当前状态或历史推测值。
template <class C>
Status local_disposition(C &c, CareAuthorizedLocalDisposition &v) {

  c.sequence("death_or_population_loss_refs", v.death_or_population_loss_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.object("accepted_disposition_clause_ref", [&](auto &a) {
    return moment_source(a, v.accepted_disposition_clause_ref);
  });
  c.object("executor_authority_ref",
           [&](auto &a) { return moment_source(a, v.executor_authority_ref); });
  text(c, "authorized_site_ref", v.authorized_site_ref);
  c.object("site_use_source_ref",
           [&](auto &a) { return moment_source(a, v.site_use_source_ref); });
  text(c, "source_place_ref", v.source_place_ref);
  text(c, "disposition_place_ref", v.disposition_place_ref);
  c.object("disposition_rule_ref",
           [&](auto &a) { return rule(a, v.disposition_rule_ref); });
  fixed_text(c, "completed_local_operation", "lawful_local_disposition");
  if (c.status().ok && (v.source_place_ref != v.disposition_place_ref ||
                        v.death_or_population_loss_refs.empty()))
    return c.reject("InvalidCareLawfulLocalDisposition");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"death_or_population_loss_refs", "accepted_disposition_clause_ref",
       "executor_authority_ref", "authorized_site_ref", "site_use_source_ref",
       "source_place_ref", "disposition_place_ref", "disposition_rule_ref",
       "completed_local_operation", "field_order"});
  return c.status();
}
// 四种实际目的地按原tag，不接受请求填的任意业务source。
template <class C, class LocationFields>
Status destination(C &c, CareClearanceDestinationBasis &v,
                   LocationFields &location_fields) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 3)
    return c.reject("InvalidCareClearanceDestination");
  if (c.reading()) {
    auto ready = tag == 0   ? select<0>(c, v)
                 : tag == 1 ? select<1>(c, v)
                 : tag == 2 ? select<2>(c, v)
                            : select<3>(c, v);
    if (!ready.ok)
      return ready;
  }
  switch (tag) {
  case 0:
    return local_placement(c, std::get<0>(v), location_fields);
  case 1:
    return travel_handover(c, std::get<1>(v), location_fields);
  case 2:
    return rearrange(c, std::get<2>(v));
  default:
    return local_disposition(c, std::get<3>(v));
  }
}
// CareFacilityClearanceStarted 完整原字段，无当前状态或历史推测值。
template <class C> Status started(C &c, CareFacilityClearanceStarted &v) {

  text(c, "source_id", v.source_id);
  fixed_text(c, "kind", "facility_clearance_started");
  c.object("occurred_at", [&](auto &a) { return moment(a, v.occurred_at); });
  c.object("original_adoption_or_obligation_trigger_ref", [&](auto &a) {
    return moment_source(a, v.original_adoption_or_obligation_trigger_ref);
  });
  c.sequence(
      "worker_capability_inputs", v.worker_capability_inputs,
      [](auto &a, auto &x) { return care_handover_codec::capability(a, x); });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"source_id", "kind", "occurred_at",
       "original_adoption_or_obligation_trigger_ref",
       "worker_capability_inputs", "field_order"});
  return c.status();
}
// ConditionReceipt只引用原结果稳定键。
template <class C> Status condition_receipt(C &c, ConditionReceiptRefV2 &v) {
  return text(c, "result_id", v.result_id);
}
// CareFacilityClearanceWorkCompleted 完整原字段，无当前状态或历史推测值。
template <class C>
Status work_completed(C &c, CareFacilityClearanceWorkCompleted &v) {

  text(c, "source_id", v.source_id);
  fixed_text(c, "kind", "facility_clearance_work_completed");
  c.object("occurred_at", [&](auto &a) { return moment(a, v.occurred_at); });
  c.object("original_action_terminal_ref", [&](auto &a) {
    return moment_source(a, v.original_action_terminal_ref);
  });
  c.optional("last_closed_gate_ref", v.last_closed_gate_ref,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.sequence("worker_condition_receipt_refs", v.worker_condition_receipt_refs,
             [](auto &a, auto &x) { return condition_receipt(a, x); });
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"source_id", "kind", "occurred_at", "original_action_terminal_ref",
       "last_closed_gate_ref", "worker_condition_receipt_refs", "field_order"});
  return c.status();
}
// actual完成保真正目的地与物理确认；work_completed自身不清出身体。
template <class C, class LocationFields>
Status completed(C &c, CareFacilityClearanceCompleted &v,
                 LocationFields &location_fields) {
  text(c, "source_id", v.source_id);
  fixed_text(c, "kind", "facility_clearance_completed");
  c.object("occurred_at", [&](auto &a) { return moment(a, v.occurred_at); });
  Integer kind = static_cast<Integer>(v.destination_basis.index());
  c.field("destination_kind", kind);
  if (c.status().ok && (kind < 0 || kind > 3))
    return c.reject("InvalidCareClearanceDestinationKind");
  c.object("destination_basis", [&](auto &a) {
    return destination(a, v.destination_basis, location_fields);
  });
  if (c.status().ok &&
      kind != static_cast<Integer>(v.destination_basis.index()))
    return c.reject("CareClearanceDestinationTagMismatch");
  c.object("scope_cleared", [&](auto &a) {
    return care_body_scope_codec::scope(a, v.scope_cleared);
  });
  c.object("scope_remaining", [&](auto &a) {
    return care_body_scope_codec::scope(a, v.scope_remaining);
  });
  c.object("physical_confirmation",
           [&](auto &a) { return confirmation(a, v.physical_confirmation); });
  c.sequence(
      "worker_capability_inputs", v.worker_capability_inputs,
      [](auto &a, auto &x) { return care_handover_codec::capability(a, x); });
  c.sequence("actual_original_holder_result_refs",
             v.actual_original_holder_result_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.sequence("actual_receiver_result_refs", v.actual_receiver_result_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  return care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"source_id", "kind", "occurred_at", "destination_kind",
       "destination_basis", "scope_cleared", "scope_remaining",
       "physical_confirmation", "worker_capability_inputs",
       "actual_original_holder_result_refs", "actual_receiver_result_refs",
       "field_order"});
}
// CareFacilityClearanceStopped 完整原字段，无当前状态或历史推测值。
template <class C> Status stopped(C &c, CareFacilityClearanceStopped &v) {

  text(c, "source_id", v.source_id);
  fixed_text(c, "kind", "facility_clearance_stopped");
  c.object("occurred_at", [&](auto &a) { return moment(a, v.occurred_at); });
  c.object("cause_ref", [&](auto &a) { return source(a, v.cause_ref); });
  c.field("cause_schema", v.cause_schema);
  c.object("original_stop_rule_ref",
           [&](auto &a) { return rule(a, v.original_stop_rule_ref); });
  c.optional("last_completed_gate_ref", v.last_completed_gate_ref,
             [](auto &a, auto &x) { return moment_source(a, x); });
  if (c.status().ok && v.cause_schema < 1)
    return c.reject("InvalidCareClearanceStopSchema");
  care_clearance_history_codec::exact_order(
      c, v.field_order,
      {"source_id", "kind", "occurred_at", "cause_ref", "cause_schema",
       "original_stop_rule_ref", "last_completed_gate_ref", "field_order"});
  return c.status();
}
// 完整二十一字段，所有原Location借调用者的同job Common codec而非复制布局。
// location_fields只为瞬时codec借用，不写入state，不承担任何业务权限。
template <class C, class LocationFields>
Status execution(C &c, FacilityClearanceExecution &v,
                 LocationFields &location_fields) {
  text(c, "execution_id", v.execution_id);
  c.object("obligation", [&](auto &a) {
    return care_clearance_origin_codec::obligation(a, v.obligation);
  });
  c.object("accepted_rule_ref",
           [&](auto &a) { return rule(a, v.accepted_rule_ref); });
  c.object("original_holder_source_ref",
           [&](auto &a) { return source(a, v.original_holder_source_ref); });
  c.field("original_holder_source_schema", v.original_holder_source_schema);
  text(c, "placement_claim_id", v.placement_claim_id);
  c.object("slot_ref",
           [&](auto &a) { return facility_fields::slot(a, v.slot_ref); });
  c.sequence("worker_actor_ids", v.worker_actor_ids,
             [](auto &a, auto &x) { return text(a, "value", x); });
  c.sequence("worker_acceptance_refs", v.worker_acceptance_refs,
             [](auto &a, auto &x) { return moment_source(a, x); });
  c.object("scope_before", [&](auto &a) {
    return care_body_scope_codec::scope(a, v.scope_before);
  });
  c.object("position_before", [&](auto &a) {
    return care_physical_position_codec::position(a, v.position_before);
  });
  c.object("started_source",
           [&](auto &a) { return started(a, v.started_source); });
  c.object("work_origin", [&](auto &a) {
    return care_clearance_origin_codec::work_origin(a, v.work_origin);
  });
  c.optional("work_binding", v.work_binding,
             [](auto &a, auto &x) { return work_fields::binding(a, x); });
  c.sequence("work_gate_sources", v.work_gate_sources, [](auto &a, auto &x) {
    return care_clearance_history_codec::gate(a, x);
  });
  c.optional("work_completed_source", v.work_completed_source,
             [](auto &a, auto &x) { return work_completed(a, x); });
  enumeration(c, "state", v.state, 4);
  c.optional("completion", v.completion, [&](auto &a, auto &x) {
    return completed(a, x, location_fields);
  });
  c.optional("terminal_without_clearance", v.terminal_without_clearance,
             [](auto &a, auto &x) { return stopped(a, x); });
  c.sequence("execution_boundaries", v.execution_boundaries,
             [](auto &a, auto &x) {
               return care_clearance_history_codec::boundary(a, x);
             });
  care_clearance_history_codec::exact_order(c, v.field_order,
                                            {"execution_id",
                                             "obligation",
                                             "accepted_rule_ref",
                                             "original_holder_source_ref",
                                             "original_holder_source_schema",
                                             "placement_claim_id",
                                             "slot_ref",
                                             "worker_actor_ids",
                                             "worker_acceptance_refs",
                                             "scope_before",
                                             "position_before",
                                             "started_source",
                                             "work_origin",
                                             "work_binding",
                                             "work_gate_sources",
                                             "work_completed_source",
                                             "state",
                                             "completion",
                                             "terminal_without_clearance",
                                             "execution_boundaries",
                                             "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_facility_clearance_execution_relations(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_facility_clearance_codec
