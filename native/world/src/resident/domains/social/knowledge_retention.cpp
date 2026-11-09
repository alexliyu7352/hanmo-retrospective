// 原 cursor/report/known/message
// 同一枚举算法；索引临时分配先计账，真实来源只借原行。
#include "knowledge_retention.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
SourceRefBuildView view(const SourceRef &s) { return {s.domain, s.kind, s.id}; }
bool same(SourceRefBuildView a, SourceRefBuildView b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
} // namespace
Status visit_social_knowledge_source_pins(const CommonStore &common,
                                          const SocialState &social,
                                          SocialKnowledgeSourcePinSink &sink) {
  if (!social.dynamic || !social.dynamic->time2)
    return common_success();
  const auto &d = *social.dynamic;
  const auto &t = *d.time2;
  CheckpointVector<SourceRefBuildView> originals;
  CheckpointVector<const MomentOwnerSource *> exacts;
  auto add = [&](SourceRefBuildView s) -> Status {
    if (s.domain.empty() || s.kind.empty() || s.id.empty())
      return common_failure("InvalidKnowledgeSourcePin");
    if (std::none_of(originals.begin(), originals.end(),
                     [&](auto v) { return same(v, s); })) {
      auto valid = sink.original(s);
      if (!valid.ok)
        return valid;
      originals.push_back(s);
    }
    return common_success();
  };
  auto time = [&](const SocialTimeKey &key) -> Status {
    const OwnedSocialIndexKey index(key);
    const auto *b = t.source_time_bindings.find(index.value());
    if (!b)
      return common_failure("KnowledgeSourceBindingMissing", index.value());
    Status valid = common_success();
    if (b->actual_source_ref.domain == "social" &&
        b->actual_source_ref.kind == "report_participant") {
      const auto *r = d.reports.find(b->actual_source_ref.id);
      const auto *h = r ? common.state().facts.find(r->fact_id) : nullptr;
      if (!h)
        return common_failure("KnowledgeParticipantSourceMissing",
                              b->actual_source_ref.id);
      valid = add({h->domain, h->kind, h->id});
    } else
      valid = add(view(b->actual_source_ref));
    if (!valid.ok)
      return valid;
    if (b->learned_after_common_fact) {
      valid = add(view(*b->learned_after_common_fact));
      if (!valid.ok)
        return valid;
    }
    if (const auto *exact = std::get_if<SocialExactSource>(&b->time_source)) {
      const auto &s = exact->original_timed_source_ref;
      if (s.source_schema <= 0)
        return common_failure("InvalidKnowledgeSourceSchema");
      if (std::none_of(exacts.begin(), exacts.end(), [&](const auto *v) {
            return v->source_schema == s.source_schema &&
                   same(view(v->source_ref), view(s.source_ref));
          })) {
        valid = sink.exact(s);
        if (!valid.ok)
          return valid;
        exacts.push_back(&s);
      }
      return add(view(s.source_ref));
    }
    return common_success();
  };
  // 先租字段副本，再构造原 typed key；索引字节仍由唯一原 OwnedSocialIndexKey
  // 编码。
  auto report_time = [&](const StableId &id) -> Status {


    auto memory = CheckpointMemoryCharge(0);
    return time(ReportObservationKey{checkpoint_fresh_string_copy(id)});
  };
  auto message_time = [&](const StableId &id, bool delivered) -> Status {


    auto memory = CheckpointMemoryCharge(0);
    if (delivered)
      return time(MessageDeliveredKey{checkpoint_fresh_string_copy(id)});
    return time(MessageCreatedKey{checkpoint_fresh_string_copy(id)});
  };
  if (t.collection_cursor.last_scanned_fact_id) {
    const auto *h =
        common.state().facts.find(*t.collection_cursor.last_scanned_fact_id);
    if (!h)
      return common_failure("ExactFactCursorAnchorUnavailable",
                            *t.collection_cursor.last_scanned_fact_id,
                            ErrorKind::internal);
    auto valid = add({h->domain, h->kind, h->id});
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : d.reports.rows()) {
    auto valid = report_time(row.first);
    if (!valid.ok)
      return valid;
  }
  for (const auto &owner : d.known_reports.rows())
    for (const auto &row : owner.second.rows()) {
      const auto &receiver = row.second.receiver;


      auto memory = CheckpointMemoryCharge(0);
      auto valid = time(SocialKnowledgeKey{
          {receiver.kind, checkpoint_fresh_string_copy(receiver.id)},
          checkpoint_fresh_string_copy(row.first)});
      if (!valid.ok)
        return valid;
    }
  for (const auto &row : d.pending_messages.rows()) {
    const auto &m = row.second;
    const auto *execution = t.message_execution_bindings.find(row.first);
    if (!execution)
      return common_failure("KnowledgeMessageExecutionMissing", row.first);
    auto valid = message_time(row.first, false);
    if (!valid.ok)
      return valid;
    if (m.address_binding) {
      valid = add(view(m.address_binding->original_business_fact_ref));
      if (!valid.ok)
        return valid;
    }
    if (m.status == MessageStatus::delivered) {
      valid = message_time(row.first, true);
      if (!valid.ok)
        return valid;
    }
    if (execution->actual_dossier_access_ref) {
      valid = add(view(*execution->actual_dossier_access_ref));
      if (!valid.ok)
        return valid;
    }
  }
  return common_success();
}
Outcome<SocialKnowledgeSourcePins>
social_knowledge_source_pins(const CommonStore &common,
                             const SocialState &social) {
  struct LegacyCollector final : SocialKnowledgeSourcePinSink {
    SocialKnowledgeSourcePins pins;
    Status original(SourceRefBuildView s) override {
      pins.original_sources.push_back(
          {StableId(s.domain), StableId(s.kind), StableId(s.id)});
      return common_success();
    }
    Status exact(const MomentOwnerSource &s) override {
      pins.exact_sources.push_back(s);
      return common_success();
    }
  } collector;
  auto valid = visit_social_knowledge_source_pins(common, social, collector);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(collector.pins), {}};
}
} // namespace hanmo::resident
