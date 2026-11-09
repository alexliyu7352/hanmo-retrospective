#include "face_to_face_contact_source.hpp"
#include "report_disclosure_codec_fields.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {
namespace {
template <class A> Status context(A &a, InteractionContext &v) {
  a.field("place_id", v.place_id);
  a.field("provider_actor_id", v.provider_actor_id);
  a.field("role_id", v.role_id);
  a.optional("kind", v.kind,
             [](auto &c, auto &x) { return c.field("value", x); });
  a.optional("encounter_id", v.encounter_id,
             [](auto &c, auto &x) { return c.field("value", x); });
  a.optional("input_error", v.input_error,
             [](auto &c, auto &x) { return c.field("value", x); });
  return dynamic_codec::strings(a, "field_order", v.field_order);
}
template <class A> Status source(A &a, SourceRef &v) {
  a.field("domain", v.domain);
  a.field("kind", v.kind);
  return a.field("id", v.id);
}
template <class A> Status moment_source(A &a, MomentOwnerSource &v) {
  a.object("source_ref", [&](auto &c) { return source(c, v.source_ref); });
  return a.field("source_schema", v.source_schema);
}
template <class A> Status disclosure(A &a, FaceToFaceReportDisclosure &v) {
  a.field("id", v.id);
  a.field("contact_source_id", v.contact_source_id);
  a.field("sender_actor_id", v.sender_actor_id);
  a.field("recipient_actor_id", v.recipient_actor_id);
  a.field("report_id", v.report_id);
  a.field("dialogue_node_id", v.dialogue_node_id);
  a.field("dialogue_phase", v.dialogue_phase);
  a.object("sender_knowledge_source",
           [&](auto &c) { return source(c, v.sender_knowledge_source); });
  a.object("permission", [&](auto &c) {
    return report_disclosure_codec::permission(c, v.permission);
  });
  return a.object("disclosed_at", [&](auto &c) {
    return dynamic_codec::exact_moment(c, v.disclosed_at);
  });
}
template <class A>
Status purchase_disclosure(A &a, HouseholdPurchaseReportDisclosure &v) {
  a.field("id", v.id);
  a.field("controller_actor_id", v.controller_actor_id);
  a.field("cohort_id", v.cohort_id);
  a.field("report_id", v.report_id);
  a.object("purchase_plan_ref",
           [&](auto &c) { return source(c, v.purchase_plan_ref); });
  a.object("accepted_policy_ref",
           [&](auto &c) { return source(c, v.accepted_policy_ref); });
  a.object("population_selection_ref",
           [&](auto &c) { return source(c, v.population_selection_ref); });
  a.field("accepted_policy_revision", v.accepted_policy_revision);
  a.object("controller_authority", [&](auto &c) {
    auto &v2 = v.controller_authority;
    c.field("actor_id", v2.actor_id);
    c.field("household_group_id", v2.household_group_id);
    c.field("fact_id", v2.fact_id);
    c.field("source_role_id", v2.source_role_id);
    c.field("operation", v2.operation);
    c.field("source_revision", v2.source_revision);
    c.field("effective_from_tick", v2.effective_from_tick);
    return c.optional("through_tick", v2.through_tick,
                      [](auto &z, auto &n) { return z.field("value", n); });
  });
  a.object("controller_knowledge_source",
           [&](auto &c) { return source(c, v.controller_knowledge_source); });
  a.object("original_observation",
           [&](auto &c) { return moment_source(c, v.original_observation); });
  a.object("controller_learned_at", [&](auto &c) {
    return dynamic_codec::exact_moment(c, v.controller_learned_at);
  });
  a.object("disclosed_at", [&](auto &c) {
    return dynamic_codec::exact_moment(c, v.disclosed_at);
  });
  a.object("permission", [&](auto &c) {
    return report_disclosure_codec::permission(c, v.permission);
  });
  a.object("actual_population", [&](auto &c) {
    c.field("cohort_id", v.actual_population.cohort_id);
    c.field("population_revision", v.actual_population.population_revision);
    return c.optional("transport_binding_id",
                      v.actual_population.transport_binding_id,
                      [](auto &z, auto &n) { return z.field("value", n); });
  });
  a.object("population_location_source",
           [&](auto &c) { return source(c, v.population_location_source); });
  return a.object("actual_contact_prefix", [&](auto &c) {
    c.field("common_fact_count", v.actual_contact_prefix.common_fact_count);
    return c.optional("last_common_fact",
                      v.actual_contact_prefix.last_common_fact,
                      [](auto &z, auto &n) { return source(z, n); });
  });
}
template <class A> Status fields(A &a, FaceToFaceContactSources &s) {
  a.field("version", s.version);
  if (s.version != 1 && s.version != 2 && s.version != 3)
    return a.reject("UnsupportedFaceToFaceContactSourceSchema");
  auto result =
      dynamic_codec::table(a, "results", s.results, [](auto &c, auto &r) {
        c.field("session_id", r.session_id);
        c.field("session_actor_id", r.session_actor_id);
        c.field("provider_actor_id", r.provider_actor_id);
        c.field("dialogue_id", r.dialogue_id);
        c.field("first_contact_node_id", r.first_contact_node_id);
        c.field("first_contact_phase", r.first_contact_phase);
        c.object("context", [&](auto &v) { return context(v, r.context); });
        return c.object("occurred_at", [&](auto &v) {
          return dynamic_codec::exact_moment(v, r.occurred_at);
        });
      });
  if (!result.ok)
    return result;
  if (s.version < 3 && !s.purchase_plan_disclosures.empty())
    return a.reject("UnselectedHouseholdPurchaseReportSources");
  if (s.version == 1)
    return s.report_disclosures.empty()
               ? a.status()
               : a.reject("UnselectedFaceToFaceReportSources");
  result =
      dynamic_codec::table(a, "report_disclosures", s.report_disclosures,
                           [](auto &c, auto &v) { return disclosure(c, v); });
  if (!result.ok || s.version == 2)
    return result;
  return dynamic_codec::table(
      a, "purchase_plan_disclosures", s.purchase_plan_disclosures,
      [](auto &c, auto &v) { return purchase_disclosure(c, v); });
}
} // namespace
Status
visit_face_to_face_contact_sources_checkpoint(CodecVisitor &a,
                                              FaceToFaceContactSources &s) {
  return fields(a, s);
}
Outcome<FaceToFaceContactSources>
decode_face_to_face_contact_sources(const BoundaryValue &v) {
  FaceToFaceContactSources s;
  dynamic_codec::JsonArchive a(v);
  auto r = fields(a, s);
  if (r.ok)
    r = a.finish();
  return r.ok ? Outcome<FaceToFaceContactSources>{true, std::move(s), {}}
              : Outcome<FaceToFaceContactSources>{false, {}, r.error};
}
Outcome<BoundaryValue>
encode_face_to_face_contact_sources(const FaceToFaceContactSources &s) {
  return dynamic_projection::encode(
      s, [](auto &a, auto &v) { return fields(a, v); });
}
Status emit_face_to_face_contact_sources_projection(
    ProjectionWriter &w, std::string_view key,
    const FaceToFaceContactSources &s) {
  return dynamic_projection::emit(
      w, key, s, [](auto &a, auto &v) { return fields(a, v); });
}
} // namespace hanmo::resident
