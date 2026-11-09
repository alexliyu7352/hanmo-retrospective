// FH5.3B fields have a single JSON/binary/projection implementation.
#pragma once
#include "care_arrangement_codec_fields.hpp"
#include "care_handover_execution.hpp"
#include "resident/domains/actor_foundation/actor_foundation_codec.hpp"
#include "resident/domains/actor_foundation/actor_foundation_scoped_codec.hpp"
#include "resident/domains/growth/growth_fixed_service_codec.hpp"
#include "resident/kernel/fixed_service_work_period_codec_fields.hpp"
namespace hanmo::resident::care_handover_codec {
inline Status command_binding(CodecVisitor &c, LifeActualCommandBinding &v) {
  return visit_life_actual_command_binding(c, v);
}
template <class C> Status command_binding(C &c, LifeActualCommandBinding &v) {
  return c.foreign_record(
      v,
      [](const BoundaryValue &input, ScopedCheckpointJsonDecode &scope) {
        return decode_life_actual_command_binding(input, scope);
      },
      emit_life_actual_command_binding_fields);
}
inline Status actor_route(CodecVisitor &c, LifeActorRoute &v,
                          const StableId &actor) {
  return visit_life_actor_route(c, v, actor);
}
template <class C>
Status actor_route(C &c, LifeActorRoute &v, const StableId &actor) {
  return c.foreign_record(
      v,
      [&](const BoundaryValue &input, ScopedCheckpointJsonDecode &scope) {
        return decode_life_actor_route(input, actor, scope);
      },
      [&](ProjectionWriter &w, const LifeActorRoute &value) {
        return emit_life_actor_route_fields(w, value, actor);
      });
}
inline Status capability(CodecVisitor &c, FrozenFacilityCapabilityInputs &v) {
  return visit_frozen_character_capability_inputs(c, v);
}
// 有实际冷读scope时只能沿同job入口；独立无scope codec保持原合法语义。
template <class C> Status capability(C &c, FrozenFacilityCapabilityInputs &v) {
  if (ScopedCheckpointJsonDecode::current())
    return c.foreign_record(
        v,
        [](const BoundaryValue &raw, ScopedCheckpointJsonDecode &scope) {
          return decode_frozen_character_capability_inputs(raw, scope);
        },
        emit_frozen_character_capability_fields);
  return c.foreign_record(
      v,
      [](const BoundaryValue &raw) {
        return decode_frozen_character_capability_inputs(raw);
      },
      emit_frozen_character_capability_fields);
}
template <class C> Status order(C &c, FieldOrder &v) {
  std::size_t index = 0;
  return c.sequence("field_order", v, [&](auto &x, auto &name) {
    care_requirement_codec::text(x, "value", name);
    if (x.status().ok)
      for (std::size_t p = 0; p < index; ++p)
        if (v[p] == name)
          return x.reject("DuplicateCareFieldOrder");
    ++index;
    return x.status();
  });
}
template <class C> Status dependency(C &c, CareHandoverDependencyChange &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 5)
    return c.reject("InvalidCareHandoverDependency");
  if (c.reading()) {
    switch (tag) {
    case 0:
      v = CareHandoverConditionChanged{};
      break;
    case 1:
      v = CareHandoverDeathChanged{};
      break;
    case 2:
      v = CareHandoverPositionChanged{};
      break;
    case 3:
      v = CareHandoverOccupationChanged{};
      break;
    case 4:
      v = CareHandoverFacilityChanged{};
      break;
    default:
      v = CareHandoverAuthorityOrTermsChanged{};
      break;
    }
  }
  return std::visit(
      [&](auto &value) -> Status {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, CareHandoverConditionChanged>)
          return c.object("condition_receipt_ref", [&](auto &x) {
            return care_requirement_codec::text(
                x, "result_id", value.condition_receipt_ref.result_id);
          });
        else if constexpr (std::is_same_v<T, CareHandoverDeathChanged>)
          return c.object("death_ref", [&](auto &x) {
            return care_requirement_codec::death(x, value.death_ref);
          });
        else {
          c.object("source_ref", [&](auto &x) {
            return care_requirement_codec::source(x, value.source_ref);
          });
          c.field("source_schema", value.source_schema);
          return c.status().ok && value.source_schema < 1
                     ? c.reject("InvalidCareHandoverDependencySchema")
                     : c.status();
        }
      },
      v);
}
template <class C> Status actor_command(C &c, CareHandoverActorCommand &value) {
  care_requirement_codec::text(c, "actor_id", value.actor_id);
  c.object("command_binding",
           [&](auto &x) { return command_binding(x, value.command_binding); });
  c.object("route_basis", [&](auto &x) {
    return actor_route(x, value.route_basis, value.actor_id);
  });
  care_requirement_codec::text(c, "accepted_terms_ref",
                               value.accepted_terms_ref);
  return care_arrangement_codec::moment_refs(c, "authority_source_refs",
                                             value.authority_source_refs);
}
template <class C> Status cause(C &c, CareHandoverBoundaryCause &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 3)
    return c.reject("InvalidCareHandoverCause");
  if (c.reading()) {
    switch (tag) {
    case 0:
      v = CareHandoverActorCommand{};
      break;
    case 1:
      v = CareHandoverAcceptedObligation{};
      break;
    case 2:
      v = CareHandoverDependencyChanged{};
      break;
    default:
      v = CareHandoverOriginalOutcome{};
      break;
    }
  }
  if (tag == 0)
    return actor_command(c, std::get<CareHandoverActorCommand>(v));
  if (tag == 1) {
    auto &value = std::get<CareHandoverAcceptedObligation>(v);
    c.object("original_obligation_ref", [&](auto &x) {
      return care_requirement_codec::source(x, value.original_obligation_ref);
    });
    c.field("original_obligation_source_schema",
            value.original_obligation_source_schema);
    c.object("accepted_rule_ref", [&](auto &x) {
      return care_requirement_codec::rule(x, value.accepted_rule_ref);
    });
    return c.status().ok && value.original_obligation_source_schema < 1
               ? c.reject("InvalidCareObligationSchema")
               : c.status();
  }
  if (tag == 2) {
    auto &value = std::get<CareHandoverDependencyChanged>(v);
    c.object("change", [&](auto &x) { return dependency(x, value.change); });
    return c.object("continuation_rule_ref", [&](auto &x) {
      return care_requirement_codec::rule(x, value.continuation_rule_ref);
    });
  }
  auto &value = std::get<CareHandoverOriginalOutcome>(v);
  c.object("outcome_source_ref", [&](auto &x) {
    return care_requirement_codec::source(x, value.outcome_source_ref);
  });
  c.field("outcome_source_schema", value.outcome_source_schema);
  return c.status().ok && value.outcome_source_schema < 1
             ? c.reject("InvalidCareHandoverOutcomeSchema")
             : c.status();
}
template <class C> Status boundary(C &c, CareHandoverExecutionBoundary1 &v) {
  care_requirement_codec::text(c, "boundary_id", v.boundary_id);
  c.field("source_schema", v.source_schema);
  care_requirement_codec::text(c, "execution_ref", v.execution_ref);
  c.field("boundary_ordinal", v.boundary_ordinal);
  c.optional("previous_boundary_ref", v.previous_boundary_ref,
             [](auto &x, auto &id) {
               return care_requirement_codec::text(x, "value", id);
             });
  Integer kind = static_cast<Integer>(v.kind);
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind < 0 || kind > 5)
    return c.reject("InvalidCareHandoverBoundaryKind");
  if (c.reading())
    v.kind = static_cast<CareHandoverBoundaryKind>(kind);
  c.object("occurred_at", [&](auto &x) {
    return family_life4_codec::moment(x, v.occurred_at);
  });
  c.object("transition_rule_ref", [&](auto &x) {
    return care_requirement_codec::rule(x, v.transition_rule_ref);
  });
  c.object("cause", [&](auto &x) { return cause(x, v.cause); });
  c.sequence("worker_capability_inputs", v.worker_capability_inputs,
             [](auto &x, auto &row) { return capability(x, row); });
  care_arrangement_codec::moment_refs(c, "actual_participation_source_refs",
                                      v.actual_participation_source_refs);
  care_arrangement_codec::moment_refs(c, "facility_use_binding_refs",
                                      v.facility_use_binding_refs);
  order(c, v.field_order);
  if (!c.status().ok)
    return c.status();
  auto valid = validate_care_handover_boundary_shape(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
template <class C> Status period(C &c, CareHandoverWorkPeriod2 &v) {
  care_requirement_codec::text(c, "source_id", v.source_id);
  c.field("source_schema", v.source_schema);
  care_requirement_codec::text(c, "execution_ref", v.execution_ref);
  c.object("work_period_ref", [&](auto &x) {
    return fixed_period_fields::ref(x, v.work_period_ref);
  });
  c.object("provided_at", [&](auto &x) {
    return family_life4_codec::moment(x, v.provided_at);
  });
  c.sequence("worker_capability_inputs", v.worker_capability_inputs,
             [](auto &x, auto &row) { return capability(x, row); });
  c.sequence("worker_condition_receipt_refs", v.worker_condition_receipt_refs,
             [](auto &x, auto &ref) {
               return care_requirement_codec::text(x, "result_id",
                                                   ref.result_id);
             });
  order(c, v.field_order);
  if (!c.status().ok)
    return c.status();
  auto valid = validate_care_handover_period_shape(v);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
template <class C>
Status execution(C &c, CareHandoverExecution &v, Integer version) {
  if (version < 3 || version > 6)
    return c.reject("CareExactHandoverProfileRequired");
  care_requirement_codec::text(c, "handover_id", v.handover_id);
  care_requirement_codec::text(c, "requirement_id", v.requirement_id);
  care_requirement_codec::text(c, "accepted_care_terms_ref",
                               v.accepted_care_terms_ref);
  care_requirement_codec::text(c, "original_action_ref", v.original_action_ref);
  c.object("execution_binding", [&](auto &x) {
    return work_fields::binding(x, v.execution_binding);
  });
  c.object("handover_rule_ref", [&](auto &x) {
    return care_requirement_codec::rule(x, v.handover_rule_ref);
  });
  care_arrangement_codec::moment_refs(c, "actual_placement_result_refs",
                                      v.actual_placement_result_refs);
  c.optional("completion_event_ref", v.completion_event_ref,
             [](auto &x, auto &ref) {
               return care_requirement_codec::source(x, ref);
             });
  c.optional("terminal_source_ref", v.terminal_source_ref,
             [](auto &x, auto &ref) {
               return care_requirement_codec::moment_source(x, ref);
             });
  if (version == 5 || version == 6) {
    c.sequence("work_period_sources", v.work_period_sources,
               [](auto &x, auto &row) { return period(x, row); });
    c.sequence("handover_execution_boundaries", v.handover_execution_boundaries,
               [](auto &x, auto &row) { return boundary(x, row); });
  } else if (!v.work_period_sources.empty() ||
             !v.handover_execution_boundaries.empty())
    return c.reject("CareHandoverHistoryRequiresVersion5");
  if (!c.status().ok)
    return c.status();
  auto valid = validate_care_handover_execution_shape(v, version);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::care_handover_codec
