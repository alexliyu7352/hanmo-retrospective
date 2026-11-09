#include "public_market_organization.hpp"
#include "organization_fact_source.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
Status validate_unselected_public_market_organization(
    const CommonStore &common, const OrganizationState &organization,
    const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &purchases,
    const PopulationTransportState &population,
    const HouseholdRetailCatalog &retail) {
  auto selected = validate_unselected_public_household_market(
      common, catalog, purchases, population, retail);
  if (!selected.ok)
    return selected;
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "organization" &&
        row.second.kind == "organization_public_market_staffing_offered")
      return common_failure("OrphanPublicMarketOrganizationFact", row.first);
  for (const auto &row : organization.activities.rows())
    for (const auto &participant : row.second.participation)
      if (participant.role == "market_service_staffing")
        return common_failure("OrphanPublicMarketActivity", row.first);
  for (const auto &row : organization.facts.rows()) {
    const auto &fact = row.second;
    if (fact.kind == "organization_public_market_staffing_offered" ||
        fact.code == "public_market_staffing_offered" ||
        fact.code == "public_market_staffing_approved" ||
        fact.code == "public_market_staffing_term_ended" ||
        fact.code == "public_market_staffing_mutual_release")
      return common_failure("OrphanPublicMarketOrganizationFact", row.first);
  }
  return common_success();
}
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{value.error};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guarded(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
SourceRef org_source(const OrganizationFact &f) {
  return {"organization", f.kind, f.id};
}
SourceRef economic(const char *kind, const StableId &id) {
  return {"local_economy", kind, id};
}
void header(const CommonStore &c, const SourceRef &s) {
  const auto *f = c.state().facts.find(s.id);
  if (!valid_source_ref(s) || !f || f->domain != s.domain || f->kind != s.kind)
    fail("PublicMarketOrganizationSourceMissing", s.id);
}
const MarketServiceDefinition &
definition(const HouseholdPurchaseState &purchases,
           const PublicMarketStaffingTerms &t) {
  const MarketServiceDefinition *out = nullptr;
  for (const auto &d : purchases.market_service_definitions)
    if (d.definition_ref.id == t.market_definition_ref.id &&
        d.definition_ref.version == t.market_definition_ref.version &&
        d.definition_ref.catalog_digest ==
            t.market_definition_ref.catalog_digest) {
      if (out)
        fail("DuplicatePublicMarketDefinition", d.definition_ref.id);
      out = &d;
    }
  if (!out || out->place_ref.empty())
    fail("PublicMarketDefinitionMissing", t.market_definition_ref.id);
  return *out;
}
const PublicMarketStaffingOffer &
offer(const PublicMarketOrganizationReadContext &x, const StableId &id) {
  const auto *o = x.purchases.public_staffing_offers.find(id);
  if (!o || o->offer_id != id)
    fail("PublicMarketOfferMissing", id);
  header(x.common, economic("public_market_staffing_offer", id));
  return *o;
}
const PublicMarketStaffingBinding &
binding(const PublicMarketOrganizationReadContext &x,
        const StableId &activity) {
  const PublicMarketStaffingBinding *out = nullptr;
  for (const auto &row : x.purchases.public_staffing_bindings.rows())
    if (row.second.original_org_activity_ref == activity) {
      if (out)
        fail("DuplicatePublicMarketActivityBinding", activity);
      out = &row.second;
    }
  if (!out)
    fail("PublicMarketActivityBindingMissing", activity);
  return *out;
}
const OrganizationFact &
original_fact(const PublicMarketOrganizationReadContext &x,
              const SourceRef &source) {
  header(x.common, source);
  const auto *f = x.organization.facts.find(source.id);
  if (!f || source.domain != "organization" || f->id != source.id ||
      f->kind != source.kind || f->office_change_payload ||
      f->grant_change_payload || f->amount != 0 ||
      !x.catalog.exact_fact_clock_enabled || !f->occurred_at ||
      f->tick != f->occurred_at->half_day_tick)
    fail("InvalidPublicMarketOrganizationFact", source.id);
  auto now = take(x.clock.read_organization_authority_now());
  if (compare(*f->occurred_at, now) > 0)
    fail("FuturePublicMarketOrganizationSource", source.id);
  return *f;
}
PublicMarketOrganizationReadContext
read_context(PublicMarketOrganizationWriteContext &x) {
  return {x.common,    x.organization, x.catalog,    x.runtime, x.identity,
          x.purchases, x.economy,      x.population, x.clock};
}
SingleGrantAuthorityRequest
authority_request(const HouseholdPurchaseState &p,
                  const PublicMarketStaffingTerms &t,
                  const ActingAuthority &acting, const char *command) {
  const auto &d = definition(p, t);
  if (acting.organization_id != t.organization_ref)
    fail("PublicMarketAuthorityOrganizationMismatch", acting.organization_id);
  return {acting, command, {ScopeRef::Kind::site, d.place_ref}, 0, {},
          false,  {}};
}
ActorFoundationAt exact_at(const WorldMoment &m) {
  ActorFoundationAt at;
  at.kind = ActorFoundationAt::Kind::exact_moment;
  at.exact_moment = m;
  return at;
}
// 当次原writer尚在构造payload时的私有短借用；actual
// actor直接来自已核ActingAuthority，不反抄Basis。
class PendingIssueRead final : public AuthorityIssueSourceRead {
public:
  PendingIssueRead(const CommonStore &c,
                   const OrganizationAuthorityClock &clock,
                   AuthorityIssueSourceView actual)
      : common_(c), clock_(clock), actual_(std::move(actual)) {}
  Outcome<WorldMoment> read_organization_authority_now() const override {
    return clock_.read_organization_authority_now();
  }
  Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &s) const override {
    return guarded<AuthorityIssueSourceView>([&] {
      if (!same_source_ref(s, actual_.source))
        fail("UnexpectedPublicMarketPendingIssue", s.id);
      header(common_, s);
      return actual_;
    });
  }

private:
  const CommonStore &common_;
  const OrganizationAuthorityClock &clock_;
  AuthorityIssueSourceView actual_;
};
// 真实申请字段与原activity逐项映射；此函数不创建/修改活动。
void activity_shape(const OrgActivity &a, const PublicMarketStaffingTerms &t,
                    const StableId &allocation, const StableId &offer_id,
                    const StableId &consent_id) {
  if (a.kind != OrgActivityKind::maintain_duty ||
      a.principal.kind != SubjectKind::organization ||
      a.principal.id != t.organization_ref ||
      a.responsible_office_id != t.responsible_office_ref ||
      a.terms_id != t.original_org_activity_terms_ref ||
      a.started_tick != t.from_tick || a.phase_started_tick != t.from_tick ||
      a.due_tick != t.until_tick || a.executors.size() != 1 ||
      a.executors[0].kind != SubjectKind::cohort ||
      a.executors[0].id != t.cohort_ref.cohort_id ||
      a.participation.size() != 1 ||
      a.participation[0].subject.kind != SubjectKind::cohort ||
      a.participation[0].subject.id != t.cohort_ref.cohort_id ||
      a.participation[0].role != "market_service_staffing" ||
      a.participation[0].binding_kind != ParticipationKind::cohort_allocation ||
      a.participation[0].binding_id != allocation ||
      a.commitment_ids != std::vector<StableId>{offer_id, consent_id} ||
      a.execution.activity_id != a.id ||
      a.execution.mode != ExecutionMode::background ||
      a.execution.execution_epoch != 0 || a.execution.local_session_id)
    fail("PublicMarketActivityShapeMismatch", a.id);
}
// 终局事实的语义先独立于末态验证；只有下方私有callback借用可在尚active时使用。
OrganizationFact
boundary_structure(const PublicMarketOrganizationReadContext &x,
                   const SourceRef &source) {
  const auto &f = original_fact(x, source);
  const auto &b = binding(x, f.activity_id);
  const auto &o = offer(x, b.offer_ref);
  const auto *a = x.organization.activities.find(f.activity_id);
  const auto *origin =
      x.population.public_market_labor_origins.find(b.labor_origin_ref.id);
  if (!a || !origin || origin->binding_ref != b.binding_id)
    fail("PublicMarketBoundaryActivityMissing", f.activity_id);
  activity_shape(*a, o.terms, b.original_labor_allocation_ref, b.offer_ref,
                 b.consent_ref);
  const bool due = f.kind == "organization_activity_completed" &&
                   f.code == "public_market_staffing_term_ended";
  const bool cancel = f.kind == "organization_activity_cancelled" &&
                      f.code == "public_market_staffing_mutual_release";
  if (!due && !cancel)
    fail("PublicMarketBoundaryKindMismatch", f.id);
  if (f.organization_id != o.terms.organization_ref ||
      f.cause_fact_ids.size() != std::size_t(due ? 2 : 3) ||
      f.cause_fact_ids[0] != origin->approved_org_fact_ref.id ||
      f.cause_fact_ids[1] != b.accepted_event_ref.id)
    fail("PublicMarketBoundaryCauseMismatch", f.id);
  if (compare(*f.occurred_at, {f.tick, {}}) != 0)
    fail("PublicMarketOriginalCalendarBoundaryRequired", f.id);
  if (due) {
    if (f.tick != o.terms.until_tick || !f.actor_id.empty() ||
        f.office_id != a->responsible_office_id ||
        x.runtime.authority_bases.find(f.id))
      fail("PublicMarketDueAuthorityMismatch", f.id);
  } else {
    const auto *consent =
        x.purchases.public_staffing_release_consents.find(f.cause_fact_ids[2]);
    if (!consent || consent->binding_ref != b.binding_id ||
        consent->terms_revision != o.terms.terms_revision ||
        consent->release_tick != f.tick || f.tick <= o.terms.from_tick ||
        f.tick >= o.terms.until_tick || f.actor_id.empty() ||
        f.office_id.empty())
      fail("PublicMarketReleaseConsentMismatch", f.id);
    header(x.common, economic("public_market_staffing_release_consent",
                              consent->consent_id));
  }
  return f;
}
} // namespace
Outcome<WorldMoment>
NativePublicMarketOrganizationRead::read_organization_authority_now() const {
  return context_.clock.read_organization_authority_now();
}
Outcome<WorldMoment>
NativePublicMarketOrganizationRead::read_organization_activity_source_moment(
    const OrganizationFact &f) const {
  return guarded<WorldMoment>([&] {
    const auto &actual = original_fact(context_, org_source(f));
    if (actual.code == "public_market_staffing_offered") {
      if (actual.kind != "organization_public_market_staffing_offered" ||
          actual.cause_fact_ids.size() != 1)
        fail("InvalidPublicMarketOfferAuthority", f.id);
      const auto &o = offer(context_, actual.cause_fact_ids[0]);
      if (!same_source_ref(o.employer_acceptance_ref, org_source(actual)) ||
          actual.tick != o.issued_at.half_day_tick ||
          compare(*actual.occurred_at, o.issued_at) != 0)
        fail("PublicMarketOfferAuthorityMismatch", f.id);
      return *actual.occurred_at;
    }
    if (actual.code == "public_market_staffing_approved") {
      if (actual.kind != "organization_activity_approved" ||
          actual.cause_fact_ids.size() != 2)
        fail("InvalidPublicMarketApproval", f.id);
      const auto &o = offer(context_, actual.cause_fact_ids[0]);
      if (actual.tick != o.terms.from_tick ||
          compare(*actual.occurred_at, {o.terms.from_tick, {}}) != 0)
        fail("PublicMarketApprovalTimeMismatch", f.id);
      return *actual.occurred_at;
    }
    if (actual.code == "public_market_staffing_term_ended" ||
        actual.code == "public_market_staffing_mutual_release") {
      boundary_structure(context_, org_source(actual));
      return *actual.occurred_at;
    }
    fail("UnsupportedOrganizationActivityTimeSource", f.id);
  });
}
Outcome<AuthorityIssueSourceView>
NativePublicMarketOrganizationRead::read_authority_issue_source(
    const SourceRef &source) const {
  return guarded<AuthorityIssueSourceView>([&] {
    const auto &f = original_fact(context_, source);
    const bool offered =
        f.kind == "organization_public_market_staffing_offered" &&
        f.code == "public_market_staffing_offered";
    const bool approved = f.kind == "organization_activity_approved" &&
                          f.code == "public_market_staffing_approved";
    const bool cancelled = f.kind == "organization_activity_cancelled" &&
                           f.code == "public_market_staffing_mutual_release";
    if ((!offered && !approved && !cancelled) || f.actor_id.empty() ||
        f.office_id.empty() || f.organization_id.empty())
      fail("PublicMarketAuthoritySourceKindMismatch", f.id);
    auto at = take(read_organization_activity_source_moment(f));
    return AuthorityIssueSourceView{source,
                                    f.actor_id,
                                    cancelled ? "cancel_public_market_staffing"
                                              : "approve_org_activity",
                                    exact_at(at),
                                    {}};
  });
}
Status NativePublicMarketOrganizationRead::validate_offer(
    const PublicMarketStaffingOffer &o) const {
  return guarded<Done>([&] {
    const auto &f = original_fact(context_, o.employer_acceptance_ref);
    if (f.kind != "organization_public_market_staffing_offered" ||
        f.code != "public_market_staffing_offered" ||
        f.cause_fact_ids != std::vector<StableId>{o.offer_id} ||
        !f.activity_id.empty() ||
        f.organization_id != o.terms.organization_ref ||
        f.office_id != o.terms.responsible_office_ref)
      fail("PublicMarketOfferAuthorityMismatch", o.offer_id);
    auto basis = take(read_registered_authority_basis(
        context_.common, context_.runtime.authority_bases,
        context_.organization, context_.catalog, context_.catalog_identity,
        {o.employer_acceptance_ref, 1}, *this));
    const auto &d = definition(context_.purchases, o.terms);
    if (basis->scope.kind != ScopeRef::Kind::site ||
        basis->scope.id != d.place_ref ||
        basis->command_kind != "approve_org_activity" ||
        basis->actor_id != f.actor_id || basis->office_id != f.office_id)
      fail("PublicMarketOfferScopeMismatch", o.offer_id);
    return Done{};
  });
}
Outcome<OrganizationFact> NativePublicMarketOrganizationRead::
    read_verified_public_market_staffing_approval(
        const SourceRef &source) const {
  return guarded<OrganizationFact>([&] {
    const auto &f = original_fact(context_, source);
    if (f.kind != "organization_activity_approved" ||
        f.code != "public_market_staffing_approved" ||
        f.cause_fact_ids.size() != 2)
      fail("PublicMarketApprovalSourceMismatch", source.id);
    const auto &o = offer(context_, f.cause_fact_ids[0]);
    const auto *consent =
        context_.purchases.public_staffing_consents.find(f.cause_fact_ids[1]);
    const auto *a = context_.organization.activities.find(f.activity_id);
    if (!consent || consent->offer_ref != o.offer_id ||
        consent->terms_revision != o.terms.terms_revision || !a ||
        f.organization_id != o.terms.organization_ref ||
        f.office_id != o.terms.responsible_office_ref ||
        f.tick != o.terms.from_tick)
      fail("PublicMarketApprovalTermsMismatch", source.id);
    header(context_.common,
           economic("public_market_staffing_consent", consent->consent_id));
    if (a->participation.size() != 1)
      fail("PublicMarketActivityParticipationMissing", a->id);
    activity_shape(*a, o.terms, a->participation[0].binding_id, o.offer_id,
                   consent->consent_id);
    auto basis = take(read_registered_authority_basis(
        context_.common, context_.runtime.authority_bases,
        context_.organization, context_.catalog, context_.catalog_identity,
        {source, 1}, *this));
    const auto &d = definition(context_.purchases, o.terms);
    if (basis->scope.kind != ScopeRef::Kind::site ||
        basis->scope.id != d.place_ref ||
        basis->command_kind != "approve_org_activity")
      fail("PublicMarketApprovalScopeMismatch", source.id);
    return f;
  });
}
Outcome<OrganizationFact> NativePublicMarketOrganizationRead::
    read_verified_public_market_staffing_boundary(
        const SourceRef &source) const {
  return guarded<OrganizationFact>([&] {
    auto f = boundary_structure(context_, source);
    const auto &b = binding(context_, f.activity_id);
    const auto *a = context_.organization.activities.find(f.activity_id);
    const bool due = f.kind == "organization_activity_completed";
    if (!a ||
        a->status != (due ? OrgActivityStatus::completed
                          : OrgActivityStatus::cancelled) ||
        a->phase != (due ? "completed" : "cancelled") ||
        a->execution.phase != a->phase || a->outcome_sequence != 1 ||
        a->result_fact_ids != std::vector<StableId>{f.id} ||
        a->result_code != f.code ||
        a->execution.last_receipt_id != std::optional<StableId>{f.id})
      fail("PublicMarketTerminalActivityMismatch", f.id);
    std::size_t releases = 0, events = 0;
    for (const auto &t : context_.population.public_market_labor_transitions)
      if (same_source_ref(t.origin_ref, b.labor_origin_ref))
        if (const auto *r = std::get_if<PublicMarketLaborReleased>(&t.change)) {
          if (!same_source_ref(r->original_org_boundary_fact_ref, source) ||
              compare(t.occurred_at, WorldMoment{f.tick, {}}) != 0 ||
              r->release_reason !=
                  (due ? PublicMarketLaborReleaseReason::completed
                       : PublicMarketLaborReleaseReason::mutually_cancelled))
            fail("PublicMarketTerminalReleaseMismatch", f.id);
          ++releases;
        }
    for (const auto &row : context_.purchases.public_staffing_events.rows())
      if (row.second.binding_ref == b.binding_id &&
          (row.second.kind == PublicMarketStaffingEventKind::completed ||
           row.second.kind ==
               PublicMarketStaffingEventKind::stop_at_boundary)) {
        if (!same_source_ref(row.second.cause_ref, source) ||
            row.second.kind !=
                (due ? PublicMarketStaffingEventKind::completed
                     : PublicMarketStaffingEventKind::stop_at_boundary) ||
            compare(row.second.occurred_at, WorldMoment{f.tick, {}}) != 0)
          fail("PublicMarketTerminalEventMismatch", f.id);
        ++events;
      }
    if (releases != 1 || events != 1 ||
        context_.economy.labor_allocations.find(
            b.original_labor_allocation_ref))
      fail("PublicMarketTerminalReleaseCoverage", f.id);
    if (!due) {
      auto basis = take(read_registered_authority_basis(
          context_.common, context_.runtime.authority_bases,
          context_.organization, context_.catalog, context_.catalog_identity,
          {source, 1}, *this));
      const auto &d =
          definition(context_.purchases, offer(context_, b.offer_ref).terms);
      if (basis->scope.kind != ScopeRef::Kind::site ||
          basis->scope.id != d.place_ref ||
          basis->command_kind != "cancel_public_market_staffing")
        fail("PublicMarketCancelScopeMismatch", f.id);
    }
    return f;
  });
}
Outcome<SourceRef> NativePublicMarketStaffingOrganizationWrite::
    record_public_market_staffing_offer_authority_in_candidate(
        const SourceRef &source, const PublicMarketStaffingTerms &terms,
        const ActingAuthority &acting) {
  return guarded<SourceRef>([&] {
    auto &x = context_;
    header(x.common, source);
    if (source.domain != "local_economy" ||
        source.kind != "public_market_staffing_offer" ||
        x.purchases.public_staffing_offers.find(source.id) ||
        acting.office_id != terms.responsible_office_ref)
      fail("InvalidPublicMarketOfferIssue", source.id);
    auto now = take(x.clock.read_organization_authority_now());
    auto request =
        authority_request(x.purchases, terms, acting, "approve_org_activity");
    auto captured = take(check_current_grant_authority(
        x.common, x.organization, x.catalog, x.identity, request, &x.clock));
    OrganizationFact f;
    f.kind = "organization_public_market_staffing_offered";
    f.organization_id = acting.organization_id;
    f.office_id = acting.office_id;
    f.actor_id = acting.actor_id;
    f.tick = now.half_day_tick;
    f.occurred_at = now;
    f.code = "public_market_staffing_offered";
    f.cause_fact_ids = {source.id};
    auto id = take(append_organization_fact_in_candidate(
        x.common, x.organization, std::move(f)));
    SourceRef issued{"organization",
                     "organization_public_market_staffing_offered", id};
    PendingIssueRead pending(
        x.common, x.clock,
        {issued, acting.actor_id, "approve_org_activity", exact_at(now), {}});
    take(register_authority_basis_for_fact(
        x.common, x.runtime.authority_bases, x.organization, x.catalog,
        x.identity, request, captured, issued, pending));
    return issued;
  });
}
// 市场接受只沿真实权源、劳力与原日历批准；缺来源或准入失败不落批准事实。
Outcome<OrganizationEffects> NativePublicMarketStaffingOrganizationWrite::
    approve_public_market_staffing_in_candidate(const SourceRef &offer_source,
                                                const SourceRef &consent_source,
                                                const StableId &activity_id,
                                                const StableId &allocation_id,
                                                const ActingAuthority &acting) {
  return guarded<OrganizationEffects>([&] {
    auto &x = context_;
    auto read = read_context(x);
    header(x.common, offer_source);
    header(x.common, consent_source);
    if (offer_source.domain != "local_economy" ||
        offer_source.kind != "public_market_staffing_offer" ||
        consent_source.domain != "local_economy" ||
        consent_source.kind != "public_market_staffing_consent")
      fail("InvalidPublicMarketAcceptanceSources");
    const auto &o = offer(read, offer_source.id);
    const auto &t = o.terms;
    const auto *consent =
        x.purchases.public_staffing_consents.find(consent_source.id);
    const auto *allocation = x.economy.labor_allocations.find(allocation_id);
    const auto *original =
        x.catalog.activity_terms.find(t.original_org_activity_terms_ref);
    auto now = take(x.clock.read_organization_authority_now());
    if (!consent || consent->offer_ref != o.offer_id ||
        consent->terms_revision != t.terms_revision ||
        consent->cohort_ref.cohort_id != t.cohort_ref.cohort_id ||
        !allocation || allocation->id != allocation_id ||
        allocation->cohort_id != t.cohort_ref.cohort_id ||
        allocation->activity_or_production_id != activity_id ||
        allocation->workers != t.workers ||
        allocation->from_tick != t.from_tick || t.until_tick <= t.from_tick ||
        allocation->through_tick != t.until_tick - 1 ||
        allocation->commitment_id != activity_id || !original ||
        original->kind != OrgActivityKind::maintain_duty ||
        original->duration_ticks != t.until_tick - t.from_tick ||
        acting.office_id != t.responsible_office_ref ||
        compare(now, WorldMoment{t.from_tick, {}}) != 0 ||
        compare(now, o.expires_at) >= 0)
      fail("PublicMarketAcceptanceSourceMismatch", activity_id);
    NativePublicMarketOrganizationRead history(read);
    take(history.validate_offer(o));
    auto request =
        authority_request(x.purchases, t, acting, "approve_org_activity");
    auto captured = take(check_current_grant_authority(
        x.common, x.organization, x.catalog, x.identity, request, &x.clock));
    OrgActivity a;
    a.id = activity_id;
    a.kind = OrgActivityKind::maintain_duty;
    a.principal = {SubjectKind::organization, t.organization_ref};
    a.responsible_office_id = t.responsible_office_ref;
    a.executors = {{SubjectKind::cohort, t.cohort_ref.cohort_id}};
    a.commitment_ids = {o.offer_id, consent->consent_id};
    a.started_tick = t.from_tick;
    a.phase_started_tick = t.from_tick;
    a.due_tick = t.until_tick;
    a.phase = "active";
    a.status = OrgActivityStatus::active;
    a.terms_id = t.original_org_activity_terms_ref;
    a.participation = {{{SubjectKind::cohort, t.cohort_ref.cohort_id},
                        "market_service_staffing",
                        ParticipationKind::cohort_allocation,
                        allocation_id}};
    a.execution.activity_id = activity_id;
    a.execution.phase = "active";
    a.execution.mode = ExecutionMode::background;
    take(approve_org_activity(x.common, x.organization, x.catalog, x.economy,
                              x.activity, captured.authority, a, &x.results,
                              &x.clock, x.calendar));
    OrganizationFact f;
    f.kind = "organization_activity_approved";
    f.organization_id = t.organization_ref;
    f.actor_id = acting.actor_id;
    f.office_id = acting.office_id;
    f.activity_id = activity_id;
    f.tick = t.from_tick;
    f.occurred_at = now;
    f.code = "public_market_staffing_approved";
    f.cause_fact_ids = {o.offer_id, consent->consent_id};
    auto id = take(append_organization_fact_in_candidate(
        x.common, x.organization, std::move(f)));
    SourceRef issued{"organization", "organization_activity_approved", id};
    PendingIssueRead pending(
        x.common, x.clock,
        {issued, acting.actor_id, "approve_org_activity", exact_at(now), {}});
    take(register_authority_basis_for_fact(
        x.common, x.runtime.authority_bases, x.organization, x.catalog,
        x.identity, request, captured, issued, pending));
    return OrganizationEffects{{id}, {id}};
  });
}
namespace {
// 私有借用仅活于本次事实→S5回调→终态栈，不可由外部命令或冷文件构造。
class InFlightBoundaryRead final : public PublicMarketStaffingOriginalRead {
public:
  InFlightBoundaryRead(const PublicMarketStaffingOriginalRead &original,
                       PublicMarketOrganizationReadContext x, SourceRef source)
      : original_(original), context_(x), source_(std::move(source)) {}
  Status validate_staffing_terms(const PublicMarketStaffingTerms &t,
                                 const WorldMoment &at,
                                 bool fresh) const override {
    return original_.validate_staffing_terms(t, at, fresh);
  }
  Status validate_staffing_office(const PublicMarketStaffingTerms &t,
                                  const ActingAuthority &a,
                                  const WorldMoment &at) const override {
    return original_.validate_staffing_office(t, a, at);
  }
  Status validate_staffing_offer_authority(
      const PublicMarketStaffingOffer &o) const override {
    return original_.validate_staffing_offer_authority(o);
  }
  Status validate_staffing_contact(const SourceRef &s,
                                   const PublicMarketStaffingTerms &t,
                                   const StableId &a,
                                   const WorldMoment &at) const override {
    return original_.validate_staffing_contact(s, t, a, at);
  }
  Status validate_staffing_compensation(const PublicMarketStaffingTerms &t,
                                        const MomentSpan &span) const override {
    return original_.validate_staffing_compensation(t, span);
  }
  Outcome<PublicMarketStaffQualification> read_staffing_qualification(
      const PublicMarketStaffingTerms &t, const CivilianPopulationRef &p,
      const SourceRef &location, const std::vector<SourceRef> &qualification,
      const MomentSpan &span) const override {
    return original_.read_staffing_qualification(t, p, location, qualification,
                                                 span);
  }
  Outcome<OrganizationFact> read_verified_public_market_staffing_approval(
      const SourceRef &s) const override {
    return original_.read_verified_public_market_staffing_approval(s);
  }
  Outcome<OrganizationFact> read_verified_public_market_staffing_boundary(
      const SourceRef &s) const override {
    if (!same_source_ref(s, source_))
      return original_.read_verified_public_market_staffing_boundary(s);
    return guarded<OrganizationFact>([&] {
      const auto f = boundary_structure(context_, s);
      const auto *activity =
          context_.organization.activities.find(f.activity_id);
      if (!activity || activity->status != OrgActivityStatus::active ||
          activity->outcome_sequence != 0 || !activity->result_fact_ids.empty())
        fail("PublicMarketInFlightBoundaryMismatch", s.id);
      return f;
    });
  }
  Status validate_staffing_labor_change(
      const PublicMarketLaborOrigin &o,
      const PublicMarketLaborTransition &t) const override {
    return original_.validate_staffing_labor_change(o, t);
  }
  Outcome<PublicMarketLaborChange>
  read_applied_staffing_labor_change(const PublicMarketLaborOrigin &o,
                                     const SourceRef &s,
                                     const WorldMoment &m) const override {
    return original_.read_applied_staffing_labor_change(o, s, m);
  }

private:
  const PublicMarketStaffingOriginalRead &original_;
  PublicMarketOrganizationReadContext context_;
  SourceRef source_;
};
// 唯一终局顺序：先原fact，S5真实处置全部成功，最后只改原Activity终态。
OrganizationEffects finish_boundary(
    PublicMarketOrganizationWriteContext &x,
    const PublicMarketStaffingContext &s5,
    PublicMarketStaffingEffectsWrite &effects, const StableId &activity_id,
    const std::optional<std::pair<StableId, ActingAuthority>> &cancellation) {
  if (&x.common != &s5.common || &x.economy != &s5.economy ||
      &x.population != &s5.population || &x.purchases != &s5.purchases)
    fail("PublicMarketBoundaryCandidateMismatch");
  auto context = read_context(x);
  const auto &b = binding(context, activity_id);
  const auto &o = offer(context, b.offer_ref);
  auto *a = x.organization.activities.find(activity_id);
  if (!a)
    fail("PublicMarketActivityMissing", activity_id);
  auto now = take(x.clock.read_organization_authority_now());
  if (compare(now, s5.now) != 0)
    fail("PublicMarketBoundaryClockMismatch", activity_id);
  if (a->status == OrgActivityStatus::completed ||
      a->status == OrgActivityStatus::cancelled) {
    if (a->result_fact_ids.size() != 1)
      fail("PublicMarketTerminalSourceMissing", activity_id);
    NativePublicMarketOrganizationRead reader(context);
    const auto *f = x.organization.facts.find(a->result_fact_ids.front());
    if (!f)
      fail("PublicMarketTerminalSourceMissing", activity_id);
    take(reader.read_verified_public_market_staffing_boundary(org_source(*f)));
    return {{f->id}, {f->id}};
  }
  if (a->status != OrgActivityStatus::active || a->outcome_sequence != 0 ||
      !a->result_fact_ids.empty())
    fail("PublicMarketActivityNotActive", activity_id);
  const auto *origin =
      x.population.public_market_labor_origins.find(b.labor_origin_ref.id);
  if (!origin)
    fail("PublicMarketLaborOriginMissing", activity_id);
  NativePublicMarketOrganizationRead history(context);
  take(history.read_verified_public_market_staffing_approval(
      origin->approved_org_fact_ref));
  OrganizationFact f;
  f.organization_id = o.terms.organization_ref;
  f.activity_id = activity_id;
  f.tick = now.half_day_tick;
  f.occurred_at = now;
  f.cause_fact_ids = {origin->approved_org_fact_ref.id,
                      b.accepted_event_ref.id};
  std::optional<SingleGrantAuthorityRequest> request;
  std::optional<SingleGrantAuthorityRead> captured;
  if (cancellation) {
    const auto *consent =
        x.purchases.public_staffing_release_consents.find(cancellation->first);
    if (!consent || consent->binding_ref != b.binding_id ||
        consent->terms_revision != o.terms.terms_revision ||
        consent->release_tick <= o.terms.from_tick ||
        consent->release_tick >= o.terms.until_tick ||
        compare(now, WorldMoment{consent->release_tick, {}}) != 0)
      fail("PublicMarketReleaseBoundaryRequired", activity_id);
    header(x.common, economic("public_market_staffing_release_consent",
                              consent->consent_id));
    take(s5.controls.validate_accepted(consent->controller_authority_ref,
                                       consent->consented_at.half_day_tick));
    take(s5.sources.validate_staffing_contact(
        consent->local_contact_source_ref, o.terms,
        consent->controller_authority_ref.actor_id, consent->consented_at));
    const auto *cohort = x.economy.cohorts.find(o.terms.cohort_ref.cohort_id);
    if (!cohort)
      fail("PublicMarketReleaseCohortMissing", activity_id);
    auto current = take(s5.controls.current(
        consent->controller_authority_ref.actor_id, cohort->household_group_id,
        household_market_staffing_release_operation, now.half_day_tick));
    if (!current ||
        current->source_revision !=
            consent->controller_authority_ref.source_revision ||
        current->fact_id != consent->controller_authority_ref.fact_id ||
        current->source_role_id !=
            consent->controller_authority_ref.source_role_id)
      fail("PublicMarketReleaseControlChanged", activity_id);
    request = authority_request(x.purchases, o.terms, cancellation->second,
                                "cancel_public_market_staffing");
    captured = take(check_current_grant_authority(
        x.common, x.organization, x.catalog, x.identity, *request, &x.clock));
    f.kind = "organization_activity_cancelled";
    f.code = "public_market_staffing_mutual_release";
    f.actor_id = cancellation->second.actor_id;
    f.office_id = cancellation->second.office_id;
    f.cause_fact_ids.push_back(consent->consent_id);
  } else {
    if (compare(now, WorldMoment{o.terms.until_tick, {}}) != 0)
      fail("PublicMarketDueBoundaryRequired", activity_id);
    f.kind = "organization_activity_completed";
    f.code = "public_market_staffing_term_ended";
    f.office_id = a->responsible_office_id;
  }
  const auto code = f.code, kind = f.kind;
  auto id = take(append_organization_fact_in_candidate(x.common, x.organization,
                                                       std::move(f)));
  SourceRef source{"organization", kind, id};
  if (cancellation) {
    PendingIssueRead pending(x.common, x.clock,
                             {source,
                              cancellation->second.actor_id,
                              "cancel_public_market_staffing",
                              exact_at(now),
                              {}});
    take(register_authority_basis_for_fact(
        x.common, x.runtime.authority_bases, x.organization, x.catalog,
        x.identity, *request, *captured, source, pending));
  }
  InFlightBoundaryRead in_flight(s5.sources, read_context(x), source);
  PublicMarketStaffingContext callback{x.common,    x.economy,   x.population,
                                       x.purchases, s5.controls, in_flight,
                                       now};
  take(on_public_market_staffing_boundary_in_candidate(
      x.common, x.economy, x.population, x.purchases, callback, effects,
      activity_id, source));
  a = x.organization.activities.find(activity_id);
  if (!a || a->status != OrgActivityStatus::active || a->outcome_sequence != 0)
    fail("PublicMarketActivityChangedDuringBoundary", activity_id);
  a->status = cancellation ? OrgActivityStatus::cancelled
                           : OrgActivityStatus::completed;
  a->phase = cancellation ? "cancelled" : "completed";
  a->outcome_sequence = 1;
  a->result_fact_ids.push_back(id);
  a->result_code = code;
  a->execution.phase = a->phase;
  a->execution.last_receipt_id = id;
  NativePublicMarketOrganizationRead final_read(read_context(x));
  take(final_read.read_verified_public_market_staffing_boundary(source));
  return {{id}, {id}};
}
} // namespace
Outcome<OrganizationEffects> end_public_market_staffing_in_candidate(
    PublicMarketOrganizationWriteContext &x,
    const PublicMarketStaffingContext &s,
    PublicMarketStaffingEffectsWrite &effects, const StableId &activity) {
  return guarded<OrganizationEffects>(
      [&] { return finish_boundary(x, s, effects, activity, std::nullopt); });
}
Outcome<OrganizationEffects> cancel_public_market_staffing_in_candidate(
    PublicMarketOrganizationWriteContext &x,
    const PublicMarketStaffingContext &s,
    PublicMarketStaffingEffectsWrite &effects, const StableId &id,
    const StableId &consent, const ActingAuthority &acting) {
  return guarded<OrganizationEffects>([&] {
    const auto *b = x.purchases.public_staffing_bindings.find(id);
    if (!b)
      fail("PublicMarketStaffingBindingMissing", id);
    return finish_boundary(x, s, effects, b->original_org_activity_ref,
                           std::make_optional(std::make_pair(consent, acting)));
  });
}
Status validate_public_market_organization(
    const PublicMarketOrganizationReadContext &x,
    const PublicMarketStaffingContext &s5) {
  return guarded<Done>([&] {
    NativePublicMarketOrganizationRead read(x);
    std::set<StableId> activities, approvals, terminals, offers;
    for (const auto &row : x.purchases.public_staffing_offers.rows()) {
      take(read.validate_offer(row.second));
      if (!offers.insert(row.second.employer_acceptance_ref.id).second)
        fail("DuplicatePublicMarketOfferAuthority", row.first);
    }
    for (const auto &row : x.purchases.public_staffing_bindings.rows()) {
      const auto &b = row.second;
      if (!activities.insert(b.original_org_activity_ref).second)
        fail("DuplicatePublicMarketActivityBinding", row.first);
      const auto *origin =
          x.population.public_market_labor_origins.find(b.labor_origin_ref.id);
      if (!origin)
        fail("PublicMarketLaborOriginMissing", row.first);
      take(read.read_verified_public_market_staffing_approval(
          origin->approved_org_fact_ref));
      if (!approvals.insert(origin->approved_org_fact_ref.id).second)
        fail("DuplicatePublicMarketApproval", row.first);
      const auto *activity =
          x.organization.activities.find(b.original_org_activity_ref);
      if (!activity)
        fail("PublicMarketActivityMissing", row.first);
      if (activity->status == OrgActivityStatus::completed ||
          activity->status == OrgActivityStatus::cancelled) {
        if (activity->result_fact_ids.size() != 1)
          fail("PublicMarketTerminalSourceMissing", row.first);
        const auto *f =
            x.organization.facts.find(activity->result_fact_ids.front());
        if (!f)
          fail("PublicMarketTerminalSourceMissing", row.first);
        take(
            read.read_verified_public_market_staffing_boundary(org_source(*f)));
        terminals.insert(f->id);
      } else if (activity->status != OrgActivityStatus::active ||
                 activity->outcome_sequence != 0 ||
                 activity->phase != "active" ||
                 activity->execution.phase != "active" ||
                 activity->execution.last_receipt_id ||
                 !activity->result_fact_ids.empty() ||
                 !activity->result_code.empty())
        fail("PublicMarketActiveActivityMismatch", row.first);
    }
    for (const auto &row : x.organization.activities.rows())
      if (row.second.participation.size() == 1 &&
          row.second.participation[0].role == "market_service_staffing" &&
          !activities.count(row.first))
        fail("OrphanPublicMarketActivity", row.first);
    for (const auto &row : x.organization.facts.rows()) {
      const auto &f = row.second;
      if ((f.code == "public_market_staffing_offered" &&
           !offers.count(row.first)) ||
          (f.code == "public_market_staffing_approved" &&
           !approvals.count(row.first)) ||
          ((f.code == "public_market_staffing_term_ended" ||
            f.code == "public_market_staffing_mutual_release") &&
           !terminals.count(row.first)))
        fail("OrphanPublicMarketOrganizationFact", row.first);
    }
    take(validate_public_market_staffing(s5));
    return Done{};
  });
}
} // namespace hanmo::resident
