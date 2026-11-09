#include "political_dismissal2.hpp"
#include "resident/domains/organization/organization_clock_adapter.hpp"
#include "resident/domains/organization/political_office_transition.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
bool same_command(const PoliticalDismissCommand &a,
                  const PoliticalDismissCommand &b) {
  return std::tie(a.authority.organization_id, a.authority.office_id,
                  a.authority.authority_epoch, a.authority.actor_id,
                  a.jurisdiction_id, a.expected_jurisdiction_revision,
                  a.expected_office_epoch) ==
         std::tie(b.authority.organization_id, b.authority.office_id,
                  b.authority.authority_epoch, b.authority.actor_id,
                  b.jurisdiction_id, b.expected_jurisdiction_revision,
                  b.expected_office_epoch);
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return std::tie(a.world_id, a.execution_epoch, a.revision, a.tick) ==
         std::tie(b.world_id, b.execution_epoch, b.revision, b.tick);
}
Status record(const PoliticalDismissal2Context &x, const StableId &key,
              const PoliticalDismissalRecord2 &r) {
  if (key != r.dismissal_source.id || r.dismissal_source.domain != "politics" ||
      r.dismissal_source.kind != "political_office_dismissed" ||
      r.office_change_source.domain != "organization" ||
      r.office_change_source.kind != "organization_office_changed" ||
      r.authority_basis_ref.source_schema != 1 ||
      !same_source_ref(r.authority_basis_ref.issued_source,
                       r.dismissal_source) ||
      r.jurisdiction_revision < 0)
    return common_failure("PoliticalDismissal2BindingMismatch", key);
  // 业务源严格schema2；不能拿Basis布局1替代实际政治body选择。
  auto at = read_politics_source_moment(x.common, x.politics, x.runtime.runtime,
                                        r.dismissal_source, 2, x.runtime.clock);
  if (!at.ok)
    return {false, {}, at.error};
  const auto &f = *x.politics.facts.find(key);
  const auto *j = x.politics.jurisdictions.find(f.jurisdiction_id);
  const auto *change = x.organizations.facts.find(r.office_change_source.id);
  const auto *head = x.common.state().facts.find(r.office_change_source.id);
  if (!j || !change || !head || head->domain != r.office_change_source.domain ||
      head->kind != r.office_change_source.kind ||
      change->kind != r.office_change_source.kind ||
      change->office_id != f.office_id ||
      change->organization_id != f.organization_id ||
      change->cause_fact_ids != std::vector<StableId>{key} ||
      !change->occurred_at || change->tick != at.value.half_day_tick)
    return common_failure("PoliticalDismissal2OfficeBindingMismatch", key);
  auto moment = compare_world_moments(*change->occurred_at, at.value);
  if (!moment.ok)
    return {false, {}, moment.error};
  if (moment.value)
    return common_failure("PoliticalDismissal2OfficeMomentMismatch", key);
  const auto &order = x.common.state().facts.order();
  auto before = std::find(order.begin(), order.end(), key),
       after = std::find(order.begin(), order.end(), change->id);
  if (before == order.end() || after == order.end() || after <= before)
    return common_failure("PoliticalDismissal2FactOrder", key);
  NativePoliticsAuthorityRead authority(x.common, x.politics, x.runtime.clock);
  auto basis = read_registered_authority_basis(
      x.common, x.runtime.runtime.authority_bases, x.organizations,
      x.organization_catalog, x.organization_catalog.authority_catalog_identity,
      r.authority_basis_ref, authority);
  if (!basis.ok)
    return {false, {}, basis.error};
  const auto &b = *basis.value;
  if (b.actor_id != f.decision_actor_id || b.actor_id != f.issuer_actor_id ||
      b.organization_id != f.organization_id ||
      b.command_kind != "political_dismiss" ||
      b.scope.kind != ScopeRef::Kind::site || b.scope.id != j->seat_place_id ||
      b.budget_ref || b.amount != 0)
    return common_failure("PoliticalDismissal2AuthorityMismatch", key);
  auto revision = x.territories.jurisdiction_revision_before_fact(
      f.jurisdiction_id, key, f.tick);
  if (!revision.ok)
    return {false, {}, revision.error};
  if (revision.value != r.jurisdiction_revision)
    return common_failure("PoliticalDismissal2TerritoryRevisionMismatch", key);
  PoliticalOfficeTransitionRead offices(
      x.common, x.politics, x.organizations, x.organization_catalog,
      x.runtime.runtime.office_transition_sources, x.runtime.clock);
  auto transition =
      offices.read_office_transition(r.dismissal_source, x.runtime.read_stamp);
  if (!transition.ok)
    return {false, {}, transition.error};
  if (transition.value.kind != OfficeTransitionKind::dismiss ||
      transition.value.old_holder != std::optional<StableId>{f.actor_id} ||
      transition.value.expected_epoch != f.office_epoch ||
      transition.value.office_id != f.office_id ||
      transition.value.new_holder !=
          (change->actor_id.empty()
               ? std::optional<StableId>{}
               : std::optional<StableId>{change->actor_id}))
    return common_failure("PoliticalDismissal2PreviousTermMismatch", key);
  return x.history.validate_signer_before(
      {b.organization_id, b.office_id, b.office_epoch, b.actor_id},
      r.dismissal_source, at.value);
}
} // namespace
bool NativePoliticalDismissalHistoryRead::bound_to(
    const CommonStore &common, const OrganizationState &organization,
    const OrganizationCatalog &catalog,
    const OrganizationAuthorityClock &clock) const noexcept {
  return &common == &common_ && &organization == &organizations_ &&
         &catalog == &catalog_ && &clock == &clock_ &&
         &ages_.common == &common_ && &ages_.foundation == &foundation_ &&
         &ages_.foundation_catalog == &foundation_catalog_;
}
Status NativePoliticalDismissalHistoryRead::validate_signer_before(
    const ActingAuthority &actor, const SourceRef &boundary,
    const WorldMoment &at) const {
  OrganizationCurrentMomentAdapter clock(clock_);
  auto presence = read_character_presence_before_fact_v2(
      common_, foundation_, foundation_catalog_, deaths_, death_sources_,
      actor.actor_id, boundary, at, clock, family_catalog_, shared_,
      ages_.born_sources);
  if (!presence.ok)
    return {false, {}, presence.error};
  if (!presence.value.present || !presence.value.alive)
    return common_failure("PoliticalDismissalHistoricalLivingSignerRequired",
                          boundary.id);
  auto age = read_life_age_basis(ages_, actor.actor_id, at, boundary.id);
  if (!age.ok)
    return {false, {}, age.error};
  if (age.value.age_years < 18)
    return common_failure("PoliticalDismissalHistoricalAdultRequired",
                          boundary.id);
  return validate_political_historical_member_duty(
      common_, organizations_, catalog_, actor, at.half_day_tick, boundary.id,
      PoliticalMemberHistoryUse::political_dismissal, nullptr, &clock_, &at);
}
Outcome<Integer> political_dismissal2_selected_version(
    const std::vector<std::pair<StableId, StableId>> &modules) {
  Integer selected = 0;
  for (const auto &m : modules)
    if (m.first == "political_dismissal_sources") {
      if (selected || m.second != "2")
        return {false, 0,
                common_failure("PoliticalDismissal2ModuleMismatch").error};
      selected = 2;
    }
  if (!selected)
    return {true, 0, {}};
  auto exact = [&](const StableId &name,
                   const std::vector<StableId> &versions) {
    std::size_t found = 0;
    for (const auto &m : modules)
      if (m.first == name) {
        if (std::find(versions.begin(), versions.end(), m.second) ==
            versions.end())
          return false;
        ++found;
      }
    return found == 1;
  };
  if (!exact("politics", {"2"}) || !exact("organization", {"6", "7"}) ||
      !exact("family_life", {"4"}) || !exact("actor_origin", {"2"}) ||
      !exact("actor_condition", {"2"}))
    return {false, 0,
            common_failure("PoliticalDismissal2DependenciesRequired").error};
  return {true, 2, {}};
}
Status validate_political_dismissal2(const PoliticalDismissal2Context &x) {
  if (!x.history.bound_to(x.common, x.organizations, x.organization_catalog,
                          x.runtime.clock))
    return common_failure("PoliticalDismissal2HistoryCandidateMismatch");
  if (x.state.version != 2 ||
      !x.organization_catalog.authority_lifecycle_enabled ||
      !x.organization_catalog.exact_fact_clock_enabled)
    return common_failure("PoliticalDismissal2ProfileRequired");
  std::vector<StableId> changes;
  const auto &order = x.common.state().facts.order();
  std::size_t previous = 0;
  bool first = true;
  for (const auto &row : x.state.dismissals.rows()) {
    auto valid = record(x, row.first, row.second);
    if (!valid.ok)
      return valid;
    auto at = std::find(order.begin(), order.end(), row.first);
    if (at == order.end() ||
        (!first && static_cast<std::size_t>(at - order.begin()) <= previous) ||
        std::find(changes.begin(), changes.end(),
                  row.second.office_change_source.id) != changes.end())
      return common_failure("PoliticalDismissal2SourceOrder", row.first);
    changes.push_back(row.second.office_change_source.id);
    previous = static_cast<std::size_t>(at - order.begin());
    first = false;
  }
  for (const auto &row : x.politics.facts.rows())
    if (row.second.kind == "political_office_dismissed" &&
        !x.state.dismissals.find(row.first))
      return common_failure("PoliticalDismissal2SourceRequired", row.first);
  return common_success();
}
Outcome<PoliticalDismissalQuote2>
preview_political_dismissal2(const PoliticalDismissal2Context &x,
                             const StableId &actor,
                             const PoliticalDismissCommand &command) {
  auto valid = validate_political_dismissal2(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto read_runtime = x.runtime.read();
  valid = preview_politics_command(x.common, x.politics, x.politics_catalog,
                                   x.organizations, x.organization_catalog,
                                   x.economy, x.activity, actor, command,
                                   &read_runtime);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto now = read_organization_authority_now(x.common, &x.runtime.clock);
  if (!now.ok)
    return {false, {}, now.error};
  const auto *j = x.politics.jurisdictions.find(command.jurisdiction_id);
  const auto *office = j ? x.organizations.offices.find(j->office_id) : nullptr;
  if (!office || !office->holder_actor_id)
    return {false,
            {},
            common_failure("PoliticalDismissalActualTargetRequired",
                           command.jurisdiction_id)
                .error};
  return {true,
          {x.common.state().identity, std::move(now.value), command, actor,
           *office->holder_actor_id, x.common.state().next_instance_id},
          {}};
}
Outcome<PoliticsEffects> apply_political_dismissal2_in_candidate(
    CommonStore &common, PoliticsState &politics,
    OrganizationState &organizations, PoliticalDismissalSources2 &sources,
    const PoliticalDismissal2Context &x, const StableId &actor,
    const PoliticalDismissCommand &command,
    const PoliticalDismissalQuote2 &quote) {
  if (&common != &x.common || &politics != &x.politics ||
      &organizations != &x.organizations || &sources != &x.state)
    return {false,
            {},
            common_failure("PoliticalDismissal2CandidateMismatch").error};
  auto current = preview_political_dismissal2(x, actor, command);
  if (!current.ok)
    return {false, {}, current.error};
  auto moment = compare_world_moments(current.value.quoted_at, quote.quoted_at);
  if (!moment.ok)
    return {false, {}, moment.error};
  if (moment.value || !same_identity(current.value.identity, quote.identity) ||
      !same_command(command, quote.command) || quote.actor_id != actor ||
      quote.removed_actor_id != current.value.removed_actor_id ||
      quote.next_instance_id != current.value.next_instance_id)
    return {false, {}, common_failure("PoliticalDismissal2QuoteChanged").error};
  auto actual = apply_politics_command_in_transaction(
      common, politics, x.politics_catalog, organizations,
      x.organization_catalog, x.economy, x.activity, actor, command,
      &x.runtime);
  if (!actual.ok)
    return actual;
  if (actual.value.fact_ids.size() != 2 ||
      actual.value.receipt_ids != actual.value.fact_ids ||
      !actual.value.appointment_id.empty())
    return {false,
            {},
            common_failure("PoliticalDismissal2OriginalWriterMismatch").error};
  PoliticalDismissalRecord2 source;
  source.dismissal_source = {"politics", "political_office_dismissed",
                             actual.value.fact_ids[0]};
  source.office_change_source = {"organization", "organization_office_changed",
                                 actual.value.fact_ids[1]};
  source.authority_basis_ref = {source.dismissal_source, 1};
  source.jurisdiction_revision = command.expected_jurisdiction_revision;
  if (!sources.dismissals.insert(source.dismissal_source.id, source))
    return {false,
            {},
            common_failure("DuplicatePoliticalDismissal2Source",
                           source.dismissal_source.id)
                .error};
  auto valid = record(x, source.dismissal_source.id, source);
  if (!valid.ok)
    return {false, {}, valid.error};
  return actual;
}
} // namespace hanmo::resident
