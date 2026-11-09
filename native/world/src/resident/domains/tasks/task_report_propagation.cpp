#include "task_report_propagation.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "task_model.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void reject(const char *c, const StableId &id = {}) {
  throw Failure{common_failure(c, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
template <class T, class Fn> Outcome<T> guard(Fn f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
const TaskTransportAgreementState &
selected(const TaskReportCarriageContext &x) {
  if (!x.tasks.transport_agreements ||
      x.tasks.transport_agreements->version != 2 ||
      !x.face.interaction.communication_links ||
      !x.face.disclosure.social.dynamic ||
      !x.face.disclosure.social.dynamic->time2 || &x.common != &x.face.common ||
      &x.common != &x.face.disclosure.common ||
      !x.movement_actions.bound_to(x.common, x.movement))
    reject("TaskReportPropagationOriginalsRequired");
  return *x.tasks.transport_agreements;
}
void current(const TaskReportCarriageContext &x, const ReadStamp &s) {
  selected(x);
  if (s.mode() != ReadMode::committed_current &&
      s.mode() != ReadMode::phase_current)
    reject("CurrentPropagationStampRequired");
  take(x.face.disclosure.authority.validate(s, x.common.state().identity,
                                            s.mode(), x.common,
                                            x.face.disclosure.versions));
}
void contact(const TaskReportCarriageContext &x, const SubjectRef &sender,
             const PropagationRecipient &recipient) {
  const auto *c = take(read_face_to_face_contact_source(
      x.common, x.face.interaction, x.face.catalog,
      recipient.actual_known_contact_source, 1));
  if (sender.kind != SubjectKind::actor ||
      recipient.subject.kind != SubjectKind::actor ||
      sender.id == recipient.subject.id ||
      !((c->session_actor_id == sender.id &&
         c->provider_actor_id == recipient.subject.id) ||
        (c->provider_actor_id == sender.id &&
         c->session_actor_id == recipient.subject.id)) ||
      compare(c->occurred_at,
              take(x.face.disclosure.clock.read_current_moment())) > 0)
    reject("TaskReportPropagationKnownContactRequired", recipient.subject.id);
}
CheckpointMemoryCharge reserve_request(const StableId &agreement,
                                       const StableId &action) {


  return CheckpointMemoryCharge(0);
}
struct Prepared final : PreparedPropagationChannel {
  TaskReportCarriageQuote quote;
  explicit Prepared(TaskReportCarriageQuote &&q) : quote(std::move(q)) {
  }
};
} // namespace
bool NativeTaskReportPropagationRead::supports(
    MessageChannelKind kind) const noexcept {
  return kind == MessageChannelKind::carrier_delivery &&
         x_.tasks.transport_agreements &&
         x_.tasks.transport_agreements->version == 2 &&
         bound_to(x_.common, x_.face.disclosure.social);
}
bool NativeTaskReportPropagationRead::supports_report(
    KnowledgeCategory k) const noexcept {
  return contacts_.supports_report(k);
}
Outcome<PropagationRecipientPage>
NativeTaskReportPropagationRead::known_recipients(
    const SubjectRef &s, Integer cursor, Integer limit,
    const ReadStamp &stamp) const {
  auto valid = guard<Done>([&] {
    current(x_, stamp);
    if (!bound_to(x_.common, x_.face.disclosure.social))
      reject("TaskReportPropagationContactCandidateMismatch");
    return Done{};
  });
  if (!valid.ok)
    return {false, {}, valid.error};
  return contacts_.known_recipients(s, cursor, limit, stamp);
}
// 只沿已受领合同与承运人的真实在途行动准备传播；无行动或无披露权限时跳过。
Outcome<std::optional<PropagationChannelProposal>>
NativeTaskReportPropagationRead::prepare(const SubjectRef &sender,
                                         const Report &report,
                                         const PropagationRecipient &recipient,
                                         MessageChannelKind kind,
                                         const ReadStamp &stamp) const {
  return guard<std::optional<PropagationChannelProposal>>(
      [&]() -> std::optional<PropagationChannelProposal> {
        current(x_, stamp);
        if (!bound_to(x_.common, x_.face.disclosure.social))
          reject("TaskReportPropagationContactCandidateMismatch");
        if (kind != MessageChannelKind::carrier_delivery)
          return {};
        contact(x_, sender, recipient);
        const auto &state = selected(x_);
        // Stable original accepted-contract order. No household, arbitrary
        // first carrier or future command is substituted when an execution is
        // absent.
        for (const auto &row : state.report_carriages.rows()) {
          const auto &r = row.second;
          if (r.dispatch || r.terms.sender_actor_id != sender.id ||
              r.terms.recipient_actor_id != recipient.subject.id ||
              r.terms.report_id != report.id)
            continue;
          const auto *agreement = state.agreements.find(r.agreement_id);
          if (!agreement || row.first != r.agreement_id)
            reject("TaskReportAcceptedScopeMissing", row.first);
          if (agreement->stopped_source)
            continue;
          const auto *actor = x_.common.actor(r.terms.carrier_actor_id);
          if (!actor || actor->action_id.empty())
            continue;
          const auto *action = x_.common.action(actor->action_id);
          if (!action || action->kind != "travel")
            continue;
          CheckpointMemoryCharge memory =
              reserve_request(r.agreement_id, actor->action_id);
          const TaskReportCarriageCommand request =
              PostTaskReportCarriage{r.agreement_id, actor->action_id};
          auto quote = preview_task_report_carriage(x_, sender.id, request);
          if (!quote.ok) {
            if (quote.error.original.code == "ReportDisclosureNotPermitted")
              continue;
            throw Failure{std::move(quote.error)};
          }
          return PropagationChannelProposal{
              recipient.subject, kind,
              checkpoint_make_shared<const Prepared>(std::move(quote.value))};
        }
        return {};
      });
}
Outcome<PropagationActualDispatch>
NativeTaskReportPropagationRead::read_actual_dispatch(
    const StableId &id) const {
  return guard<PropagationActualDispatch>([&] {
    selected(x_);
    take(validate_task_report_carriages(x_));
    const auto *m =
        x_.face.disclosure.social.dynamic->pending_messages.find(id);
    const MessageBusinessLink *business = nullptr;
    for (const auto &row :
         x_.face.interaction.communication_links->links.rows())
      if (row.second.original_public_message_ref == id) {
        if (business)
          reject("DuplicateTaskReportMessage", id);
        business = &row.second;
      }
    if (!m || !business || m->channel != "carrier_delivery" ||
        business->original_business_fact_ref.domain != "tasks" ||
        business->original_business_fact_ref.kind !=
            "task_report_carriage_dispatched" ||
        business->original_business_source_schema != 1)
      reject("TaskReportActualPropagationDispatchRequired", id);
    TaskReportCarriageCommunicationRead original(x_);
    auto sent = take(original.origin(business->original_business_fact_ref, 1));
    const auto *address = std::get_if<ActorAddress>(&sent.addressed_to);
    if (!address || m->report_id != sent.report_id ||
        !same_social_subject(m->sender, sent.sender))
      reject("TaskReportPropagationContentMismatch", id);
    std::optional<WorldMoment> delivered;
    if (m->status == MessageStatus::delivered) {
      const auto *execution = x_.face.disclosure.social.dynamic->time2
                                  ->message_execution_bindings.find(id);
      const auto *d =
          execution && execution->actual_delivery_ref
              ? x_.face.interaction.communication_links->deliveries.find(
                    execution->actual_delivery_ref->id)
              : nullptr;
      if (!d || d->business_link_id != business->id ||
          d->actual_receiver.kind != SubjectKind::actor ||
          d->actual_receiver.id != address->actor_id || !d->contents_disclosed)
        reject("TaskReportActualPropagationHandoverRequired", id);
      delivered = d->delivered_at;
    }
    return PropagationActualDispatch{id,
                                     sent.report_id,
                                     sent.sender,
                                     {SubjectKind::actor, address->actor_id},
                                     MessageChannelKind::carrier_delivery,
                                     sent.sent_at,
                                     std::move(delivered),
                                     business->original_business_fact_ref};
  });
}
Status NativeTaskReportPropagationRead::validate_known_recipient_at_cut(
    const SubjectRef &s, const SubjectRef &r,
    const OriginalSocialRelationCut &cut) const {
  if (!bound_to(x_.common, x_.face.disclosure.social))
    return common_failure("PropagationOwnerCandidateMismatch", s.id,
                          ErrorKind::internal);
  return contacts_.validate_known_recipient_at_cut(s, r, cut);
}
Status NativeTaskReportPropagationRead::validate_actual_dispatch_after_cut(
    const PropagationActualDispatch &dispatch,
    const OriginalSocialRelationCut &cut) const {
  return guard<Done>([&] {
    const auto original = take(read_actual_dispatch(dispatch.message_id));
    take(validate_propagation_original_dispatch_after_cut(x_.common, dispatch,
                                                          original, cut));
    return Done{};
  });
}
Status NativeTaskReportPropagationRead::validate_historical_disclosure(
    const SocialPropagationDecision &d,
    const PropagationActualDispatch &dispatch) const {
  return guard<Done>([&] {
    const auto actual = take(read_actual_dispatch(dispatch.message_id));
    if (actual.report_id != d.report_id ||
        !same_social_subject(actual.sender, d.subject) ||
        !same_social_subject(actual.recipient, dispatch.recipient) ||
        !same(actual.original_business_source,
              dispatch.original_business_source) ||
        compare(actual.created_at, {d.evaluated_at, {}}))
      reject("TaskReportPropagationHistoricalMismatch", dispatch.message_id);
    return Done{};
  });
}
Outcome<StableId> NativeTaskReportPropagationWrite::adopt(
    const PropagationChannelProposal &p, const SubjectRef &sender,
    const StableId &report, const ReadStamp &stamp) {
  return guard<StableId>([&] {
    current(x_, stamp);
    if (&authority_ != &x_.face.disclosure.authority ||
        &common_ != &x_.common || &tasks_ != &x_.tasks ||
        &social_ != &x_.face.disclosure.social ||
        &links_ != &*x_.face.interaction.communication_links ||
        p.channel != MessageChannelKind::carrier_delivery ||
        sender.kind != SubjectKind::actor ||
        p.recipient.kind != SubjectKind::actor)
      reject("TaskReportPropagationCandidateMismatch");
    const auto *prepared =
        dynamic_cast<const Prepared *>(p.original_prepared.get());
    if (!prepared)
      reject("TaskReportPropagationPreparedRequired");
    const auto *request =
        std::get_if<PostTaskReportCarriage>(&prepared->quote.command);
    if (!request || prepared->quote.actor_id != sender.id)
      reject("TaskReportPropagationPreparedMismatch");
    const auto *r = tasks_.transport_agreements->report_carriages.find(
        request->agreement_id);
    if (!r || r->terms.report_id != report ||
        r->terms.sender_actor_id != sender.id ||
        r->terms.recipient_actor_id != p.recipient.id)
      reject("TaskReportPropagationScopeChanged");
    ReadStampWriteBoundary boundary(authority_);
    auto fresh = take(
        preview_task_report_carriage(x_, sender.id, prepared->quote.command));
    if (compare(fresh.quoted_at, prepared->quote.quoted_at))
      reject("TaskReportPropagationMomentChanged");
    return take(apply_task_report_carriage_in_candidate(
        common_, tasks_, social_, links_, x_, sender.id, fresh));
  });
}
} // namespace hanmo::resident
