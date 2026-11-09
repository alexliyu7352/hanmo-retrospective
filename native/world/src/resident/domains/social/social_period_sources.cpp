#include "social_knowledge_cut.hpp"
// 唯一Social期间来源：Opening冻结原队列，关闭复用原deliver并在同候选记录真实消费。
#include "social_period_sources.hpp"
#include "social_time2.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.artifact_sha256 == b.artifact_sha256 &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions;
}
bool same_ref(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
Status integral(const WorldMoment &at) {
  auto check = compare_world_moments(at, at);
  if (!check.ok)
    return {false, {}, check.error};
  auto zero = compare_fractions(at.sub_half_day, CanonicalFraction{});
  if (!zero.ok)
    return {false, {}, zero.error};
  return zero.value ? common_failure("SocialPeriodRequiresIntegerBoundary")
                    : common_success();
}
bool macro_message(const DynamicSocialState &state, const StableId &id) {
  const auto *binding =
      state.time2 ? state.time2->message_execution_bindings.find(id) : nullptr;
  const auto *legacy =
      binding ? std::get_if<SocialLegacyChannel>(&binding->source_binding)
              : nullptr;
  return legacy && legacy->original_profile == "local_society_v1";
}
Outcome<WorldMoment> actual_time(const DynamicSocialState &state,
                                 const SocialTimeKey &key) {
  if (!state.time2)
    return {false, {}, common_failure("ExactSocialStateRequired").error};
  const auto *b = state.time2->source_time_bindings.find(social_time_key(key));
  const auto *m = b ? std::get_if<SocialExactMoment>(&b->time_source) : nullptr;
  if (!m)
    return {false,
            {},
            common_failure("SocialWriterMomentRequired", social_time_key(key))
                .error};
  auto valid = compare_world_moments(m->moment, m->moment);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, m->moment, {}};
}
} // namespace
Outcome<const SocialPeriodOpening *>
read_social_period_opening_source(const SocialPeriodSources &sources,
                                  const MomentOwnerSource &source,
                                  const CatalogIdentity &catalog) {
  const auto &r = source.source_ref;
  if (source.source_schema != 1 || r.domain != "social" ||
      r.kind != "social_period_open")
    return {false,
            {},
            common_failure("SocialOpeningSourceKindMismatch", r.id,
                           ErrorKind::internal)
                .error};
  const auto *value = sources.openings.find(r.id);
  if (!value || value->period_id != r.id ||
      value->provider_rule_id != social_due_provider_id ||
      value->provider_rule_version != 1 ||
      !same_catalog(value->catalog_identity, catalog))
    return {
        false,
        {},
        common_failure("SocialOpeningSourceMissing", r.id, ErrorKind::internal)
            .error};
  auto whole = integral(value->opened_at);
  if (!whole.ok)
    return {false, {}, whole.error};
  if (value->opened_at.half_day_tick < 0 ||
      value->opened_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      value->through_tick != value->opened_at.half_day_tick + 1)
    return {false,
            {},
            common_failure("SocialOpeningSourceBoundaryMismatch", r.id,
                           ErrorKind::internal)
                .error};
  return {true, value, {}};
}
Status open_social_due_period(const CommonStore &common,
                              const SocialState &social,
                              SocialPeriodSources &sources,
                              const StableId &period, const WorldMoment &at,
                              const CatalogIdentity &catalog) {
  auto whole = integral(at);
  if (!whole.ok)
    return whole;
  if (at.half_day_tick == std::numeric_limits<Integer>::max() ||
      period.empty() || sources.openings.find(period) ||
      sources.closures.find(period))
    return common_failure("InvalidSocialPeriodOpening", period);
  if (!social.dynamic || !social.dynamic->time2)
    return common_failure("ExactSocialStateRequired");
  SocialPeriodOpening opening;
  opening.period_id = period;
  opening.opened_at = at;
  opening.through_tick = at.half_day_tick + 1;
  opening.catalog_identity = catalog;
  for (const auto &row : social.dynamic->pending_messages.rows())
    if (row.second.status == MessageStatus::pending &&
        macro_message(*social.dynamic, row.first)) {
      auto created = actual_time(*social.dynamic, MessageCreatedKey{row.first});
      if (!created.ok)
        return {false, {}, created.error};
      auto before = compare_world_moments(created.value, at);
      if (!before.ok)
        return {false, {}, before.error};
      if (before.value <= 0 && row.second.due_tick <= opening.through_tick)
        opening.due_message_ids.push_back(row.first);
    }
  if (!sources.openings.insert(period, std::move(opening)))
    return common_failure("DuplicateSocialPeriodOpening", period);
  return common_success();
}
Status deliver_social_due_period(CommonStore &common, SocialState &social,
                                 SocialPeriodSources &sources,
                                 const StableId &period, const WorldMoment &at,
                                 const CatalogIdentity &catalog) {
  auto whole = integral(at);
  if (!whole.ok)
    return whole;
  const auto *opening = sources.openings.find(period);
  if (!opening || sources.closures.find(period) ||
      !same_catalog(opening->catalog_identity, catalog) ||
      at.half_day_tick != opening->through_tick)
    return common_failure("InvalidSocialPeriodClose", period);
  if (!social.dynamic || !social.dynamic->time2)
    return common_failure("ExactSocialStateRequired");
  auto &d = *social.dynamic;
  auto &t = *d.time2;
  std::set<StableId> newly_known, first_knowledge;
  for (const auto &id : opening->due_message_ids) {
    const auto *message = d.pending_messages.find(id);
    const auto *execution = t.message_execution_bindings.find(id);
    if (!message || !macro_message(d, id) ||
        message->status != MessageStatus::pending || !execution ||
        execution->actual_delivery_ref ||
        !execution->actual_knowledge_keys.empty() ||
        t.source_time_bindings.find(social_time_key(MessageDeliveredKey{id})))
      return common_failure("SocialOpeningMessageAlreadyConsumed", id);
    if (!find_social_knowledge(d, {message->receiver, message->report_id}) &&
        first_knowledge
            .insert(social_time_key(
                SocialKnowledgeKey{message->receiver, message->report_id}))
            .second)
      newly_known.insert(id);
  }
  DynamicStepInput step;
  step.from_tick = opening->opened_at.half_day_tick;
  step.to_tick = opening->through_tick;
  step.due_message_ids = opening->due_message_ids;
  auto delivered = deliver_dynamic_reports_at_period(common, social, step, at);
  if (!delivered.ok)
    return delivered;
  const SourceRef source{"social", "social_period_close", period};
  for (const auto &id : opening->due_message_ids) {
    const auto *message = d.pending_messages.find(id);
    auto *execution = t.message_execution_bindings.find(id);
    if (!message || message->status != MessageStatus::delivered || !execution)
      return common_failure("SocialActualDeliveryMissing", id);
    SocialTimeKey key = MessageDeliveredKey{id};
    if (!t.source_time_bindings.insert(social_time_key(key),
                                       {key, SocialExactMoment{at}, source}))
      return common_failure("DuplicateMessageDeliveryBinding", id);
    execution->actual_delivery_ref = source;
    if (newly_known.count(id)) {
      SocialKnowledgeKey knowledge{message->receiver, message->report_id};
      const auto *actual = find_social_knowledge(d, knowledge);
      if (!actual || actual->source_message_id != id ||
          actual->learned_tick != at.half_day_tick)
        return common_failure("SocialActualKnowledgeMissing", id);
      SocialTimeKey learned = knowledge;
      SocialSourceTimeBinding acquired{learned, SocialExactMoment{at}, source};
      auto cut = capture_social_knowledge_cut(common, d.version, acquired);
      if (!cut.ok)
        return cut;
      if (!t.source_time_bindings.insert(social_time_key(learned),
                                         std::move(acquired)))
        return common_failure("DuplicateKnowledgeTimeBinding", id);
      execution->actual_knowledge_keys.push_back(std::move(knowledge));
    }
  }
  if (!sources.closures.insert(period, {period, at, opening->due_message_ids}))
    return common_failure("DuplicateSocialPeriodClose", period);
  return common_success();
}
Status validate_social_due_period_completion(const CommonStore &common,
                                             const SocialState &social,
                                             const SocialPeriodSources &sources,
                                             const StableId &period,
                                             const WorldMoment &at,
                                             const CatalogIdentity &catalog) {
  auto whole = integral(at);
  if (!whole.ok)
    return whole;
  const auto *opening = sources.openings.find(period);
  const auto *closure = sources.closures.find(period);
  if (!opening || !closure ||
      !same_catalog(opening->catalog_identity, catalog) ||
      opening->through_tick != at.half_day_tick ||
      closure->delivered_message_ids != opening->due_message_ids)
    return common_failure("SocialDueReceiptStageNotClosed", period);
  auto equal = compare_world_moments(at, closure->closed_at);
  if (!equal.ok)
    return {false, {}, equal.error};
  if (equal.value)
    return common_failure("SocialClosingMomentMismatch", period);
  if (!social.dynamic || !social.dynamic->time2)
    return common_failure("ExactSocialStateRequired");
  for (const auto &id : closure->delivered_message_ids) {
    const auto *m = social.dynamic->pending_messages.find(id);
    const auto *b = social.dynamic->time2->message_execution_bindings.find(id);
    if (!m || m->status != MessageStatus::delivered || !b ||
        !b->actual_delivery_ref ||
        !same_ref(*b->actual_delivery_ref,
                  {"social", "social_period_close", period}))
      return common_failure("SocialActualDeliveryMissing", id);
  }
  return common_success();
}
// 旧关闭入口仍走同一原正文；此叶只核已递送成果，不再投递或改知识。
Status close_social_due_period(CommonStore &common, SocialState &social,
                               SocialPeriodSources &sources,
                               const StableId &period, const WorldMoment &at,
                               const CatalogIdentity &catalog) {
  return validate_social_due_period_completion(common, social, sources, period,
                                               at, catalog);
}
Status validate_social_due_periods(const CommonStore &common,
                                   const SocialState &social,
                                   const SocialPeriodSources &sources,
                                   const WorldTimelineState &timeline,
                                   const CatalogIdentity &catalog,
                                   Integer initial_tick) {
  if (!social.dynamic || !social.dynamic->time2 || initial_tick < 0 ||
      initial_tick > common.tick())
    return common_failure("InvalidSocialPeriodScope");
  const auto &d = *social.dynamic;
  const auto &t = *d.time2;
  const bool open = timeline.boundary_state == PeriodBoundaryState::open;
  const auto periods =
      static_cast<std::uint64_t>(common.tick() - initial_tick) + (open ? 1 : 0);
  if (periods != sources.openings.size() ||
      sources.closures.size() !=
          static_cast<std::uint64_t>(common.tick() - initial_tick))
    return common_failure("SocialPeriodHistoryGap");
  std::set<StableId> consumed;
  Integer expected = initial_tick;
  StableId latest;
  for (const auto &row : sources.openings.rows()) {
    const auto &opening = row.second;
    auto whole = integral(opening.opened_at);
    if (!whole.ok)
      return whole;
    if (row.first != opening.period_id ||
        opening.opened_at.half_day_tick != expected ||
        expected == std::numeric_limits<Integer>::max() ||
        opening.through_tick != expected + 1 ||
        opening.provider_rule_id != social_due_provider_id ||
        opening.provider_rule_version != 1 ||
        !same_catalog(opening.catalog_identity, catalog))
      return common_failure("InvalidSocialPeriodHistory", row.first);
    const auto *closure = sources.closures.find(row.first);
    const bool active = open && row.first == timeline.period_id;
    if (active) {
      if (closure || expected != common.tick() ||
          timeline.opening_receipt_ref != row.first ||
          timeline.close_receipt_ref)
        return common_failure("InvalidCurrentSocialOpening", row.first);
    } else if (!closure)
      return common_failure("UnclosedHistoricalSocialPeriod", row.first);
    std::vector<StableId> due;
    for (const auto &message : d.pending_messages.rows()) {
      if (!macro_message(d, message.first))
        continue;
      auto created = actual_time(d, MessageCreatedKey{message.first});
      if (!created.ok)
        return {false, {}, created.error};
      auto before = compare_world_moments(created.value, opening.opened_at);
      if (!before.ok)
        return {false, {}, before.error};
      if (before.value <= 0 &&
          message.second.due_tick <= opening.through_tick &&
          !consumed.count(message.first))
        due.push_back(message.first);
    }
    if (due != opening.due_message_ids)
      return common_failure("IncompleteSocialOpeningDueSet", row.first);
    if (closure) {
      auto closed = integral(closure->closed_at);
      if (!closed.ok)
        return closed;
      if (closure->period_id != row.first ||
          closure->closed_at.half_day_tick != opening.through_tick ||
          closure->delivered_message_ids != opening.due_message_ids)
        return common_failure("IncompleteSocialPeriodConsumption", row.first);
    }
    for (const auto &id : opening.due_message_ids) {
      const auto *message = d.pending_messages.find(id);
      const auto *execution = t.message_execution_bindings.find(id);
      if (!message || !execution)
        return common_failure("MissingSocialPeriodMessage", id);
      const auto *binding =
          t.source_time_bindings.find(social_time_key(MessageDeliveredKey{id}));
      if (active) {
        if (message->status != MessageStatus::pending || binding ||
            execution->actual_delivery_ref ||
            !execution->actual_knowledge_keys.empty())
          return common_failure("OpenSocialMessageConsumed", id);
      } else {
        const SourceRef expected_ref{"social", "social_period_close",
                                     row.first};
        if (message->status != MessageStatus::delivered || !binding ||
            !execution->actual_delivery_ref ||
            !same_ref(*execution->actual_delivery_ref, expected_ref) ||
            !same_ref(binding->actual_source_ref, expected_ref) ||
            !consumed.insert(id).second)
          return common_failure("SocialPeriodDeliverySourceMismatch", id);
        auto delivered = actual_time(d, MessageDeliveredKey{id});
        if (!delivered.ok)
          return {false, {}, delivered.error};
        auto equal = compare_world_moments(delivered.value, closure->closed_at);
        if (!equal.ok)
          return {false, {}, equal.error};
        if (equal.value)
          return common_failure("SocialPeriodDeliveryMomentMismatch", id);
        for (const auto &key : execution->actual_knowledge_keys) {
          const auto *known = find_social_knowledge(d, key);
          if (!known || !same_social_subject(key.receiver, message->receiver) ||
              key.report_id != message->report_id ||
              known->source_message_id != id)
            return common_failure("SocialDeliveryKnowledgeMismatch", id);
          auto learned = actual_time(d, key);
          if (!learned.ok)
            return {false, {}, learned.error};
          auto same = compare_world_moments(learned.value, closure->closed_at);
          if (!same.ok)
            return {false, {}, same.error};
          if (same.value)
            return common_failure("SocialDeliveryKnowledgeMomentMismatch", id);
        }
        if (execution->actual_knowledge_keys.size() > 1)
          return common_failure("DuplicateSocialDeliveryKnowledge", id);
      }
    }
    latest = row.first;
    ++expected;
  }
  for (const auto &row : sources.closures.rows())
    if (!sources.openings.find(row.first))
      return common_failure("OrphanSocialPeriodClosure", row.first);
  if (periods) {
    if (timeline.period_id != latest ||
        timeline.opening_receipt_ref != latest ||
        (!open && timeline.close_receipt_ref != latest))
      return common_failure("SocialTimelineSourceMismatch");
  } else if (open || timeline.opening_receipt_ref || timeline.close_receipt_ref)
    return common_failure("InvalidInitialSocialTimeline");
  for (const auto &row : d.pending_messages.rows())
    if (row.second.status == MessageStatus::delivered &&
        macro_message(d, row.first) && !consumed.count(row.first))
      return common_failure("UncoveredSocialMacroDelivery", row.first);
  return common_success();
}
} // namespace hanmo::resident
