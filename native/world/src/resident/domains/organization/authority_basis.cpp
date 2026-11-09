// 持久权源从真实业务fact边界反读；换任、Grant变更和资源新版本不会把历史签发改成今日授权。
#include "authority_basis.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "grant_authority.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
bool same_source(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same_catalog(const CatalogIdentity& a,const CatalogIdentity& b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
bool same_office_source(const OfficeAuthoritySource& a,const OfficeAuthoritySource& b){if(a.index()!=b.index())return false;if(const auto* initial=std::get_if<AuthoredOfficeAuthoritySource>(&a)){const auto& other=std::get<AuthoredOfficeAuthoritySource>(b);return initial->office_id==other.office_id&&initial->initial_epoch==other.initial_epoch&&same_catalog(initial->catalog_identity,other.catalog_identity);}return std::get<OfficeChangedAuthoritySource>(a).fact_id==std::get<OfficeChangedAuthoritySource>(b).fact_id;}
bool same_grant_source(const GrantAuthoritySource& a,const GrantAuthoritySource& b){if(a.index()!=b.index())return false;if(const auto* initial=std::get_if<AuthoredGrantAuthoritySource>(&a)){const auto& other=std::get<AuthoredGrantAuthoritySource>(b);return initial->grant_id==other.grant_id&&same_catalog(initial->catalog_identity,other.catalog_identity);}const auto& x=std::get<GrantRevisionAuthoritySource>(a);const auto& y=std::get<GrantRevisionAuthoritySource>(b);return x.fact_id==y.fact_id&&x.grant_id==y.grant_id&&x.revision==y.revision;}
bool same_at(const ActorFoundationAt& a,const ActorFoundationAt& b){if(a.kind!=b.kind)return false;if(a.kind==ActorFoundationAt::Kind::legacy_tick)return a.legacy_tick==b.legacy_tick;auto compared=compare_world_moments(a.exact_moment,b.exact_moment);return compared.ok&&compared.value==0;}
Integer tick_of(const ActorFoundationAt& at){return at.kind==ActorFoundationAt::Kind::legacy_tick?at.legacy_tick:at.exact_moment.half_day_tick;}
bool has(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
// 实例号只作为已存在实际fact的稳定同tick序位；不按字符串排序判先后。
Outcome<Integer> fact_sequence(const CommonStore& c,const SourceRef& source){const auto* fact=c.state().facts.find(source.id);if(!fact||fact->id!=source.id||fact->domain!=source.domain||fact->kind!=source.kind)return {false,{},common_failure("AuthorityIssueSourceMissing",source.id).error};const auto colon=source.id.rfind(':');if(colon==StableId::npos)return {false,{},common_failure("AuthorityIssueSequenceMissing",source.id).error};const auto number=source.id.substr(colon+1);Integer value=0;auto parsed=std::from_chars(number.data(),number.data()+number.size(),value);if(parsed.ec!=std::errc{}||parsed.ptr!=number.data()+number.size()||value<=0||value>=c.state().next_instance_id||number!=std::to_string(value))return {false,{},common_failure("AuthorityIssueSequenceInvalid",source.id).error};return {true,value,{}};}
struct HistoricalGrant {GrantRuntimeRecord value;GrantAuthoritySource source;};
// 唯一原Common顺序fold返回真实catalog/GrantChange借用；旧签发与新采用共用。
struct HistoricalGrantView {
  const Grant *terms = nullptr;
  const GrantRuntimeRecord *runtime = nullptr;
  const OrganizationFact *change = nullptr;
};
Outcome<HistoricalGrantView>
grant_view_before(const CommonStore &c, const OrganizationState &s,
                  const OrganizationCatalog &catalog, const StableId &id,
                  const StableId &boundary) {
  const auto &order = c.state().facts.order();
  const auto limit = std::find(order.begin(), order.end(), boundary);
  if (limit == order.end())
    return {false,
            {},
            common_failure("AuthorityIssueSourceMissing", boundary).error};
  HistoricalGrantView out{catalog.grants.find(id), nullptr, nullptr};
  if (catalog.authority_lifecycle_enabled)
    for (auto it = order.begin(); it != limit; ++it) {
      const auto *fact = s.facts.find(*it);
      if (!fact || !fact->grant_change_payload ||
          fact->grant_change_payload->grant_id != id)
        continue;
      out.runtime = &fact->grant_change_payload->after;
      out.terms = &out.runtime->terms;
      out.change = fact;
    }
  if (!out.terms)
    return {
        false, {}, common_failure("HistoricalGrantSourceMissing", id).error};
  return {true, out, {}};
}
// 旧拥有值ABI保持；只有已存在签发调用需要该副本，不为新业务另造Grant状态。
Outcome<HistoricalGrant> grant_before(const CommonStore &c,
                                      const OrganizationState &s,
                                      const OrganizationCatalog &catalog,
                                      const CatalogIdentity &identity,
                                      const StableId &id,
                                      const StableId &boundary) {
  auto source = grant_view_before(c, s, catalog, id, boundary);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &v = source.value;
  if (v.runtime)
    return {
        true,
        {*v.runtime,
         GrantRevisionAuthoritySource{
             v.change->id, id, v.change->grant_change_payload->after_revision}},
        {}};
  GrantRuntimeRecord value;
  value.terms = *v.terms;
  if (const auto *office = catalog.offices.find(v.terms->office_id)) {
    value.issuer_office_id = office->id;
    value.issuer_office_epoch = office->epoch;
  }
  return {
      true, {std::move(value), AuthoredGrantAuthoritySource{identity, id}}, {}};
}
// 原父链规则只有这一份；remember只负责本次防环，不持第二权限账。
template <class Remember>
Status parent_chain_before_view(const CommonStore &c,
                                const OrganizationState &s,
                                const OrganizationCatalog &catalog,
                                const CatalogIdentity &identity,
                                const HistoricalGrantView &g,
                                const StableId &boundary, const WorldMoment &at,
                                const OrganizationAuthorityClock &clock,
                                Remember &remember) {
  const auto &terms = *g.terms;
  auto window =
      authority_window_contains(terms.valid_from, terms.valid_until, at);
  if (!window.ok)
    return {false, {}, window.error};
  if (!remember(terms.id) || (g.runtime && g.runtime->revoked) || !window.value)
    return common_failure("HistoricalGrantUnavailable", terms.id);
  if (g.runtime && g.runtime->revision > 0 &&
      terms.continuity_policy_id == "stop_when_vacant" &&
      !g.runtime->issuer_office_id.empty()) {
    auto term = read_office_term_before_fact_at(c, s, catalog, identity,
                                                g.runtime->issuer_office_id, at,
                                                boundary, clock);
    if (!term.ok)
      return {false, {}, term.error};
    if (!term.value.holder_actor_id ||
        term.value.epoch != g.runtime->issuer_office_epoch)
      return common_failure("HistoricalGrantIssuerTermEnded", terms.id);
  }
  if (g.runtime && g.runtime->parent_grant_id) {
    auto parent =
        grant_view_before(c, s, catalog, *g.runtime->parent_grant_id, boundary);
    if (!parent.ok)
      return {false, {}, parent.error};
    if (!parent.value.terms->may_delegate ||
        !grant_terms_within(terms, *parent.value.terms))
      return common_failure("HistoricalParentGrantScopeDenied", terms.id);
    return parent_chain_before_view(c, s, catalog, identity, parent.value,
                                    boundary, at, clock, remember);
  }
  return common_success();
}
Status parent_chain_before(const CommonStore &c, const OrganizationState &s,
                           const OrganizationCatalog &catalog,
                           const CatalogIdentity &identity,
                           const HistoricalGrant &grant,
                           const StableId &boundary, const WorldMoment &at,
                           const OrganizationAuthorityClock &clock,
                           std::set<StableId> &seen) {
  auto remember = [&](const StableId &id) {
    return seen.insert(id).second && seen.size() <= 128;
  };
  HistoricalGrantView view{&grant.value.terms, &grant.value, nullptr};
  return parent_chain_before_view(c, s, catalog, identity, view, boundary, at,
                                  clock, remember);
}

}
Outcome<Grant> read_historical_authority_proof(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const AuthorityProof &proof,
    const SourceRef &boundary, const OrganizationAuthorityClock &clock) {
  auto denied = [&](const StableId &code) -> Outcome<Grant> {
    return {false, {}, common_failure(code, boundary.id).error};
  };
  const auto *head = c.state().facts.find(boundary.id);
  if (!head || head->domain != boundary.domain || head->kind != boundary.kind ||
      proof.schema_version != 2 || proof.grant_id.empty() ||
      proof.grant_revision < 0 ||
      proof.source_identity.world_id != c.state().identity.world_id ||
      proof.source_identity.execution_epoch < 0 ||
      proof.source_identity.revision < 0 ||
      proof.source_identity.revision > c.revision() ||
      proof.candidate_revision != proof.source_identity.revision ||
      proof.source_identity.tick < 0 ||
      proof.source_identity.tick > proof.observed_at.half_day_tick ||
      proof.amount < 0 || !proof.resource_versions.empty())
    return denied("InvalidHistoricalAuthorityProof");
  auto now = read_organization_authority_now(c, &clock);
  if (!now.ok)
    return {false, {}, now.error};
  auto past = compare_world_moments(proof.observed_at, now.value);
  if (!past.ok)
    return {false, {}, past.error};
  if (past.value > 0)
    return denied("AuthorityProofObservationInFuture");
  const auto &identity = catalog.authority_catalog_identity;
  if (identity.content_digest.empty() ||
      identity.content_digest != c.state().metadata.content_digest)
    return denied("AuthorityProofCatalogIdentityRequired");
  auto history = validate_grant_authority_history(c, s, catalog, &clock);
  if (!history.ok)
    return {false, {}, history.error};
  auto office =
      read_office_term_before_fact_at(c, s, catalog, identity, proof.office_id,
                                      proof.observed_at, boundary.id, clock);
  if (!office.ok)
    return {false, {}, office.error};
  if (office.value.organization_id != proof.organization_id ||
      office.value.status != OfficeStatus::active ||
      office.value.epoch != proof.authority_epoch ||
      office.value.holder_actor_id != std::optional<StableId>{proof.actor_id} ||
      !same_office_source(office.value.source, proof.office_source))
    return denied("HistoricalOfficeProofMismatch");
  // The original office list, or its real prior Issue fact, owns membership.
  // Merely defining an unused Grant with this office_id is not authorization.
  bool attached = false;
  if (catalog.grants.find(proof.grant_id)) {
    const auto *initial = catalog.offices.find(proof.office_id);
    attached = initial && has(initial->grant_ids, proof.grant_id);
  } else {
    for (const auto &id : c.state().facts.order()) {
      if (id == boundary.id)
        break;
      const auto *fact = s.facts.find(id);
      if (fact && fact->grant_change_payload) {
        const auto &change = *fact->grant_change_payload;
        if (change.kind == GrantChangeKind::issue &&
            change.grant_id == proof.grant_id &&
            change.after.terms.office_id == proof.office_id)
          attached = true;
      }
    }
  }
  if (!attached)
    return denied("HistoricalOfficeGrantMembershipMismatch");
  auto grant =
      grant_before(c, s, catalog, identity, proof.grant_id, boundary.id);
  if (!grant.ok)
    return {false, {}, grant.error};
  const auto &runtime = grant.value.value;
  const auto &terms = runtime.terms;
  if (runtime.revision != proof.grant_revision ||
      !same_grant_source(grant.value.source, proof.grant_source) ||
      terms.office_id != proof.office_id ||
      !has(terms.allowed_command_kinds, proof.command_kind) ||
      proof.amount > terms.amount_limit ||
      std::none_of(
          terms.scopes.begin(), terms.scopes.end(), [&](const auto &scope) {
            return scope.kind == proof.scope.kind && scope.id == proof.scope.id;
          }))
    return denied("HistoricalGrantProofMismatch");
  std::set<StableId> seen;
  auto chain = parent_chain_before(c, s, catalog, identity, grant.value,
                                   boundary.id, proof.observed_at, clock, seen);
  if (!chain.ok)
    return {false, {}, chain.error};
  return {true, terms, {}};
}
Outcome<AuthorityBasis> make_authority_basis_for_fact(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const SingleGrantAuthorityRequest& request,const SingleGrantAuthorityRead& read,const SourceRef& source,const AuthorityIssueSourceRead& sources,const std::optional<BudgetAuthorizationRef>& budget,const AuthorityBudgetRead* budgets){
  const auto &current = c.state().identity;
  const auto &before = read.source_identity;
  if (current.world_id != before.world_id ||
      current.execution_epoch != before.execution_epoch ||
      current.revision != before.revision || current.tick != before.tick)
    return {false, {}, common_failure("StaleAuthorityBasisCapture").error};
  auto actual = sources.read_authority_issue_source(source);
  if (!actual.ok)
    return {false, {}, actual.error};
  auto sequence = fact_sequence(c, source);
  if (!sequence.ok)
    return {false, {}, sequence.error};
  const auto &proof = read.authority;
  if (proof.schema_version != 2 || proof.grant_id != read.effective_grant.id ||
      proof.grant_revision != read.grant_revision ||
      proof.source_identity.world_id != before.world_id ||
      proof.source_identity.execution_epoch != before.execution_epoch ||
      proof.source_identity.revision != before.revision ||
      proof.source_identity.tick != before.tick ||
      !same_office_source(proof.office_source, read.office_source) ||
      !same_grant_source(proof.grant_source, read.grant_source))
    return {
        false, {}, common_failure("AuthorityBasisProofBindingMismatch").error};
  if (!same_authority_amount_components(request.amount_components,
                                        read.amount_components) ||
      !same_authority_amount_components(request.amount_components,
                                        actual.value.amount_components) ||
      proof.actor_id != request.acting.actor_id ||
      proof.organization_id != request.acting.organization_id ||
      proof.office_id != request.acting.office_id ||
      proof.authority_epoch != request.acting.authority_epoch ||
      proof.command_kind != request.command_kind ||
      proof.scope.kind != request.scope.kind ||
      proof.scope.id != request.scope.id || proof.amount != request.amount)
    return {false, {}, common_failure("AuthorityBasisCaptureMismatch").error};
  // Exact sources bind the same actual M; legacy tick precision is not
  // promoted.
  if (actual.value.occurred_at.kind == ActorFoundationAt::Kind::exact_moment) {
    auto same_m = compare_world_moments(proof.observed_at,
                                        actual.value.occurred_at.exact_moment);
    if (!same_m.ok)
      return {false, {}, same_m.error};
    if (same_m.value != 0)
      return {
          false, {}, common_failure("AuthorityBasisProofMomentMismatch").error};
  } else if (catalog.exact_fact_clock_enabled ||
             proof.observed_at.half_day_tick !=
                 actual.value.occurred_at.legacy_tick)
    return {
        false, {}, common_failure("AuthorityBasisProofMomentMismatch").error};
  AuthorityBasis out;
  out.source_identity = before;
  out.actor_id = proof.actor_id;
  out.organization_id = proof.organization_id;
  out.office_id = proof.office_id;
  out.office_epoch = proof.authority_epoch;
  out.grant_id = read.effective_grant.id;
  out.grant_revision = read.grant_revision;
  out.command_kind = request.command_kind;
  out.scope = request.scope;
  out.resources = request.resources;
  out.amount = request.amount;
  out.amount_limit = read.effective_grant.amount_limit;
  out.amount_components = request.amount_components;
  out.budget_ref = budget;
  out.valid_from = read.effective_grant.valid_from;
  out.valid_until = read.effective_grant.valid_until;
  out.continuity_policy_id = read.effective_grant.continuity_policy_id;
  out.issued_tick = tick_of(actual.value.occurred_at);
  out.issued_at = actual.value.occurred_at;
  out.issued_fact_sequence = sequence.value;
  out.issued_source = source;
  out.office_source = read.office_source;
  out.grant_source = read.grant_source;
  auto historical =
      grant_before(c, s, catalog, identity, out.grant_id, source.id);
  if (!historical.ok)
    return {false, {}, historical.error};
  if (historical.value.value.parent_grant_id) {
    auto parent =
        grant_before(c, s, catalog, identity,
                     *historical.value.value.parent_grant_id, source.id);
    if (!parent.ok)
      return {false, {}, parent.error};
    out.parent_grant_ref =
        ParentGrantRef{parent.value.value.terms.id, parent.value.value.revision,
                       parent.value.source};
    out.delegation_id = historical.value.value.last_change_fact_id;
  }
  auto valid = validate_historical_authority_basis(c, s, catalog, identity, out,
                                                   sources, budgets);
  return {valid.ok, std::move(out), valid.error};
}
Status validate_historical_authority_basis(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const AuthorityBasis& basis,const AuthorityIssueSourceRead& sources,const AuthorityBudgetRead* budgets){
    auto fail=[](const StableId& code){return common_failure(code);};if(basis.source_identity.world_id!=c.state().identity.world_id||basis.source_identity.execution_epoch<0||basis.source_identity.revision<0||basis.source_identity.revision>c.revision()||basis.source_identity.tick<0||basis.source_identity.tick>basis.issued_tick||basis.issued_tick<0||basis.amount<0||basis.resources.size()>128)return fail("InvalidHistoricalAuthorityBasis");auto now=read_organization_authority_now(c,&sources);if(!now.ok)return {false,{},now.error};const auto issued=basis.issued_at.kind==ActorFoundationAt::Kind::exact_moment?basis.issued_at.exact_moment:WorldMoment{basis.issued_at.legacy_tick,{}};auto in_past=compare_world_moments(issued,now.value);if(!in_past.ok)return {false,{},in_past.error};if(in_past.value>0)return fail("AuthorityIssueInFuture");auto actual=sources.read_authority_issue_source(basis.issued_source);if(!actual.ok)return {false,{},actual.error};if(!same_source(actual.value.source,basis.issued_source)||actual.value.actor_id!=basis.actor_id||actual.value.command_kind!=basis.command_kind||!same_authority_amount_components(actual.value.amount_components,basis.amount_components)||!same_at(actual.value.occurred_at,basis.issued_at)||basis.issued_tick!=tick_of(basis.issued_at))return fail("AuthorityIssueSourceMismatch");auto sequence=fact_sequence(c,basis.issued_source);if(!sequence.ok)return {false,{},sequence.error};if(sequence.value!=basis.issued_fact_sequence)return fail("AuthorityIssueSequenceMismatch");auto office=read_office_term_before_fact_at(c,s,catalog,identity,basis.office_id,issued,basis.issued_source.id,sources);if(!office.ok)return {false,{},office.error};if(office.value.organization_id!=basis.organization_id||office.value.epoch!=basis.office_epoch||office.value.holder_actor_id!=std::optional<StableId>{basis.actor_id}||!same_office_source(office.value.source,basis.office_source))return fail("HistoricalOfficeBasisMismatch");auto grant=grant_before(c,s,catalog,identity,basis.grant_id,basis.issued_source.id);if(!grant.ok)return {false,{},grant.error};const auto& g=grant.value.value;const auto& terms=g.terms;if(g.revision!=basis.grant_revision||!same_grant_source(grant.value.source,basis.grant_source)||terms.office_id!=basis.office_id||!has(terms.allowed_command_kinds,basis.command_kind)||terms.amount_limit!=basis.amount_limit||basis.amount>terms.amount_limit||terms.valid_from!=basis.valid_from||terms.valid_until!=basis.valid_until||terms.continuity_policy_id!=basis.continuity_policy_id||std::none_of(terms.scopes.begin(),terms.scopes.end(),[&](const auto& scope){return scope.kind==basis.scope.kind&&scope.id==basis.scope.id;}))return fail("HistoricalGrantBasisMismatch");if(!basis.amount_components.empty()){if(!catalog.authority_lifecycle_enabled||basis.amount!=0)return fail("AuthorityAmountProfileMismatch");auto units=validate_authority_amount_components(c,basis.amount_components,basis.resources,terms.amount_limit);if(!units.ok)return units;}std::set<std::pair<ResourceKey::Kind,StableId>> resources;for(const auto& resource:basis.resources){if(!resources.insert({resource.kind,resource.id}).second||((resource.kind==ResourceKey::Kind::wallet)?!has(terms.wallet_ids,resource.id):(resource.kind==ResourceKey::Kind::inventory)?!has(terms.inventory_ids,resource.id):true))return fail("HistoricalGrantResourceDenied");}std::set<StableId> chain;auto valid=parent_chain_before(c,s,catalog,identity,grant.value,basis.issued_source.id,issued,sources,chain);if(!valid.ok)return valid;if(g.parent_grant_id){if(!basis.parent_grant_ref||basis.delegation_id!=std::optional<StableId>{g.last_change_fact_id})return fail("HistoricalGrantParentMissing");auto parent=grant_before(c,s,catalog,identity,*g.parent_grant_id,basis.issued_source.id);if(!parent.ok)return {false,{},parent.error};if(basis.parent_grant_ref->grant_id!=*g.parent_grant_id||basis.parent_grant_ref->grant_revision!=parent.value.value.revision||!same_grant_source(basis.parent_grant_ref->authority_source,parent.value.source))return fail("HistoricalGrantParentMismatch");}else if(basis.parent_grant_ref||basis.delegation_id)return fail("UnexpectedHistoricalGrantParent");if(basis.budget_ref){if(!budgets)return fail("AuthorityBudgetSourceUnavailable");return budgets->validate_budget_authorization(*basis.budget_ref,basis);}return common_success();
}
}

namespace hanmo::resident {
Outcome<HistoricalAuthorityGrantRead> read_historical_authority_grant(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const CatalogIdentity &identity,
    const AuthorityBasis &basis, const AuthorityIssueSourceRead &sources,
    const AuthorityBudgetRead *budgets) {
  auto valid = validate_historical_authority_basis(
      common, state, catalog, identity, basis, sources, budgets);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto source = grant_view_before(common, state, catalog, basis.grant_id,
                                  basis.issued_source.id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &v = source.value;
  if (v.terms->id != basis.grant_id ||
      (v.runtime ? v.runtime->revision : 0) != basis.grant_revision)
    return {
        false,
        {},
        common_failure("HistoricalGrantBasisMismatch", basis.grant_id).error};
  return {true, {v.terms, v.runtime, &basis.grant_source}, {}};
}
Outcome<ActualTreatyUseGrantRead>
read_authority_grant_before_treaty_envoy_signature2(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const CatalogIdentity &identity,
    const AuthorityBasis &issue, const AuthorityIssueSourceRead &issue_sources,
    const MomentOwnerSource &actual_use, const WorldMoment &at,
    const TreatyEnvoyActualUseSourceRead &use_sources,
    const AuthorityBudgetRead *budgets) {
  const auto &use = actual_use.source_ref;
  auto denied = [&](const char *code) -> Outcome<ActualTreatyUseGrantRead> {
    return {false, {}, common_failure(code, use.id).error};
  };
  if (!use_sources.bound_to(c, s) || actual_use.source_schema != 2 ||
      issue.issued_at.kind != ActorFoundationAt::Kind::exact_moment ||
      !catalog.authority_lifecycle_enabled ||
      !catalog.exact_fact_clock_enabled || identity.content_digest.empty() ||
      !same_catalog(identity, catalog.authority_catalog_identity) ||
      identity.content_digest != c.state().metadata.content_digest ||
      issue.issued_source.domain != "politics" ||
      issue.issued_source.kind != "treaty_negotiation_mandated" ||
      issue.command_kind != "diplomatic_commission_envoy" ||
      use.domain != "politics" || use.kind != "treaty_terms_ratified" ||
      !valid_source_ref(use))
    return denied("ActualTreatyGrantUseSelectionRequired");
  auto use_position = read_actual_common_fact_position(c, use);
  if (!use_position.ok)
    return {false, {}, use_position.error};
  auto issue_position =
      read_actual_common_fact_position(c, issue.issued_source);
  if (!issue_position.ok)
    return {false, {}, issue_position.error};
  if (issue_position.value >= use_position.value)
    return denied("ActualTreatyGrantUseBeforeIssue");
  auto actual = use_sources.read_actual_envoy_signature2(actual_use);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &raw = actual.value;
  if (!raw.actual_head || raw.actual_head != c.state().facts.find(use.id) ||
      raw.actual_head->id != use.id || raw.actual_head->domain != use.domain ||
      raw.actual_head->kind != use.kind || !raw.actual_mandate ||
      raw.actual_mandate->source_schema != 1 ||
      !same_source(raw.actual_mandate->source_ref, issue.issued_source) ||
      !raw.envoy_actor_id || raw.envoy_actor_id->empty() || !raw.signed_at)
    return denied("ActualTreatyGrantUseOriginalRequired");
  auto exact = compare_world_moments(*raw.signed_at, at);
  if (!exact.ok)
    return {false, {}, exact.error};
  if (exact.value != 0)
    return denied("ActualTreatyGrantUseMomentMismatch");
  auto now = read_organization_authority_now(c, &use_sources);
  if (!now.ok)
    return {false, {}, now.error};
  auto past = compare_world_moments(at, now.value);
  if (!past.ok)
    return {false, {}, past.error};
  if (past.value > 0)
    return denied("ActualTreatyGrantUseInFuture");
  auto issued = validate_historical_authority_basis(
      c, s, catalog, identity, issue, issue_sources, budgets);
  if (!issued.ok)
    return {false, {}, issued.error};
  const auto &issue_at = issue.issued_at.exact_moment;
  auto ordered = compare_world_moments(issue_at, at);
  if (!ordered.ok)
    return {false, {}, ordered.error};
  if (ordered.value > 0)
    return denied("ActualTreatyGrantUseBeforeIssueMoment");
  auto history = validate_grant_authority_history(c, s, catalog, &use_sources);
  if (!history.ok)
    return {false, {}, history.error};
  auto grant = grant_view_before(c, s, catalog, issue.grant_id, use.id);
  if (!grant.ok)
    return {false, {}, grant.error};
  if (grant.value.terms->id != issue.grant_id ||
      grant.value.terms->office_id != issue.office_id)
    return denied("ActualTreatyGrantUseIdentityMismatch");
  CheckpointVector<const StableId *> seen;
  // 同原父链算法；防环只借原ID，不构造一套临时Grant/parent状态。
  auto remember = [&](const StableId &id) {
    for (const auto *p : seen)
      if (*p == id)
        return false;
    if (seen.size() >= 128)
      return false;
    seen.push_back(&id);
    return true;
  };
  auto chain = parent_chain_before_view(c, s, catalog, identity, grant.value,
                                        use.id, at, use_sources, remember);
  if (!chain.ok)
    return {false, {}, chain.error};
  for (const auto *id : seen) {
    const auto &order = c.state().facts.order();
    for (std::size_t n = 0; n < use_position.value; ++n) {
      const auto *f = s.facts.find(order[n]);
      if (!f || !f->grant_change_payload ||
          f->grant_change_payload->grant_id != *id)
        continue;
      if (!f->occurred_at)
        return denied("ActualTreatyGrantUseExactHistoryRequired");
      auto before = compare_world_moments(*f->occurred_at, at);
      if (!before.ok)
        return {false, {}, before.error};
      if (before.value > 0)
        return denied("ActualTreatyGrantUseHistoryMomentMismatch");
    }
  }
  auto office = read_office_term_before_fact_at(
      c, s, catalog, identity, issue.office_id, at, use.id, use_sources);
  if (!office.ok)
    return {false, {}, office.error};
  if (office.value.organization_id != issue.organization_id ||
      office.value.office_id != issue.office_id)
    return denied("ActualTreatyGrantUseOfficeMismatch");
  ActualTreatyUseGrantRead out;
  out.terms = grant.value.terms;
  out.runtime = grant.value.runtime;
  out.grant_change = grant.value.change;
  out.authored_office = catalog.offices.find(issue.office_id);
  out.office_epoch = office.value.epoch;
  out.office_status = office.value.status;
  if (!out.authored_office)
    return denied("ActualTreatyGrantUseOfficeMissing");
  if (const auto *changed =
          std::get_if<OfficeChangedAuthoritySource>(&office.value.source)) {
    out.office_change = s.facts.find(changed->fact_id);
    if (!out.office_change || out.office_change->id != changed->fact_id ||
        out.office_change->office_id != issue.office_id)
      return denied("ActualTreatyGrantUseOfficeSourceMismatch");
    if (!out.office_change->actor_id.empty())
      out.holder_actor_id = &out.office_change->actor_id;
  } else if (out.authored_office->holder_actor_id)
    out.holder_actor_id = &*out.authored_office->holder_actor_id;
  if (bool(out.holder_actor_id) != bool(office.value.holder_actor_id) ||
      (out.holder_actor_id &&
       *out.holder_actor_id != *office.value.holder_actor_id))
    return denied("ActualTreatyGrantUseOfficeHolderMismatch");
  // 返回真实最新范围，不把issue revision固定成永久权限，也不擅定机构连续性。
  return {true, out, {}};
}
Outcome<ActualShopLicensingGrantRead> read_authority_grant_before_shop_licensing2(
 const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,
 const CatalogIdentity& identity,const MomentOwnerSource& actual_use,
 const ActualShopLicensingAuthoritySourceRead& use_sources) {
 const auto& use=actual_use.source_ref;
 auto denied=[&](const char* code)->Outcome<ActualShopLicensingGrantRead>{return {false,{},common_failure(code,use.id).error};};
 if(!use_sources.bound_to(c,s)||actual_use.source_schema!=2||
    !catalog.authority_lifecycle_enabled||!catalog.exact_fact_clock_enabled||
    identity.content_digest.empty()||!same_catalog(identity,catalog.authority_catalog_identity)||
    identity.content_digest!=c.state().metadata.content_digest||
    use.domain!="political_shop_licensing"||!valid_source_ref(use)||
    (use.kind!="political_wanted_registered"&&use.kind!="political_wanted_released"&&
     use.kind!="political_shop_clearance_recorded"&&use.kind!="political_shop_permit_revoked"))
   return denied("ActualShopLicensingGrantSelectionRequired");
 auto use_position=read_actual_common_fact_position(c,use);if(!use_position.ok)return {false,{},use_position.error};
 auto actual=use_sources.read_actual_shop_authority2(actual_use);if(!actual.ok)return {false,{},actual.error};
 const auto& raw=actual.value;
 if(!raw.actual_head||raw.actual_head!=c.state().facts.find(use.id)||
    raw.actual_head->id!=use.id||raw.actual_head->domain!=use.domain||raw.actual_head->kind!=use.kind||
    !raw.actor||raw.actor->actor_id.empty()||raw.actor->office_id.empty()||raw.actor->organization_id.empty()||
    !raw.grant_id||raw.grant_id->empty()||!raw.occurred_at)
   return denied("ActualShopLicensingGrantOriginalRequired");
 const auto& at=*raw.occurred_at;
 auto now=read_organization_authority_now(c,&use_sources);if(!now.ok)return {false,{},now.error};
 auto past=compare_world_moments(at,now.value);if(!past.ok)return {false,{},past.error};
 if(past.value>0)return denied("ActualShopLicensingGrantFuture");
  auto history = validate_grant_authority_history(c, s, catalog, &use_sources);
  if (!history.ok)
    return {false, {}, history.error};
  auto grant = grant_view_before(c, s, catalog, *raw.grant_id, use.id);
  if (!grant.ok)
    return {false, {}, grant.error};
  if (grant.value.terms->id != *raw.grant_id ||
      grant.value.terms->office_id != raw.actor->office_id)
    return denied("ActualShopLicensingGrantIdentityMismatch");
  CheckpointVector<const StableId *> seen;
  // 同原父链算法；防环只借原ID，不构造一套临时Grant/parent状态。
  auto remember = [&](const StableId &id) {
    for (const auto *p : seen)
      if (*p == id)
        return false;
    if (seen.size() >= 128)
      return false;
    seen.push_back(&id);
    return true;
  };
  auto chain = parent_chain_before_view(c, s, catalog, identity, grant.value,
                                        use.id, at, use_sources, remember);
  if (!chain.ok)
    return {false, {}, chain.error};
  for (const auto *id : seen) {
    const auto &order = c.state().facts.order();
    for (std::size_t n = 0; n < use_position.value; ++n) {
      const auto *f = s.facts.find(order[n]);
      if (!f || !f->grant_change_payload ||
          f->grant_change_payload->grant_id != *id)
        continue;
      if (!f->occurred_at)
        return denied("ActualShopLicensingGrantExactHistoryRequired");
      auto before = compare_world_moments(*f->occurred_at, at);
      if (!before.ok)
        return {false, {}, before.error};
      if (before.value > 0)
        return denied("ActualShopLicensingGrantHistoryMomentMismatch");
    }
  }
  auto office = read_office_term_before_fact_at(
      c, s, catalog, identity, raw.actor->office_id, at, use.id, use_sources);
  if (!office.ok)
    return {false, {}, office.error};
  if (office.value.organization_id != raw.actor->organization_id ||
      office.value.office_id != raw.actor->office_id)
    return denied("ActualShopLicensingGrantOfficeMismatch");
  ActualShopLicensingGrantRead out;
  out.terms = grant.value.terms;
  out.runtime = grant.value.runtime;
  out.grant_change = grant.value.change;
  out.authored_office = catalog.offices.find(raw.actor->office_id);
  out.office_epoch = office.value.epoch;
  out.office_status = office.value.status;
  if (!out.authored_office)
    return denied("ActualShopLicensingGrantOfficeMissing");
  if (const auto *changed =
          std::get_if<OfficeChangedAuthoritySource>(&office.value.source)) {
    out.office_change = s.facts.find(changed->fact_id);
    if (!out.office_change || out.office_change->id != changed->fact_id ||
        out.office_change->office_id != raw.actor->office_id)
      return denied("ActualShopLicensingGrantOfficeSourceMismatch");
    if (!out.office_change->actor_id.empty())
      out.holder_actor_id = &out.office_change->actor_id;
  } else if (out.authored_office->holder_actor_id)
    out.holder_actor_id = &*out.authored_office->holder_actor_id;
  if (bool(out.holder_actor_id) != bool(office.value.holder_actor_id) ||
      (out.holder_actor_id &&
       *out.holder_actor_id != *office.value.holder_actor_id))
    return denied("ActualShopLicensingGrantOfficeHolderMismatch");
  if(!out.holder_actor_id||*out.holder_actor_id!=raw.actor->actor_id||
     out.office_epoch!=raw.actor->authority_epoch||out.office_status!=OfficeStatus::active)
    return denied("ActualShopLicensingSignerMismatch");
  // 实际处分时权源，不用今日任职倒验原司法签字。
  return {true, out, {}};
}
} // namespace hanmo::resident
