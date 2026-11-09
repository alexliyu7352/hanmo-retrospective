// 动态报告只传播已结算事实；显式观察和步初消息集合决定知情，不推动第二份世界时间。
#include "resident/domains/director/director_model.hpp"
#include "social_model.hpp"
#include "social_period_sources.hpp"
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 主体键带类型，组织和同名人物不能共享一份知识。
StableId subject_key(const SubjectRef &s) {
  return std::to_string(static_cast<int>(s.kind)) + ":" + s.id;
}
bool same_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
const KnownReport *known(const DynamicSocialState &s, const SubjectRef &subject,
                         const StableId &report) {
  auto *table = s.known_reports.find(subject_key(subject));
  return table ? table->find(report) : nullptr;
}
bool participant(const Report &r, const SubjectRef &subject) {
  return std::any_of(
      r.observation.participants.begin(), r.observation.participants.end(),
      [&](const auto &p) { return same_subject(p.subject, subject); });
}
// 私有报告不能因为发件人知道就向任意接收者泄露；公开报告仍须有消息或当地观察。
bool can_receive(const Report &r, const SubjectRef &subject) {
  return (r.visibility_scope == ReportVisibility::local_public &&
          subject.kind == SubjectKind::actor) ||
         participant(r, subject);
}
void learn(DynamicSocialState &s, const Report &r, const SubjectRef &receiver,
           Integer tick, const SubjectRef &source,
           const std::optional<StableId> &message = {}) {
  auto key = subject_key(receiver);
  if (!s.known_reports.find(key))
    s.known_reports.insert(key, {});
  auto *table = s.known_reports.find(key);
  if (!table->find(r.id))
    table->insert(r.id, {receiver, r.id, message, tick, source});
}
bool at_site(const CommonStore &c, const SubjectRef &subject,
             const StableId &site) {
  if (subject.kind != SubjectKind::actor)
    return false;
  auto location = c.resolved_location(subject.id);
  if (!location.ok)
    return false;
  auto *local = std::get_if<SettlementLocation>(&location.value);
  return local && (local->place_id == site || local->settlement_id == site);
}
// 恢复时逐字段比对不可变源结果，非负但被篡改的数量也不能被接受为已知真相。
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
    if (!same_subject(a.participants[i].subject, b.participants[i].subject) ||
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
// 只有已登记的闭合结果形态可以产生报告，未知事实不会被当作任意运行载荷。
bool recognized(const DynamicFactView &f, bool named = false) {
  if (f.rule_version == "named_membership_v1")
    return named &&
           (f.kind == "membership_offered" || f.kind == "membership_decided" ||
            f.kind == "membership_withdrawn" ||
            f.kind == "membership_expired" ||
            f.kind == "membership_wage_ended" ||
            f.kind == "organization_member_response");
  return f.kind == "life_activity" || f.supply.has_value() ||
         f.obligation.has_value() || f.kind == "organization_member_response" ||
         f.kind == "organization_activity_approved" ||
         f.kind == "organization_activity_completed" ||
         f.kind == "organization_activity_failed" ||
         f.kind == "organization_office_changed" ||
         f.kind == "member_response" || f.kind == "organization_member" ||
         f.kind == "organization_activity" || f.kind == "org_activity" ||
         f.kind == "delivery" || f.kind == "task_delivery";
}
// 生活报告只承认闭合的原动作类别，不能用任意kind或文本充当分享许可。
StableId life_category(const Report &report) {
  if (report.payload_kind != "life_activity" ||
      report.observation.kind != "life_activity" ||
      report.observation.rule_version != "regional_life_v1")
    return {};
  const auto &outcome = report.observation.outcome;
  if (outcome == "work_completed" || outcome == "work_cancelled")
    return "work";
  if (outcome == "trade_completed" || outcome == "trade_cancelled")
    return "trade";
  return {};
}
// 问讯只对真实看管者借入在场例外；缺源、死亡和普通不可用均不放行。
static Status validate_news_dialogue_availability(
    const CommonStore &common, const StableId &actor,
    const std::function<Outcome<bool>(const StableId &)>
        &custody_dialogue_available) {
  const auto *person = common.actor(actor);
  if (!person || !person->alive)
    return common_failure("PersonAbsent");
  if (person->available)
    return common_success();
  if (!custody_dialogue_available)
    return common_failure("PersonAbsent");
  auto available = custody_dialogue_available(actor);
  if (!available.ok)
    return {false, {}, available.error};
  return available.value ? common_success() : common_failure("PersonAbsent");
}
// 已发生的同地传播保留冻结地点/时刻，人物后来迁移不改写历史；未授权接收不能借普通消息伪装。
bool local_life_message(const DynamicSocialState &state, const Report &report,
                        const PendingMessage &message) {
  return state.version == 2 && message.channel == "face_to_face_life" &&
         message.local_place_id && !message.local_place_id->empty() &&
         message.status == MessageStatus::delivered &&
         message.sender.kind == SubjectKind::actor &&
         message.receiver.kind == SubjectKind::actor &&
         !same_subject(message.sender, message.receiver) &&
         participant(report, message.sender) &&
         !life_category(report).empty() &&
         message.created_tick == message.due_tick &&
         message.created_tick >= report.observation_tick &&
         (!report.valid_until_tick ||
          message.created_tick <= *report.valid_until_tick);
}
// 私密报告仅在真实当面证明存在时可由非参与者知道；普通报告仍沿原接收范围。
bool lawful_known_report(const DynamicSocialState &state, const Report &report,
                         const KnownReport &entry) {
  if (can_receive(report, entry.receiver))
    return true;
  if (!entry.source_message_id)
    return false;
  const auto *message = state.pending_messages.find(*entry.source_message_id);
  return message && message->report_id == report.id &&
         same_subject(message->receiver, entry.receiver) &&
         same_subject(message->sender, entry.source_subject) &&
         local_life_message(state, report, *message);
}
} // namespace
Status initialize_dynamic_social(SocialState &s, bool regional_life) {
  if (s.dynamic)
    return common_failure("DynamicSocialAlreadyInitialized");
  s.dynamic.emplace();
  s.dynamic->version = regional_life ? 2 : 1;
  if (!s.field_order.empty() &&
      std::find(s.field_order.begin(), s.field_order.end(), "dynamic") ==
          s.field_order.end())
    s.field_order.push_back("dynamic");
  return common_success();
}
Status collect_dynamic_reports(CommonStore &c, SocialState &s,
                               const DynamicResultRead &results, Integer tick,
                               const DirectorCatalog *routes,
                               const DirectorState *registry) {
  if (!s.dynamic)
    return common_failure("DynamicSocialUnavailable", {},
                          ErrorKind::unsupported_capability);
  if (routes && routes->dynamic_subjects &&
      (!registry || !registry->dynamic_subjects))
    return common_failure("DirectorReportRegistryRequired");
  if (tick != c.tick() || tick < 0 || s.dynamic->last_collected_tick > tick)
    return common_failure("DynamicReportTickMismatch");
  auto &d = *s.dynamic;
  const Integer after =
      d.last_collected_tick > 0 ? d.last_collected_tick - 1 : -1;
  for (const auto &id : results.fact_ids(after, tick)) {
    const auto report_id = "report:" + id;
    if (d.reports.find(report_id))
      continue;
    auto fact = results.fact(id);
    if (!fact)
      return common_failure("DynamicFactProjectionRequired", id,
                            ErrorKind::unsupported_capability);
    if (!recognized(*fact, routes && routes->named_membership) ||
        (fact->kind == "life_activity" && d.version != 2))
      continue;
    auto *header = c.state().facts.find(id);
    if (!header || fact->id != id || header->kind != fact->kind ||
        fact->tick < 0 || fact->tick > tick)
      return common_failure("InvalidDynamicReportSource", id);
    Report report;
    report.id = report_id;
    report.fact_id = id;
    report.payload_kind = fact->kind;
    report.observation_tick = fact->tick;
    report.observation = *fact;
    report.reliability_kind = fact->kind == "life_activity"
                                  ? ReportReliability::witnessed
                                  : ReportReliability::official;
    if (!fact->participants.empty())
      report.origin_subject = fact->participants.front().subject;
    else if (fact->supply)
      report.origin_subject = {SubjectKind::organization,
                               fact->supply->organization_id};
    else if (fact->obligation)
      report.origin_subject = {SubjectKind::organization,
                               fact->obligation->principal_id};
    else
      return common_failure("DynamicReportMissingOrigin", id);
    if (fact->publicly_visible)
      report.visibility_scope = ReportVisibility::local_public;
    d.reports.insert(report_id, report);
    for (const auto &person : fact->participants)
      if (person.subject.kind != SubjectKind::actor ||
          c.actor(person.subject.id))
        learn(d, report, person.subject, tick, person.subject);
    // 只按作者声明投递公开例行报告；新消息最早下个半日送达，未声明货源保持未知。
    if (routes && !routes->dynamic_subjects && fact->publicly_visible &&
        fact->supply)
      for (const auto &route : routes->subjects.rows()) {
        if (std::find(route.second.report_source_keys.begin(),
                      route.second.report_source_keys.end(),
                      fact->supply->source_key) ==
            route.second.report_source_keys.end())
          continue;
        const SubjectRef receiver{SubjectKind::actor, route.first};
        if (same_subject(receiver, report.origin_subject) ||
            known(d, receiver, report.id))
          continue;
        if (tick == std::numeric_limits<Integer>::max())
          return common_failure("DynamicReportTickOverflow");
        auto queued = queue_dynamic_report(
            c, s, report.id, report.origin_subject, receiver,
            "official_supply_report", tick, tick + 1);
        if (!queued.ok)
          return {false, {}, queued.error};
      }
  }
  if (routes && routes->dynamic_subjects) {
    // 新入职者只订阅每个来源最近一份已发布公开报告；队列幂等且最早下半日获知。
    std::map<StableId, const Report *> latest;
    for (const auto &row : d.reports.rows()) {
      const auto &report = row.second;
      if (!report.observation.supply ||
          report.visibility_scope != ReportVisibility::local_public ||
          report.observation_tick > tick ||
          (report.valid_until_tick && tick > *report.valid_until_tick))
        continue;
      const auto &source = report.observation.supply->source_key;
      auto found = latest.find(source);
      if (found == latest.end() ||
          found->second->observation_tick < report.observation_tick ||
          (found->second->observation_tick == report.observation_tick &&
           found->second->observation.supply->source_revision <=
               report.observation.supply->source_revision))
        latest[source] = &report;
    }
    for (const auto &row : registry->registrations.rows()) {
      if (row.second.lifecycle == DirectorLifecycle::retired ||
          row.second.obligations_only)
        continue;
      const auto *policy = director_policy(*registry, *routes, row.first);
      if (!policy)
        return common_failure("UnknownDirectorPolicy", row.first);
      const SubjectRef receiver{SubjectKind::actor, row.first};
      for (const auto &source : policy->report_source_keys) {
        auto found = latest.find(source);
        if (found == latest.end())
          continue;
        const auto &report = *found->second;
        if (same_subject(receiver, report.origin_subject) ||
            known(d, receiver, report.id))
          continue;
        if (tick == std::numeric_limits<Integer>::max())
          return common_failure("DynamicReportTickOverflow");
        auto queued = queue_dynamic_report(
            c, s, report.id, report.origin_subject, receiver,
            "official_supply_report", tick, tick + 1);
        if (!queued.ok)
          return {false, {}, queued.error};
      }
    }
  }
  d.last_collected_tick = tick;
  return common_success();
}
Status observe_dynamic_report(const CommonStore &c, SocialState &s,
                              const StableId &report_id,
                              const SubjectRef &receiver, Integer tick) {
  if (!s.dynamic)
    return common_failure("DynamicSocialUnavailable", {},
                          ErrorKind::unsupported_capability);
  if (tick != c.tick() || receiver.kind != SubjectKind::actor ||
      !c.actor(receiver.id))
    return common_failure("InvalidReportObserver");
  auto &d = *s.dynamic;
  auto *report = d.reports.find(report_id);
  if (!report)
    return common_failure("ReportUnavailable");
  if (known(d, receiver, report_id))
    return common_success();
  if (d.time2)
    return common_failure("ExactReportObservationSourceRequired", report_id,
                          ErrorKind::unsupported_capability);
  const bool local =
      report->visibility_scope == ReportVisibility::local_public &&
      report->observation.supply &&
      at_site(c, receiver, report->observation.supply->site_id);
  if (!participant(*report, receiver) && !local)
    return common_failure("ReportUnavailable");
  if (report->observation_tick > tick)
    return common_failure("InvalidReportObservationTime");
  learn(d, *report, receiver, tick, receiver);
  return common_success();
}
Outcome<StableId>
queue_dynamic_report(CommonStore &c, SocialState &s, const StableId &report_id,
                     const SubjectRef &sender, const SubjectRef &receiver,
                     const StableId &channel, Integer created, Integer due) {
  auto fail = [](const StableId &code) -> Outcome<StableId> {
    return {false, {}, common_failure(code).error};
  };
  if (!s.dynamic)
    return fail("DynamicSocialUnavailable");
  auto &d = *s.dynamic;
  auto *report = d.reports.find(report_id);
  auto *source = known(d, sender, report_id);
  if (!report || !source || source->learned_tick > created ||
      !can_receive(*report, receiver))
    return fail("ReportUnavailable");
  if (channel == "face_to_face_life")
    return fail("LocalReportCommandRequired");
  if (sender.id.empty() || receiver.id.empty() ||
      same_subject(sender, receiver) || channel.empty() ||
      created != c.tick() || created < 0 ||
      created == std::numeric_limits<Integer>::max() || due < created + 1)
    return fail("InvalidMessageSchedule");
  if ((sender.kind == SubjectKind::actor && !c.actor(sender.id)) ||
      (receiver.kind == SubjectKind::actor && !c.actor(receiver.id)))
    return fail("InvalidMessageParticipant");
  if (auto *received = known(d, receiver, report_id))
    return {true, received->source_message_id.value_or(report_id), {}};
  for (const auto &row : d.pending_messages.rows())
    if (row.second.report_id == report_id &&
        same_subject(row.second.receiver, receiver) &&
        row.second.status != MessageStatus::cancelled)
      return {true, row.first, {}};
  auto id = c.next_id("message");
  if (!id.ok)
    return id;
  d.pending_messages.insert(id.value,
                            {id.value, report_id, sender, receiver, channel,
                             created, due, MessageStatus::pending});
  return id;
}
static Status deliver_dynamic_reports_original(CommonStore &c, SocialState &s,
                                               const DynamicStepInput &step,
                                               Integer applied_period_end) {
  if (!s.dynamic)
    return common_failure("DynamicSocialUnavailable", {},
                          ErrorKind::unsupported_capability);
  if (step.from_tick < 0 ||
      step.from_tick == std::numeric_limits<Integer>::max() ||
      step.to_tick != step.from_tick + 1 || step.to_tick != applied_period_end)
    return common_failure("DynamicMessageTickMismatch");
  auto &d = *s.dynamic;
  std::set<StableId> unique;
  // 先检查全部捕获条目再写，坏的尾项不会留下前半份投递。
  for (const auto &id : step.due_message_ids) {
    if (!unique.insert(id).second)
      return common_failure("DuplicateCapturedMessage", id);
    auto *m = d.pending_messages.find(id);
    if (!m || !d.reports.find(m->report_id) ||
        m->created_tick > step.from_tick || m->due_tick > step.to_tick)
      return common_failure("InvalidCapturedMessage", id);
    auto *source = known(d, m->sender, m->report_id);
    if (!source || source->learned_tick > m->created_tick ||
        !can_receive(*d.reports.find(m->report_id), m->receiver))
      return common_failure("InvalidMessageProvenance", id);
  }
  for (const auto &id : step.due_message_ids) {
    auto *m = d.pending_messages.find(id);
    if (m->status != MessageStatus::pending)
      continue;
    auto *r = d.reports.find(m->report_id);
    auto *source = known(d, m->sender, m->report_id);
    if (!source || source->learned_tick > m->created_tick ||
        !can_receive(*r, m->receiver))
      return common_failure("InvalidMessageProvenance", id);
    learn(d, *r, m->receiver, step.to_tick, m->sender, m->id);
    m->status = MessageStatus::delivered;
  }
  return common_success();
}
Status deliver_dynamic_reports(CommonStore &c, SocialState &s,
                               const DynamicStepInput &step) {
  return deliver_dynamic_reports_original(c, s, step, c.tick());
}
Status deliver_dynamic_reports_at_period(CommonStore &c, SocialState &s,
                                         const DynamicStepInput &step,
                                         const WorldMoment &actual) {
  auto valid = compare_world_moments(actual, actual);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto zero = compare_fractions(actual.sub_half_day, CanonicalFraction{});
  if (!zero.ok)
    return {false, {}, zero.error};
  if (zero.value || actual.half_day_tick != step.to_tick)
    return common_failure("DynamicMessageActualPeriodMismatch");
  return deliver_dynamic_reports_original(c, s, step, actual.half_day_tick);
}
Status validate_dynamic_social(const CommonStore &c, const SocialState &s,
                               const DynamicResultRead *results,
                               bool named_membership) {
  if (!s.dynamic)
    return common_success();
  const auto &d = *s.dynamic;
  if (d.time2)
    return common_failure("UnexpectedExactSocialState");
  for (const auto &row : d.pending_messages.rows())
    if (row.second.address_binding)
      return common_failure("UnexpectedSocialAddressBinding", row.first);
  if ((d.version != 1 && d.version != 2) || d.last_collected_tick < -1 ||
      d.last_collected_tick > c.tick())
    return common_failure("InvalidDynamicSocialVersion");
  for (const auto &row : d.reports.rows()) {
    const auto &r = row.second;
    auto *h = c.state().facts.find(r.fact_id);
    if (r.id != row.first || r.id != "report:" + r.fact_id ||
        r.fact_id != r.observation.id || r.payload_kind != r.observation.kind ||
        r.observation_tick != r.observation.tick || !h ||
        h->kind != r.payload_kind || r.origin_subject.id.empty() ||
        static_cast<Integer>(r.visibility_scope) < 0 ||
        static_cast<Integer>(r.visibility_scope) > 2 ||
        static_cast<Integer>(r.reliability_kind) < 0 ||
        static_cast<Integer>(r.reliability_kind) > 2 ||
        r.observation_tick < 0 || r.observation_tick > c.tick() ||
        !recognized(r.observation, named_membership) ||
        (r.payload_kind == "life_activity" &&
         (d.version != 2 ||
          r.visibility_scope != ReportVisibility::participants ||
          life_category(r).empty())) ||
        (r.visibility_scope == ReportVisibility::local_public &&
         !r.observation.publicly_visible) ||
        (r.valid_until_tick && *r.valid_until_tick < r.observation_tick))
      return common_failure("InvalidDynamicReport", row.first);
    if (!participant(r, r.origin_subject) &&
        !(r.observation.supply &&
          r.origin_subject.kind == SubjectKind::organization &&
          r.origin_subject.id == r.observation.supply->organization_id) &&
        !(r.observation.obligation &&
          r.origin_subject.kind == SubjectKind::organization &&
          r.origin_subject.id == r.observation.obligation->principal_id))
      return common_failure("InvalidReportOrigin", row.first);
    if (results) {
      auto source = results->fact(r.fact_id);
      if (!source || !same_fact(*source, r.observation))
        return common_failure("InvalidDynamicReportSource", row.first);
    }
    if (r.observation.supply) {
      const auto &v = *r.observation.supply;
      if (v.physical_amount < 0 || v.target_amount < 0 ||
          v.source_revision < 0 || v.source_fact_id != r.fact_id)
        return common_failure("InvalidSupplyReport", row.first);
    }
    if (r.observation.obligation) {
      const auto &v = *r.observation.obligation;
      if (v.arrear_id.empty() || v.due_tick < 0 || v.due_amount < 0 ||
          v.paid_amount < 0 || v.outstanding_amount < 0 ||
          v.paid_amount > v.due_amount ||
          v.outstanding_amount != v.due_amount - v.paid_amount ||
          v.source_fact_id != r.fact_id)
        return common_failure("InvalidObligationReport", row.first);
    }
  }
  // 投递完成不等于首次获知；保留更早的合法观察，并在下面按其自身来源验证知情时间。
  for (const auto &row : d.pending_messages.rows()) {
    const auto &m = row.second;
    auto *r = d.reports.find(m.report_id);
    auto *source = known(d, m.sender, m.report_id);
    const bool local = r && local_life_message(d, *r, m);
    if (row.first != m.id || !r || !source ||
        static_cast<Integer>(m.status) < 0 ||
        static_cast<Integer>(m.status) > 2 || m.channel.empty() ||
        (m.receiver.kind == SubjectKind::actor && !c.actor(m.receiver.id)) ||
        m.created_tick < 0 || m.created_tick > c.tick() ||
        m.created_tick == std::numeric_limits<Integer>::max() ||
        (!local && m.due_tick <= m.created_tick) ||
        source->learned_tick > m.created_tick ||
        (!local && !can_receive(*r, m.receiver)) ||
        same_subject(m.sender, m.receiver) ||
        (!local && (m.local_place_id || m.channel == "face_to_face_life")))
      return common_failure("InvalidDynamicMessage", row.first);
    if (m.status == MessageStatus::delivered) {
      auto *k = known(d, m.receiver, m.report_id);
      if (!k || m.due_tick > c.tick() || k->learned_tick > c.tick())
        return common_failure("InvalidDeliveredMessage", row.first);
    }
  }
  for (const auto &row : d.known_reports.rows())
    for (const auto &entry : row.second.rows()) {
      const auto &k = entry.second;
      auto *r = d.reports.find(k.report_id);
      if (row.first != subject_key(k.receiver) ||
          (k.receiver.kind == SubjectKind::actor && !c.actor(k.receiver.id)) ||
          entry.first != k.report_id || !r ||
          k.learned_tick < r->observation_tick || k.learned_tick > c.tick() ||
          !lawful_known_report(d, *r, k))
        return common_failure("InvalidKnownReport", entry.first);
      if (k.source_message_id) {
        auto *m = d.pending_messages.find(*k.source_message_id);
        if (!m || m->status != MessageStatus::delivered ||
            m->report_id != k.report_id ||
            !same_subject(m->sender, k.source_subject) ||
            !same_subject(m->receiver, k.receiver) ||
            m->due_tick > k.learned_tick)
          return common_failure("InvalidKnownReportProvenance", entry.first);
      } else if (!same_subject(k.receiver, k.source_subject))
        return common_failure("InvalidKnownReportProvenance", entry.first);
    }
  return common_success();
}
// 当前人在当前地点说出其真实参与的结果；允许类别来自作者合同，失败不登记曝光或知识。
Status
preview_share_life_report(const CommonStore &common, const SocialState &social,
                          const StableId &report_id, const StableId &speaker,
                          const StableId &listener,
                          const std::vector<StableId> &allowed,
                          const std::function<Outcome<bool>(const StableId &)>
                              &custody_dialogue_available) {
  if (!social.dynamic || social.dynamic->version != 2)
    return common_failure("RegionalLifeUnavailable");
  const auto &state = *social.dynamic;
  const auto *report = state.reports.find(report_id);
  const SubjectRef sender{SubjectKind::actor, speaker},
      receiver{SubjectKind::actor, listener};
  const auto *source = known(state, sender, report_id);
  if (!report || !source || !participant(*report, sender) ||
      source->learned_tick > common.tick() ||
      report->observation_tick > common.tick())
    return common_failure("ReportUnavailable");
  const auto category = life_category(*report);
  std::set<StableId> categories;
  for (const auto &value : allowed)
    if ((value != "work" && value != "trade") ||
        !categories.insert(value).second)
      return common_failure("InvalidLifeSharingPolicy");
  if (category.empty() || !categories.count(category))
    return common_failure("ReportSharingNotAllowed");
  if (report->valid_until_tick && common.tick() > *report->valid_until_tick)
    return common_failure("ReportExpired");
  const auto *from = common.actor(speaker);
  const auto *to = common.actor(listener);
  if (!from || !to || speaker == listener)
    return common_failure("PersonAbsent");
  for (const auto &actor : {speaker, listener}) {
    auto available = validate_news_dialogue_availability(
        common, actor, custody_dialogue_available);
    if (!available.ok)
      return available;
  }
  if (!from->action_id.empty() || !to->action_id.empty())
    return common_failure("ActorBusy");
  auto a = common.resolved_location(speaker),
       b = common.resolved_location(listener);
  const auto *first =
      a.ok ? std::get_if<SettlementLocation>(&a.value) : nullptr;
  const auto *second =
      b.ok ? std::get_if<SettlementLocation>(&b.value) : nullptr;
  if (!first || !second || first->place_id != second->place_id ||
      first->settlement_id != second->settlement_id)
    return common_failure("NotAtPlace");
  return common_success();
}
// 直接传播仍有一份实际来源消息；不入延时队列，也不让本轮新知继续多跳。
Status share_life_report(CommonStore &common, SocialState &social,
                         const StableId &report_id, const StableId &speaker,
                         const StableId &listener,
                         const std::vector<StableId> &allowed,
                         const std::function<Outcome<bool>(const StableId &)>
                             &custody_dialogue_available) {
  auto checked =
      preview_share_life_report(common, social, report_id, speaker, listener,
                                allowed, custody_dialogue_available);
  if (!checked.ok)
    return checked;
  auto &state = *social.dynamic;
  const SubjectRef sender{SubjectKind::actor, speaker},
      receiver{SubjectKind::actor, listener};
  if (known(state, receiver, report_id))
    return common_success();
  auto location = common.resolved_location(speaker);
  const auto *place =
      location.ok ? std::get_if<SettlementLocation>(&location.value) : nullptr;
  if (!place)
    return common_failure("NotAtPlace");
  auto id = common.next_id("message");
  if (!id.ok)
    return {false, {}, id.error};
  PendingMessage message{id.value,
                         report_id,
                         sender,
                         receiver,
                         "face_to_face_life",
                         common.tick(),
                         common.tick(),
                         MessageStatus::delivered,
                         place->place_id};
  state.pending_messages.insert(id.value, message);
  learn(state, *state.reports.find(report_id), receiver, common.tick(), sender,
        id.value);
  return common_success();
}
// 问讯保原较早记忆的稳定次序；新生活报告仍限定真实参与者及作者准许的类别。
Outcome<SocialNewsQuote>
preview_social_news(const CommonStore &common, const SocialState &social,
                    const CommonCatalog &catalog, const StableId &listener,
                    const StableId &speaker,
                    const std::vector<StableId> &allowed,
                    const SocialFactRead *facts,
                    const std::function<Outcome<bool>(const StableId &)>
                        &custody_dialogue_available) {
  const auto fail = [](const StableId &code) -> Outcome<SocialNewsQuote> {
    return {false, {}, common_failure(code).error};
  };
  const auto *from = common.actor(speaker);
  const auto *to = common.actor(listener);
  if (!from || !to || speaker == listener)
    return fail("InvalidSocialParticipants");
  for (const auto &actor : {speaker, listener}) {
    auto available = validate_news_dialogue_availability(
        common, actor, custody_dialogue_available);
    if (!available.ok)
      return {false, {}, available.error};
  }
  if (!from->action_id.empty() || !to->action_id.empty())
    return fail("ActorBusy");
  auto a = common.resolved_location(speaker),
       b = common.resolved_location(listener);
  const auto *source =
      a.ok ? std::get_if<SettlementLocation>(&a.value) : nullptr;
  const auto *destination =
      b.ok ? std::get_if<SettlementLocation>(&b.value) : nullptr;
  if (!source || !destination || source->place_id != destination->place_id ||
      source->settlement_id != destination->settlement_id)
    return fail("NotAtPlace");
  std::set<StableId> categories;
  for (const auto &value : allowed)
    if ((value != "work" && value != "trade") ||
        !categories.insert(value).second)
      return fail("InvalidLifeSharingPolicy");
  auto checked = validate_social(common, social, catalog, facts);
  if (!checked.ok)
    return {false, {}, checked.error};
  checked =
      validate_dynamic_social(common, social, nullptr,
                              facts && facts->permits_named_membership_facts());
  if (!checked.ok)
    return {false, {}, checked.error};
  if (const auto *memories = social.memories.find(speaker)) {
    auto ids = memories->order();
    std::sort(ids.begin(), ids.end());
    const auto *received = social.memories.find(listener);
    for (const auto &id : ids)
      if (memories->find(id)->learned_tick < common.tick() &&
          (!received || !received->find(id)))
        return {true, {SocialNewsQuote::Kind::legacy_fact, id}, {}};
  }
  if (social.dynamic && social.dynamic->version == 2) {
    const auto &state = *social.dynamic;
    const auto *reports =
        state.known_reports.find(subject_key({SubjectKind::actor, speaker}));
    if (reports) {
      auto ids = reports->order();
      std::sort(ids.begin(), ids.end());
      for (const auto &id : ids) {
        if (reports->find(id)->learned_tick >= common.tick() ||
            known(state, {SubjectKind::actor, listener}, id))
          continue;
        auto available =
            preview_share_life_report(common, social, id, speaker, listener,
                                      allowed, custody_dialogue_available);
        if (available.ok)
          return {true, {SocialNewsQuote::Kind::life_report, id}, {}};
        if (available.error.kind != ErrorKind::rule)
          return {false, {}, available.error};
      }
    }
  }
  return {true, {}, {}};
}
// 应用前重核同一消息身份；原社会转述和生活当面传播均只执行一次，不创建行动或奖励。
Outcome<SocialEffects>
apply_social_news(CommonStore &common, SocialState &social,
                  const CommonCatalog &catalog, const StableId &listener,
                  const StableId &speaker, const SocialNewsQuote &quote,
                  const std::vector<StableId> &allowed,
                  const SocialFactRead *facts,
                  const std::function<Outcome<bool>(const StableId &)>
                      &custody_dialogue_available) {
  auto checked =
      preview_social_news(common, social, catalog, listener, speaker, allowed,
                          facts, custody_dialogue_available);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (checked.value.kind != quote.kind ||
      checked.value.source_id != quote.source_id)
    return {false, {}, common_failure("StaleNewsQuote").error};
  if (quote.kind == SocialNewsQuote::Kind::legacy_fact)
    return hear_social_news(common, social, catalog, listener, speaker,
                            common.tick(), facts);
  SocialEffects result;
  if (quote.kind == SocialNewsQuote::Kind::none)
    return {true, std::move(result), {}};
  auto shared =
      share_life_report(common, social, quote.source_id, speaker, listener,
                        allowed, custody_dialogue_available);
  if (!shared.ok)
    return {false, {}, shared.error};
  const auto *heard =
      known(*social.dynamic, {SubjectKind::actor, listener}, quote.source_id);
  if (!heard || !heard->source_message_id)
    return {false, {}, common_failure("MissingLifeNewsProvenance").error};
  result.applied = true;
  result.events.push_back(*heard->source_message_id);
  return {true, std::move(result), {}};
}
// 只清理中央已确认不再保留的生活来源；有待送消息则拒绝整次清理。
Status prune_life_reports(SocialState &social,
                          const std::vector<StableId> &removed_fact_ids) {
  if (!social.dynamic)
    return common_success();
  auto &state = *social.dynamic;
  std::set<StableId> removed(removed_fact_ids.begin(), removed_fact_ids.end()),
      reports;
  for (const auto &row : state.reports.rows())
    if (row.second.payload_kind == "life_activity" &&
        removed.count(row.second.fact_id))
      reports.insert(row.first);
  for (const auto &row : state.pending_messages.rows())
    if (reports.count(row.second.report_id) &&
        row.second.status == MessageStatus::pending)
      return common_failure("LifeReportStillPending", row.first);
  std::vector<StableId> messages;
  for (const auto &row : state.pending_messages.rows())
    if (reports.count(row.second.report_id))
      messages.push_back(row.first);
  for (const auto &id : messages)
    state.pending_messages.erase(id);
  for (auto &row : state.known_reports.rows())
    for (const auto &id : reports)
      row.second.erase(id);
  for (const auto &id : reports)
    state.reports.erase(id);
  return common_success();
}
} // namespace hanmo::resident
