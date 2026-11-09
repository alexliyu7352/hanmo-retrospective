#include "organization_lifecycle_contract.hpp"
#include "organization_lifecycle_catalog.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
namespace hanmo::resident {
namespace {
// 只读冷验键借原根或本次枚举拥有体；节点由同 job 实际分配器计费。
using BorrowedLifecycleIds =
    std::set<std::string_view, std::less<std::string_view>,
             CheckpointAllocator<std::string_view>>;
using BorrowedLifecycleCause =
    std::tuple<std::string_view, Integer, std::string_view, std::string_view,
               std::string_view>;
// 这些SourceRef只指本owner真实追加的载荷；不会给尚未发生的业务预分完成源。
SourceRef origin_ref(const StableId &id) {
  return {"organization", "organization_lifecycle_origin", id};
}
SourceRef transition_ref(const StableId &id) {
  return {"organization", "organization_lifecycle_transition", id};
}
bool same_optional(const std::optional<SourceRef> &a,
                   const std::optional<SourceRef> &b) {
  return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
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
Status fact(const CommonStore &c, const SourceRef &s) {
  const auto *f = c.state().facts.find(s.id);
  return valid_source_ref(s) && f && f->domain == s.domain && f->kind == s.kind
             ? common_success()
             : common_failure("OrganizationLifecycleFactMissing", s.id);
}
// 有限原状态机；真实目标仍由原业务source导出，函数不接受作者自定义边。
Status edge(const std::optional<OrganizationStatus> &before,
            const AppliedOrganizationLifecycleCause &x) {
  using S = OrganizationStatus;
  using C = OrganizationLifecycleCause;
  using T = OrganizationTerminalReason;
  if (!organization_lifecycle_tag(x.resulting_lifecycle) ||
      bool(x.terminal_reason) != (x.resulting_lifecycle == S::terminal) ||
      x.terminal_reason == T::legacy_dissolved)
    return common_failure("InvalidOrganizationLifecycleTerminal",
                          x.organization_id);
  const auto is = [&](S s) { return before && *before == s; };
  const auto active = [&] {
    return is(S::active) || is(S::limited) || is(S::exile);
  };
  bool ok = false;
  switch (x.kind) {
  case C::formation_registered:
    ok = !before && x.resulting_lifecycle == S::forming && !x.outcome_ref;
    break;
  case C::formation_completed:
    ok = is(S::forming) &&
         (x.resulting_lifecycle == S::active ||
          x.resulting_lifecycle == S::limited) &&
         x.outcome_ref.has_value();
    break;
  case C::institutional_limit_applied:
    ok = is(S::active) && x.resulting_lifecycle == S::limited;
    break;
  case C::institutional_limit_released:
    ok = is(S::limited) && x.resulting_lifecycle == S::active;
    break;
  case C::merge_accepted:
    ok = active() && x.resulting_lifecycle == S::merging;
    break;
  case C::merge_abandoned:
    ok = is(S::merging) && (x.resulting_lifecycle == S::active ||
                            x.resulting_lifecycle == S::limited ||
                            x.resulting_lifecycle == S::exile);
    break;
  case C::merge_completed:
    ok = is(S::merging) && x.resulting_lifecycle == S::terminal &&
         x.terminal_reason == T::merged_into_successor &&
         x.outcome_ref.has_value();
    break;
  case C::exile_continuity_applied:
    ok = (is(S::active) || is(S::limited) || is(S::merging)) &&
         x.resulting_lifecycle == S::exile;
    break;
  case C::restoration_applied:
    ok = is(S::exile) && (x.resulting_lifecycle == S::active ||
                          x.resulting_lifecycle == S::limited);
    break;
  case C::dissolution_accepted:
  case C::continuity_failed:
    ok = (active() || is(S::forming) || is(S::merging)) &&
         x.resulting_lifecycle == S::dissolving;
    break;
  case C::dissolution_completed:
    ok = (is(S::dissolving) ||
          (is(S::forming) && x.terminal_reason == T::formation_abandoned)) &&
         x.resulting_lifecycle == S::terminal && x.outcome_ref.has_value() &&
         (x.terminal_reason == T::formation_abandoned ||
          x.terminal_reason == T::dissolved_by_decision ||
          x.terminal_reason == T::continuity_exhausted);
    break;
  }
  return ok ? common_success()
            : common_failure("OrganizationLifecycleEdgeDenied",
                             x.organization_id);
}
// 重放核实际来源全部语义，不能只比同ID便重新接受不同发生时间或结果。
Status agrees(const OrganizationLifecycleTransition &t,
              const AppliedOrganizationLifecycleCause &p) {
  auto m = compare_world_moments(t.effective_moment, p.occurred_at);
  if (!m.ok)
    return {false, {}, m.error};
  if (t.organization_id != p.organization_id || t.cause_kind != p.kind ||
      !same_source_ref(t.cause_ref, p.source) ||
      !same_optional(t.outcome_ref, p.outcome_ref) ||
      t.after_status != p.resulting_lifecycle ||
      t.terminal_reason != p.terminal_reason || m.value != 0)
    return common_failure("OrganizationLifecycleSourceMismatch", t.id);
  return edge(t.before_status, p);
}
} // namespace
Status initialize_organization_lifecycle_in_candidate(
    CommonStore &c, OrganizationState &org, OrganizationLifecycleState &s,
    const OrganizationCatalog &base,
    const OrganizationLifecycleCatalog &catalog,
    const OrganizationAuthoredOriginSourceRead &sources) {
  auto catalog_valid = validate_organization_lifecycle_catalog(catalog, base);
  if (!catalog_valid.ok)
    return catalog_valid;
  if (!base.authority_lifecycle_enabled ||
      !s.organization_metadata.rows().empty() ||
      !s.lifecycle_origins.rows().empty() ||
      !s.lifecycle_transitions.rows().empty() ||
      !s.lifecycle_migration_decisions.rows().empty() ||
      !s.lifecycle_outcomes.rows().empty() ||
      !s.terminal_dispositions.rows().empty() ||
      !same_catalog(base.authority_catalog_identity, catalog.identity))
    return common_failure("OrganizationLifecycleInitializationConflict");
  std::set<StableId> selected;
  for (const auto &binding : catalog.authored_seeds) {
    if (!selected.insert(binding.first).second)
      return common_failure("DuplicateOrganizationLifecycleSeed",
                            binding.first);
    const auto *body = org.organizations.find(binding.first);
    const auto *initial = base.organizations.find(binding.first);
    const auto *policy = catalog.seed_policies.find(binding.second.id);
    if (!body || !initial || !policy ||
        policy->version != binding.second.version ||
        binding.second.catalog_digest != catalog.identity.content_digest ||
        body->kind != policy->organization_kind ||
        body->status != policy->initial_lifecycle ||
        initial->kind != body->kind || initial->status != body->status ||
        !organization_lifecycle_tag(body->status) ||
        bool(policy->terminal_reason) !=
            (body->status == OrganizationStatus::terminal) ||
        policy->terminal_reason == OrganizationTerminalReason::legacy_dissolved)
      return common_failure("InvalidOrganizationLifecycleSeed", binding.first);
    auto ready =
        sources.validate_authored_origin(*body, *policy, catalog.identity);
    if (!ready.ok)
      return ready;
    auto id = c.next_id("organization_lifecycle_origin");
    if (!id.ok)
      return {false, {}, id.error};
    auto appended = c.append_fact(
        {id.value, "organization_lifecycle_origin", "organization"});
    if (!appended.ok)
      return appended;
    OrganizationLifecycleOrigin origin{
        id.value, body->id,
        OrganizationAuthoredOrigin{catalog.identity, body->id, binding.second}};
    if (!s.lifecycle_origins.insert(id.value, std::move(origin)) ||
        !s.organization_metadata.insert(
            body->id, {body->id, 0, origin_ref(id.value), {}}))
      return common_failure("DuplicateOrganizationLifecycleOrigin", body->id);
  }
  if (selected.size() != org.organizations.rows().size() ||
      selected.size() != base.organizations.rows().size())
    return common_failure("OrganizationLifecycleSeedCoverage");
  return common_success();
}
Outcome<SourceRef> register_founded_organization_in_candidate(
    CommonStore &, OrganizationState &, OrganizationLifecycleState &,
    const OrganizationLifecycleCatalog &, const SourceRef &decision,
    const ReadStamp &, const OrganizationLifecycleSourceRead &) {
  // P5 成立写路后置：不读取业务来源、不分配 ID，也不改变开局静态起源。
  return {false,
          {},
          common_failure("NOT_IMPLEMENTED", decision.id,
                         ErrorKind::unsupported_capability)
              .error};
}
Outcome<const OrganizationLifecycleTransition *>
read_organization_lifecycle_transition(const CommonStore &c,
                                       const OrganizationLifecycleState &s,
                                       const SourceRef &ref) {
  const auto *row = s.lifecycle_transitions.find(ref.id);
  auto valid = fact(c, ref);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  if (!same_source_ref(ref, transition_ref(ref.id)) || !row ||
      row->id != ref.id)
    return {
        false, nullptr,
        common_failure("OrganizationLifecycleTransitionMissing", ref.id).error};
  return {true, row, {}};
}
Outcome<SourceRef> consume_organization_lifecycle_source_in_candidate(
    CommonStore &, OrganizationState &, OrganizationLifecycleState &,
    const StableId &organization, Integer, const SourceRef &,
    const ReadStamp &, const OrganizationLifecycleSourceRead &) {
  // P5 生命周期变更写路后置：不消费来源、不追加事实，也不改变组织状态。
  return {false,
          {},
          common_failure("NOT_IMPLEMENTED", organization,
                         ErrorKind::unsupported_capability)
              .error};
}
// 全量冷参照算法按原Common事实采用顺序重建，不能按ID词序制造同刻因果。
Status validate_organization_lifecycle(
    const CommonStore &c, const OrganizationState &org,
    const OrganizationLifecycleState &s, const OrganizationCatalog &base,
    const OrganizationLifecycleCatalog &catalog, const ReadStamp &stamp,
    const OrganizationLifecycleSourceRead &sources, const WorldMoment &now) {
  auto catalog_valid = validate_organization_lifecycle_catalog(catalog, base);
  if (!catalog_valid.ok)
    return catalog_valid;
  if (!base.authority_lifecycle_enabled ||
      !same_catalog(base.authority_catalog_identity, catalog.identity))
    return common_failure("OrganizationLifecycleCatalogMismatch");
  auto expected = sources.enumerate_original_organization_ids(stamp);
  if (!expected.ok)
    return {false, {}, expected.error};
  CheckpointVector<const StableId *> ids;
  ids.reserve(expected.value.ids.size());
  for (const auto &id : expected.value.ids)
    ids.push_back(&id);
  std::sort(ids.begin(), ids.end(),
            [](const auto *a, const auto *b) { return *a < *b; });
  const bool repeated = std::adjacent_find(ids.begin(), ids.end(),
                                           [](const auto *a, const auto *b) {
                                             return *a == *b;
                                           }) != ids.end();
  if (repeated || ids.size() != org.organizations.rows().size() ||
      ids.size() != s.organization_metadata.rows().size() ||
      ids.size() != s.lifecycle_origins.rows().size())
    return common_failure("OrganizationLifecycleRegistryCoverage");
  CheckpointMap<std::string_view, std::size_t> fact_order;
  std::size_t seq = 0;
  for (const auto &id : c.state().facts.order())
    fact_order.emplace(id, seq++);
  const auto before_fact = [&](const SourceRef &a, const SourceRef &b) {
    auto x = fact_order.find(a.id), y = fact_order.find(b.id);
    return x != fact_order.end() && y != fact_order.end() &&
           x->second < y->second;
  };
  BorrowedLifecycleIds used_origins, used_migrations, used_transitions,
      used_dispositions;
  std::set<BorrowedLifecycleCause, std::less<BorrowedLifecycleCause>,
           CheckpointAllocator<BorrowedLifecycleCause>>
      once;
  for (const auto *original_id : ids) {
    const auto &id = *original_id;
    const auto *body = org.organizations.find(id);
    const auto *meta = s.organization_metadata.find(id);
    if (!body || body->id != id || !meta || meta->organization_id != id ||
        meta->revision < 0 || !organization_lifecycle_tag(body->status))
      return common_failure("OrganizationLifecycleBodyMissing", id);
    const auto *origin = s.lifecycle_origins.find(meta->origin_ref.id);
    if (!origin || origin->id != meta->origin_ref.id ||
        origin->organization_id != id ||
        (meta->origin_ref.domain != "organization" ||
         meta->origin_ref.kind != "organization_lifecycle_origin") ||
        !used_origins.insert(origin->id).second)
      return common_failure("OrganizationLifecycleOriginMismatch", id);
    auto valid = fact(c, meta->origin_ref);
    if (!valid.ok)
      return valid;
    std::optional<OrganizationStatus> current;
    std::optional<OrganizationTerminalReason> terminal;
    if (const auto *a =
            std::get_if<OrganizationAuthoredOrigin>(&origin->origin)) {
      const auto *seed = base.organizations.find(a->organization_row_id);
      const auto *policy =
          catalog.seed_policies.find(a->lifecycle_seed_policy_ref.id);
      bool selected = false;
      for (const auto &b : catalog.authored_seeds)
        if (b.first == id && b.second.id == a->lifecycle_seed_policy_ref.id &&
            b.second.version == a->lifecycle_seed_policy_ref.version &&
            b.second.catalog_digest ==
                a->lifecycle_seed_policy_ref.catalog_digest)
          selected = true;
      if (!selected || !same_catalog(a->catalog_identity, catalog.identity) ||
          !seed || seed->id != id || seed->kind != body->kind || !policy ||
          policy->version != a->lifecycle_seed_policy_ref.version ||
          a->lifecycle_seed_policy_ref.catalog_digest !=
              catalog.identity.content_digest ||
          policy->organization_kind != body->kind ||
          policy->initial_lifecycle != seed->status ||
          bool(policy->terminal_reason) !=
              (seed->status == OrganizationStatus::terminal) ||
          policy->terminal_reason ==
              OrganizationTerminalReason::legacy_dissolved)
        return common_failure("OrganizationAuthoredOriginMismatch", id);
      valid =
          sources.validate_authored_origin(*seed, *policy, catalog.identity);
      if (!valid.ok)
        return valid;
      current = policy->initial_lifecycle;
      terminal = policy->terminal_reason;
    } else if (const auto *f =
                   std::get_if<OrganizationFoundedOrigin>(&origin->origin)) {
      auto actual =
          sources.read_founded_organization(f->founding_decision_ref, stamp);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (actual.value.record.id != id ||
          actual.value.record.kind != body->kind ||
          !same_source_ref(actual.value.registration_result_ref,
                           f->registration_result_ref) ||
          meta->revision == 0 ||
          !before_fact(f->registration_result_ref, meta->origin_ref))
        return common_failure("FoundedOrganizationOriginMismatch", id);
    } else if (const auto *m =
                   std::get_if<OrganizationMigratedOrigin>(&origin->origin)) {
      const auto *d =
          s.lifecycle_migration_decisions.find(m->migration_decision_ref.id);
      if (!d || d->id != m->migration_decision_ref.id ||
          !used_migrations.insert(d->id).second ||
          m->migration_decision_ref.domain != "organization" ||
          m->migration_decision_ref.kind != "organization_lifecycle_migrated" ||
          (d->original_profile != 2 && d->original_profile != 3) ||
          d->original_profile != m->original_profile ||
          d->original_organization_id != id ||
          m->original_organization_id != id ||
          d->original_status != m->original_status ||
          d->source_checkpoint_identity.world_id !=
              m->source_checkpoint_identity.world_id ||
          d->source_checkpoint_identity.revision !=
              m->source_checkpoint_identity.revision ||
          d->source_checkpoint_identity.tick !=
              m->source_checkpoint_identity.tick ||
          !same_catalog(d->source_checkpoint_identity.catalog,
                        m->source_checkpoint_identity.catalog) ||
          !organization_lifecycle_tag(d->selected_lifecycle) ||
          bool(d->selected_terminal_reason) !=
              (d->selected_lifecycle == OrganizationStatus::terminal))
        return common_failure("OrganizationMigrationOriginMismatch", id);
      if (d->original_status != "active" && d->original_status != "limited" &&
          d->original_status != "vacant" && d->original_status != "dissolved")
        return common_failure("InvalidLegacyOrganizationStatus", id);
      if ((d->original_status == "active" &&
           d->selected_lifecycle != OrganizationStatus::active) ||
          (d->original_status == "limited" &&
           d->selected_lifecycle != OrganizationStatus::limited) ||
          (d->original_status == "vacant" &&
           d->selected_lifecycle != OrganizationStatus::active &&
           d->selected_lifecycle != OrganizationStatus::limited) ||
          (d->original_status == "dissolved" &&
           (d->selected_lifecycle != OrganizationStatus::terminal ||
            d->selected_terminal_reason !=
                OrganizationTerminalReason::legacy_dissolved)))
        return common_failure("OrganizationMigrationLifecycleMismatch", id);
      valid = sources.validate_migration(*d, *body);
      if (!valid.ok)
        return valid;
      valid = fact(c, m->migration_decision_ref);
      if (!valid.ok)
        return valid;
      current = d->selected_lifecycle;
      terminal = d->selected_terminal_reason;
    } else
      return common_failure("UnknownOrganizationLifecycleOrigin", id);
    Integer revision = 0;
    std::optional<SourceRef> last;
    std::optional<WorldMoment> at;
    const SourceRef *previous = &meta->origin_ref;
    for (const auto &key : s.lifecycle_transitions.order()) {
      const auto *row = s.lifecycle_transitions.find(key);
      if (!row || row->id != key)
        return common_failure("OrganizationLifecycleTransitionKeyMismatch",
                              key);
      if (row->organization_id != id)
        continue;
      if (!used_transitions.insert(key).second ||
          revision == std::numeric_limits<Integer>::max() ||
          row->before_revision != revision ||
          row->after_revision != revision + 1 ||
          row->before_status != current ||
          !before_fact(*previous, transition_ref(key)) ||
          !before_fact(row->cause_ref, transition_ref(key)))
        return common_failure("OrganizationLifecycleChainDiscontinuity", key);
      if (!once.emplace(id, static_cast<Integer>(row->cause_kind),
                        row->cause_ref.domain, row->cause_ref.kind,
                        row->cause_ref.id)
               .second)
        return common_failure("DuplicateOrganizationLifecycleCause", key);
      auto stored =
          read_organization_lifecycle_transition(c, s, transition_ref(key));
      if (!stored.ok)
        return {false, {}, stored.error};
      auto cause = sources.read_applied_lifecycle_cause(row->cause_ref, stamp);
      if (!cause.ok)
        return {false, {}, cause.error};
      valid = agrees(*row, cause.value);
      if (!valid.ok)
        return valid;
      auto upper = compare_world_moments(row->effective_moment, now);
      if (!upper.ok)
        return {false, {}, upper.error};
      if (upper.value > 0)
        return common_failure("FutureOrganizationLifecycleTransition", key);
      if (at) {
        auto order = compare_world_moments(*at, row->effective_moment);
        if (!order.ok)
          return {false, {}, order.error};
        if (order.value > 0)
          return common_failure("OrganizationLifecycleTimeReversal", key);
      }
      if (row->outcome_ref) {
        const auto *outcome = s.lifecycle_outcomes.find(row->outcome_ref->id);
        if (!outcome || row->outcome_ref->domain != "organization" ||
            row->outcome_ref->kind != "organization_lifecycle_outcome")
          return common_failure("OrganizationLifecycleOutcomeMissing", key);
        valid = sources.validate_lifecycle_outcome(*outcome, stamp);
        if (!valid.ok)
          return valid;
      }
      current = row->after_status;
      terminal = row->terminal_reason;
      revision = row->after_revision;
      last = transition_ref(key);
      previous = &*last;
      at = row->effective_moment;
    }
    if (!current || *current != body->status || meta->revision != revision ||
        !same_optional(last, meta->last_lifecycle_transition_ref))
      return common_failure("OrganizationLifecycleCurrentMismatch", id);
    BorrowedLifecycleIds offices;
    for (const auto &office_id : body->office_ids) {
      const auto *office = org.offices.find(office_id);
      if (!office || office->organization_id != id ||
          !offices.insert(office_id).second)
        return common_failure("OrganizationOfficeOriginMissing", office_id);
      valid =
          sources.validate_organization_office_origin(*body, *office, stamp);
      if (!valid.ok)
        return valid;
    }
    for (const auto &office : org.offices.rows())
      if (office.second.organization_id == id && !offices.count(office.first))
        return common_failure("OrganizationOfficeOriginCoverage", office.first);
    if (body->status == OrganizationStatus::terminal && last) {
      std::size_t count = 0;
      for (const auto &row : s.terminal_dispositions.rows())
        if (row.second.organization_id == id) {
          ++count;
          const auto *outcome =
              s.lifecycle_outcomes.find(row.second.lifecycle_outcome_ref.id);
          if (row.first != row.second.id || !outcome ||
              !used_dispositions.insert(row.first).second)
            return common_failure("OrganizationTerminalDispositionMissing", id);
          valid = sources.validate_terminal_disposition(row.second, *outcome,
                                                        stamp);
          if (!valid.ok)
            return valid;
        }
      if (count != 1)
        return common_failure("OrganizationTerminalDispositionCoverage", id);
    }
  }
  if (used_origins.size() != s.lifecycle_origins.rows().size() ||
      used_migrations.size() != s.lifecycle_migration_decisions.rows().size() ||
      used_transitions.size() != s.lifecycle_transitions.rows().size() ||
      used_dispositions.size() != s.terminal_dispositions.rows().size())
    return common_failure("OrphanOrganizationLifecycleSource");
  for (const auto &outcome : s.lifecycle_outcomes.rows()) {
    if (outcome.first != outcome.second.id)
      return common_failure("OrganizationLifecycleOutcomeKeyMismatch",
                            outcome.first);
    auto valid = sources.validate_lifecycle_outcome(outcome.second, stamp);
    if (!valid.ok)
      return valid;
  }
  // 两种制度关系各自无环；其他政治关系不借administrative_parent伪装。
  for (int relation = 0; relation < 2; ++relation)
    for (const auto &item : org.organizations.rows()) {
      BorrowedLifecycleIds seen;
      const OrganizationRecord *p = &item.second;
      while (p) {
        if (!seen.insert(p->id).second)
          return common_failure("OrganizationParentCycle", item.first);
        const auto &parent =
            relation == 0 ? p->administrative_parent : p->branch_parent;
        if (!parent)
          break;
        p = org.organizations.find(*parent);
        if (!p)
          return common_failure("OrganizationParentMissing", *parent);
      }
    }
  return common_success();
}
} // namespace hanmo::resident
