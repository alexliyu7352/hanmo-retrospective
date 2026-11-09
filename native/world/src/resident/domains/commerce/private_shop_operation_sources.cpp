#include "private_shop_operation_sources.hpp"
#include "private_shop_capability.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool rule(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
WorldMoment context(const PrivateShopOperationSourceContext &x) {
  const auto at = take(x.clock.read_current_moment());
  if ((x.shops.catalog.version != 2 && x.shops.catalog.version != 3) ||
      x.shops.state.version != x.shops.catalog.version ||
      x.sources.schema_version != 1 || x.capabilities.schema_version != 1 ||
      x.shops.exact_operations != &x || &x.shops.common != &x.spatial.common ||
      &x.shops.common_catalog != &x.spatial.common_catalog ||
      &x.body.common != &x.shops.common ||
      &x.body.state != &x.spatial.foundation ||
      &x.body.catalog != &x.spatial.actors || x.body.elapsed != &x.elapsed ||
      x.body.scale != &x.scale || cmp(at, x.body.actual_now) ||
      cmp(at, take(x.spatial.spatial.current_moment())))
    fail("PrivateShopOperationSourceContextMismatch");
  return at;
}
const MarketServiceDefinition &
market(const PrivateShopOperationSourceContext &x,
       const ActorFoundationRuleRef &ref, const PrivateShopRecord &s) {
  const MarketServiceDefinition *found = nullptr;
  for (const auto &d : x.markets.market_service_definitions)
    if (rule(d.definition_ref, ref)) {
      if (found)
        fail("DuplicatePrivateShopMarketDefinition", ref.id);
      found = &d;
    }
  if (!found || ref.version <= 0 ||
      ref.catalog_digest != x.shops.common.state().metadata.content_digest ||
      found->slot_ref.provider_domain != "commerce" ||
      found->slot_ref.slot_kind != "market_service" ||
      found->slot_ref.facility_id.empty() ||
      found->place_ref != s.terms.place_id ||
      found->asset_source_ref.domain != "commerce" ||
      found->asset_source_ref.kind != "initial_shop_premises" ||
      found->asset_source_ref.id != s.terms.premises_id ||
      found->physical_service_slots <= 0)
    fail("PrivateShopMarketDefinitionRequired", ref.id);
  return *found;
}
MomentSpan window(const WorldMoment &at) {
  WorldMoment until = at;
  if (at.half_day_tick < 0 ||
      at.half_day_tick > std::numeric_limits<Integer>::max() - 2)
    fail("PrivateShopServiceWindowOverflow");
  until.half_day_tick += 2;
  return {at, until};
}
void committed(const MarketServiceDefinition &d, const MomentSpan &span) {
  if (cmp(span.from, span.to) >= 0)
    fail("PrivateShopPositiveServiceWindowRequired", d.definition_ref.id);
  bool covered = false;
  for (const auto &v : d.opening_intervals) {
    if (cmp(v.from, v.to) >= 0)
      fail("InvalidPrivateShopOpeningInterval", d.definition_ref.id);
    if (cmp(v.from, span.from) <= 0 && cmp(span.to, v.to) <= 0)
      covered = true;
  }
  if (!covered)
    fail("PrivateShopAcceptedOpeningWindowRequired", d.definition_ref.id,
         ErrorKind::rule);
}
ActorSpatialActualFactPrefix current_prefix(const CommonStore &c) {
  const auto p = take(read_current_common_fact_prefix(c));
  ActorSpatialActualFactPrefix result;
  result.common_fact_count = p.common_fact_count;
  if (p.last_common_fact)
    result.last_common_fact =
        SourceRef{p.last_common_fact->domain, p.last_common_fact->kind,
                  p.last_common_fact->id};
  return result;
}
ActorHistoricalSpatialPoint
accepted_position(const PrivateShopOperationSourceContext &x,
                  const PrivateShopOperation &o,
                  const PrivateShopOperationAcceptance &a) {
  const auto &v = x.spatial;
  auto point = take(read_actor_spatial_location_before_source(
      v.common, v.common_catalog, v.movement, v.carriers, v.registrations,
      v.foundation, v.actors, v.spatial, o.actor_id,
      a.actual_presence_source_ref, private_shop_operation_started_source(o.id),
      a.accepted_at, x.military_handoffs, v.budget, v.family_catalog, v.births,
      v.military_actions));
  return point;
}
void place(const PrivateShopOperationSourceContext &x,
           const MovementAnchor &actual, const StableId &id) {
  if (!take(same_spatial_anchor(
          actual, take(x.spatial.spatial.original_site_anchor(id)))))
    fail("PrivateShopActualPresenceRequired", id);
}
void staff(const PrivateShopOperationSourceContext &x,
           const PrivateShopRecord &s, const PrivateShopOperation &o,
           const PrivateShopOperationAcceptance &a) {
  if (o.staff_contract_id.empty()) {
    if (o.actor_id != s.terms.proprietor_actor_id)
      fail("PrivateShopOriginalOwnerOperatorRequired", o.id);
    return;
  }
  const auto *verified = take(read_original_private_shop_staff_contract(
      x.shops, {private_shop_module, "private_shop_staff_accepted",
                o.staff_contract_id}));
  if (!verified || verified->id != o.staff_contract_id ||
      verified->terms.shop_id != s.id ||
      verified->terms.worker_actor_id != o.actor_id ||
      verified->terms.employer_actor_id != s.terms.proprietor_actor_id ||
      verified->terms.service_item_id != o.item_id ||
      cmp({verified->accepted_tick, {}}, a.accepted_at) > 0 ||
      cmp(a.committed_service_window.to, {verified->through_tick, {}}) > 0)
    fail("PrivateShopOriginalStaffWindowRequired", o.id);
  const auto start = take(read_actual_common_fact_position(
      x.shops.common, private_shop_operation_started_source(o.id)));
  if (take(read_actual_common_fact_position(
          x.shops.common, {private_shop_module, "private_shop_staff_accepted",
                           verified->id})) >= start)
    fail("PrivateShopStaffAcceptedAfterOperation", o.id);
  if (!verified->terminal_fact_id.empty() &&
      take(read_actual_common_fact_position(
          x.shops.common, {private_shop_module, "private_shop_staff_ended",
                           verified->terminal_fact_id})) < start)
    fail("PrivateShopStaffEndedBeforeOperation", o.id);
}
const PrivateShopOperationAcceptance &
acceptance(const PrivateShopOperationSourceContext &x,
           const MomentOwnerSource &ref) {
  const auto at = context(x);
  if (ref.source_schema != 2 ||
      !same_source_ref(ref.source_ref, private_shop_operation_started_source(
                                           ref.source_ref.id)))
    fail("UnsupportedPrivateShopOperationStartSource", ref.source_ref.id,
         ErrorKind::unsupported_capability);
  const PrivateShopOperationAcceptance *found = nullptr;
  for (const auto &v : x.sources.accepted_operations)
    if (v.operation_ref == ref.source_ref.id) {
      if (found)
        fail("DuplicatePrivateShopOperationAcceptance", v.operation_ref);
      found = &v;
    }
  const auto *o = x.shops.state.operations.find(ref.source_ref.id);
  if (!found || !o || o->id != ref.source_ref.id || o->shop_id.empty() ||
      o->actor_id.empty() || !x.shops.common_catalog.items.find(o->item_id))
    fail("PrivateShopActualOperationRequired", ref.source_ref.id);
  const auto start =
      take(read_actual_common_fact_position(x.shops.common, ref.source_ref));
  const auto *s = take(read_original_private_shop_opening(
      x.shops, {private_shop_module, private_shop_open_kind, o->shop_id}));
  // E22.2 explicitly permits this original first-epoch projection only. There
  // is no invented transition table and no inferred successor epoch here.
  const SourceRef opening{private_shop_module, private_shop_open_kind, s->id};
  if (!same_source_ref(found->original_shop_epoch_ref, opening))
    fail("PrivateShopOriginalTransitionEpochRequired", o->id,
         ErrorKind::unsupported_capability);
  if (take(read_actual_common_fact_position(x.shops.common, opening)) >=
          start ||
      found->accepted_at.half_day_tick != o->start_tick ||
      cmp(found->accepted_at, at) > 0)
    fail("PrivateShopOriginalOperationOrderMismatch", o->id);
  if (x.shops.state.version == 3 &&
      (!s->opened_at || cmp(*s->opened_at, found->accepted_at) > 0))
    fail("PrivateShopOpeningAfterOperation", o->id);
  const auto expected = window(found->accepted_at);
  if (cmp(found->committed_service_window.from, expected.from) ||
      cmp(found->committed_service_window.to, expected.to))
    fail("PrivateShopOriginalH2WindowMismatch", o->id);
  const auto &definition = market(x, found->market_definition_ref, *s);
  committed(definition, found->committed_service_window);
  for (const auto &r : x.shops.state.closings.rows())
    if (r.second.shop_id == s->id &&
        take(read_actual_common_fact_position(
            x.shops.common,
            {private_shop_module, "private_shop_closed", r.first})) < start)
      fail("PrivateShopOperationAfterClosing", o->id);
  staff(x, *s, *o, *found);
  const auto point = accepted_position(x, *o, *found);
  if (point.actor_id != o->actor_id ||
      cmp(point.observed_at, found->accepted_at))
    fail("PrivateShopOriginalPresenceMismatch", o->id);
  place(x, point.actual_anchor, definition.place_ref);
  return *found;
}
} // namespace
SourceRef private_shop_operation_started_source(const StableId &id) {
  return {private_shop_module, "private_shop_operating_started", id};
}
Outcome<const PrivateShopOperationAcceptance *>
read_original_private_shop_operation_acceptance(
    const PrivateShopOperationSourceContext &x, const MomentOwnerSource &ref) {
  try {
    return {true, &acceptance(x, ref), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_private_shop_operation_sources(
    const PrivateShopOperationSourceContext &x) {
  try {
    context(x);
    for (std::size_t n = 0; n < x.sources.accepted_operations.size(); ++n) {
      const auto &row = x.sources.accepted_operations[n];
      for (std::size_t k = 0; k < n; ++k)
        if (x.sources.accepted_operations[k].operation_ref == row.operation_ref)
          fail("DuplicatePrivateShopOperationAcceptance", row.operation_ref);
      acceptance(x,
                 {private_shop_operation_started_source(row.operation_ref), 2});
    }
    for (const auto &row : x.sources.labor_applied_sources)
      take(read_original_private_shop_labor(
          x, {private_shop_labor_applied_source(row.id), 1}));
    for (const auto &row : x.sources.terminal_sources)
      take(read_original_private_shop_operation_terminal(
          x, {row.original_result_ref, 2}));
    for (const auto &accepted : x.sources.accepted_operations) {
      const auto *o = x.shops.state.operations.find(accepted.operation_ref);
      if (o && !o->result_fact_id.empty()) {
        const auto *r = x.shops.state.operating_results.find(o->result_fact_id);
        if (!r)
          fail("PrivateShopOriginalTerminalRequired", o->id);
        take(read_original_private_shop_operation_terminal(
            x, {{private_shop_module,
                 r->completed ? "private_shop_operating_completed"
                              : "private_shop_operating_interrupted",
                 r->id},
                2}));
      }
    }
    std::size_t labor = 0;
    for (const auto &fact : x.shops.common.state().facts.rows())
      if (fact.second.domain == private_shop_module &&
          fact.second.kind == "private_shop_labor_applied") {
        take(read_original_private_shop_labor(
            x, {{fact.second.domain, fact.second.kind, fact.second.id}, 1}));
        ++labor;
      }
    if (labor != x.sources.labor_applied_sources.size())
      fail("PrivateShopLaborSourceCoverageMismatch");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status visit_private_shop_operation_acceptance_pins(
    const PrivateShopOperationSourceContext &x, const MomentOwnerSource &ref,
    PrivateShopOperationSourcePinSink &sink) {
  try {
    const auto &a = acceptance(x, ref);
    const auto &o = *x.shops.state.operations.find(a.operation_ref);
    std::vector<MomentOwnerSource> seen;
    auto keep = [&](const MomentOwnerSource &source) {
      for (const auto &old : seen)
        if (old.source_schema == source.source_schema &&
            same_source_ref(old.source_ref, source.source_ref))
          return;
      seen.push_back(source);
      take(sink.source(source));
    };
    keep({a.original_shop_epoch_ref, x.shops.state.version == 3 ? 3 : 1});
    take(sink.locked_rule(a.market_definition_ref));
    if (!o.staff_contract_id.empty())
      keep({{private_shop_module, "private_shop_staff_accepted",
             o.staff_contract_id},
            1});
    const auto point = accepted_position(x, o, a);
    keep(
        {a.actual_presence_source_ref, take(actor_spatial_history_source_schema(
                                           a.actual_presence_source_ref))});
    for (const auto &original : point.original_sources)
      keep(original);
    const auto &profile =
        *take(read_original_private_shop_operation_profile(x, o.id));
    for (const auto &source : profile.acceptance_condition_source_refs)
      keep(source);
    if (profile.accepted_purpose_ref)
      take(sink.locked_rule(*profile.accepted_purpose_ref));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
namespace {

CheckpointCopyLease acceptance_memory(const StableId &id,
                                      const SourceRef &opening,
                                      const ActorFoundationRuleRef &rule,
                                      const SourceRef &presence) {


  return CheckpointCopyLease::reserve();
}
void swap_source(SourceRef &a, SourceRef &b) noexcept {
  a.domain.swap(b.domain);
  a.kind.swap(b.kind);
  a.id.swap(b.id);
}
void swap_moment(WorldMoment &a, WorldMoment &b) noexcept {
  std::swap(a.half_day_tick, b.half_day_tick);
  a.sub_half_day.swap(b.sub_half_day);
}
CheckpointCopyLease
acceptance_token_memory(const ActorSpatialActualFactPrefix &prefix,
                        const PrivateShopOperationQuote &q,
                        const StableId &shop, const ActorConditionHead &body) {


  return CheckpointCopyLease::reserve();
}
} // namespace
// 六个业务字段保持原 wire；每次复制先保留本 job 的真实字符串费用。
PrivateShopOperationAcceptance::PrivateShopOperationAcceptance(
    const StableId &id, const SourceRef &opening,
    const ActorFoundationRuleRef &rule, const WorldMoment &at,
    const MomentSpan &span, const SourceRef &presence)
    : copied_fields_(acceptance_memory(id, opening, rule, presence)),
      operation_ref(checkpoint_fresh_string_copy(id)),
      original_shop_epoch_ref(opening), market_definition_ref(rule),
      accepted_at(at), committed_service_window(span),
      actual_presence_source_ref(presence) {}
PrivateShopOperationAcceptance::PrivateShopOperationAcceptance(
    const PrivateShopOperationAcceptance &v)
    : PrivateShopOperationAcceptance(v.operation_ref, v.original_shop_epoch_ref,
                                     v.market_definition_ref, v.accepted_at,
                                     v.committed_service_window,
                                     v.actual_presence_source_ref) {}
// 先构造可抛的空分数，再以原 no-throw swap 交付所有字段和同一租约。
PrivateShopOperationAcceptance::PrivateShopOperationAcceptance(
    PrivateShopOperationAcceptance &&v)
    : PrivateShopOperationAcceptance() {
  swap(v);
}
void PrivateShopOperationAcceptance::swap(
    PrivateShopOperationAcceptance &v) noexcept {
  if (this == &v)
    return;
  std::swap(copied_fields_, v.copied_fields_);
  operation_ref.swap(v.operation_ref);
  swap_source(original_shop_epoch_ref, v.original_shop_epoch_ref);
  market_definition_ref.id.swap(v.market_definition_ref.id);
  std::swap(market_definition_ref.version, v.market_definition_ref.version);
  market_definition_ref.catalog_digest.swap(
      v.market_definition_ref.catalog_digest);
  swap_moment(accepted_at, v.accepted_at);
  swap_moment(committed_service_window.from, v.committed_service_window.from);
  swap_moment(committed_service_window.to, v.committed_service_window.to);
  swap_source(actual_presence_source_ref, v.actual_presence_source_ref);
}
PrivateShopOperationAcceptance &PrivateShopOperationAcceptance::operator=(
    const PrivateShopOperationAcceptance &v) {
  if (this != &v) {
    PrivateShopOperationAcceptance fresh(v);
    swap(fresh);
  }
  return *this;
}
PrivateShopOperationAcceptance &PrivateShopOperationAcceptance::operator=(
    PrivateShopOperationAcceptance &&v) noexcept {
  if (this != &v)
    swap(v);
  return *this;
}
struct PreparedPrivateShopOperationAcceptance::Impl
    : CheckpointAllocatedObject {
  CheckpointCopyLease copied_fields;
  const CommonStore *common = nullptr;
  const PrivateShopState *shops = nullptr;
  const PrivateShopOperationSourcesState *sources = nullptr;
  WorldIdentity identity;
  ActorSpatialActualFactPrefix prefix;
  PrivateShopOperationAcceptance accepted;
  StableId actor, shop, item, staff;
  Integer markup = 0;
  ActorConditionHead original_condition;
  Impl(const PrivateShopOperationSourceContext &x,
       const PrivateShopOperationQuote &q, const PrivateShopRecord &s,
       const ActorSpatialActualFactPrefix &before,
       const ActorConditionHead &body, PrivateShopOperationAcceptance &&record)
      : copied_fields(acceptance_token_memory(before, q, s.id, body)),
        common(&x.shops.common), shops(&x.shops.state), sources(&x.sources),
        identity(q.source_identity), prefix(before),
        accepted(std::move(record)),
        actor(checkpoint_fresh_string_copy(q.actor_id)),
        shop(checkpoint_fresh_string_copy(s.id)),
        item(checkpoint_fresh_string_copy(q.command.item_id)),
        staff(checkpoint_fresh_string_copy(q.staff_contract_id)),
        markup(s.markup_basis_points), original_condition(body) {}
};
struct PrivateShopOperationAcceptanceAccess {
  static PreparedPrivateShopOperationAcceptance
  make(std::unique_ptr<PreparedPrivateShopOperationAcceptance::Impl> p) {
    return PreparedPrivateShopOperationAcceptance(std::move(p));
  }
  static auto create(const PrivateShopOperationSourceContext &x,
                     const PrivateShopOperationQuote &q,
                     const PrivateShopRecord &s,
                     const ActorSpatialActualFactPrefix &prefix,
                     const ActorConditionHead &body,
                     PrivateShopOperationAcceptance &&record) {
    return std::make_unique<PreparedPrivateShopOperationAcceptance::Impl>(
        x, q, s, prefix, body, std::move(record));
  }
  static auto &value(PreparedPrivateShopOperationAcceptance &p) {
    return p.impl_;
  }
};
PreparedPrivateShopOperationAcceptance::
    PreparedPrivateShopOperationAcceptance() = default;
PreparedPrivateShopOperationAcceptance::PreparedPrivateShopOperationAcceptance(
    std::unique_ptr<Impl> p)
    : impl_(std::move(p)) {}
PreparedPrivateShopOperationAcceptance::
    ~PreparedPrivateShopOperationAcceptance() = default;
PreparedPrivateShopOperationAcceptance::PreparedPrivateShopOperationAcceptance(
    PreparedPrivateShopOperationAcceptance &&) noexcept = default;
PreparedPrivateShopOperationAcceptance &
PreparedPrivateShopOperationAcceptance::operator=(
    PreparedPrivateShopOperationAcceptance &&) noexcept = default;
Outcome<PreparedPrivateShopOperationAcceptance>
prepare_private_shop_operation_acceptance(
    const PrivateShopOperationSourceContext &x,
    const PrivateShopOperationQuote &q,
    const ActorFoundationRuleRef &definition) {
  try {
    const auto at = context(x);
    auto committed_boundary = validate_private_shop_committed_read3(x.shops);
    if (!committed_boundary.ok)
      throw Failure{std::move(committed_boundary.error)};
    if (!identity(q.source_identity, x.shops.common.state().identity) ||
        q.duration != 2 || x.shops.phase)
      fail("StalePrivateShopOperationAcceptance", q.command.shop_id,
           ErrorKind::rule);
    const auto *s = take(read_original_private_shop_opening(
        x.shops,
        {private_shop_module, private_shop_open_kind, q.command.shop_id}));
    const auto &d = market(x, definition, *s);
    const auto span = window(at);
    committed(d, span);
    const auto *actor = x.shops.common.actor(q.actor_id);
    if (!actor || !actor->alive || !actor->available ||
        !actor->action_id.empty() ||
        x.shops.activity.in_open_battle(q.actor_id) ||
        x.shops.activity.in_interaction(q.actor_id) || s->status == "closed")
      fail("PrivateShopOperatorUnavailable", q.actor_id, ErrorKind::rule);
    if (take(private_shop_operation_staff(x.shops, *s, q.actor_id)) !=
        q.staff_contract_id)
      fail("PrivateShopStaffAcceptanceChanged", q.actor_id, ErrorKind::rule);
    const auto body =
        take(read_current_character_condition(x.body, q.actor_id));
    const auto purpose = ActorFoundationRuleRef{
        "private_shop_operation", 1, x.body.catalog.identity.content_digest};
    if (!take(read_character_capability_at_head(x.body, body.head, purpose,
                                                std::nullopt, at))
             .allowed)
      fail("PrivateShopPurposeUnavailable", q.actor_id, ErrorKind::rule);
    const auto prefix = current_prefix(x.shops.common);
    const auto &v = x.spatial;
    const auto location =
        take(read_actor_spatial_location_at_actual_fact_prefix(
            v.common, v.common_catalog, v.movement, v.carriers, v.registrations,
            v.foundation, v.actors, v.spatial, q.actor_id, at, prefix,
            x.military_handoffs, v.budget, v.family_catalog, v.births,
            v.military_actions));
    if (location.actor_id != q.actor_id ||
        location.actual_location_sources.size() != 1)
      fail("PrivateShopCanonicalPresenceRequired", q.actor_id);
    place(x, *take(require_actor_ordinary_movement_anchor(location)),
          d.place_ref);
    const auto &head = location.actual_location_sources.front();
    if (!is_actor_spatial_head_source_kind(head.source) ||
        take(actor_spatial_history_source_schema(head.source)) != head.schema)
      fail("PrivateShopOriginalSpatialHeadRequired", q.actor_id);
    auto p = PrivateShopOperationAcceptanceAccess::create(
        x, q, *s, prefix, body.head,
        PrivateShopOperationAcceptance{
            {},
            {private_shop_module, private_shop_open_kind, s->id},
            definition,
            at,
            span,
            head.source});
    return {true, PrivateShopOperationAcceptanceAccess::make(std::move(p)), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status adopt_private_shop_operation_acceptance_in_candidate(
    CommonStore &c, PrivateShopOperationSourcesState &s,
    PrivateShopOperationCapabilitiesState &capabilities,
    const PrivateShopOperationSourceContext &x,
    PreparedPrivateShopOperationAcceptance &&prepared, const StableId &id) {
  try {
    auto &p = PrivateShopOperationAcceptanceAccess::value(prepared);
    if (!p || &c != &x.shops.common || &s != &x.sources ||
        &capabilities != &x.capabilities || p->common != &c ||
        p->shops != &x.shops.state || p->sources != &s ||
        !identity(p->identity, c.state().identity) ||
        cmp(context(x), p->accepted.accepted_at))
      fail("PrivateShopAcceptanceCandidateMismatch", id);
    const auto before = take(read_actual_common_fact_prefix(c, p->prefix));
    const auto position = take(read_actual_common_fact_position(
        c, private_shop_operation_started_source(id)));
    const auto *o = x.shops.state.operations.find(id);
    const auto *action = c.action(id);
    const auto *actor = o ? c.actor(o->actor_id) : nullptr;
    if (position != before || !o || !action || !actor || o->id != id ||
        o->actor_id != p->actor || o->shop_id != p->shop ||
        o->item_id != p->item || o->staff_contract_id != p->staff ||
        o->markup_basis_points != p->markup ||
        o->start_tick != p->accepted.accepted_at.half_day_tick ||
        !o->attendance_ticks.empty() || !o->result_fact_id.empty() ||
        action->id != id || action->kind != "private_shop_operate" ||
        action->actor_id != p->actor ||
        action->participants != std::vector<StableId>{p->actor} ||
        actor->action_id != id)
      fail("PrivateShopFreshOriginalStartRequired", id);
    const auto current =
        take(read_current_character_condition(x.body, p->actor));
    const auto accepted_condition = take(read_character_condition_head_source(
        x.body, p->original_condition, p->accepted.accepted_at));
    const auto current_condition = take(read_character_condition_head_source(
        x.body, current.head, p->accepted.accepted_at));
    const auto capability = take(read_character_capability_at_head(
        x.body, current.head,
        {"private_shop_operation", 1, x.body.catalog.identity.content_digest},
        std::nullopt, p->accepted.accepted_at));
    if (accepted_condition.source_schema != current_condition.source_schema ||
        !same_source_ref(accepted_condition.source, current_condition.source) ||
        !capability.allowed)
      fail("PrivateShopAcceptedConditionChanged", p->actor, ErrorKind::rule);
    for (const auto &old : s.accepted_operations)
      if (old.operation_ref == id)
        fail("PrivateShopAcceptanceAlreadyAdopted", id);
    for (const auto &old : capabilities.operation_profiles)
      if (old.operation_ref == id)
        fail("PrivateShopProfileAlreadyAdopted", id);
    auto conditions =
        take(private_shop_capability_condition_sources(x.body, capability));
    capabilities.operation_profiles.push_back(
        {id, PrivateShopOperationProfileKind::purpose_capability1, std::nullopt,
         capability.purpose_ref, std::move(conditions)});
    // 新 operation 键与其余字段一起先收费再构造；交付后不再依赖 pending token
    // 的租约。
    s.accepted_operations.emplace_back(id, p->accepted.original_shop_epoch_ref,
                                       p->accepted.market_definition_ref,
                                       p->accepted.accepted_at,
                                       p->accepted.committed_service_window,
                                       p->accepted.actual_presence_source_ref);
    p.reset();
    acceptance(x, {private_shop_operation_started_source(id), 2});
    take(read_original_private_shop_operation_profile(x, id));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
