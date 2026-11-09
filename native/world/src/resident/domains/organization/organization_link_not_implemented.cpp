// 原完整组织入口：逐一校验同候选身体、生命周期、权源、市场与日历；任何真源拒绝原样返回。
#include "membership_office_transition.hpp"
#include "organization_original_runtime.hpp"
#include "organization_service_effects_codec.hpp"
#include "organization_validation_detail.hpp"
#include "resident/kernel/read_stamp.hpp"

namespace hanmo::resident {
namespace {
// 不把同 tick 当精确时刻相等，也不从请求或 Common.tick 补造发生时刻。
Status same_complete_moment(const WorldMoment &left, const WorldMoment &right) {
  auto compared = compare_world_moments(left, right);
  if (!compared.ok)
    return {false, {}, compared.error};
  return compared.value == 0
             ? common_success()
             : common_failure("OrganizationCompleteClockMismatch");
}
// 事实头与 payload 共用实际发生时刻；冷读不得接受未来源或重新推算来源时间。
Status exact_facts(const OrganizationState &state, const WorldMoment &now) {
  for (const auto &row : state.facts.rows()) {
    const auto &fact = row.second;
    if (!fact.occurred_at || fact.tick != fact.occurred_at->half_day_tick)
      return common_failure("OrganizationExactFactMomentRequired", row.first);
    auto before = compare_world_moments(*fact.occurred_at, now);
    if (!before.ok)
      return {false, {}, before.error};
    if (before.value > 0)
      return common_failure("FutureOrganizationFact", row.first);
    const WorldMoment *source =
        fact.office_change_payload
            ? &fact.office_change_payload->effective_moment
        : fact.grant_change_payload
            ? &fact.grant_change_payload->effective_moment
            : nullptr;
    if (source) {
      auto same = same_complete_moment(*fact.occurred_at, *source);
      if (!same.ok)
        return same;
    }
  }
  return common_success();
}
} // namespace

// 唯一调用者政治完整 phase 随后另核 named membership、政治身体及
// Time2；这里不另造其账。
Status validate_complete_organization(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const DynamicResultRead *results, const ActivityRead *activities,
    const OrganizationResourceHistoryRead &history,
    const OrganizationProspectiveGrantRead &prospective,
    const OrganizationCompleteValidationContext &context) {
  const auto &market = context.market;
  const auto *staffing = context.staffing;
  if (&market.common != &common || &market.organization != &state ||
      &market.catalog != &catalog || &market.runtime != &context.runtime ||
      &market.economy != &economy ||
      (staffing &&
       (&staffing->common != &common || &staffing->economy != &economy ||
        &staffing->population != &market.population ||
        &staffing->purchases != &market.purchases)))
    return common_failure("OrganizationCompleteCandidateMismatch");
  if (!staffing) {
    if (!context.household_catalog || !context.retail_catalog)
      return common_failure("OrganizationPublicMarketSelectionCatalogRequired",
                            {}, ErrorKind::unsupported_capability);
    auto unselected = validate_unselected_public_market_organization(
        common, state, *context.household_catalog, market.purchases,
        market.population, *context.retail_catalog);
    if (!unselected.ok)
      return unselected;
  }
  if (!catalog.enabled || !catalog.authority_lifecycle_enabled ||
      !catalog.exact_fact_clock_enabled)
    return common_failure("OrganizationCompleteExactProfileRequired");
  auto valid = validate_organization_runtime_codec_selection(
      market.catalog_identity, catalog);
  if (!valid.ok)
    return valid;
  const auto &stamp = context.office_sources.stamp;
  if (stamp.mode() != ReadMode::phase_current &&
      stamp.mode() != ReadMode::committed_current)
    return common_failure("OrganizationCompleteCurrentStampRequired");
  valid = context.stamp_authority.validate(
      stamp, common.state().identity, stamp.mode(), common, context.versions);
  if (!valid.ok)
    return valid;
  auto now = read_organization_authority_now(common, &context.clock);
  if (!now.ok)
    return {false, {}, now.error};
  auto market_now = market.clock.read_organization_authority_now();
  if (!market_now.ok)
    return {false, {}, market_now.error};
  valid = same_complete_moment(now.value, market_now.value);
  if (!valid.ok)
    return valid;
  auto authority_now =
      context.authority_sources.read_organization_authority_now();
  if (!authority_now.ok)
    return {false, {}, authority_now.error};
  valid = same_complete_moment(now.value, authority_now.value);
  if (!valid.ok)
    return valid;
  if (staffing) {
    valid = same_complete_moment(now.value, staffing->now);
    if (!valid.ok)
      return valid;
  }
  auto lower = compare_world_moments(stamp.span().from, now.value);
  if (!lower.ok)
    return {false, {}, lower.error};
  auto upper = compare_world_moments(now.value, stamp.span().to);
  if (!upper.ok)
    return {false, {}, upper.error};
  if (lower.value > 0 || upper.value > 0)
    return common_failure("OrganizationCompleteStampMomentMismatch");

  valid = organization_detail::validate_organization_body(
      common, state, catalog, economy, results, activities, history,
      prospective, &context.office_sources, &context);
  if (!valid.ok)
    return valid;
  valid = exact_facts(state, now.value);
  if (!valid.ok)
    return valid;
  valid = validate_organization_runtime_sources(
      common, state, context.runtime, catalog, context.lifecycle_catalog,
      market.catalog_identity, stamp, context.lifecycle_sources,
      context.relation_sources, context.office_sources.transitions,
      context.office_sources.deaths, context.authority_sources, context.budgets,
      now.value);
  if (!valid.ok)
    return valid;
  valid = validate_organization_original_authorities(
      common, state, catalog, context.runtime, context.clock);
  if (!valid.ok)
    return valid;
  if (catalog.named_membership_enabled) {
    valid = validate_membership_office_transitions(
        common, state, catalog, context.runtime.office_transition_sources,
        context.clock, stamp);
    if (!valid.ok)
      return valid;
  }
  if (staffing) {
    valid = validate_public_market_organization(market, *staffing);
    if (!valid.ok)
      return valid;
  }
  valid = validate_organization_calendar_windows(
      state, catalog, context.calendar, context.training);
  if (!valid.ok)
    return valid;
  return validate_organization_service_effects(
      state, catalog.service_effects_enabled ? 1 : 0);
}
} // namespace hanmo::resident
