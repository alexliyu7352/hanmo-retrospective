// 原Organization事实承载变更，current_grants仅为其唯一当前投影；冷读不重发授权。
#include "grant_authority.hpp"
#include "grant_authority_history_provenance.hpp"
#include "grant_terms_definition.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "organization_office_history.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <string_view>
namespace hanmo::resident {
namespace {
bool has(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
bool scope(const ScopeRef &a, const ScopeRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
bool same_grant(const Grant &a, const Grant &b) {
  if (a.id != b.id || a.office_id != b.office_id ||
      a.allowed_command_kinds != b.allowed_command_kinds ||
      a.wallet_ids != b.wallet_ids || a.inventory_ids != b.inventory_ids ||
      a.amount_limit != b.amount_limit || a.valid_from != b.valid_from ||
      a.valid_until != b.valid_until || a.may_delegate != b.may_delegate ||
      a.continuity_policy_id != b.continuity_policy_id ||
      a.scopes.size() != b.scopes.size())
    return false;
  for (std::size_t n = 0; n < a.scopes.size(); ++n)
    if (!scope(a.scopes[n], b.scopes[n]))
      return false;
  return true;
}
bool same_runtime(const GrantRuntimeRecord &a, const GrantRuntimeRecord &b) {
  return same_grant(a.terms, b.terms) && a.revision == b.revision &&
         a.revoked == b.revoked && a.parent_grant_id == b.parent_grant_id &&
         a.parent_revision == b.parent_revision && a.policy_id == b.policy_id &&
         a.last_change_fact_id == b.last_change_fact_id &&
         a.issuer_office_id == b.issuer_office_id &&
         a.issuer_office_epoch == b.issuer_office_epoch;
}
Status terms_shape(const CommonStore &c, const OrganizationState &s,
                   const Grant &g, bool require_id = true) {
  const auto *office = s.offices.find(g.office_id);
  if (!office)
    return common_failure("InvalidRuntimeGrant", g.id);
  auto definition = validate_runtime_grant_terms_definition(g, require_id);
  if (!definition.ok)
    return definition;
  std::set<StableId> wallets, inventories;
  for (const auto &id : g.wallet_ids) {
    const auto *resource = c.wallet(id);
    if (!resource || resource->owner.kind != Owner::Kind::organization ||
        resource->owner.id != office->organization_id ||
        !wallets.insert(id).second)
      return common_failure("AuthorityResourceMismatch", id);
  }
  for (const auto &id : g.inventory_ids) {
    const auto *resource = c.inventory(id);
    if (!resource || resource->owner.kind != Owner::Kind::organization ||
        resource->owner.id != office->organization_id ||
        !inventories.insert(id).second)
      return common_failure("AuthorityResourceMismatch", id);
  }
  return common_success();
}
StableId kind_name(GrantChangeKind kind) {
  return kind == GrantChangeKind::issue   ? "organization_grant_issued"
         : kind == GrantChangeKind::amend ? "organization_grant_amended"
                                          : "organization_grant_revoked";
}
StableId command_name(GrantChangeKind kind) {
  return kind == GrantChangeKind::issue   ? "issue_grant"
         : kind == GrantChangeKind::amend ? "amend_grant"
                                          : "revoke_grant";
}
// 父链只沿现行授权缩限，撤销/缩权即时影响未来派生命令；旧已接受义务另由原连续性核。
// 原父链只读去重键借真实根ID，节点使用同job真实allocator。
using CurrentGrantVisited = std::set<std::string_view, std::less<>,
                                     CheckpointAllocator<std::string_view>>;
Outcome<const GrantRuntimeRecord *>
read_chain(const CommonStore &c, const OrganizationState &s,
           const OrganizationCatalog &catalog, const StableId &id,
           CurrentGrantVisited &seen) {
  const auto *g = s.current_grants.find(id);
  if (!g || g->terms.id != id || !seen.insert(g->terms.id).second ||
      seen.size() > 128)
    return {false, {}, common_failure("InvalidGrantParentChain", id).error};
  if (g->revoked)
    return {false, {}, common_failure("GrantRevoked", id).error};
  const auto *office = s.offices.find(g->terms.office_id);
  if (!office)
    return {
        false, {}, common_failure("UnknownOffice", g->terms.office_id).error};
  if (g->revision > 0 && g->terms.continuity_policy_id == "stop_when_vacant" &&
      !g->issuer_office_id.empty()) {
    const auto *issuer = s.offices.find(g->issuer_office_id);
    if (!issuer || !issuer->holder_actor_id ||
        issuer->status != OfficeStatus::active ||
        issuer->epoch != g->issuer_office_epoch)
      return {false, {}, common_failure("GrantIssuerTermEnded", id).error};
  }
  if (g->parent_grant_id) {
    auto parent = read_chain(c, s, catalog, *g->parent_grant_id, seen);
    if (!parent.ok)
      return parent;
    if (!parent.value->terms.may_delegate ||
        !grant_terms_within(g->terms, parent.value->terms))
      return {false, {}, common_failure("GrantParentNoLongerCovers", id).error};
  }
  return {true, g, {}};
}
} // namespace
bool grant_terms_within(const Grant &child, const Grant &parent) {
  if (child.amount_limit > parent.amount_limit ||
      child.valid_from < parent.valid_from ||
      (parent.valid_until &&
       (!child.valid_until || *child.valid_until > *parent.valid_until)) ||
      (child.may_delegate && !parent.may_delegate))
    return false;
  for (const auto &id : child.allowed_command_kinds)
    if (!has(parent.allowed_command_kinds, id))
      return false;
  for (const auto &id : child.wallet_ids)
    if (!has(parent.wallet_ids, id))
      return false;
  for (const auto &id : child.inventory_ids)
    if (!has(parent.inventory_ids, id))
      return false;
  for (const auto &item : child.scopes)
    if (std::none_of(parent.scopes.begin(), parent.scopes.end(),
                     [&](const auto &allowed) { return scope(item, allowed); }))
      return false;
  return true;
}
Status initialize_grant_authority(const CommonStore &c, OrganizationState &s,
                                  const OrganizationCatalog &catalog) {
  if (!catalog.authority_lifecycle_enabled)
    return s.current_grants.empty() ? common_success()
                                    : common_failure("UnexpectedRuntimeGrants");
  if (c.revision() != 0 || c.tick() != 0 || !s.current_grants.empty())
    return common_failure("GrantAuthorityColdSeedRequired");
  for (const auto &row : catalog.grants.rows()) {
    GrantRuntimeRecord r;
    r.terms = row.second;
    const auto *office = s.offices.find(r.terms.office_id);
    if (!office)
      return common_failure("UnknownOffice", r.terms.office_id);
    r.issuer_office_id = office->id;
    r.issuer_office_epoch = office->epoch;
    if (!s.current_grants.insert(row.first, std::move(r)))
      return common_failure("DuplicateRuntimeGrant", row.first);
  }
  return common_success();
}
Outcome<GrantRuntimeRecord>
read_current_grant(const CommonStore &c, const OrganizationState &s,
                   const OrganizationCatalog &catalog, const StableId &id) {
  if (!catalog.authority_lifecycle_enabled) {
    const auto *original = catalog.grants.find(id);
    if (!original)
      return {false, {}, common_failure("UnknownGrant", id).error};
    GrantRuntimeRecord value;
    value.terms = *original;
    return {true, std::move(value), {}};
  }
  CurrentGrantVisited seen;
  auto current = read_chain(c, s, catalog, id, seen);
  if (!current.ok)
    return {false, {}, current.error};
  // 原裸ABI仅末端保留兼容复制；正式借读不经过此复制。
  return {true, *current.value, {}};
}
// 完整原当前父链通过后，只返回实际catalog/current_grants中terms，不持久化、不跨写保存。
Outcome<const Grant *> read_current_grant_terms_borrowed(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const StableId &id) {
  if (!catalog.authority_lifecycle_enabled) {
    const auto *original = catalog.grants.find(id);
    if (!original)
      return {false, {}, common_failure("UnknownGrant", id).error};
    return {true, original, {}};
  }
  CurrentGrantVisited seen;
  auto current = read_chain(c, s, catalog, id, seen);
  if (!current.ok)
    return {false, {}, current.error};
  return {true, &current.value->terms, {}};
}
namespace {
// 只借当前候选的原Office/Grant；结果仅活到一次调用结束，不能持久化或跨写边界使用。
struct PreparedGrantChange {
  const GrantPolicy *policy = nullptr;
  const OfficeState *target = nullptr;
  const GrantRuntimeRecord *old = nullptr;
  SingleGrantAuthorityRequest request;
  SingleGrantAuthorityRead authorized;
  GrantRuntimeRecord after;
};
// 抽出原writer分配ID以前的同一串验证，顺序与错误保持不变，不写Common或组织。
Outcome<PreparedGrantChange>
prepare_grant_change(const CommonStore &c, const OrganizationState &s,
                     const OrganizationCatalog &catalog,
                     const GrantChangeCommand &command, const WorldMoment &at,
                     const OrganizationAuthorityClock &clock) {
  auto fail = [](const StableId &code) -> Outcome<PreparedGrantChange> {
    return {false, {}, common_failure(code).error};
  };
  if (!catalog.authority_lifecycle_enabled)
    return fail("RuntimeGrantCapabilityRequired");
  auto time = encode_world_moment(at);
  if (!time.ok)
    return {false, {}, time.error};
  auto now = read_organization_authority_now(c, &clock);
  if (!now.ok)
    return {false, {}, now.error};
  auto actual = compare_world_moments(at, now.value);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (actual.value != 0)
    return fail("GrantActualBoundaryMismatch");
  const auto *policy = catalog.grant_policies.find(command.policy_id);
  const auto *target = s.offices.find(command.terms.office_id);
  if (!policy || policy->issuing_office_id != command.acting.office_id ||
      !target || target->epoch != command.expected_office_epoch ||
      target->organization_id != command.acting.organization_id ||
      !has(policy->target_office_ids, target->id) ||
      policy->maximum_window_ticks <= 0)
    return fail("GrantPolicyDenied");
  const auto *old = s.current_grants.find(command.grant_id);
  if (command.kind == GrantChangeKind::issue) {
    if (old || !command.grant_id.empty() || !command.terms.id.empty() ||
        command.expected_grant_revision != 0)
      return fail("GrantAlreadyExists");
  } else if (!old || old->revoked ||
             old->revision != command.expected_grant_revision ||
             old->terms.office_id != target->id)
    return fail("StaleGrantRevision");
  const auto *org = s.organizations.find(command.acting.organization_id);
  if (!org)
    return fail("UnknownOrganization");
  if (!org->headquarters_site)
    return fail("OrganizationFixedHeadquartersRequired");
  SingleGrantAuthorityRequest request;
  request.acting = command.acting;
  request.command_kind = command_name(command.kind);
  request.scope = {ScopeRef::Kind::site, *org->headquarters_site};
  request.requires_delegation = true;
  auto authorized = check_current_grant_authority(
      c, s, catalog, catalog.authority_catalog_identity, request, &clock);
  if (!authorized.ok)
    return {false, {}, authorized.error};
  GrantRuntimeRecord after;
  if (old)
    after = *old;
  if (command.kind == GrantChangeKind::revoke)
    after.revoked = true;
  else {
    auto valid = terms_shape(c, s, command.terms,
                             command.kind != GrantChangeKind::issue);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto starts =
        compare_world_moments(WorldMoment{command.terms.valid_from, {}}, at);
    if (!starts.ok)
      return {false, {}, starts.error};
    if (starts.value < 0 ||
        *command.terms.valid_until - command.terms.valid_from >
            policy->maximum_window_ticks ||
        !grant_terms_within(command.terms, policy->maximum) ||
        !grant_terms_within(command.terms, authorized.value.effective_grant))
      return fail("GrantDelegationExceedsAuthority");
    after.terms = command.terms;
    after.parent_grant_id = command.parent_grant_id;
    after.parent_revision = command.expected_parent_revision;
    if (!after.parent_grant_id ||
        *after.parent_grant_id != authorized.value.effective_grant.id ||
        after.parent_revision != authorized.value.grant_revision)
      return fail("GrantParentAuthorityMismatch");
    if (!command.grant_id.empty()) {
      auto parent = after.parent_grant_id;
      std::set<StableId> ancestry;
      while (parent) {
        if (*parent == command.grant_id || !ancestry.insert(*parent).second)
          return fail("GrantParentCycle");
        const auto *source = s.current_grants.find(*parent);
        if (!source)
          return fail("GrantParentMissing");
        parent = source->parent_grant_id;
      }
    }
    after.policy_id = policy->id;
    after.issuer_office_id = command.acting.office_id;
    after.issuer_office_epoch = command.acting.authority_epoch;
  }
  if (after.revision == std::numeric_limits<Integer>::max())
    return fail("GrantRevisionOverflow");
  return {true,
          {policy, target, old, std::move(request), std::move(authorized.value),
           std::move(after)},
          {}};
}
} // namespace

// 公共预览只读取实际世界时刻、任期、唯一父Grant及政策；不分配ID，不生成版本索引。
Status preview_grant_change(const CommonStore &c, const OrganizationState &s,
                            const OrganizationCatalog &catalog,
                            const GrantChangeCommand &command,
                            const WorldMoment &at,
                            const OrganizationAuthorityClock &clock) {
  auto prepared = prepare_grant_change(c, s, catalog, command, at, clock);
  if (!prepared.ok) {
    return {false, {}, std::move(prepared.error)};
  }
  // 原writer此门位于next_id之后。预览不得分配ID；只读相同身份条件，writer保原次序。
  if (command.kind != GrantChangeKind::issue &&
      prepared.value.after.terms.id != command.grant_id) {
    return common_failure("GrantIdentityChangeDenied");
  }
  return common_success();
}

// 唯一writer重跑同一预检，随后沿原顺序分配ID并追加原事实与当前Grant投影。
Outcome<StableId> apply_grant_change_in_candidate(
    CommonStore &c, OrganizationState &s, const OrganizationCatalog &catalog,
    const GrantChangeCommand &command, const WorldMoment &at,
    const OrganizationAuthorityClock &clock) {
  auto prepared = prepare_grant_change(c, s, catalog, command, at, clock);
  if (!prepared.ok) {
    return {false, {}, std::move(prepared.error)};
  }
  auto fail = [](const StableId &code) -> Outcome<StableId> {
    return {false, {}, common_failure(code).error};
  };
  const auto *policy = prepared.value.policy;
  const auto *target = prepared.value.target;
  const auto *old = prepared.value.old;
  const auto &request = prepared.value.request;
  const auto &authorized = prepared.value.authorized;
  auto after = std::move(prepared.value.after);
  const Integer before_revision = after.revision;
  after.revision += 1;
  auto id = c.next_id("organization");
  if (!id.ok)
    return id;
  if (command.kind == GrantChangeKind::issue)
    after.terms.id = id.value;
  else if (after.terms.id != command.grant_id)
    return fail("GrantIdentityChangeDenied");
  after.last_change_fact_id = id.value;
  OrganizationFact fact;
  fact.id = id.value;
  fact.kind = kind_name(command.kind);
  fact.organization_id = target->organization_id;
  fact.office_id = target->id;
  fact.actor_id = command.acting.actor_id;
  fact.tick = at.half_day_tick;
  if (catalog.exact_fact_clock_enabled)
    fact.occurred_at = at;
  GrantChangePayload change;
  change.kind = command.kind;
  change.grant_id = after.terms.id;
  change.policy_id = policy->id;
  change.authority_grant_id = authorized.effective_grant.id;
  change.authority_grant_revision = authorized.grant_revision;
  change.before_revision = before_revision;
  change.after_revision = after.revision;
  change.issuer = command.acting;
  change.authority_scope = request.scope;
  change.target_office_epoch = target->epoch;
  if (old)
    change.before = *old;
  change.after = after;
  change.effective_moment = at;
  fact.grant_change_payload = std::move(change);
  if (const auto *ref =
          std::get_if<GrantRevisionAuthoritySource>(&authorized.grant_source))
    fact.cause_fact_ids.push_back(ref->fact_id);
  auto appended = c.append_fact({id.value, fact.kind, "organization"});
  if (!appended.ok)
    return {false, {}, appended.error};
  if (!s.facts.insert(id.value, std::move(fact)))
    return fail("DuplicateGrantChangeFact");
  if (old)
    *s.current_grants.find(after.terms.id) = after;
  else {
    if (!s.current_grants.insert(after.terms.id, after))
      return fail("DuplicateRuntimeGrant");
    s.offices.find(target->id)->grant_ids.push_back(after.terms.id);
  }
  return {true, id.value, {}};
}
// 冷链只解释实际历史源；资源现在可已合法转移，当前Owner由外层原资源历史validator另核。
namespace {
Status validate_grant_authority_history_impl(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const OrganizationAuthorityClock *clock,
    GrantAuthorityHistoryOfficeSink *sink) {
  if (!catalog.authority_lifecycle_enabled)
    return s.current_grants.empty() ? common_success()
                                    : common_failure("UnexpectedRuntimeGrants");
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  OrderedTable<GrantRuntimeRecord> derived;
  auto expected_offices = catalog.offices;
  for (const auto &row : catalog.grants.rows()) {
    const auto *office = catalog.offices.find(row.second.office_id);
    if (!office)
      return common_failure("UnknownInitialGrantOffice", row.first);
    GrantRuntimeRecord root;
    root.terms = row.second;
    root.issuer_office_id = office->id;
    root.issuer_office_epoch = office->epoch;
    derived.insert(row.first, std::move(root));
  }
  for (const auto &row : catalog.grant_policies.rows()) {
    const auto &policy = row.second;
    if (row.first != policy.id ||
        !catalog.offices.find(policy.issuing_office_id) ||
        policy.maximum_window_ticks <= 0 || policy.target_office_ids.empty() ||
        policy.target_office_ids.size() > 128)
      return common_failure("InvalidGrantPolicy", row.first);
    std::set<StableId> targets;
    for (const auto &target : policy.target_office_ids)
      if (!catalog.offices.find(target) || !targets.insert(target).second)
        return common_failure("InvalidGrantPolicyTarget", target);
  }
  std::set<StableId> seen;
  for (const auto &fact_id : c.state().facts.order()) {
    const auto *fact = s.facts.find(fact_id);
    if (!fact || !fact->grant_change_payload)
      continue;
    const auto &change = *fact->grant_change_payload;
    const auto *head = c.state().facts.find(fact_id);
    const auto *policy = catalog.grant_policies.find(change.policy_id);
    const auto *prior = derived.find(change.grant_id);
    const auto *authority = derived.find(change.authority_grant_id);
    const auto *target = s.offices.find(change.after.terms.office_id);
    auto sequence = organization_instance_sequence(fact_id, "organization");
    if ((change.kind != GrantChangeKind::issue &&
         change.kind != GrantChangeKind::amend &&
         change.kind != GrantChangeKind::revoke) ||
        !sequence || !head || head->domain != "organization" ||
        head->kind != kind_name(change.kind) || fact->kind != head->kind ||
        fact->office_change_payload || !policy || !authority || !target ||
        target->organization_id != change.issuer.organization_id ||
        authority->revoked ||
        authority->revision != change.authority_grant_revision ||
        authority->terms.office_id != change.issuer.office_id ||
        !authority->terms.may_delegate ||
        !has(authority->terms.allowed_command_kinds,
             command_name(change.kind)) ||
        fact->tick < authority->terms.valid_from ||
        (authority->terms.valid_until &&
         fact->tick > *authority->terms.valid_until) ||
        std::none_of(
            authority->terms.scopes.begin(), authority->terms.scopes.end(),
            [&](const auto &allowed) {
              return scope(allowed, change.authority_scope);
            }) ||
        change.before_revision < 0 ||
        change.before_revision == std::numeric_limits<Integer>::max() ||
        change.after_revision != change.before_revision + 1 ||
        change.after.revision != change.after_revision ||
        change.after.last_change_fact_id != fact_id ||
        change.after.terms.id != change.grant_id ||
        fact->actor_id != change.issuer.actor_id ||
        fact->organization_id != change.issuer.organization_id ||
        fact->office_id != change.after.terms.office_id ||
        change.effective_moment.half_day_tick != fact->tick ||
        policy->issuing_office_id != change.issuer.office_id ||
        !has(policy->target_office_ids, target->id) ||
        !seen.insert(fact_id).second)
      return common_failure("InvalidGrantChangeSource", fact_id);
    if (catalog.exact_fact_clock_enabled) {
      if (!fact->occurred_at)
        return common_failure("GrantFactExactMomentRequired", fact_id);
      auto actual =
          compare_world_moments(*fact->occurred_at, change.effective_moment);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (actual.value)
        return common_failure("GrantFactExactMomentMismatch", fact_id);
    }
    auto moment = encode_world_moment(change.effective_moment);
    if (!moment.ok)
      return {false, {}, moment.error};
    auto future = compare_world_moments(change.effective_moment, now.value);
    if (!future.ok)
      return {false, {}, future.error};
    if (future.value > 0)
      return common_failure("GrantChangeInFuture", fact_id);
    auto active = authority_window_contains(authority->terms.valid_from,
                                            authority->terms.valid_until,
                                            change.effective_moment);
    if (!active.ok)
      return {false, {}, active.error};
    if (!active.value)
      return common_failure("GrantHistoryAuthorityExpired", fact_id);
    auto read_term_at = [&](const StableId &office) {
      auto result = clock
                        ? read_office_term_before_fact_at(
                              c, s, catalog, catalog.authority_catalog_identity,
                              office, change.effective_moment, fact_id, *clock)
                        : read_office_term_before_fact(
                              c, s, catalog, catalog.authority_catalog_identity,
                              office, fact->tick, fact_id);
      if (!result.ok || !sink)
        return result;
      // 只在这个真实read成功后报告原拥有行，不泄露result/derived短栈副本。
      const auto *initial_office = catalog.offices.find(office);
      const auto *current_office = s.offices.find(office);
      if (!initial_office || !current_office)
        return Outcome<OfficeTermRead>{
            false,
            {},
            common_failure("GrantHistoryOfficeProvenanceRequired", office)
                .error};
      auto emitted =
          sink->office_use(c, s, catalog, *head, *fact, change.effective_moment,
                           *initial_office, *current_office);
      if (!emitted.ok)
        return Outcome<OfficeTermRead>{false, {}, emitted.error};
      return result;
    };
    auto signer = read_term_at(change.issuer.office_id);
    if (!signer.ok)
      return {false, {}, signer.error};
    if (signer.value.organization_id != change.issuer.organization_id ||
        signer.value.holder_actor_id !=
            std::optional<StableId>{change.issuer.actor_id} ||
        signer.value.epoch != change.issuer.authority_epoch)
      return common_failure("InvalidHistoricalOfficeAuthority", fact_id);
    auto target_term = read_term_at(target->id);
    if (!target_term.ok)
      return {false, {}, target_term.error};
    if (target_term.value.epoch != change.target_office_epoch)
      return common_failure("GrantTargetTermMismatch", fact_id);
    // 验签时所有祖先均使用该事件之前的当前版本；根缩限不能由后代保旧版本越过。
    std::set<StableId> ancestors;
    const GrantRuntimeRecord *ancestor = authority;
    while (ancestor) {
      if (ancestor->revoked || !ancestors.insert(ancestor->terms.id).second ||
          ancestors.size() > 128)
        return common_failure("InvalidGrantHistoryParentChain", fact_id);
      if (ancestor->revision > 0 &&
          ancestor->terms.continuity_policy_id == "stop_when_vacant") {
        auto term = read_term_at(ancestor->issuer_office_id);
        if (!term.ok)
          return {false, {}, term.error};
        if (!term.value.holder_actor_id ||
            term.value.status != OfficeStatus::active ||
            term.value.epoch != ancestor->issuer_office_epoch)
          return common_failure("GrantHistoryIssuerTermEnded", fact_id);
      }
      if (!ancestor->parent_grant_id)
        break;
      const auto *parent = derived.find(*ancestor->parent_grant_id);
      if (!parent || !parent->terms.may_delegate ||
          !grant_terms_within(ancestor->terms, parent->terms))
        return common_failure("GrantHistoryParentNoLongerCovers", fact_id);
      ancestor = parent;
    }
    std::vector<StableId> expected_causes;
    if (authority->revision > 0)
      expected_causes.push_back(authority->last_change_fact_id);
    if (fact->cause_fact_ids != expected_causes)
      return common_failure("GrantAuthorityCauseMismatch", fact_id);
    if (change.kind == GrantChangeKind::issue) {
      if (prior || change.before || change.before_revision != 0 ||
          change.grant_id != fact_id)
        return common_failure("GrantIssueHistoryMismatch", fact_id);
      expected_offices.find(target->id)->grant_ids.push_back(change.grant_id);
    } else if (!prior || !change.before ||
               !same_runtime(*prior, *change.before) ||
               prior->revision != change.before_revision || prior->revoked ||
               prior->terms.office_id != target->id)
      return common_failure("GrantBeforeHistoryMismatch", fact_id);
    if (change.kind == GrantChangeKind::revoke) {
      if (!prior)
        return common_failure("GrantRevocationHistoryMismatch", fact_id);
      auto expected = *prior;
      expected.revoked = true;
      expected.revision = change.after_revision;
      expected.last_change_fact_id = fact_id;
      if (!same_runtime(expected, change.after))
        return common_failure("GrantRevocationHistoryMismatch", fact_id);
    } else {
      const auto &after = change.after;
      const auto &terms = after.terms;
      if (after.revoked || terms.amount_limit < 0 ||
          terms.valid_from < fact->tick || !terms.valid_until ||
          *terms.valid_until < terms.valid_from ||
          *terms.valid_until - terms.valid_from >
              policy->maximum_window_ticks ||
          terms.allowed_command_kinds.empty() || terms.scopes.empty() ||
          after.parent_grant_id !=
              std::optional<StableId>{authority->terms.id} ||
          after.parent_revision != authority->revision ||
          !grant_terms_within(terms, authority->terms) ||
          !grant_terms_within(terms, policy->maximum) ||
          after.policy_id != policy->id ||
          after.issuer_office_id != change.issuer.office_id ||
          after.issuer_office_epoch != change.issuer.authority_epoch ||
          ancestors.count(change.grant_id))
        return common_failure("GrantDelegationHistoryMismatch", fact_id);
    }
    if (prior)
      *derived.find(change.grant_id) = change.after;
    else
      derived.insert(change.grant_id, change.after);
  }
  if (derived.size() != s.current_grants.size())
    return common_failure("RuntimeGrantCoverageMismatch");
  for (const auto &row : derived.rows()) {
    const auto *actual = s.current_grants.find(row.first);
    if (!actual || !same_runtime(row.second, *actual))
      return common_failure("RuntimeGrantCurrentMismatch", row.first);
  }
  for (const auto &row : expected_offices.rows()) {
    const auto *actual = s.offices.find(row.first);
    if (!actual || actual->grant_ids != row.second.grant_ids)
      return common_failure("RuntimeOfficeGrantProjectionMismatch", row.first);
  }
  for (const auto &row : s.facts.rows())
    if ((row.second.grant_change_payload && !seen.count(row.first)) ||
        ((row.second.kind == "organization_grant_issued" ||
          row.second.kind == "organization_grant_amended" ||
          row.second.kind == "organization_grant_revoked") &&
         !row.second.grant_change_payload))
      return common_failure("OrphanGrantChangeSource", row.first);
  return common_success();
}
} // namespace
Status
validate_grant_authority_history(const CommonStore &c,
                                 const OrganizationState &s,
                                 const OrganizationCatalog &catalog,
                                 const OrganizationAuthorityClock *clock) {
  return validate_grant_authority_history_impl(c, s, catalog, clock, nullptr);
}
Status visit_grant_authority_history_office_uses(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const OrganizationAuthorityClock &clock,
    GrantAuthorityHistoryOfficeSink &sink) {
  return validate_grant_authority_history_impl(c, s, catalog, &clock, &sink);
}
} // namespace hanmo::resident
