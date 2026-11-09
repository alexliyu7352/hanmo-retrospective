#include "face_to_face_propagation.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
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
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
void current(const FaceToFaceReportContext &x, const ReadStamp &s) {
  if (s.mode() != ReadMode::committed_current &&
      s.mode() != ReadMode::phase_current)
    reject("CurrentPropagationStampRequired");
  take(x.disclosure.authority.validate(s, x.common.state().identity, s.mode(),
                                       x.common, x.disclosure.versions));
}
const FaceToFaceContactSources &selected(const FaceToFaceReportContext &x) {
  if (&x.disclosure.common != &x.common)
    throw Failure{common_failure("FaceToFacePropagationCandidateMismatch", {},
                                 ErrorKind::internal)
                      .error};
  if (!x.interaction.face_to_face_contacts ||
      (x.interaction.face_to_face_contacts->version != 2 &&
       x.interaction.face_to_face_contacts->version != 3) ||
      !x.interaction.communication_links || !x.disclosure.social.dynamic ||
      !x.disclosure.social.dynamic->time2)
    reject("FaceToFaceReportSources2Required");
  return *x.interaction.face_to_face_contacts;
}
const StableId *other(const FaceToFaceContactResult &r, const SubjectRef &s) {
  if (s.kind != SubjectKind::actor)
    return nullptr;
  if (r.session_actor_id == s.id)
    return &r.provider_actor_id;
  if (r.provider_actor_id == s.id)
    return &r.session_actor_id;
  return nullptr;
}
// One source-text construction for the original validation temporary and the
// actual owned output row. Reservation precedes every fresh string allocation.
SourceRef contact_source_copy(const StableId &id,
                              CheckpointMemoryCharge &memory) {
  const std::string_view parts[] = {"interaction",
                                    face_to_face_contact_source_kind, id};


  (memory).grow(0);
  return {std::string(parts[0]), std::string(parts[1]), std::string(parts[2])};
}

struct Prepared final : PreparedPropagationChannel {
  FaceToFaceReportQuote quote;
  explicit Prepared(FaceToFaceReportQuote &&q) noexcept : quote(std::move(q)) {}
};
// The caller supplied actual source is checked even when no current session is
// available. A contact records a known person; it does not disclose any report.
void known_contact(const FaceToFaceReportContext &x, const SubjectRef &s,
                   const PropagationRecipient &r) {
  const auto *c = take(read_face_to_face_contact_source(
      x.common, x.interaction, x.catalog, r.actual_known_contact_source, 1));
  const auto *id = other(*c, s);
  if (!id || r.subject.kind != SubjectKind::actor || r.subject.id != *id)
    reject("PropagationKnownContactMismatch", r.subject.id);
  if (take(compare_world_moments(
          c->occurred_at, take(x.disclosure.clock.read_current_moment()))) > 0)
    reject("PropagationKnownContactFromFuture", r.subject.id);
}
} // namespace
bool NativeFaceToFacePropagationRead::supports(
    MessageChannelKind c) const noexcept {
  return c == MessageChannelKind::face_to_face;
}
bool NativeFaceToFacePropagationRead::supports_report(
    KnowledgeCategory c) const noexcept {
  return c == KnowledgeCategory::supply || c == KnowledgeCategory::arrear ||
         c == KnowledgeCategory::action_outcome ||
         c == KnowledgeCategory::intelligence_probe ||
         c == KnowledgeCategory::cultural_content;
}
Outcome<PropagationRecipientPage>
NativeFaceToFacePropagationRead::known_recipients(
    const SubjectRef &s, Integer cursor, Integer limit,
    const ReadStamp &stamp) const {
  return guard<PropagationRecipientPage>([&] {
    const auto &x = context_;
    current(x, stamp);
    const auto &state = selected(x);
    if (s.kind != SubjectKind::actor || cursor < 0 || limit < 1 || limit > 64)
      reject("InvalidPropagationContactPage", s.id);
    // Stable actor order and a rotating start, with bounded result allocation.
    // Repeated historical sessions do not create duplicate recipients.
    auto visit_unique = [&](auto fn) {
      for (const auto &row : state.results.rows()) {
        const auto *id = other(row.second, s);
        if (!id)
          continue;
        bool prior = false;
        for (const auto &p : state.results.rows()) {
          if (&p == &row)
            break;
          const auto *previous = other(p.second, s);
          if (previous && *previous == *id) {
            prior = true;
            break;
          }
        }
        if (prior)
          continue;
        {
          CheckpointMemoryCharge source_memory;
          const auto source = contact_source_copy(row.first, source_memory);
          take(read_face_to_face_contact_source(x.common, x.interaction,
                                                x.catalog, source, 1));
        }
        fn(*id, row.first);
      }
    };
    std::size_t count = 0;
    visit_unique([&](const auto &, const auto &) { ++count; });
    PropagationRecipientPage out;
    if (!count)
      return out;
    const auto start = static_cast<std::size_t>(cursor) % count;
    const auto wanted =
        std::min<std::size_t>(count, static_cast<std::size_t>(limit));


    (out.memory_).grow(0);
    out.values_.reserve(wanted);
    for (std::size_t n = 0; n < wanted; ++n) {
      const auto rank = (start + n) % count;
      const StableId *chosen = nullptr;
      const StableId *source = nullptr;
      visit_unique([&](const auto &id, const auto &ref) {
        std::size_t less = 0;
        visit_unique([&](const auto &other_id, const auto &) {
          if (other_id < id)
            ++less;
        });
        if (less == rank) {
          chosen = &id;
          source = &ref;
        }
      });
      if (!chosen || !source)
        reject("PropagationKnownContactOrderMismatch");


      (out.memory_).grow(0);
      out.values_.push_back({{SubjectKind::actor, *chosen},
                             contact_source_copy(*source, out.memory_)});
    }
    return out;
  });
}
Outcome<std::optional<PropagationChannelProposal>>
NativeFaceToFacePropagationRead::prepare(const SubjectRef &s,
                                         const Report &report,
                                         const PropagationRecipient &r,
                                         MessageChannelKind channel,
                                         const ReadStamp &stamp) const {
  return guard<std::optional<PropagationChannelProposal>>(
      [&]() -> std::optional<PropagationChannelProposal> {
        const auto &x = context_;
        current(x, stamp);
        selected(x);
        if (channel != MessageChannelKind::face_to_face)
          return {};
        known_contact(x, s, r);
        bool active = false;
        for (const auto &session : x.interaction.interactions) {
          const bool pair =
              (session.actor_id == s.id &&
               session.context.provider_actor_id == r.subject.id) ||
              (session.actor_id == r.subject.id &&
               session.context.provider_actor_id == s.id);
          if (pair && (session.phase == "say" || session.phase == "choice") &&
              face_to_face_contact_context_valid(session.context)) {
            if (active)
              reject("AmbiguousInteractionContact");
            active = true;
          }
        }
        if (!active)
          return {};


        auto request_memory = CheckpointMemoryCharge(0);
        const TellKnownReport request{report.id, r.subject.id};
        auto quote = preview_face_to_face_report(x, s.id, request);
        if (!quote.ok) {
          if (quote.error.original.code == "ReportDisclosureNotPermitted")
            return {};
          throw Failure{std::move(quote.error)};
        }
        return PropagationChannelProposal{
            r.subject, channel,
            checkpoint_make_shared<const Prepared>(std::move(quote.value))};
      });
}
Outcome<PropagationActualDispatch>
NativeFaceToFacePropagationRead::read_actual_dispatch(
    const StableId &id) const {
  return guard<PropagationActualDispatch>([&] {
    const auto &x = context_;
    selected(x);
    const auto *m = x.disclosure.social.dynamic->pending_messages.find(id);
    const MessageBusinessLink *l = nullptr;
    for (const auto &row : x.interaction.communication_links->links.rows())
      if (row.second.original_public_message_ref == id) {
        if (l)
          reject("DuplicateReportDisclosureMessage", id);
        l = &row.second;
      }
    if (!m || !l || m->channel != "face_to_face" ||
        m->status != MessageStatus::delivered)
      reject("ActualFaceToFaceDisclosureMissing", id);
    const auto *d = take(read_face_to_face_report_source(
        x, l->original_business_fact_ref, l->original_business_source_schema));
    if (m->report_id != d->report_id || m->sender.kind != SubjectKind::actor ||
        m->sender.id != d->sender_actor_id)
      reject("FaceToFaceReportContentMismatch", id);
    const auto *b =
        x.disclosure.social.dynamic->time2->message_execution_bindings.find(id);
    const auto *delivery =
        b && b->actual_delivery_ref
            ? x.interaction.communication_links->deliveries.find(
                  b->actual_delivery_ref->id)
            : nullptr;
    if (!delivery || delivery->business_link_id != l->id ||
        delivery->actual_receiver.kind != SubjectKind::actor ||
        delivery->actual_receiver.id != d->recipient_actor_id ||
        !delivery->contents_disclosed ||
        !same(delivery->arrival_ref, l->original_business_fact_ref) ||
        take(compare_world_moments(delivery->delivered_at, d->disclosed_at)))
      reject("ActualFaceToFaceDeliveryMissing", id);
    return PropagationActualDispatch{
        id,
        d->report_id,
        {SubjectKind::actor, d->sender_actor_id},
        {SubjectKind::actor, d->recipient_actor_id},
        MessageChannelKind::face_to_face,
        d->disclosed_at,
        delivery->delivered_at,
        l->original_business_fact_ref};
  });
}
Status NativeFaceToFacePropagationRead::validate_known_recipient_at_cut(
    const SubjectRef &sender, const SubjectRef &recipient,
    const OriginalSocialRelationCut &cut) const {
  return guard<Done>([&] {
    const auto &x = context_;
    const auto &state = selected(x);
    if (sender.kind != SubjectKind::actor ||
        recipient.kind != SubjectKind::actor || sender.id.empty() ||
        recipient.id.empty() || sender.id == recipient.id)
      reject("PropagationKnownContactSubjectMismatch", recipient.id);
    const auto count =
        take(read_actual_common_fact_prefix(x.common, cut.original_prefix));
    const auto &order = x.common.state().facts.order();
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
      const auto *head = x.common.state().facts.find(order[ordinal]);
      if (!head || head->id != order[ordinal])
        reject("PropagationKnownContactPrefixHeadMissing", order[ordinal]);
      if (head->domain != "interaction" ||
          head->kind != face_to_face_contact_source_kind)
        continue;
      const auto *row = state.results.find(head->id);
      const auto *known = row ? other(*row, sender) : nullptr;
      if (!known || *known != recipient.id)
        continue;


      auto memory = CheckpointMemoryCharge(0);
      const SourceRef source{head->domain, head->kind, head->id};
      const auto *actual = take(read_face_to_face_contact_source(
          x.common, x.interaction, x.catalog, source, 1));
      if (take(compare_world_moments(actual->occurred_at, cut.adopted_at)) > 0)
        reject("PropagationKnownContactAfterAdoption", recipient.id);
      return Done{};
    }
    reject("PropagationHistoricalKnownContactRequired", recipient.id);
  });
}
Status NativeFaceToFacePropagationRead::validate_actual_dispatch_after_cut(
    const PropagationActualDispatch &dispatch,
    const OriginalSocialRelationCut &cut) const {
  return guard<Done>([&] {
    const auto original = take(read_actual_dispatch(dispatch.message_id));
    take(validate_propagation_original_dispatch_after_cut(
        context_.common, dispatch, original, cut));
    return Done{};
  });
}
Status NativeFaceToFacePropagationRead::validate_historical_disclosure(
    const SocialPropagationDecision &decision,
    const PropagationActualDispatch &dispatch) const {
  return guard<Done>([&] {
    auto actual = take(read_actual_dispatch(dispatch.message_id));
    if (actual.report_id != decision.report_id ||
        !same_social_subject(actual.sender, decision.subject) ||
        !same_social_subject(actual.recipient, dispatch.recipient) ||
        !same(actual.original_business_source,
              dispatch.original_business_source) ||
        take(compare_world_moments(actual.created_at,
                                   WorldMoment{decision.evaluated_at, {}})))
      reject("PropagationHistoricalDisclosureMismatch", dispatch.message_id);
    return Done{};
  });
}
Outcome<StableId> NativeFaceToFacePropagationWrite::adopt(
    const PropagationChannelProposal &p, const SubjectRef &sender,
    const StableId &report, const ReadStamp &stamp) {
  return guard<StableId>([&] {
    const auto &x = context_;
    current(x, stamp);
    if (&authority_ != &x.disclosure.authority || &common_ != &x.common ||
        &interaction_ != &x.interaction || &social_ != &x.disclosure.social ||
        p.channel != MessageChannelKind::face_to_face ||
        sender.kind != SubjectKind::actor)
      reject("PropagationFaceToFaceCandidateMismatch");
    const auto *prepared =
        dynamic_cast<const Prepared *>(p.original_prepared.get());
    if (!prepared || prepared->quote.sender_actor_id != sender.id ||
        prepared->quote.request.report_id != report ||
        p.recipient.kind != SubjectKind::actor ||
        prepared->quote.request.recipient_actor_id != p.recipient.id)
      reject("PropagationFaceToFacePreparedMismatch");
    ReadStampWriteBoundary boundary(authority_);


    auto request_memory = CheckpointMemoryCharge(0);
    const TellKnownReport request{report, p.recipient.id};
    auto fresh = take(preview_face_to_face_report(x, sender.id, request));
    if (fresh.contact.session_id != prepared->quote.contact.session_id)
      reject("PropagationContactChanged", report);
    return take(tell_known_report_in_candidate(common_, interaction_, social_,
                                               x, sender.id, fresh));
  });
}
} // namespace hanmo::resident
