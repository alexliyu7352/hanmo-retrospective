// 原撤职业务仍唯一改变Office；本层只保存本次签权，任何末失败完整回滚两原账。
#include "political_dismissal_rules.hpp"
#include "resident/domains/politics/politics_internal.hpp"
namespace hanmo::resident {
namespace {
const PoliticalDismissalSourcePolicy *
policy_for(const PoliticalDismissalSourceCatalog &c,
           const StableId &jurisdiction) {
  for (const auto &row : c.policies.rows())
    if (row.second.jurisdiction_id == jurisdiction)
      return &row.second;
  return nullptr;
}
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
} // namespace
Outcome<PoliticalDismissalSourceQuote>
preview_political_dismissal_source(const PoliticalDismissalContext &x,
                                   const StableId &actor,
                                   const PoliticalDismissCommand &command) {
  const auto fail =
      [&](const StableId &code) -> Outcome<PoliticalDismissalSourceQuote> {
    return {false, {}, common_failure(code, actor).error};
  };
  auto valid = validate_political_dismissal_sources_complete(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (actor != x.common.state().metadata.player_actor_id ||
      command.authority.actor_id != actor)
    return fail("PoliticalDismissalPlayerOnly");
  // 撤职预览借原写候选的只读视图，后续写入仍使用原完整上下文。
  std::optional<PoliticalOrganizationRuntimeReadContext> read_runtime;
  if (x.organization_runtime)
    read_runtime.emplace(x.organization_runtime->read());
  valid = preview_politics_command(x.common, x.politics, x.politics_catalog,
                                   x.organizations, x.organization_catalog,
                                   x.economy, x.activity, actor, command,
                                   read_runtime ? &*read_runtime : nullptr);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *p = policy_for(x.catalog, command.jurisdiction_id);
  if (!p)
    return fail("PoliticalDismissalJurisdictionNotCovered");
  valid = validate_political_dismissal_current_authority(
      x.common, x.politics, x.politics_catalog, x.organizations,
      x.organization_catalog, *p, command);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto now = read_organization_authority_now(
      x.common,
      x.organization_runtime ? &x.organization_runtime->clock : nullptr);
  if (!now.ok)
    return {false, {}, now.error};
  valid = x.seats(*p, now.value.half_day_tick, {});
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = politics_detail::available(x.common, x.activity, actor);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = politics_detail::political_adult(
      x.common, actor, read_runtime ? &*read_runtime : nullptr);
  if (!valid.ok)
    return {false, {}, valid.error};
  OrganizationAccess organizations(x.common, x.organizations,
                                   x.organization_catalog, x.economy);
  if (!organizations.actor_accepts_duty(actor,
                                        command.authority.organization_id))
    return fail("PoliticalDismissalMemberDeclinesDuty");
  auto living =
      read_actor_presence(x.common, x.common_catalog, x.death_origin, actor);
  if (!living.ok)
    return {false, {}, living.error};
  if (!living.value.alive)
    return fail("PoliticalDismissalLivingSignerRequired");
  const auto *j = x.politics.jurisdictions.find(p->jurisdiction_id);
  const auto *office = j ? x.organizations.offices.find(j->office_id) : nullptr;
  if (!office || !office->holder_actor_id)
    return fail("PoliticalDismissalActualTargetRequired");
  if (x.state.dismissals.size() >= 4096)
    return fail("PoliticalDismissalSourceLimit");
  return {true,
          {x.common.state().identity, command, actor, p->id,
           *office->holder_actor_id, x.common.state().next_instance_id},
          {}};
}
Outcome<PoliticsEffects> apply_political_dismissal_with_source_in_candidate(
    CommonStore &common, PoliticsState &politics,
    OrganizationState &organizations, PoliticalDismissalSourceState &sources,
    const PoliticalDismissalContext &x, const StableId &actor,
    const PoliticalDismissCommand &command,
    const PoliticalDismissalSourceQuote &quote) {
  const auto fail = [](const StableId &code) -> Outcome<PoliticsEffects> {
    return {false, {}, common_failure(code).error};
  };
  if (&common != &x.common || &politics != &x.politics ||
      &organizations != &x.organizations || &sources != &x.state)
    return fail("PoliticalDismissalCandidateMismatch");
  auto fresh = preview_political_dismissal_source(x, actor, command);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  const auto &q = fresh.value;
  if (!same_identity(quote.identity, q.identity) ||
      !same_command(quote.command, command) || quote.actor_id != actor ||
      quote.policy_id != q.policy_id ||
      quote.removed_actor_id != q.removed_actor_id ||
      quote.next_instance_id != q.next_instance_id)
    return fail("PoliticalDismissalQuoteChanged");
  auto &next_politics = politics;
  auto &next_organizations = organizations;
  auto &next_sources = sources;
  auto actual = apply_politics_command_in_transaction(
      common, next_politics, x.politics_catalog, next_organizations,
      x.organization_catalog, x.economy, x.activity, actor, command,
      x.organization_runtime);
  if (!actual.ok)
    return actual;
  if (q.next_instance_id > std::numeric_limits<Integer>::max() - 2 ||
      common.state().next_instance_id != q.next_instance_id + 2 ||
      !actual.value.appointment_id.empty() ||
      actual.value.fact_ids.size() != 2 ||
      actual.value.receipt_ids != actual.value.fact_ids)
    return fail("PoliticalDismissalOriginalWriterMismatch");
  const auto *p = x.catalog.policies.find(q.policy_id);
  PoliticalDismissalSource src{
      q.policy_id,
      actual.value.fact_ids[0],
      actual.value.fact_ids[1],
      p->grant_id,
      q.removed_actor_id,
      command.authority,
      next_politics.facts.find(actual.value.fact_ids[0])->tick,
      command.expected_jurisdiction_revision,
      command.expected_office_epoch};
  const StableId source_id = src.dismissal_fact_id;
  if (!next_sources.dismissals.insert(source_id, {std::move(src), {}}))
    return fail("PoliticalDismissalDuplicateSource");
  PoliticalDismissalContext after{common,
                                  x.common_catalog,
                                  next_politics,
                                  x.politics_catalog,
                                  next_organizations,
                                  x.organization_catalog,
                                  x.economy,
                                  x.activity,
                                  x.catalog,
                                  next_sources,
                                  x.death_origin,
                                  x.seats,
                                  x.player_before,
                                  x.result_matches,
                                  x.organization_runtime};
  auto valid = validate_political_dismissal_sources(after);
  if (!valid.ok)
    return {false, {}, valid.error};
  return actual;
}
Outcome<PoliticsEffects> apply_political_dismissal_with_source(
    CommonStore &common, PoliticsState &politics,
    OrganizationState &organizations, PoliticalDismissalSourceState &sources,
    const PoliticalDismissalContext &x, const StableId &actor,
    const PoliticalDismissCommand &command,
    const PoliticalDismissalSourceQuote &quote) {
  if (&common != &x.common || &politics != &x.politics ||
      &organizations != &x.organizations || &sources != &x.state)
    return {
        false, {}, common_failure("PoliticalDismissalCandidateMismatch").error};
  local_economy_detail::PhaseSavepoint guard(common);
  if (!guard.token().ok)
    return {false, {}, guard.token().error};
  auto next_politics = politics;
  auto next_organizations = organizations;
  auto next_sources = sources;
  std::optional<OrganizationRuntime> next_runtime;
  std::optional<PoliticalOrganizationRuntimeWriteContext> runtime;
  if (x.organization_runtime) {
    next_runtime.emplace(x.organization_runtime->runtime);
    runtime.emplace(PoliticalOrganizationRuntimeWriteContext{
        *next_runtime, x.organization_runtime->clock,
        x.organization_runtime->read_stamp, x.organization_runtime->transitions,
        x.organization_runtime->qualifications});
  }
  PoliticalDismissalContext candidate{common,
                                      x.common_catalog,
                                      next_politics,
                                      x.politics_catalog,
                                      next_organizations,
                                      x.organization_catalog,
                                      x.economy,
                                      x.activity,
                                      x.catalog,
                                      next_sources,
                                      x.death_origin,
                                      x.seats,
                                      x.player_before,
                                      x.result_matches,
                                      runtime ? &*runtime : nullptr};
  auto actual = apply_political_dismissal_with_source_in_candidate(
      common, next_politics, next_organizations, next_sources, candidate, actor,
      command, quote);
  if (!actual.ok)
    return actual;
  auto committed = guard.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  politics = std::move(next_politics);
  organizations = std::move(next_organizations);
  sources = std::move(next_sources);
  if (runtime)
    x.organization_runtime->runtime = std::move(*next_runtime);
  return actual;
}
} // namespace hanmo::resident
