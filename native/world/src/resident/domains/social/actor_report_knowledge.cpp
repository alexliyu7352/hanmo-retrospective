#include "actor_report_knowledge.hpp"
#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "social_knowledge_cut.hpp"
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const char *code, const StableId &id = {},
                ErrorKind kind = ErrorKind::internal) {
  return {false, {}, common_failure(code, id, kind).error};
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
Status equal(const WorldMoment &a, const WorldMoment &b, const StableId &id) {
  auto order = compare_world_moments(a, b);
  return !order.ok     ? Status{false, {}, order.error}
         : order.value ? common_failure("KnowledgeAcquisitionMomentMismatch",
                                        id, ErrorKind::internal)
                       : common_success();
}
const SocialSourceTimeBinding *binding(const SocialTime2State &t,
                                       const SocialTimeKey &key) {
  return t.source_time_bindings.find(social_time_key(key));
}
} // namespace
Outcome<std::optional<ActorReportKnowledgeReceiptView>>
read_actor_report_knowledge_receipt(const ActorReportKnowledgeContext &x,
                                    const StableId &actor,
                                    const StableId &report_id) {
  using Result = std::optional<ActorReportKnowledgeReceiptView>;
  if (actor.empty() || !x.common.actor(actor))
    return fail<Result>("KnowledgeObserverMissing", actor);
  auto known = read_knowledge_receipt_view_v2(x.common, x.social, x.results,
                                              {SubjectKind::actor, actor},
                                              report_id, x.stamp);
  if (!known.ok)
    return {false, {}, known.error};
  if (!known.value)
    return {true, std::nullopt, {}};
  const auto &d = *x.social.dynamic;
  const auto &t = *d.time2;
  const auto &k = *known.value->original;
  const auto *r = d.reports.find(report_id);
  if (!r)
    return fail<Result>("KnownReportSourceMissing", report_id);
  auto valid = validate_exact_report_source(x.common, *r, x.results, x.stamp);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *b = binding(t, SocialKnowledgeKey{k.receiver, report_id});
  if (!b || std::holds_alternative<SocialLegacyBoundary>(b->time_source))
    return fail<Result>("KnowledgeActualAcquisitionRequired", report_id);
  auto observed = binding(t, ReportObservationKey{report_id});
  if (!observed)
    return fail<Result>("KnowledgeReportObservationMissing", report_id);
  auto at = read_social_bound_moment(*observed, x.results, x.stamp);
  if (!at.ok)
    return {false, {}, at.error};
  auto order = compare_world_moments(at.value, known.value->learned_at);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return fail<Result>("KnowledgeBeforeObservation", report_id);
  ActorReportKnowledgeReceiptView out;
  out.report = r;
  out.knowledge = std::move(*known.value);
  out.acquisition = b;
  if (!k.source_message_id) {
    if (is_public_supply_inspection_source(b->actual_source_ref)) {
      // 反读真实 Inspect 原源，不因当地公开或此人当前在场推断已读。
      auto inspected = validate_public_supply_inspection_knowledge_source(
          x.common, *r, k, *b, x.results, x.stamp);
      if (!inspected.ok)
        return {false, {}, inspected.error};
      return {true, std::move(out), {}};
    }
    const auto *exact = std::get_if<SocialExactSource>(&b->time_source);
    if (exact && exact->original_timed_source_ref.source_schema == 2 &&
        exact->original_timed_source_ref.source_ref.domain ==
            "family_collateral_legacy")
      return fail<Result>("NOT_IMPLEMENTED", report_id,
                          ErrorKind::unsupported_capability);
    if (!same_social_subject(k.receiver, k.source_subject) ||
        !same(b->actual_source_ref,
              {"social", "report_participant", report_id}) ||
        !std::holds_alternative<SocialExactMoment>(b->time_source))
      return fail<Result>("InvalidDirectKnowledgeSource", report_id);
    auto observers =
        x.results.direct_fact_observers_borrowed(r->fact_id, x.stamp);
    if (!observers.ok)
      return {false, {}, observers.error};
    if (!observers.value ||
        std::none_of(
            observers.value->begin(), observers.value->end(),
            [&](const auto &s) { return same_social_subject(s, k.receiver); }))
      return fail<Result>("UnwitnessedDirectKnowledge", report_id);
    return {true, std::move(out), {}};
  }
  const auto *m = d.pending_messages.find(*k.source_message_id);
  const auto *e = m ? t.message_execution_bindings.find(m->id) : nullptr;
  if (!m || !e || m->status != MessageStatus::delivered ||
      m->report_id != report_id || e->message_id != m->id ||
      !same_social_subject(k.source_subject, m->sender) ||
      !e->actual_delivery_ref)
    return fail<Result>("KnowledgeActualMessageMissing", report_id);
  Integer matches = 0;
  for (const auto &key : e->actual_knowledge_keys)
    if (same_social_subject(key.receiver, k.receiver) &&
        key.report_id == report_id)
      ++matches;
  if (matches != 1)
    return fail<Result>("KnowledgeActualMessageCoverage", report_id);
  const auto *created = binding(t, MessageCreatedKey{m->id});
  const auto *delivered = binding(t, MessageDeliveredKey{m->id});
  if (!created || !delivered ||
      !same(delivered->actual_source_ref, *e->actual_delivery_ref))
    return fail<Result>("KnowledgeMessageTimeMissing", m->id);
  auto sent = read_social_bound_moment(*created, x.results, x.stamp);
  auto received = read_social_bound_moment(*delivered, x.results, x.stamp);
  if (!sent.ok)
    return {false, {}, sent.error};
  if (!received.ok)
    return {false, {}, received.error};
  order = compare_world_moments(sent.value, received.value);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0 || received.value.half_day_tick < m->due_tick)
    return fail<Result>("KnowledgeMessageDeliveryBeforeDue", m->id);
  auto sender = read_knowledge_receipt_view_v2(x.common, x.social, x.results,
                                               m->sender, report_id, x.stamp);
  if (!sender.ok)
    return {false, {}, sender.error};
  if (!sender.value)
    return fail<Result>("KnowledgeMessageSenderUnknown", m->id);
  order = compare_world_moments(sender.value->learned_at, sent.value);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return fail<Result>("KnowledgeMessageSenderNotYetKnown", m->id);
  out.message = m;
  if (!m->address_binding) {
    const auto *legacy = std::get_if<SocialLegacyChannel>(&e->source_binding);
    if (!legacy || legacy->original_profile != "local_society_v1" ||
        !same_social_subject(m->receiver, k.receiver) ||
        !same(b->actual_source_ref, *e->actual_delivery_ref) ||
        e->actual_delivery_ref->domain != "social" ||
        e->actual_delivery_ref->kind != "social_period_close")
      return fail<Result>("KnowledgeLegacyDeliverySourceMismatch", m->id);
    if (!x.periods || !x.timeline || !x.catalog)
      return fail<Result>("KnowledgeOriginalSocialPeriodRequired", m->id,
                          ErrorKind::unsupported_capability);
    valid =
        validate_social_due_periods(x.common, x.social, *x.periods, *x.timeline,
                                    *x.catalog, x.initial_tick);
    if (!valid.ok)
      return {false, {}, valid.error};
    valid = equal(received.value, out.knowledge.learned_at, m->id);
    if (!valid.ok)
      return {false, {}, valid.error};
    out.kind = ActorReportKnowledgeKind::legacy_channel;
    return {true, std::move(out), {}};
  }
  if (!x.communications || !x.channels)
    return fail<Result>("KnowledgeOriginalCommunicationRequired", m->id,
                        ErrorKind::unsupported_capability);
  const auto *source = std::get_if<SocialS3BusinessLink>(&e->source_binding);
  if (!source ||
      !same(source->original_business_fact_ref,
            m->address_binding->original_business_fact_ref) ||
      source->original_business_fact_ref.domain != "interaction" ||
      source->original_business_fact_ref.kind != "communication_link" ||
      e->actual_delivery_ref->domain != "interaction" ||
      e->actual_delivery_ref->kind != "communication_delivery")
    return fail<Result>("KnowledgeBusinessLinkMismatch", m->id);
  auto delivery = read_verified_communication_delivery_source(
      x.common, *x.communications, e->actual_delivery_ref->id, *x.channels, *r);
  if (!delivery.ok)
    return {false, {}, delivery.error};
  out.business = delivery.value.business;
  out.delivery = delivery.value.delivery;
  if (b->learned_after_common_fact) {
    auto causal = validate_social_knowledge_cut_source(
        x.common, *b, out.delivery->arrival_ref);
    if (!causal.ok)
      return {false, {}, causal.error};
  }
  if (out.business->id != source->original_business_fact_ref.id ||
      out.business->original_public_message_ref != m->id)
    return fail<Result>("KnowledgeBusinessMessageMismatch", m->id);
  auto origin =
      x.channels->origin(out.business->original_business_fact_ref,
                         out.business->original_business_source_schema);
  if (!origin.ok)
    return {false, {}, origin.error};
  if (origin.value.report_id != report_id ||
      !same_social_subject(origin.value.sender, m->sender) ||
      origin.value.channel != m->channel ||
      origin.value.due_tick != m->due_tick ||
      origin.value.local_place_id != m->local_place_id)
    return fail<Result>("KnowledgeMessageOriginMismatch", m->id);
  valid = equal(sent.value, origin.value.sent_at, m->id);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = equal(received.value, out.delivery->delivered_at, m->id);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *exact = std::get_if<SocialExactSource>(&b->time_source);
  if (!exact || exact->original_timed_source_ref.source_schema != 1 ||
      !same(exact->original_timed_source_ref.source_ref, b->actual_source_ref))
    return fail<Result>("KnowledgeAcquisitionSourceMismatch", m->id);
  if (same(b->actual_source_ref, *e->actual_delivery_ref)) {
    if (!out.delivery->contents_disclosed || out.delivery->custody_receiver ||
        !same_social_subject(out.delivery->actual_receiver, k.receiver))
      return fail<Result>("KnowledgeDeliveryNotPersonal", m->id);
    valid = equal(out.knowledge.learned_at, out.delivery->delivered_at, m->id);
    if (!valid.ok)
      return {false, {}, valid.error};
    out.kind = ActorReportKnowledgeKind::personal_delivery;
    return {true, std::move(out), {}};
  }
  if (b->actual_source_ref.domain != "interaction" ||
      b->actual_source_ref.kind != "communication_dossier_access")
    return fail<Result>("KnowledgeDossierSourceMismatch", m->id);
  const auto *a =
      x.communications->dossier_accesses.find(b->actual_source_ref.id);
  if (!a || a->id != b->actual_source_ref.id ||
      a->business_link_id != out.business->id ||
      !same_social_subject(a->reader, k.receiver) ||
      std::count_if(
          out.business->dossier_source_refs.begin(),
          out.business->dossier_source_refs.end(),
          [&](const auto &s) { return same(s, b->actual_source_ref); }) != 1)
    return fail<Result>("KnowledgeDossierReceiptMissing", m->id);
  auto access =
      x.channels->dossier_access(a->original_access_ref, a->source_schema);
  if (!access.ok)
    return {false, {}, access.error};
  if (access.value.id != a->id ||
      access.value.business_link_id != a->business_link_id ||
      access.value.source_schema != a->source_schema ||
      !same(access.value.original_access_ref, a->original_access_ref) ||
      !same_social_subject(access.value.reader, a->reader) ||
      !same(access.value.authority_source, a->authority_source))
    return fail<Result>("KnowledgeDossierOriginalMismatch", m->id);
  valid = equal(access.value.accessed_at, a->accessed_at, m->id);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = equal(out.knowledge.learned_at, a->accessed_at, m->id);
  if (!valid.ok)
    return {false, {}, valid.error};
  order = compare_world_moments(out.delivery->delivered_at, a->accessed_at);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return fail<Result>("KnowledgeDossierBeforeDeposit", m->id);
  if (b->learned_after_common_fact) {
    auto causal = validate_social_knowledge_cut_source(x.common, *b,
                                                       a->original_access_ref);
    if (!causal.ok)
      return {false, {}, causal.error};
  }
  out.kind = ActorReportKnowledgeKind::dossier_read;
  out.dossier = a;
  return {true, std::move(out), {}};
}
Status validate_actor_report_knowledge_before_source(
    const ActorReportKnowledgeContext &x,
    const ActorReportKnowledgeReceiptView &receipt, const SourceRef &consumer,
    const WorldMoment &at) {
  if (!receipt.report || !receipt.knowledge.original ||
      receipt.knowledge.original->receiver.kind != SubjectKind::actor)
    return common_failure("ActualActorKnowledgeReceiptRequired", consumer.id,
                          ErrorKind::internal);
  const auto *head = x.common.state().facts.find(consumer.id);
  if (!head || head->domain != consumer.domain || head->kind != consumer.kind)
    return common_failure("KnowledgeConsumerSourceMissing", consumer.id,
                          ErrorKind::internal);
  auto fresh = read_actor_report_knowledge_receipt(
      x, receipt.knowledge.original->receiver.id, receipt.report->id);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  if (!fresh.value ||
      fresh.value->knowledge.original != receipt.knowledge.original ||
      fresh.value->acquisition != receipt.acquisition)
    return common_failure("KnowledgeReceiptOwnerMismatch", consumer.id,
                          ErrorKind::internal);
  auto order = compare_world_moments(fresh.value->knowledge.learned_at, at);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value < 0)
    return common_success();
  if (order.value > 0)
    return common_failure("KnowledgeLearnedAfterConsumer", consumer.id,
                          ErrorKind::rule);
  if (fresh.value->acquisition->learned_after_common_fact)
    return validate_social_knowledge_cut_before_source(
        x.common, *fresh.value->acquisition, consumer);
  if (fresh.value->kind == ActorReportKnowledgeKind::personal_delivery)
    return x.channels->validate_delivery_before_common_source(
        *fresh.value->delivery, consumer);
  return common_failure("KnowledgeSameMomentAcquisitionCutRequired",
                        consumer.id, ErrorKind::unsupported_capability);
}
} // namespace hanmo::resident
