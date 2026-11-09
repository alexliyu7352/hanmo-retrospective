#include "organization_calendar.hpp"
#include "organization_original_runtime.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 原活动主表不按角色重新复制；只过滤本次原Opening的ID，避免generic腿先把市场置终态。
bool market_activity(const OrgActivity &a) {
  for (const auto &p : a.participation)
    if (p.role == "market_service_staffing")
      return true;
  return false;
}
Status
calendar_market_sources(const OrganizationCalendarWriteContext &context) {
  const auto &x = context.organization;
  if (bool(context.staffing) != bool(context.staffing_effects))
    return common_failure("OrganizationCalendarCandidateMismatch");
  if (context.staffing) {
    const auto &staffing = *context.staffing;
    if (&x.common != &staffing.common || &x.purchases != &staffing.purchases ||
        &x.economy != &staffing.economy ||
        &x.population != &staffing.population)
      return common_failure("OrganizationCalendarCandidateMismatch");
    return common_success();
  }
  if (!context.household_catalog || !context.retail_catalog)
    return common_failure("OrganizationPublicMarketSelectionCatalogRequired",
                          {}, ErrorKind::unsupported_capability);
  return validate_unselected_public_market_organization(
      x.common, x.organization, *context.household_catalog, x.purchases,
      x.population, *context.retail_catalog);
}
void append(OrganizationEffects &out, const OrganizationEffects &in) {
  out.fact_ids.insert(out.fact_ids.end(), in.fact_ids.begin(),
                      in.fact_ids.end());
  out.receipt_ids.insert(out.receipt_ids.end(), in.receipt_ids.begin(),
                         in.receipt_ids.end());
}
} // namespace
Outcome<OrganizationEffects> step_organization_calendar_activity_in_candidate(
    OrganizationCalendarWriteContext &context, const ExactPeriodContext &period,
    const StableId &id) {
  auto &x = context.organization;
  if (&x.common != &context.opening.common ||
      &x.organization != &context.opening.organization ||
      &x.economy != &context.opening.economy ||
      &x.runtime != &context.original_runtime.runtime ||
      !context.original_runtime.calendar)
    return {false,
            {},
            common_failure("OrganizationCalendarCandidateMismatch").error};
  auto market_sources = calendar_market_sources(context);
  if (!market_sources.ok)
    return {false, {}, market_sources.error};
  auto now = x.clock.read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto boundary = compare_world_moments(now.value, {period.through_tick, {}});
  if (!boundary.ok)
    return {false, {}, boundary.error};
  if (boundary.value != 0 ||
      period.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      period.through_tick != period.opening_at.half_day_tick + 1)
    return {
        false,
        {},
        common_failure("OrganizationCalendarBoundaryRequired", period.period_id)
            .error};
  auto original = read_economic_calendar_input(context.opening, period);
  if (!original.ok)
    return {false, {}, original.error};
  if (std::count(original.value.existing_activity_ids.begin(),
                 original.value.existing_activity_ids.end(), id) != 1)
    return {
        false,
        {},
        common_failure("OrganizationActivityNotInOriginalOpening", id).error};
  const auto *activity = x.organization.activities.find(id);
  if (!activity)
    return {false,
            {},
            common_failure("OrganizationOpeningActivityMissing", id).error};
  if (!market_activity(*activity)) {
    original.value.existing_activity_ids = {id};
    return step_organization_activities(
        x.common, x.organization, x.catalog, x.economy, x.activity, x.results,
        original.value, &x.clock, context.original_runtime.calendar,
        context.training, context.original_runtime.production_population);
  }
  const PublicMarketStaffingBinding *binding = nullptr;
  for (const auto &row : x.purchases.public_staffing_bindings.rows())
    if (row.second.original_org_activity_ref == id) {
      if (binding)
        return {
            false,
            {},
            common_failure("DuplicatePublicMarketActivityBinding", id).error};
      binding = &row.second;
    }
  if (!binding)
    return {false,
            {},
            common_failure("PublicMarketActivityBindingMissing", id).error};
  if (activity->status == OrgActivityStatus::completed ||
      activity->status == OrgActivityStatus::cancelled)
    return {true, {}, {}};
  if (activity->status != OrgActivityStatus::active)
    return {
        false, {}, common_failure("PublicMarketActivityNotActive", id).error};
  if (activity->due_tick < period.through_tick)
    return {false, {}, common_failure("MissedPublicMarketBoundary", id).error};
  if (activity->due_tick == period.through_tick)
    return end_public_market_staffing_in_candidate(
        x, *context.staffing, *context.staffing_effects, id);
  return {true, {}, {}};
}
Outcome<OrganizationEffects> step_organization_calendar_in_candidate(
    OrganizationCalendarWriteContext &context,
    const ExactPeriodContext &period) {
  auto &x = context.organization;
  if (&x.common != &context.opening.common ||
      &x.organization != &context.opening.organization ||
      &x.economy != &context.opening.economy)
    return {false,
            {},
            common_failure("OrganizationCalendarCandidateMismatch").error};
  auto market_sources = calendar_market_sources(context);
  if (!market_sources.ok)
    return {false, {}, market_sources.error};
  auto now = x.clock.read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  auto boundary = compare_world_moments(now.value, {period.through_tick, {}});
  if (!boundary.ok)
    return {false, {}, boundary.error};
  if (boundary.value != 0 ||
      period.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      period.through_tick != period.opening_at.half_day_tick + 1)
    return {
        false,
        {},
        common_failure("OrganizationCalendarBoundaryRequired", period.period_id)
            .error};
  auto opening = read_economic_calendar_input(context.opening, period);
  if (!opening.ok)
    return {false, {}, opening.error};
  DynamicStepInput original = std::move(opening.value);
  std::vector<StableId> ordinary;
  OrganizationEffects out;
  for (const auto &id : original.existing_activity_ids) {
    const auto *activity = x.organization.activities.find(id);
    if (!activity)
      return {false,
              {},
              common_failure("OrganizationOpeningActivityMissing", id).error};
    if (!market_activity(*activity)) {
      ordinary.push_back(id);
      continue;
    }
    const PublicMarketStaffingBinding *binding = nullptr;
    for (const auto &row : x.purchases.public_staffing_bindings.rows())
      if (row.second.original_org_activity_ref == id) {
        if (binding)
          return {
              false,
              {},
              common_failure("DuplicatePublicMarketActivityBinding", id).error};
        binding = &row.second;
      }
    if (!binding)
      return {false,
              {},
              common_failure("PublicMarketActivityBindingMissing", id).error};
    if (activity->status == OrgActivityStatus::completed ||
        activity->status == OrgActivityStatus::cancelled)
      continue;
    if (activity->status != OrgActivityStatus::active)
      return {
          false, {}, common_failure("PublicMarketActivityNotActive", id).error};
    if (activity->due_tick < period.through_tick)
      return {
          false, {}, common_failure("MissedPublicMarketBoundary", id).error};
    if (activity->due_tick == period.through_tick) {
      auto ended = end_public_market_staffing_in_candidate(
          x, *context.staffing, *context.staffing_effects, id);
      if (!ended.ok)
        return ended;
      append(out, ended.value);
    }
  }
  original.existing_activity_ids = std::move(ordinary);
  auto stepped = step_organization_activities(
      x.common, x.organization, x.catalog, x.economy, x.activity, x.results,
      original, &x.clock, context.original_runtime.calendar, context.training,
      context.original_runtime.production_population);
  if (!stepped.ok)
    return stepped;
  append(out, stepped.value);
  // 欠薪/成员原规则仍一次在原政治尾执行；新Office生命周期只接受K3原已应用来源。
  auto reconciled = reconcile_organization(
      x.common, x.organization, x.catalog, x.economy, x.activity,
      period.through_tick, &context.original_runtime);
  if (!reconciled.ok)
    return reconciled;
  append(out, reconciled.value);
  return {true, std::move(out), {}};
}
Outcome<WorldMoment> NativeOrganizationCalendarSourceMomentRead::
    read_organization_activity_source_moment(
        const OrganizationFact &supplied) const {
  const auto *original = state_.facts.find(supplied.id);
  const auto *head = common_.state().facts.find(supplied.id);
  if (!original || !head || head->domain != "organization" ||
      head->kind != original->kind || original->tick < 0)
    return {false,
            {},
            common_failure("OrganizationCalendarSourceMismatch", supplied.id)
                .error};
  const auto &fact = *original;
  if (fact.code == "public_market_staffing_offered" ||
      fact.code == "public_market_staffing_approved" ||
      fact.code == "public_market_staffing_term_ended" ||
      fact.code == "public_market_staffing_mutual_release")
    return market_.read_organization_activity_source_moment(fact);
  const auto *activity = state_.activities.find(fact.activity_id);
  if (!activity || activity->principal.kind != SubjectKind::organization ||
      activity->principal.id != fact.organization_id ||
      market_activity(*activity) ||
      !catalog_.activity_terms.find(activity->terms_id))
    return {false,
            {},
            common_failure("OrganizationCalendarActivitySourceMissing", fact.id)
                .error};
  const bool approval = fact.kind == "organization_activity_approved" ||
                        fact.kind == "population_training_approved";
  if (approval) {
    if ((catalog_.exact_fact_clock_enabled
             ? fact.tick > activity->started_tick
             : fact.tick != activity->started_tick) ||
        fact.office_id != activity->responsible_office_id ||
        fact.actor_id.empty() || !fact.cause_fact_ids.empty() ||
        !fact.code.empty() ||
        (fact.kind == "population_training_approved") !=
            (activity->kind == OrgActivityKind::population_training))
      return {false,
              {},
              common_failure("OrganizationCalendarApprovalMismatch", fact.id)
                  .error};
  } else if (fact.kind == "organization_activity_completed" ||
             fact.kind == "organization_activity_failed" ||
             fact.kind == "population_training_completed") {
    if (fact.tick < activity->due_tick ||
        std::find(activity->result_fact_ids.begin(),
                  activity->result_fact_ids.end(),
                  fact.id) == activity->result_fact_ids.end() ||
        activity->execution.last_receipt_id !=
            std::optional<StableId>{fact.id} ||
        activity->outcome_sequence != 1 || fact.code != activity->result_code ||
        (fact.kind == "population_training_completed") !=
            (activity->kind == OrgActivityKind::population_training))
      return {false,
              {},
              common_failure("OrganizationCalendarTerminalMismatch", fact.id)
                  .error};
  } else
    return {false,
            {},
            common_failure("UnsupportedOrganizationActivityTimeSource", fact.id,
                           ErrorKind::unsupported_capability)
                .error};
  if (!catalog_.exact_fact_clock_enabled || !fact.occurred_at ||
      fact.occurred_at->half_day_tick != fact.tick)
    return {
        false,
        {},
        common_failure("OrganizationExactFactSourceRequired", fact.id).error};
  return {true, *fact.occurred_at, {}};
}
} // namespace hanmo::resident
