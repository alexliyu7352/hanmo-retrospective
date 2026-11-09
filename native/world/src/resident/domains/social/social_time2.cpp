#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "social_knowledge_cut.hpp"
#include "social_time2_version.hpp"
// 精确来源跟随原collect/queue/learn，原业务writer不被第二套传播规则替代。
#include "propagation.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "social_period_sources.hpp"
#include "social_time2.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <string_view>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> bad(const char *code, const StableId &path = {}) {
  return {false, {}, common_failure(code, path, ErrorKind::internal).error};
}
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
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same_fact(const DynamicFactView &a, const DynamicFactView &b) {
  if (a.id != b.id || a.kind != b.kind || a.rule_version != b.rule_version ||
      a.tick != b.tick || a.cause_fact_ids != b.cause_fact_ids ||
      a.affair_ids != b.affair_ids || a.subject_id != b.subject_id ||
      a.outcome != b.outcome || a.publicly_visible != b.publicly_visible ||
      a.participants.size() != b.participants.size() ||
      a.supply.has_value() != b.supply.has_value() ||
      a.obligation.has_value() != b.obligation.has_value())
    return false;
  for (std::size_t i = 0; i < a.participants.size(); ++i)
    if (!same_social_subject(a.participants[i].subject,
                             b.participants[i].subject) ||
        a.participants[i].role != b.participants[i].role)
      return false;
  if (a.supply) {
    const auto &x = *a.supply;
    const auto &y = *b.supply;
    if (x.source_key != y.source_key ||
        x.organization_id != y.organization_id || x.site_id != y.site_id ||
        x.inventory_id != y.inventory_id || x.item_id != y.item_id ||
        x.source_fact_id != y.source_fact_id ||
        x.target_amount != y.target_amount ||
        x.physical_amount != y.physical_amount ||
        x.source_revision != y.source_revision)
      return false;
  }
  if (a.obligation) {
    const auto &x = *a.obligation;
    const auto &y = *b.obligation;
    if (x.arrear_id != y.arrear_id || x.obligation_id != y.obligation_id ||
        x.principal_id != y.principal_id ||
        x.beneficiary_id != y.beneficiary_id ||
        x.source_fact_id != y.source_fact_id || x.due_tick != y.due_tick ||
        x.due_amount != y.due_amount || x.paid_amount != y.paid_amount ||
        x.outstanding_amount != y.outstanding_amount)
      return false;
  }
  return true;
}
// 此adapter只供原S5报告规则读取已经捕获的真值；fact_ids仅本次源游标冻结集合。
class ReportProjection final : public DynamicResultRead {
  const ExactResultTimeRead &source_;
  ExactResultReadStamp stamp_;
  std::vector<StableId> ids_;

public:
  ReportProjection(const ExactResultTimeRead &source,
                   const ExactResultReadStamp &stamp,
                   std::vector<StableId> ids = {})
      : source_(source), stamp_(stamp), ids_(std::move(ids)) {}
  const DeliveryFact *delivery(const StableId &) const override {
    return nullptr;
  }
  std::optional<DynamicFactView> fact(const StableId &id) const override {
    auto f = source_.fact_v2_borrowed(id, stamp_);
    if (!f.ok || !f.value || f.value->kind != "local_supply_observation")
      return {};
    return legacy_report_projection(*f.value);
  }
  std::vector<StableId> fact_ids(Integer, Integer) const override {
    return ids_;
  }
};
Status bind(SocialTime2State &state, SocialTimeKey key, SocialTimeSource time,
            SourceRef source) {
  auto id = social_time_key(key);
  if (!state.source_time_bindings.insert(
          id, {std::move(key), std::move(time), std::move(source)}))
    return common_failure("DuplicateSocialTimeBinding", id);
  return common_success();
}
} // namespace
bool same_social_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
namespace {
// 原索引语法只列一处借用片段；计长、普通兼容返回与 owning 返回共用这些片段。
template <class T, class Emit> void key_number(T value, Emit &emit) {
  char digits[std::numeric_limits<T>::digits10 + 3];
  auto encoded = std::to_chars(digits, digits + sizeof(digits), value);
  if (encoded.ec != std::errc{})
    throw std::logic_error("SocialIndexIntegerEncodingFailed");
  emit(
      std::string_view{digits, static_cast<std::size_t>(encoded.ptr - digits)});
}
template <class Emit>
void social_key_parts(const SubjectRef &subject, Emit emit) {
  key_number(static_cast<Integer>(subject.kind), emit);
  emit(":");
  emit(subject.id);
}
template <class Emit>
void social_key_parts(const SocialTimeKey &key, Emit emit) {
  std::visit(
      [&](const auto &k) {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, ReportObservationKey>) {
          emit("report:");
          emit(k.report_id);
        } else if constexpr (std::is_same_v<T, MessageCreatedKey>) {
          emit("created:");
          emit(k.message_id);
        } else if constexpr (std::is_same_v<T, MessageDeliveredKey>) {
          emit("delivered:");
          emit(k.message_id);
        } else {
          emit("known:");
          key_number(static_cast<Integer>(k.receiver.kind), emit);
          emit(":");
          key_number(k.receiver.id.size(), emit);
          emit(":");
          emit(k.receiver.id);
          emit(k.report_id);
        }
      },
      key);
}
template <class Key> std::size_t social_key_size(const Key &key) {
  std::size_t size = 0;
  social_key_parts(key, [&](std::string_view part) {
    if (part.size() > SIZE_MAX - size)
      throw std::bad_alloc();
    size += part.size();
  });
  return size;
}
template <class Key> StableId make_social_key(const Key &key) {
  const auto size = social_key_size(key);
  StableId value(size, '\0');
  auto output = value.begin();
  social_key_parts(key, [&](std::string_view part) {
    output = std::copy(part.begin(), part.end(), output);
  });
  return value;
}
template <class Key> CheckpointMemoryCharge reserve_social_key(const Key &key) {


  return CheckpointMemoryCharge(0);
}
} // namespace
StableId social_subject_key(const SubjectRef &subject) {
  return make_social_key(subject);
}
StableId social_time_key(const SocialTimeKey &key) {
  return make_social_key(key);
}
OwnedSocialIndexKey::OwnedSocialIndexKey(const SubjectRef &subject)
    : memory_(reserve_social_key(subject)), value_(make_social_key(subject)) {}
OwnedSocialIndexKey::OwnedSocialIndexKey(const SocialTimeKey &key)
    : memory_(reserve_social_key(key)), value_(make_social_key(key)) {}
namespace {
bool consume_key_part(std::string_view &key, std::string_view part) {
  if (key.size() < part.size() || key.substr(0, part.size()) != part)
    return false;
  key.remove_prefix(part.size());
  return true;
}
template <class T> bool consume_key_integer(std::string_view &key, T value) {
  char digits[std::numeric_limits<T>::digits10 + 3];
  auto encoded = std::to_chars(digits, digits + sizeof(digits), value);
  return encoded.ec == std::errc{} &&
         consume_key_part(
             key, {digits, static_cast<std::size_t>(encoded.ptr - digits)});
}
bool subject_key_matches(std::string_view key, const SubjectRef &subject) {
  return consume_key_integer(key, static_cast<Integer>(subject.kind)) &&
         consume_key_part(key, ":") && consume_key_part(key, subject.id) &&
         key.empty();
}
bool knowledge_key_matches(std::string_view key, const SubjectRef &subject,
                           const StableId &report) {
  return consume_key_part(key, "known:") &&
         consume_key_integer(key, static_cast<Integer>(subject.kind)) &&
         consume_key_part(key, ":") &&
         consume_key_integer(key, subject.id.size()) &&
         consume_key_part(key, ":") && consume_key_part(key, subject.id) &&
         consume_key_part(key, report) && key.empty();
}
} // namespace
const OrderedTable<KnownReport> *
find_social_knowledge_table(const DynamicSocialState &state,
                            const SubjectRef &receiver) {
  for (const auto &owner : state.known_reports.rows())
    if (subject_key_matches(owner.first, receiver))
      return &owner.second;
  return nullptr;
}
const KnownReport *find_social_knowledge(const DynamicSocialState &state,
                                         const SubjectRef &receiver,
                                         const StableId &report) {
  const auto *table = find_social_knowledge_table(state, receiver);
  return table ? table->find(report) : nullptr;
}
const KnownReport *find_social_knowledge(const DynamicSocialState &state,
                                         const SocialKnowledgeKey &key) {
  return find_social_knowledge(state, key.receiver, key.report_id);
}
Status collect_exact_supply_reports(CommonStore &common, SocialState &state,
                                    const ExactResultTimeRead &source,
                                    const ExactResultReadStamp &stamp,
                                    const WorldMoment &at,
                                    const CatalogIdentity &catalog,
                                    const DirectorCatalog *routes) {
  if (!state.dynamic || !state.dynamic->time2 ||
      !social_time2_version(state.dynamic->version))
    return common_failure("ExactSocialStateRequired");
  if (!same_identity(common.state().identity, source.source_identity()))
    return common_failure("ExactSocialReadIdentityMismatch", {},
                          ErrorKind::internal);
  auto same = compare_world_moments(at, source.source_moment());
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("ExactSocialReadMomentMismatch");
  auto &dynamic = *state.dynamic;
  auto &time = *dynamic.time2;
  for (const auto &row : dynamic.reports.rows())
    if (!time.source_time_bindings.find(
            social_time_key(ReportObservationKey{row.first})))
      return common_failure("UnboundPreexistingReport", row.first);
  for (const auto &row : dynamic.pending_messages.rows())
    if (!time.source_time_bindings.find(
            social_time_key(MessageCreatedKey{row.first})) ||
        !time.message_execution_bindings.find(row.first))
      return common_failure("UnboundPreexistingMessage", row.first);
  for (const auto &receiver : dynamic.known_reports.rows())
    for (const auto &row : receiver.second.rows())
      if (!time.source_time_bindings.find(social_time_key(
              SocialKnowledgeKey{row.second.receiver, row.first})))
        return common_failure("UnboundPreexistingKnowledge", row.first);
  const auto upper = source.last_fact_id();
  auto last = time.collection_cursor.last_scanned_fact_id;
  std::vector<StableId> reports;
  bool complete = false;
  while (!complete) {
    auto page = source.fact_ids_after_source_cursor(last, upper, 64, stamp);
    if (!page.ok)
      return {false, {}, page.error};
    for (const auto &id : page.value.ids) {
      auto fact = source.fact_v2_borrowed(id, stamp);
      if (!fact.ok)
        return {false, {}, fact.error};
      if (!fact.value)
        return common_failure("MissingExactSocialSource", id);
      if (fact.value->kind == "local_supply_observation")
        reports.push_back(id);
      else if (fact.value->kind != "local_supply_source" &&
               !(fact.value->kind == "life_activity" &&
                 (fact.value->outcome == "rest_active" ||
                  fact.value->outcome == "rest_completed")))
        return common_failure("UnsupportedExactReportSource", id,
                              ErrorKind::unsupported_capability);
    }
    last = page.value.last_scanned_fact_id;
    complete = page.value.complete;
  }
  // 原collect只有整界排信入口；分数调用仅可消化明确无需Report的rest头。
  if (!reports.empty()) {
    auto zero = compare_fractions(at.sub_half_day, CanonicalFraction{});
    if (!zero.ok)
      return {false, {}, zero.error};
    if (zero.value)
      return common_failure("ExactSupplyCollectionRequiresOriginalBoundary", {},
                            ErrorKind::unsupported_capability);
    ReportProjection projection(source, stamp, reports);
    auto written = collect_dynamic_reports(common, state, projection,
                                           common.tick(), routes);
    if (!written.ok)
      return written;
  }
  for (const auto &row : dynamic.reports.rows())
    if (!time.source_time_bindings.find(
            social_time_key(ReportObservationKey{row.first}))) {
      auto fact = source.fact_v2_borrowed(row.second.fact_id, stamp);
      if (!fact.ok)
        return {false, {}, fact.error};
      if (!fact.value ||
          !std::holds_alternative<LegacyFactBoundary>(fact.value->time_basis))
        return common_failure("UnsupportedExactReportTimeSource", row.first,
                              ErrorKind::unsupported_capability);
      auto added = bind(
          time, ReportObservationKey{row.first},
          SocialLegacyBoundary{fact.value->occurred_at.half_day_tick, catalog},
          {"local_economy", fact.value->kind, fact.value->id});
      if (!added.ok)
        return added;
    }
  for (const auto &receiver : dynamic.known_reports.rows())
    for (const auto &row : receiver.second.rows()) {
      SocialKnowledgeKey key{row.second.receiver, row.first};
      if (!time.source_time_bindings.find(social_time_key(key))) {
        if (row.second.source_message_id)
          return common_failure("UnboundPreexistingMessageKnowledge",
                                row.first);
        SocialSourceTimeBinding learned{
            key,
            SocialExactMoment{at},
            {"social", "report_participant", row.first}};
        auto cut =
            capture_social_knowledge_cut(common, dynamic.version, learned);
        if (!cut.ok)
          return cut;
        if (!time.source_time_bindings.insert(social_time_key(key),
                                              std::move(learned)))
          return common_failure("DuplicateSocialTimeBinding", row.first);
      }
    }
  for (const auto &row : dynamic.pending_messages.rows())
    if (!time.source_time_bindings.find(
            social_time_key(MessageCreatedKey{row.first}))) {
      if (row.second.status != MessageStatus::pending ||
          row.second.created_tick != at.half_day_tick)
        return common_failure("UnboundPreexistingMessage", row.first);
      auto added =
          bind(time, MessageCreatedKey{row.first}, SocialExactMoment{at},
               {"social", "pending_message", row.first});
      if (!added.ok)
        return added;
      SocialMessageExecutionBinding binding;
      binding.message_id = row.first;
      binding.source_binding = SocialLegacyChannel{"local_society_v1"};
      binding.channel_rule_ref = {catalog, "official_supply_report"};
      if (!time.message_execution_bindings.insert(row.first,
                                                  std::move(binding)))
        return common_failure("DuplicateMessageExecutionBinding", row.first);
    }
  time.collection_cursor = {last, at};
  dynamic.last_collected_tick = at.half_day_tick;
  return common_success();
}
Outcome<WorldMoment>
read_social_bound_moment(const SocialSourceTimeBinding &binding,
                         const ExactResultTimeRead &source,
                         const ExactResultReadStamp &stamp) {
  auto check = source.validate_history_stamp(stamp);
  if (!check.ok)
    return {false, {}, check.error};
  WorldMoment result;
  if (const auto *legacy =
          std::get_if<SocialLegacyBoundary>(&binding.time_source)) {
    if (legacy->tick < 0)
      return bad<WorldMoment>("InvalidSocialLegacyBoundary");
    result = {legacy->tick, {}};
  } else if (const auto *exact =
                 std::get_if<SocialExactMoment>(&binding.time_source))
    result = exact->moment;
  else {
    const auto &ref = std::get<SocialExactSource>(binding.time_source)
                          .original_timed_source_ref;
    auto actual = source.source_moment_v2(ref, stamp);
    if (!actual.ok)
      return {false, {}, actual.error};
    result = actual.value;
  }
  auto valid = compare_world_moments(result, result);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto past = compare_world_moments(result, source.source_moment());
  if (!past.ok)
    return {false, {}, past.error};
  if (past.value > 0)
    return bad<WorldMoment>("SocialTimeSourceNotApplied");
  return {true, std::move(result), {}};
}
Status validate_social_time2(const CommonStore &common,
                             const SocialState &state,
                             const ExactResultTimeRead &source,
                             const ExactResultReadStamp &stamp,
                             const CatalogIdentity &catalog) {
  if (!state.dynamic || !state.dynamic->time2 ||
      !social_time2_version(state.dynamic->version))
    return common_failure("ExactSocialStateRequired");
  if (!same_identity(common.state().identity, source.source_identity()))
    return common_failure("ExactSocialReadIdentityMismatch", {},
                          ErrorKind::internal);
  const auto &d = *state.dynamic;
  const auto &t = *d.time2;
  auto propagation_shape = validate_social_propagation_shape(d);
  if (!propagation_shape.ok)
    return propagation_shape;
  auto cursor = source.fact_ids_after_source_cursor(
      t.collection_cursor.last_scanned_fact_id, source.last_fact_id(), 1,
      stamp);
  if (!cursor.ok)
    return {false, {}, cursor.error};
  auto through = compare_world_moments(t.collection_cursor.through_moment,
                                       source.source_moment());
  if (!through.ok)
    return {false, {}, through.error};
  if (through.value > 0 ||
      d.last_collected_tick != t.collection_cursor.through_moment.half_day_tick)
    return common_failure("InvalidExactSocialCollectionCursor");
  std::set<StableId> expected;
  auto required =
      [&](const SocialTimeKey &key,
          Integer tick) -> Outcome<const SocialSourceTimeBinding *> {
    auto id = social_time_key(key);
    expected.insert(id);
    const auto *b = t.source_time_bindings.find(id);
    if (!b || social_time_key(b->key) != id ||
        b->actual_source_ref.domain.empty() ||
        b->actual_source_ref.kind.empty() || b->actual_source_ref.id.empty())
      return bad<const SocialSourceTimeBinding *>("MissingSocialTimeBinding",
                                                  id);
    auto at = read_social_bound_moment(*b, source, stamp);
    if (!at.ok)
      return {false, {}, at.error};
    if (tick >= 0 && at.value.half_day_tick != tick)
      return bad<const SocialSourceTimeBinding *>(
          "SocialTimeProjectionMismatch", id);
    return {true, b, {}};
  };
  const auto &fact_order = common.state().facts.order();
  const auto scanned =
      t.collection_cursor.last_scanned_fact_id
          ? std::find(fact_order.begin(), fact_order.end(),
                      *t.collection_cursor.last_scanned_fact_id)
          : fact_order.end();
  for (const auto &row : d.reports.rows()) {
    const auto &r = row.second;
    const auto position =
        std::find(fact_order.begin(), fact_order.end(), r.fact_id);
    const bool beyond = !t.collection_cursor.last_scanned_fact_id ||
                        scanned == fact_order.end() ||
                        position == fact_order.end() || position > scanned;
    const auto *h = common.state().facts.find(r.fact_id);
    // 未安装的 Family 源不取得游标前收知豁免；原严格采集边界保持。
    if (beyond)
      return common_failure("ReportBeyondCollectionCursor", row.first);
    if (row.first != r.id || r.id != "report:" + r.fact_id || !h ||
        h->kind != r.payload_kind || r.origin_subject.id.empty() ||
        static_cast<Integer>(r.visibility_scope) < 0 ||
        static_cast<Integer>(r.visibility_scope) > 2 ||
        static_cast<Integer>(r.reliability_kind) < 0 ||
        static_cast<Integer>(r.reliability_kind) > 2 ||
        r.observation_tick < 0 ||
        r.observation_tick > source.source_moment().half_day_tick ||
        (r.valid_until_tick && *r.valid_until_tick < r.observation_tick))
      return common_failure("InvalidExactReport", row.first);
    auto reportable = source.reportable_fact(r.fact_id, stamp);
    if (!reportable.ok)
      return {false, {}, reportable.error};
    if (!reportable.value)
      return common_failure("UnreportableExactSource", row.first);
    auto fact = source.fact_v2_borrowed(r.fact_id, stamp);
    if (!fact.ok)
      return {false, {}, fact.error};
    if (!fact.value ||
        !same_fact(r.observation, legacy_report_projection(*fact.value)) ||
        r.payload_kind != fact.value->kind ||
        r.observation_tick != fact.value->occurred_at.half_day_tick ||
        (r.visibility_scope == ReportVisibility::local_public &&
         !fact.value->publicly_visible))
      return common_failure("ExactReportProjectionMismatch", row.first);
    const auto *fact_owner =
        std::get_if<OwnerFactMoment>(&fact.value->time_basis);
    if (fact_owner && fact_owner->source_schema == 2 &&
        fact_owner->source.domain == "family_collateral_legacy")
      return common_failure("NOT_IMPLEMENTED", r.fact_id,
                            ErrorKind::unsupported_capability);
    bool origin = false;
    for (const auto &p : fact.value->participants)
      if (same_social_subject(p.subject, r.origin_subject))
        origin = true;
    if (fact.value->supply &&
        r.origin_subject.kind == SubjectKind::organization &&
        r.origin_subject.id == fact.value->supply->organization_id)
      origin = true;
    if (fact.value->obligation &&
        r.origin_subject.kind == SubjectKind::organization &&
        r.origin_subject.id == fact.value->obligation->principal_id)
      origin = true;
    if (!origin)
      return common_failure("ExactReportOriginMismatch", row.first);
    auto binding =
        required(ReportObservationKey{row.first}, r.observation_tick);
    if (!binding.ok)
      return {false, {}, binding.error};
    if (!same_ref(binding.value->actual_source_ref,
                  {h->domain, h->kind, r.fact_id}))
      return common_failure("ExactReportActualSourceMismatch", row.first);
    if (const auto *legacy =
            std::get_if<LegacyFactBoundary>(&fact.value->time_basis)) {
      const auto *bound =
          std::get_if<SocialLegacyBoundary>(&binding.value->time_source);
      if (!bound || bound->tick != legacy->original_tick ||
          !same_catalog(bound->original_catalog_identity, catalog))
        return common_failure("InvalidLegacyReportBasis", row.first);
    } else {
      const auto &owner = std::get<OwnerFactMoment>(fact.value->time_basis);
      const auto *bound =
          std::get_if<SocialExactSource>(&binding.value->time_source);
      if (!bound ||
          bound->original_timed_source_ref.source_schema !=
              owner.source_schema ||
          !same_ref(bound->original_timed_source_ref.source_ref, owner.source))
        return common_failure("InvalidOwnerReportBasis", row.first);
    }
    auto at = read_social_bound_moment(*binding.value, source, stamp);
    if (!at.ok)
      return {false, {}, at.error};
    auto equal = compare_world_moments(at.value, fact.value->occurred_at);
    if (!equal.ok)
      return {false, {}, equal.error};
    if (equal.value)
      return common_failure("ReportObservationTimeMismatch", row.first);
  }
  for (const auto &row : d.pending_messages.rows()) {
    const auto &m = row.second;
    const auto *r = d.reports.find(m.report_id);
    const auto *sender = find_social_knowledge(d, {m.sender, m.report_id});
    if (row.first != m.id || !r || !sender || m.sender.id.empty() ||
        m.channel.empty() || m.created_tick < 0 ||
        m.created_tick > source.source_moment().half_day_tick ||
        m.due_tick < m.created_tick || static_cast<Integer>(m.status) < 0 ||
        static_cast<Integer>(m.status) > 2)
      return common_failure("InvalidExactMessage", row.first);
    auto created = required(MessageCreatedKey{row.first}, m.created_tick);
    if (!created.ok)
      return {false, {}, created.error};
    if (!std::holds_alternative<SocialExactMoment>(created.value->time_source))
      return common_failure("MessageCreatedMomentRequired", row.first);
    auto sent_at = read_social_bound_moment(*created.value, source, stamp);
    if (!sent_at.ok)
      return {false, {}, sent_at.error};
    const auto *sender_binding = t.source_time_bindings.find(
        social_time_key(SocialKnowledgeKey{m.sender, m.report_id}));
    if (!sender_binding)
      return common_failure("MessageSenderKnowledgeMissing", row.first);
    auto sender_at = read_social_bound_moment(*sender_binding, source, stamp);
    if (!sender_at.ok)
      return {false, {}, sender_at.error};
    auto order = compare_world_moments(sender_at.value, sent_at.value);
    if (!order.ok)
      return {false, {}, order.error};
    if (order.value > 0)
      return common_failure("MessageContentNotYetKnown", row.first);
    const auto *execution = t.message_execution_bindings.find(row.first);
    if (!execution || execution->message_id != row.first ||
        !same_catalog(execution->channel_rule_ref.catalog_identity, catalog) ||
        execution->channel_rule_ref.rule_ref.empty())
      return common_failure("InvalidMessageExecutionBinding", row.first);
    if (const auto *legacy =
            std::get_if<SocialLegacyChannel>(&execution->source_binding)) {
      if (m.address_binding || legacy->original_profile != "local_society_v1" ||
          m.receiver.id.empty() || m.receiver.kind == SubjectKind::cohort ||
          same_social_subject(m.receiver, m.sender) ||
          execution->actual_dossier_access_ref ||
          execution->channel_rule_ref.rule_ref != m.channel ||
          !same_ref(created.value->actual_source_ref,
                    {"social", "pending_message", m.id}))
        return common_failure("InvalidLegacyChannelBinding", row.first);
    } else {
      const auto &business =
          std::get<SocialS3BusinessLink>(execution->source_binding);
      if (!m.address_binding || !m.receiver.id.empty() ||
          !same_ref(business.original_business_fact_ref,
                    m.address_binding->original_business_fact_ref) ||
          business.original_business_fact_ref.domain != "interaction" ||
          business.original_business_fact_ref.kind != "communication_link")
        return common_failure("InvalidS3MessageLink", row.first);
    }
    if (m.status != MessageStatus::delivered) {
      if (execution->actual_delivery_ref ||
          execution->actual_dossier_access_ref ||
          !execution->actual_knowledge_keys.empty() ||
          t.source_time_bindings.find(
              social_time_key(MessageDeliveredKey{m.id})))
        return common_failure("UndeliveredMessageHasReceipt", row.first);
    } else {
      auto delivered = required(MessageDeliveredKey{m.id}, -1);
      if (!delivered.ok)
        return {false, {}, delivered.error};
      if (!execution->actual_delivery_ref ||
          !same_ref(*execution->actual_delivery_ref,
                    delivered.value->actual_source_ref))
        return common_failure("InvalidMessageDeliveryBinding", row.first);
      auto at = read_social_bound_moment(*delivered.value, source, stamp);
      if (!at.ok)
        return {false, {}, at.error};
      auto before = compare_world_moments(at.value, sent_at.value);
      if (!before.ok)
        return {false, {}, before.error};
      if (before.value < 0 || at.value.half_day_tick < m.due_tick)
        return common_failure("MessageDeliveryBeforeDue", row.first);
      if (!m.address_binding &&
          !find_social_knowledge(d, {m.receiver, m.report_id}))
        return common_failure("LegacyDeliveryKnowledgeMissing", row.first);
    }
    std::set<StableId> keys;
    for (const auto &key : execution->actual_knowledge_keys) {
      const auto *k = find_social_knowledge(d, key);
      if (!keys.insert(social_time_key(key)).second || !k ||
          key.report_id != m.report_id || k->source_message_id != m.id ||
          !same_social_subject(k->source_subject, m.sender))
        return common_failure("InvalidActualKnowledgeKey", row.first);
    }
  }
  if (t.message_execution_bindings.size() != d.pending_messages.size())
    return common_failure("ExtraMessageExecutionBinding");
  for (const auto &owner : d.known_reports.rows())
    for (const auto &row : owner.second.rows()) {
      const auto &k = row.second;
      const auto *r = d.reports.find(k.report_id);
      if (owner.first != social_subject_key(k.receiver) ||
          row.first != k.report_id || !r || k.receiver.id.empty() ||
          (k.receiver.kind == SubjectKind::actor &&
           !common.actor(k.receiver.id)) ||
          k.learned_tick < r->observation_tick ||
          k.learned_tick > source.source_moment().half_day_tick)
        return common_failure("InvalidExactKnowledge", row.first);
      auto binding =
          required(SocialKnowledgeKey{k.receiver, row.first}, k.learned_tick);
      if (!binding.ok)
        return {false, {}, binding.error};
      if (std::holds_alternative<SocialLegacyBoundary>(
              binding.value->time_source))
        return common_failure("KnowledgeRequiresActualMoment", row.first);
      auto learned = read_social_bound_moment(*binding.value, source, stamp);
      if (!learned.ok)
        return {false, {}, learned.error};
      const auto *observed = t.source_time_bindings.find(
          social_time_key(ReportObservationKey{row.first}));
      if (!observed)
        return common_failure("KnowledgeReportBindingMissing", row.first);
      auto observed_at = read_social_bound_moment(*observed, source, stamp);
      if (!observed_at.ok)
        return {false, {}, observed_at.error};
      auto order = compare_world_moments(learned.value, observed_at.value);
      if (!order.ok)
        return {false, {}, order.error};
      if (order.value < 0)
        return common_failure("KnowledgeBeforeSource", row.first);
      if (k.source_message_id) {
        const auto *m = d.pending_messages.find(*k.source_message_id);
        const auto *e = t.message_execution_bindings.find(*k.source_message_id);
        if (!m || !e || m->status != MessageStatus::delivered ||
            m->report_id != k.report_id ||
            !same_social_subject(k.source_subject, m->sender))
          return common_failure("KnowledgeMessageProvenanceMismatch",
                                row.first);
        bool covered = false;
        for (const auto &key : e->actual_knowledge_keys)
          if (same_social_subject(key.receiver, k.receiver) &&
              key.report_id == k.report_id)
            covered = true;
        if (!covered)
          return common_failure("UncoveredActualKnowledge", row.first);
        if (!m->address_binding &&
            (!same_social_subject(m->receiver, k.receiver) ||
             !e->actual_delivery_ref ||
             !same_ref(binding.value->actual_source_ref,
                       *e->actual_delivery_ref)))
          return common_failure("LegacyKnowledgeDeliveryMismatch", row.first);
        if (m->address_binding) {
          const auto &actual = binding.value->actual_source_ref;
          if (actual.domain != "interaction" ||
              (actual.kind != "communication_delivery" &&
               actual.kind != "communication_dossier_access") ||
              !std::holds_alternative<SocialExactSource>(
                  binding.value->time_source))
            return common_failure("InvalidS3KnowledgeSource", row.first);
        }
      } else if (is_public_supply_inspection_source(
                     binding.value->actual_source_ref)) {
        // 本人实际读公告只登记取得源，不把旧库存观察改成当前观察。
        auto inspected = validate_public_supply_inspection_knowledge_source(
            common, *r, k, *binding.value, source, stamp);
        if (!inspected.ok)
          return inspected;
      } else {
        const auto *exact =
            std::get_if<SocialExactSource>(&binding.value->time_source);
        if (exact && exact->original_timed_source_ref.source_schema == 2 &&
            exact->original_timed_source_ref.source_ref.domain ==
                "family_collateral_legacy")
          return common_failure("NOT_IMPLEMENTED", row.first,
                                ErrorKind::unsupported_capability);
        if (!same_social_subject(k.receiver, k.source_subject) ||
            !same_ref(binding.value->actual_source_ref,
                      {"social", "report_participant", row.first}) ||
            !std::holds_alternative<SocialExactMoment>(
                binding.value->time_source))
          return common_failure("InvalidDirectKnowledgeSource", row.first);
        auto allowed = source.direct_fact_observers_borrowed(r->fact_id, stamp);
        if (!allowed.ok)
          return {false, {}, allowed.error};
        if (std::none_of(allowed.value->begin(), allowed.value->end(),
                         [&](const auto &x) {
                           return same_social_subject(x, k.receiver);
                         }))
          return common_failure("UnwitnessedDirectKnowledge", row.first);
      }
    }
  for (const auto &row : t.source_time_bindings.rows()) {
    auto cut = validate_social_knowledge_cut(common, d, row.second);
    if (!cut.ok)
      return cut;
  }
  if (expected.size() != t.source_time_bindings.size())
    return common_failure("ExtraSocialTimeBinding");
  for (const auto &row : t.source_time_bindings.rows())
    if (!expected.count(row.first) ||
        row.first != social_time_key(row.second.key))
      return common_failure("InvalidSocialTimeBindingKey", row.first);
  return common_success();
}
Status validate_exact_report_source(const CommonStore &common, const Report &r,
                                    const ExactResultTimeRead &source,
                                    const ExactResultReadStamp &stamp) {
  if (!same_identity(common.state().identity, source.source_identity()))
    return common_failure("ExactSocialReadIdentityMismatch");
  const auto *h = common.state().facts.find(r.fact_id);
  auto actual = source.fact_v2_borrowed(r.fact_id, stamp);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (!actual.value || !h || h->kind != r.payload_kind ||
      r.id != "report:" + r.fact_id || r.payload_kind != actual.value->kind ||
      r.observation_tick != actual.value->occurred_at.half_day_tick ||
      !same_fact(r.observation, legacy_report_projection(*actual.value)))
    return common_failure("InvalidExactReportSource", r.id);
  auto reportable = source.reportable_fact(r.fact_id, stamp);
  if (!reportable.ok)
    return {false, {}, reportable.error};
  if (!reportable.value ||
      (r.visibility_scope == ReportVisibility::local_public &&
       !actual.value->publicly_visible))
    return common_failure("ExactReportDisclosureMismatch", r.id);
  return common_success();
}
Outcome<std::optional<KnowledgeReceiptViewV2>> read_knowledge_receipt_view_v2(
    const CommonStore &common, const SocialState &social,
    const ExactResultTimeRead &source, const SubjectRef &receiver,
    const StableId &report, const ExactResultReadStamp &stamp) {
  using View = std::optional<KnowledgeReceiptViewV2>;
  if (!same_identity(common.state().identity, source.source_identity()))
    return bad<View>("ExactSocialReadIdentityMismatch");
  auto checked = source.validate_history_stamp(stamp);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (!social.dynamic || !social.dynamic->time2)
    return bad<View>("ExactSocialStateRequired");
  const auto &state = *social.dynamic;
  const auto *known = find_social_knowledge(state, receiver, report);
  if (!known)
    return {true, std::nullopt, {}};
  if (known->report_id != report ||
      !same_social_subject(known->receiver, receiver))
    return bad<View>("KnowledgeReceiptKeyMismatch", report);
  const SocialSourceTimeBinding *binding = nullptr;
  for (const auto &row : state.time2->source_time_bindings.rows()) {
    const auto *key = std::get_if<SocialKnowledgeKey>(&row.second.key);
    if (!key || !same_social_subject(key->receiver, receiver) ||
        key->report_id != report)
      continue;
    if (binding || !knowledge_key_matches(row.first, receiver, report))
      return bad<View>("KnowledgeTimeBindingKeyMismatch", report);
    binding = &row.second;
  }
  if (!binding)
    return bad<View>("MissingKnowledgeTimeBinding", report);
  auto cut = validate_social_knowledge_cut(common, state, *binding);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto at = read_social_bound_moment(*binding, source, stamp);
  if (!at.ok)
    return {false, {}, at.error};
  if (at.value.half_day_tick != known->learned_tick)
    return bad<View>("KnowledgeTimeProjectionMismatch", report);
  return {true, KnowledgeReceiptViewV2{known, std::move(at.value)}, {}};
}
Outcome<std::optional<KnowledgeReceiptV2>>
read_knowledge_receipt_v2(const CommonStore &common, const SocialState &social,
                          const ExactResultTimeRead &source,
                          const SubjectRef &receiver, const StableId &report,
                          const ExactResultReadStamp &stamp) {
  auto view = read_knowledge_receipt_view_v2(common, social, source, receiver,
                                             report, stamp);
  if (!view.ok)
    return {false, {}, view.error};
  if (!view.value)
    return {true, std::nullopt, {}};
  const auto &known = *view.value->original;
  return {true,
          KnowledgeReceiptV2{known.receiver, known.report_id,
                             known.source_message_id, known.learned_tick,
                             known.source_subject,
                             std::move(view.value->learned_at)},
          {}};
}
Outcome<const KnowledgeReceiptV2 *>
ExactSocialReportRead::knowledge_receipt_v2_borrowed(
    const SubjectRef &receiver, const StableId &report,
    const ExactResultReadStamp &stamp) const {
  auto valid = source_->validate_history_stamp(stamp);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  for (const auto &receipt : receipts_)
    if (receipt.report_id == report &&
        same_social_subject(receipt.receiver, receiver))
      return {true, &receipt, {}};
  return {true, nullptr, {}};
}
Outcome<std::optional<KnowledgeReceiptV2>>
ExactSocialReportRead::knowledge_receipt_v2(
    const SubjectRef &receiver, const StableId &report,
    const ExactResultReadStamp &stamp) const {
  auto read = knowledge_receipt_v2_borrowed(receiver, report, stamp);
  if (!read.ok)
    return {false, {}, read.error};
  return {true,
          read.value ? std::optional<KnowledgeReceiptV2>{*read.value}
                     : std::nullopt,
          {}};
}
Outcome<std::shared_ptr<const ExactSocialReportRead>>
capture_exact_social_report_read(
    const CommonStore &common, const SocialState &state,
    std::shared_ptr<const ExactResultTimeRead> source,
    const CatalogIdentity &catalog) {
  if (!source)
    return bad<std::shared_ptr<const ExactSocialReportRead>>(
        "ExactResultReadRequired");
  auto valid =
      validate_social_time2(common, state, *source, source->stamp(), catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  try {
    CheckpointMemoryCharge memory(0);
    std::shared_ptr<ExactSocialReportRead> result(
        new ExactSocialReportRead(std::move(memory), std::move(source)),
        std::default_delete<ExactSocialReportRead>{},
        CheckpointAllocator<ExactSocialReportRead>{});
    std::size_t count = 0;
    for (const auto &owner : state.dynamic->known_reports.rows()) {
      if (owner.second.size() > SIZE_MAX - count)
        throw std::bad_alloc();
      count += owner.second.size();
    }
    result->receipts_.reserve(count);
    // Copy only the actual receipt projection, not the complete Social
    // report/message tables. The charge precedes every fresh string copy and
    // outlives all snapshot fields.
    for (const auto &owner : state.dynamic->known_reports.rows())
      for (const auto &row : owner.second.rows()) {
        const auto &known = row.second;
        const auto *binding =
            state.dynamic->time2->source_time_bindings.find(social_time_key(
                SocialKnowledgeKey{known.receiver, known.report_id}));
        if (!binding)
          return bad<std::shared_ptr<const ExactSocialReportRead>>(
              "MissingKnowledgeTimeBinding", row.first);
        auto at = read_social_bound_moment(*binding, *result->source_,
                                           result->source_->stamp());
        if (!at.ok)
          return {false, {}, at.error};


        (result->memory_).grow(0);
        result->receipts_.push_back(
            {known.receiver, known.report_id, known.source_message_id,
             known.learned_tick, known.source_subject, std::move(at.value)});
      }
    return {true, std::move(result), {}};
  } catch (const std::bad_alloc &) {
    return bad<std::shared_ptr<const ExactSocialReportRead>>(
        "ExactKnowledgeReadResourceExhausted");
  }
}
} // namespace hanmo::resident
