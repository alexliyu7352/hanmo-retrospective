#include "face_to_face_report.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/social/social_time2_version.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <new>
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void reject(const char *c, const StableId &id = {},
                         ErrorKind kind = ErrorKind::rule) {
  throw Failure{common_failure(c, id, kind).error};
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
bool same(const ReportDisclosurePermission &a,
          const ReportDisclosurePermission &b) {
  return a.kind == b.kind &&
         a.original_source.source_schema == b.original_source.source_schema &&
         same(a.original_source.source_ref, b.original_source.source_ref);
}
// 为给定原始结果标识构造既有领域引用；不生成或登记新的结果。
SourceRef make_face_to_face_report_source_ref(const StableId &id) {
  return {"interaction", face_to_face_report_source_kind, id};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
const FaceToFaceContactSources &selected(const FaceToFaceReportContext &x) {
  if (&x.common != &x.disclosure.common)
    reject("FaceToFaceReportCandidateMismatch", {}, ErrorKind::internal);
  if (!x.interaction.face_to_face_contacts ||
      (x.interaction.face_to_face_contacts->version != 2 &&
       x.interaction.face_to_face_contacts->version != 3) ||
      !x.interaction.communication_links || !x.disclosure.social.dynamic ||
      !x.disclosure.social.dynamic->time2)
    reject("FaceToFaceReportSources2Required", {},
           ErrorKind::unsupported_capability);
  return *x.interaction.face_to_face_contacts;
}
const InteractionSession &session(const FaceToFaceReportContext &x,
                                  const FaceToFaceContactProof &p) {
  const InteractionSession *found = nullptr;
  for (const auto &s : x.interaction.interactions)
    if (s.id == p.session_id && s.actor_id == p.session_actor_id) {
      if (found)
        reject("AmbiguousReportDisclosureSession", p.session_id);
      found = &s;
    }
  if (!found || (found->phase != "say" && found->phase != "choice"))
    reject("ReportDisclosureDialogueStepRequired", p.session_id);
  return *found;
}
// 与原known:key的字节公式相同，直接比较借用片段，避免为只读查找构造深key。
bool knowledge_key_equals(const StableId &key, const StableId &sender,
                          const StableId &report) {
  char kind[24], count[32];
  const auto k = std::to_chars(kind, kind + sizeof(kind),
                               static_cast<Integer>(SubjectKind::actor));
  const auto n = std::to_chars(count, count + sizeof(count), sender.size());
  std::string_view rest(key);
  auto part = [&](std::string_view text) {
    if (rest.size() < text.size() || rest.substr(0, text.size()) != text)
      return false;
    rest.remove_prefix(text.size());
    return true;
  };
  return part("known:") && part({kind, std::size_t(k.ptr - kind)}) &&
         part(":") && part({count, std::size_t(n.ptr - count)}) && part(":") &&
         part(sender) && part(report) && rest.empty();
}
const std::pair<StableId, SocialSourceTimeBinding> &
known_binding(const FaceToFaceReportContext &x, const StableId &sender,
              const StableId &report) {
  const auto *d =
      x.disclosure.social.dynamic ? &*x.disclosure.social.dynamic : nullptr;
  if (d && d->time2)
    for (const auto &row : d->time2->source_time_bindings.rows())
      if (knowledge_key_equals(row.first, sender, report)) {
        if (row.second.actual_source_ref.id.empty())
          break;
        return row;
      }
  reject("ReportDisclosureKnowledgeSourceMissing", report);
}
const SourceRef &known_source(const FaceToFaceReportContext &x,
                              const StableId &sender, const StableId &report) {
  return known_binding(x, sender, report).second.actual_source_ref;
}

std::shared_ptr<const ReadStamp>
capture(const FaceToFaceReportContext &x, const StableId &sender,
        const TellKnownReport &q, const FaceToFaceContactProof &contact,
        const ReportDisclosurePermission *permission) {
  const auto *r = x.disclosure.social.dynamic->reports.find(q.report_id);
  if (!r)
    reject("ReportDisclosureNotKnown", q.report_id);
  const auto &dialogue = session(x, contact).dialogue_id;
  const auto &known = known_binding(x, sender, q.report_id).first;
  const SourceRef *extra =
      permission ? &permission->original_source.source_ref : nullptr;
  const auto *h = extra ? x.common.state().facts.find(extra->id) : nullptr;
  const bool extra_fact =
      extra && h && h->domain == extra->domain && h->kind == extra->kind;
  const bool add_source =
      extra_fact && extra->id != r->fact_id && extra->id != contact.session_id;
  const bool add_domain = extra && !extra_fact;
  // 固定容器reserve一次；每个深字段在fresh构造前计费，临时销毁后才退还。


  auto memory = CheckpointMemoryCharge(0);
  std::vector<ResourceKey> resources;
  resources.reserve(2);
  resources.push_back({ResourceKey::Kind::actor, StableId(sender)});
  resources.push_back(
      {ResourceKey::Kind::actor, StableId(q.recipient_actor_id)});
  std::vector<DomainReadKey> domains;
  domains.reserve(4 + std::size_t(add_domain));
  domains.push_back(
      {"interaction", "face_to_face_contact", StableId(contact.session_id)});
  domains.push_back({"interaction", "dialogue", StableId(dialogue)});
  domains.push_back({"social", "report", StableId(q.report_id)});
  domains.push_back({"social", "known_report", StableId(known)});
  std::vector<StableId> sources;
  sources.reserve(2 + std::size_t(add_source));
  sources.emplace_back(r->fact_id);
  sources.emplace_back(contact.session_id);
  if (add_source)
    sources.emplace_back(extra->id);
  if (add_domain)
    domains.push_back(
        {StableId(extra->domain), StableId(extra->kind), StableId(extra->id)});
  return take(x.disclosure.clock.capture(resources, domains, sources));
}

const MessageBusinessLink *link(const FaceToFaceReportContext &x,
                                const SourceRef &source) {
  const MessageBusinessLink *found = nullptr;
  for (const auto &r : x.interaction.communication_links->links.rows())
    if (same(r.second.original_business_fact_ref, source)) {
      if (found)
        reject("DuplicateReportDisclosureMessage", source.id);
      found = &r.second;
    }
  return found;
}
const FaceToFaceReportDisclosure &source(const FaceToFaceReportContext &x,
                                         const SourceRef &s, Integer schema) {
  const auto &state = selected(x);
  take(read_original_face_to_face_report_source(x.common, x.interaction,
                                                x.catalog, s, schema));
  if (schema != 1 || s.domain != "interaction" ||
      s.kind != face_to_face_report_source_kind)
    reject("UnsupportedFaceToFaceReportSource", s.id);
  const auto *d = state.report_disclosures.find(s.id);
  const auto *h = x.common.state().facts.find(s.id);
  if (!d || d->id != s.id || !h || h->domain != s.domain || h->kind != s.kind)
    reject("FaceToFaceReportSourceMissing", s.id);
  const auto *contact = take(read_face_to_face_contact_source(
      x.common, x.interaction, x.catalog,
      {"interaction", face_to_face_contact_source_kind, d->contact_source_id},
      1));
  if (d->sender_actor_id == d->recipient_actor_id ||
      !((contact->session_actor_id == d->sender_actor_id &&
         contact->provider_actor_id == d->recipient_actor_id) ||
        (contact->provider_actor_id == d->sender_actor_id &&
         contact->session_actor_id == d->recipient_actor_id)) ||
      cmp(d->disclosed_at, contact->occurred_at) < 0 ||
      cmp(d->disclosed_at, take(x.disclosure.clock.read_current_moment())) > 0)
    reject("FaceToFaceReportParticipantsMismatch", s.id);
  const DialogueNode *node = nullptr;
  for (const auto &dialogue : x.catalog.dialogues)
    if (dialogue.id == contact->dialogue_id)
      for (const auto &n : dialogue.nodes)
        if (n.id == d->dialogue_node_id) {
          if (node)
            reject("AmbiguousDisclosureDialogueNode", s.id);
          node = &n;
        }
  if (!node || node->kind != d->dialogue_phase ||
      (d->dialogue_phase != "say" && d->dialogue_phase != "choice"))
    reject("ReportDisclosureDialogueSourceMismatch", s.id);
  if (!same(known_source(x, d->sender_actor_id, d->report_id),
            d->sender_knowledge_source))
    reject("ReportDisclosureKnowledgeSourceChanged", s.id);
  take(validate_report_disclosure_permission_history(
      x.disclosure, d->report_id, {SubjectKind::actor, d->sender_actor_id},
      {SubjectKind::actor, d->recipient_actor_id}, d->permission,
      d->disclosed_at, s));
  return *d;
}
} // namespace
// 赋值前先真正销毁旧深字段，再退其预留；不以赋空字符串猜capacity消失。
FaceToFaceReportQuote &
FaceToFaceReportQuote::operator=(FaceToFaceReportQuote &&other) noexcept {
  if (this != &other) {
    this->~FaceToFaceReportQuote();
    new (this) FaceToFaceReportQuote(std::move(other));
  }
  return *this;
}
Outcome<TellKnownReport> decode_tell_known_report(const BoundaryValue &v) {
  try {
    using namespace boundary;
    fields(v, {"report_id", "recipient_actor_id"}, {}, "tell_known_report");
    TellKnownReport result{
        get_string(v, "report_id", "tell_known_report"),
        get_string(v, "recipient_actor_id", "tell_known_report")};
    if (result.report_id.empty() || result.recipient_actor_id.empty())
      return {false,
              {},
              common_failure("ReportDisclosureNamedRecipientRequired").error};
    return {true, std::move(result), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_tell_known_report_fields(ProjectionWriter &w,
                                     const TellKnownReport &v) {
  w.field("report_id", v.report_id);
  return w.field("recipient_actor_id", v.recipient_actor_id);
}
Outcome<const FaceToFaceReportDisclosure *>
read_original_face_to_face_report_source(const CommonStore &common,
                                         const InteractionState &interaction,
                                         const InteractionCatalog &catalog,
                                         const SourceRef &s, Integer schema) {
  return guard<const FaceToFaceReportDisclosure *>([&] {
    if (schema != 1 || s.domain != "interaction" ||
        s.kind != face_to_face_report_source_kind ||
        !interaction.face_to_face_contacts ||
        (interaction.face_to_face_contacts->version != 2 &&
         interaction.face_to_face_contacts->version != 3))
      reject("FaceToFaceReportSources2Required", s.id);
    const auto *d =
        interaction.face_to_face_contacts->report_disclosures.find(s.id);
    const auto *h = common.state().facts.find(s.id);
    if (!d || d->id != s.id || !h || h->domain != s.domain ||
        h->kind != s.kind || d->report_id.empty() ||
        d->sender_knowledge_source.domain.empty() ||
        d->sender_knowledge_source.kind.empty() ||
        d->sender_knowledge_source.id.empty() ||
        d->permission.original_source.source_schema < 1 ||
        d->permission.original_source.source_ref.id.empty())
      reject("FaceToFaceReportSourceMissing", s.id);
    const auto *c = take(read_face_to_face_contact_source(
        common, interaction, catalog,
        {"interaction", face_to_face_contact_source_kind, d->contact_source_id},
        1));
    if (d->sender_actor_id == d->recipient_actor_id ||
        !((c->session_actor_id == d->sender_actor_id &&
           c->provider_actor_id == d->recipient_actor_id) ||
          (c->provider_actor_id == d->sender_actor_id &&
           c->session_actor_id == d->recipient_actor_id)) ||
        cmp(d->disclosed_at, c->occurred_at) < 0)
      reject("FaceToFaceReportParticipantsMismatch", s.id);
    const auto *contact_header =
        common.state().facts.find(d->contact_source_id);
    if (!contact_header ||
        take(read_actual_common_fact_position(common, *contact_header)) >=
            take(read_actual_common_fact_position(common, *h)))
      reject("ReportDisclosureBeforeContact", s.id);
    bool node = false;
    for (const auto &dialogue : catalog.dialogues)
      if (dialogue.id == c->dialogue_id)
        for (const auto &n : dialogue.nodes)
          if (n.id == d->dialogue_node_id && n.kind == d->dialogue_phase &&
              (n.kind == "say" || n.kind == "choice")) {
            if (node)
              reject("AmbiguousDisclosureDialogueNode", s.id);
            node = true;
          }
    if (!node)
      reject("ReportDisclosureDialogueSourceMismatch", s.id);
    return d;
  });
}
Outcome<FaceToFaceReportQuote>
preview_face_to_face_report(const FaceToFaceReportContext &x,
                            const StableId &sender, const TellKnownReport &q) {
  return guard<FaceToFaceReportQuote>([&] {
    selected(x);
    if (sender.empty() || q.recipient_actor_id.empty() ||
        sender == q.recipient_actor_id)
      reject("ReportDisclosureNamedRecipientRequired", q.recipient_actor_id);
    FaceToFaceReportQuote out;
    const auto now = take(x.disclosure.clock.read_current_moment());
    auto contact = take(preview_face_to_face_contact(
        x.common, x.interaction, x.catalog, x.access, sender,
        q.recipient_actor_id, out.memory));
    CheckpointMemoryCharge current_source_memory;
    take(read_current_face_to_face_contact_source(
        x.common, x.interaction, x.catalog, x.access, sender,
        q.recipient_actor_id, now, current_source_memory));
    const auto &known = known_source(x, sender, q.report_id);


    (out.memory).grow(0);
    out.sender_actor_id = StableId(sender);
    out.request =
        TellKnownReport{StableId(q.report_id), StableId(q.recipient_actor_id)};
    out.contact = std::move(contact);
    out.sender_knowledge_source = SourceRef{
        StableId(known.domain), StableId(known.kind), StableId(known.id)};


    auto subject_memory = CheckpointMemoryCharge(0);
    const SubjectRef from{SubjectKind::actor, StableId(sender)},
        to{SubjectKind::actor, StableId(q.recipient_actor_id)};
    auto first = capture(x, sender, q, out.contact, nullptr);
    out.permission = take(prepare_report_disclosure_permission(
        x.disclosure, q.report_id, from, to, *first, out.memory));
    out.stamp = capture(x, sender, q, out.contact, &out.permission);
    CheckpointMemoryCharge current_memory;
    auto current = take(prepare_report_disclosure_permission(
        x.disclosure, q.report_id, from, to, *out.stamp, current_memory));
    if (!same(out.permission, current))
      reject("ReportDisclosurePermissionChanged", q.report_id);
    return out;
  });
}
Outcome<const FaceToFaceReportDisclosure *>
read_face_to_face_report_source(const FaceToFaceReportContext &x,
                                const SourceRef &s, Integer schema) {
  return guard<const FaceToFaceReportDisclosure *>(
      [&] { return &source(x, s, schema); });
}
// 校验候选与知情权限后沿原消息链披露；重复命令复用原投递，不新增结果。
Outcome<StableId> tell_known_report_in_candidate(
    CommonStore &common, InteractionState &interaction, SocialState &social,
    const FaceToFaceReportContext &x, const StableId &authenticated,
    const FaceToFaceReportQuote &q) {
  return guard<StableId>([&] {
    const auto &state = selected(x);
    if (&common != &x.common || &interaction != &x.interaction ||
        &social != &x.disclosure.social || authenticated != q.sender_actor_id ||
        !q.stamp)
      reject("ReportDisclosureCandidateOrCallerMismatch");
    take(x.disclosure.authority.validate(*q.stamp, common.state().identity,
                                         q.stamp->mode(), common,
                                         x.disclosure.versions));
    if (q.stamp->mode() != ReadMode::committed_current &&
        q.stamp->mode() != ReadMode::phase_current)
      reject("CurrentReportDisclosureStampRequired");
    // 同一原会话、内容与接收者幂等；重放不另分配公信，也不改变首次知情。
    for (const auto &row : state.report_disclosures.rows())
      if (row.second.contact_source_id == q.contact.session_id &&
          row.second.sender_actor_id == authenticated &&
          row.second.recipient_actor_id == q.request.recipient_actor_id &&
          row.second.report_id == q.request.report_id) {
        const auto &old =
            source(x, make_face_to_face_report_source_ref(row.first), 1);
        const auto *l = link(x, make_face_to_face_report_source_ref(old.id));
        if (!l)
          reject("ReportDisclosureMessageMissing", old.id);
        const auto *m = social.dynamic->pending_messages.find(
            l->original_public_message_ref);
        if (!m || m->status != MessageStatus::delivered)
          reject("ReportDisclosureDeliveryMissing", old.id);
        return m->id;
      }
    auto current =
        take(preview_face_to_face_report(x, authenticated, q.request));
    if (current.contact.session_id != q.contact.session_id ||
        !same(current.permission, q.permission) ||
        !same(current.sender_knowledge_source, q.sender_knowledge_source))
      reject("ReportDisclosureQuoteChanged", q.request.report_id);
    take(revalidate_face_to_face_contact(common, interaction, x.catalog,
                                         x.access, current.contact));
    const auto now = take(x.disclosure.clock.read_current_moment());
    const auto &step = session(x, current.contact);
    FaceToFaceReportDisclosure result;
    result.id = take(common.next_id("report_disclosure"));
    result.contact_source_id = current.contact.session_id;
    result.sender_actor_id = authenticated;
    result.recipient_actor_id = q.request.recipient_actor_id;
    result.report_id = q.request.report_id;
    result.dialogue_node_id = step.node_id;
    result.dialogue_phase = step.phase;
    result.sender_knowledge_source = current.sender_knowledge_source;
    result.permission = current.permission;
    result.disclosed_at = now;
    const SourceRef actual = make_face_to_face_report_source_ref(result.id);
    take(common.append_fact({actual.id, actual.kind, actual.domain}));
    if (!interaction.face_to_face_contacts->report_disclosures.insert(
            actual.id, std::move(result)))
      reject("DuplicateFaceToFaceReportSource", actual.id);
    // 已登记为真实交互结果；待投递、已投递与知情状态仍仅由社交领域维护。
    FaceToFaceReportCommunicationRead read(x);
    auto message = take(post_communication_in_candidate(
        common, social, *interaction.communication_links, actual, 1, read,
        x.disclosure.results, now));
    take(deliver_communication_in_candidate(common, social,
                                            *interaction.communication_links,
                                            message, actual, 1, read, now));
    auto *business = interaction.communication_links->links.find(
        social.dynamic->pending_messages.find(message)
            ->address_binding->original_business_fact_ref.id);
    if (!business)
      reject("ReportDisclosureBusinessLinkMissing", message);
    business->route_source_refs.push_back({"interaction",
                                           face_to_face_contact_source_kind,
                                           current.contact.session_id});
    take(read_face_to_face_report_source(x, actual, 1));
    return message;
  });
}
Outcome<CommunicationOrigin>
FaceToFaceReportCommunicationRead::origin(const SourceRef &s,
                                          Integer schema) const {
  return guard<CommunicationOrigin>([&] {
    const auto &x = context_;
    const auto &d = source(x, s, schema);
    const auto *contact = take(read_face_to_face_contact_source(
        x.common, x.interaction, x.catalog,
        {"interaction", face_to_face_contact_source_kind, d.contact_source_id},
        1));
    CommunicationOrigin out;
    out.source = s;
    out.source_schema = 1;
    out.report_id = d.report_id;
    out.official_content_ref = take(
        read_report_disclosure_official_content(x.disclosure, d.report_id));
    out.channel = "face_to_face";
    out.sender = {SubjectKind::actor, d.sender_actor_id};
    out.addressed_to = ActorAddress{d.recipient_actor_id};
    out.sent_at = d.disclosed_at;
    out.due_tick = d.disclosed_at.half_day_tick;
    out.channel_rule_ref = {x.catalog_identity, contact->dialogue_id};
    if (social_regional_time2_version(x.disclosure.social.dynamic->version) &&
        !contact->context.place_id.empty())
      out.local_place_id = contact->context.place_id;
    return out;
  });
}
Outcome<CommunicationArrival>
FaceToFaceReportCommunicationRead::arrival(const SourceRef &s,
                                           Integer schema) const {
  return guard<CommunicationArrival>([&] {
    const auto &d = source(context_, s, schema);
    const auto *l = link(context_, s);
    if (!l)
      reject("ReportDisclosureMessageMissing", s.id);
    const auto *contact = take(read_face_to_face_contact_source(
        context_.common, context_.interaction, context_.catalog,
        {"interaction", face_to_face_contact_source_kind, d.contact_source_id},
        1));
    return CommunicationArrival{s, 1, l->original_public_message_ref,
                                d.disclosed_at, contact->context.place_id};
  });
}
// 按已到达的原披露解析具名接收者；地址或到达证据不一致时拒绝。
Outcome<CommunicationResolvedRecipient>
FaceToFaceReportCommunicationRead::resolve_arrived_address(
    const CommunicationAddress &a, const CommunicationArrival &arrival) const {
  return guard<CommunicationResolvedRecipient>([&] {
    const auto &d = source(context_, arrival.source, arrival.source_schema);
    const auto *named = std::get_if<ActorAddress>(&a);
    if (!named || named->actor_id != d.recipient_actor_id ||
        cmp(arrival.arrived_at, d.disclosed_at))
      reject("FaceToFaceReportAddressMismatch", d.id);
    auto original = take(this->arrival(arrival.source, arrival.source_schema));
    if (original.public_message_id != arrival.public_message_id ||
        original.site_id != arrival.site_id)
      reject("FaceToFaceReportArrivalMismatch", d.id);
    return CommunicationResolvedRecipient{
        {SubjectKind::actor, d.recipient_actor_id},
        make_face_to_face_report_source_ref(d.id),
        0,
        true,
        {}};
  });
}
Status FaceToFaceReportCommunicationRead::validate_historical_recipient(
    const CommunicationAddress &a, const CommunicationArrival &r,
    const CommunicationResolvedRecipient &recipient) const {
  return guard<Done>([&] {
    auto actual = take(resolve_arrived_address(a, r));
    if (!same_social_subject(actual.receiver, recipient.receiver) ||
        !same(actual.authority_source, recipient.authority_source) ||
        recipient.office_epoch || !recipient.disclose_contents ||
        recipient.custody_receiver)
      reject("FaceToFaceReportRecipientChanged", r.source.id);
    return Done{};
  });
}
Status
FaceToFaceReportCommunicationRead::validate_delivery_before_common_source(
    const CommunicationDeliverySource &delivery, const SourceRef &later) const {
  auto actual = arrival(delivery.arrival_ref, delivery.arrival_schema);
  if (!actual.ok)
    return {false, {}, actual.error};
  return validate_atomic_communication_arrival_before_common_source(
      context_.common, *context_.interaction.communication_links, delivery,
      actual.value, later);
}
Status
FaceToFaceReportCommunicationRead::may_disclose(const CommunicationOrigin &o,
                                                const Report &r) const {
  return guard<Done>([&] {
    const auto &d = source(context_, o.source, o.source_schema);
    if (d.report_id != r.id || o.sender.kind != SubjectKind::actor ||
        o.sender.id != d.sender_actor_id)
      reject("FaceToFaceReportContentMismatch", d.id);
    return Done{};
  });
}
Outcome<CommunicationCancellation>
FaceToFaceReportCommunicationRead::cancellation(const SourceRef &s,
                                                Integer) const {
  return {
      false,
      {},
      common_failure("CompletedFaceToFaceDisclosureCannotCancel", s.id).error};
}
Outcome<CommunicationDossierAccess>
FaceToFaceReportCommunicationRead::dossier_access(const SourceRef &s,
                                                  Integer) const {
  return {false,
          {},
          common_failure("FaceToFaceDisclosureIsNotDossierAccess", s.id).error};
}
// 核对披露结果与投递来源闭包；缺失、重复或孤立结果保持原拒绝。
Status validate_face_to_face_report_sources(const FaceToFaceReportContext &x) {
  return guard<Done>([&] {
    const auto &state = selected(x);
    FaceToFaceReportCommunicationRead reader(x);
    for (const auto &row : state.report_disclosures.rows()) {
      const auto &d =
          source(x, make_face_to_face_report_source_ref(row.first), 1);
      const auto *l = link(x, make_face_to_face_report_source_ref(d.id));
      const auto *m = l ? x.disclosure.social.dynamic->pending_messages.find(
                              l->original_public_message_ref)
                        : nullptr;
      if (!l || !m || m->status != MessageStatus::delivered ||
          m->report_id != d.report_id || m->sender.kind != SubjectKind::actor ||
          m->sender.id != d.sender_actor_id ||
          l->original_business_source_schema != 1)
        reject("ReportDisclosureMessageCoverageMismatch", d.id);
      auto actual =
          take(reader.arrival(make_face_to_face_report_source_ref(d.id), 1));
      auto receiver =
          take(reader.resolve_arrived_address(l->addressed_to, actual));
      const auto *binding =
          x.disclosure.social.dynamic->time2->message_execution_bindings.find(
              m->id);
      const auto *delivery =
          binding && binding->actual_delivery_ref
              ? x.interaction.communication_links->deliveries.find(
                    binding->actual_delivery_ref->id)
              : nullptr;
      if (!delivery ||
          !same(delivery->arrival_ref,
                make_face_to_face_report_source_ref(d.id)) ||
          !same_social_subject(delivery->actual_receiver, receiver.receiver) ||
          !delivery->contents_disclosed ||
          cmp(delivery->delivered_at, d.disclosed_at))
        reject("ReportDisclosureDeliveryCoverageMismatch", d.id);
      for (const auto &prior : state.report_disclosures.rows()) {
        if (&prior == &row)
          break;
        const auto &p = prior.second;
        if (p.contact_source_id == d.contact_source_id &&
            p.sender_actor_id == d.sender_actor_id &&
            p.recipient_actor_id == d.recipient_actor_id &&
            p.report_id == d.report_id)
          reject("DuplicateFaceToFaceReportCommand", d.id);
      }
    }
    for (const auto &h : x.common.state().facts.rows())
      if (h.second.domain == "interaction" &&
          h.second.kind == face_to_face_report_source_kind &&
          !state.report_disclosures.find(h.first))
        reject("OrphanFaceToFaceReportSource", h.first);
    for (const auto &row : x.interaction.communication_links->links.rows())
      if (row.second.original_business_fact_ref.domain == "interaction" &&
          row.second.original_business_fact_ref.kind ==
              face_to_face_report_source_kind)
        take(read_face_to_face_report_source(
            x, row.second.original_business_fact_ref,
            row.second.original_business_source_schema));
    return Done{};
  });
}
} // namespace hanmo::resident
