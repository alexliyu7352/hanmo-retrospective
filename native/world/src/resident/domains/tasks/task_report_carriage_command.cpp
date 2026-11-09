#include "task_report_carriage_command.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "task_model.hpp"
#include "task_report_carriage_codec_fields.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void reject(const char *c, const StableId &id = {}) {
  throw Failure{common_failure(c, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
template <class T, class Fn> Outcome<T> guard(Fn fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same(const MomentOwnerSource &a, const MomentOwnerSource &b) {
  return a.source_schema == b.source_schema && same(a.source_ref, b.source_ref);
}
bool same(const ReportDisclosurePermission &a,
          const ReportDisclosurePermission &b) {
  return a.kind == b.kind && same(a.original_source, b.original_source);
}
const StableId &agreement_id(const TaskReportCarriageCommand &q) {
  return std::visit(
      [](const auto &v) -> const StableId & {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PostTaskReportCarriage>)
          return v.agreement_id;
        else
          return v.carriage_id;
      },
      q);
}

std::shared_ptr<const ReadStamp> capture(const TaskReportCarriageContext &x,
                                         const TaskReportCarriage &r,
                                         const TaskReportCarriageQuote &q) {
  const auto *report =
      x.face.disclosure.social.dynamic->reports.find(r.terms.report_id);
  if (!report)
    reject("TaskReportContentMissing", r.terms.report_id);
  std::vector<ResourceKey> resources;
  std::vector<DomainReadKey> domains;
  std::vector<StableId> facts;
  CheckpointMemoryCharge memory;


  // At most five base domains and two actual source domains. Sources use only
  // verified Common Fact IDs; typed owner sources remain domain keys.
  (void)0;


  const StableId *known = nullptr;
  for (const auto &row :
       x.face.disclosure.social.dynamic->time2->source_time_bindings.rows()) {
    const auto *k = std::get_if<SocialKnowledgeKey>(&row.second.key);
    if (k && k->receiver.kind == SubjectKind::actor &&
        k->receiver.id == r.terms.sender_actor_id &&
        k->report_id == report->id) {
      if (known)
        reject("AmbiguousTaskReportKnowledge", report->id);
      known = &row.first;
    }
  }
  if (!known)
    reject("TaskReportSenderKnowledgeMissing", report->id);


  auto plan_source = [&](const MomentOwnerSource &v) {
    const auto *h = x.common.state().facts.find(v.source_ref.id);
    if (h && h->domain == v.source_ref.domain && h->kind == v.source_ref.kind)
      (void)0;
    else
      (void)0;
  };
  const auto *posting = std::get_if<PostTaskReportCarriage>(&q.command);
  if (posting)
    (void)0;
  else
    plan_source(std::get<HandOverTaskReportCarriage>(q.command).actual_arrival);
  if (q.actual_contact)
    plan_source(*q.actual_contact);
  if (q.permission)
    plan_source(q.permission->original_source);
  (memory).grow(0);
  resources.reserve(3);
  domains.reserve(7);
  facts.reserve(6);
  for (const auto *id : {&r.terms.sender_actor_id, &r.terms.carrier_actor_id,
                         &r.terms.recipient_actor_id})
    resources.push_back({ResourceKey::Kind::actor, *id});
  domains.push_back({"tasks", "task_carrier_agreement", r.agreement_id});
  domains.push_back({"tasks", "task_report_carriage", r.agreement_id});
  domains.push_back({"social", "report", report->id});
  domains.push_back({"social", "known_report", *known});
  facts.emplace_back(r.agreement_id);
  facts.emplace_back(report->fact_id);
  facts.emplace_back(r.terms.actual_carrier_disclosure.source_ref.id);
  auto source = [&](const MomentOwnerSource &v) {
    const auto *h = x.common.state().facts.find(v.source_ref.id);
    if (h && h->domain == v.source_ref.domain && h->kind == v.source_ref.kind) {
      if (std::find(facts.begin(), facts.end(), v.source_ref.id) == facts.end())
        facts.emplace_back(v.source_ref.id);
    } else
      domains.push_back(
          {v.source_ref.domain, v.source_ref.kind, v.source_ref.id});
  };
  if (posting)
    domains.push_back({"travel", "personal_action", posting->actual_action_id});
  else
    source(std::get<HandOverTaskReportCarriage>(q.command).actual_arrival);
  if (q.actual_contact)
    source(*q.actual_contact);
  if (q.permission)
    source(q.permission->original_source);
  return take(x.face.disclosure.clock.capture(
      std::move(resources), std::move(domains), std::move(facts)));
}
} // namespace
Outcome<TaskReportCarriageCommand>
decode_task_report_carriage_command(const StableId &kind,
                                    const BoundaryValue &v) {
  try {
    using namespace boundary;
    if (kind == "dispatch_known_report_carriage") {
      fields(v, {"agreement_id", "actual_action_id"}, {}, kind);
      PostTaskReportCarriage q{get_string(v, "agreement_id", kind),
                               get_string(v, "actual_action_id", kind)};
      if (q.agreement_id.empty() || q.actual_action_id.empty())
        return {false,
                {},
                common_failure("TaskReportDispatchSourceRequired").error};
      return {true, std::move(q), {}};
    }
    if (kind == "hand_over_carried_report") {
      fields(v, {"carriage_id", "actual_arrival"}, {}, kind);
      HandOverTaskReportCarriage q;
      q.carriage_id = get_string(v, "carriage_id", kind);
      const auto *arrival = v.find("actual_arrival");
      dynamic_codec::JsonArchive archive(*arrival);
      auto valid =
          report_disclosure_codec::moment_source(archive, q.actual_arrival);
      if (valid.ok)
        valid = archive.finish();
      if (!valid.ok)
        return {false, {}, valid.error};
      if (q.carriage_id.empty() || q.actual_arrival.source_schema != 1 ||
          q.actual_arrival.source_ref.domain != "travel" ||
          q.actual_arrival.source_ref.kind != "movement_arrival" ||
          q.actual_arrival.source_ref.id.empty())
        return {
            false, {}, common_failure("TaskReportRealArrivalRequired").error};
      return {true, std::move(q), {}};
    }
    return {false,
            {},
            common_failure("UnsupportedTaskReportCarriageCommand", kind,
                           ErrorKind::unsupported_capability)
                .error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status
emit_task_report_carriage_command_fields(ProjectionWriter &w,
                                         const TaskReportCarriageCommand &q) {
  if (const auto *p = std::get_if<PostTaskReportCarriage>(&q)) {
    w.field("agreement_id", p->agreement_id);
    return w.field("actual_action_id", p->actual_action_id);
  }
  const auto &p = std::get<HandOverTaskReportCarriage>(q);
  w.field("carriage_id", p.carriage_id);
  return w.object("actual_arrival", [&](auto &child) {
    DynamicProjectionArchive archive(child);
    return report_disclosure_codec::moment_source(
        archive, const_cast<MomentOwnerSource &>(p.actual_arrival));
  });
}
// 核对原受领合同、出行或到达事实与披露权限；只准备报价，不发布送达。
Outcome<TaskReportCarriageQuote>
preview_task_report_carriage(const TaskReportCarriageContext &x,
                             const StableId &actor,
                             const TaskReportCarriageCommand &q) {
  return guard<TaskReportCarriageQuote>([&] {
    if (&x.common != &x.face.common || &x.common != &x.face.disclosure.common ||
        !x.tasks.transport_agreements ||
        x.tasks.transport_agreements->version != 2 ||
        !x.face.disclosure.social.dynamic ||
        !x.face.disclosure.social.dynamic->time2 ||
        !x.movement_actions.bound_to(x.common, x.movement))
      reject("TaskReportCarriageSameCandidateRequired");
    const auto &id = agreement_id(q);
    const auto *r = x.tasks.transport_agreements->report_carriages.find(id);
    const auto *a = x.tasks.transport_agreements->agreements.find(id);
    if (!r || !a || r->agreement_id != id)
      reject("TaskReportAcceptedScopeMissing", id);
    take(read_task_carrier_agreement_moment(
        x.common, x.tasks, {"tasks", "task_carrier_agreement_accepted", id},
        1));
    take(x.original_terms.validate_frozen_acceptance(*a));
    take(x.original_terms.validate_report_delivery_acceptance(*a, r->terms));
    take(x.original_terms.validate_report_delivery_coverage(*a, &r->terms));
    take(validate_native_task_report_delivery_acceptance(x, *a, r->terms));
    TaskReportCarriageQuote out;


    if (const auto *p = std::get_if<PostTaskReportCarriage>(&q))
      (void)0;
    else
      (void)0;
    (out.memory).grow(0);
    out.actor_id = actor;
    out.command = q;
    out.quoted_at = take(x.face.disclosure.clock.read_current_moment());
    if (const auto *p = std::get_if<PostTaskReportCarriage>(&q)) {
      if (actor != r->terms.sender_actor_id || a->stopped_source)
        reject("TaskReportSenderScopeRequired", actor);
      const auto proof = take(x.movement_actions.read_action_time_owner(
          movement_action_time_registration(), p->actual_action_id));
      const auto begun = take(
          x.movement_actions.read_action_time_source(proof.started_source));
      if (proof.actor_id != r->terms.carrier_actor_id ||
          take(compare_world_moments(begun, a->accepted_at)) < 0 ||
          take(compare_world_moments(begun, out.quoted_at)) > 0)
        reject("TaskReportActualDepartureRequired", p->actual_action_id);
      out.stamp = capture(x, *r, out);
      take(validate_task_report_carriage_dispatch_request(x, actor, *p,
                                                          *out.stamp));
      out.permission = take(prepare_report_disclosure_permission(
          x.face.disclosure, r->terms.report_id,
          {SubjectKind::actor, r->terms.sender_actor_id},
          {SubjectKind::actor, r->terms.recipient_actor_id}, *out.stamp,
          out.memory));
      out.stamp = capture(x, *r, out);
    } else {
      const auto &handover = std::get<HandOverTaskReportCarriage>(q);
      if (actor != r->terms.carrier_actor_id || !r->dispatch)
        reject("TaskReportCarrierControlRequired", actor);
      const auto arrival = take(read_verified_movement_arrival(
          x.movement, handover.actual_arrival.source_ref,
          handover.actual_arrival.source_schema));
      const auto *execution =
          std::get_if<PersonalMovementExecution>(&arrival.execution);
      const auto *place =
          x.common_catalog.places.find(a->terms.destination_site_id);
      if (!execution || execution->action_id != r->dispatch->action_id ||
          !place || !arrival.destination_settlement_id ||
          *arrival.destination_settlement_id != place->settlement_id ||
          take(compare_world_moments(arrival.occurred_at, out.quoted_at)) > 0)
        reject("TaskReportRealArrivalRequired", id);
      out.actual_contact = MomentOwnerSource{
          take(read_current_face_to_face_contact_source(
              x.common, x.face.interaction, x.face.catalog, x.face.access,
              actor, r->terms.recipient_actor_id, out.quoted_at, out.memory)),
          1};
      const auto *contact = take(read_face_to_face_contact_source(
          x.common, x.face.interaction, x.face.catalog,
          out.actual_contact->source_ref, 1));
      if (contact->context.place_id != a->terms.destination_site_id)
        reject("TaskReportActualHandoverSiteRequired", id);
      out.stamp = capture(x, *r, out);
    }
    return out;
  });
}
Outcome<StableId> apply_task_report_carriage_in_candidate(
    CommonStore &c, TaskState &t, SocialState &s,
    CommunicationLinksState &links, const TaskReportCarriageContext &x,
    const StableId &actor, const TaskReportCarriageQuote &q) {
  return guard<StableId>([&] {
    if (actor != q.actor_id || !q.stamp)
      reject("TaskReportPreparedCallerMismatch", actor);
    take(x.face.disclosure.authority.validate(*q.stamp, c.state().identity,
                                              q.stamp->mode(), c,
                                              x.face.disclosure.versions));
    auto fresh = take(preview_task_report_carriage(x, actor, q.command));
    if (take(compare_world_moments(q.quoted_at, fresh.quoted_at)) ||
        q.permission.has_value() != fresh.permission.has_value() ||
        (q.permission && !same(*q.permission, *fresh.permission)) ||
        q.actual_contact.has_value() != fresh.actual_contact.has_value() ||
        (q.actual_contact && !same(*q.actual_contact, *fresh.actual_contact)))
      reject("TaskReportCarriageQuoteChanged", agreement_id(q.command));
    if (const auto *p = std::get_if<PostTaskReportCarriage>(&q.command))
      return take(post_task_report_carriage_in_candidate(
          c, t, s, links, x, actor, *p, *fresh.stamp));
    return take(hand_over_task_report_carriage_in_candidate(
        c, t, s, links, x, actor,
        std::get<HandOverTaskReportCarriage>(q.command), *fresh.stamp));
  });
}
} // namespace hanmo::resident
