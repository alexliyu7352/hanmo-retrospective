#include "native_complete_director_sources.hpp"
#include "economic_supply_policy_integration.hpp"
#include "native_political_world_scope.hpp"
#include "native_social_selection.hpp"
#include "native_task_work_catalog.hpp"
#include "resident/domains/director/native_director_world_scope.hpp"
#include "resident/domains/local_economy/household_world_scope.hpp"
#include "resident/domains/local_economy/market_history/economic_fifo_world_scope.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
Status finite_policy(const DirectorPolicy &p) {
  for (auto t : p.templates)
    if (t != DirectorTemplate::private_livelihood)
      return common_failure("CompleteDirectorOriginalPolicySourcesRequired",
                            director_template_id(t),
                            ErrorKind::unsupported_capability);
  if (p.named_membership_policy_id || !p.recruitment_posts.empty() ||
      p.political_career || p.diplomatic_response ||
      p.diplomatic_delegation_policy_id || p.supply_renewal_intent_id ||
      p.shop_registry_intent_id || p.governance_intent_id ||
      p.governance_agency_intent_id || p.office_resignation_response_intent_id)
    return common_failure("CompleteDirectorOriginalPolicySourcesRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
// 选择门读真目录，反覆盖读原表；不是以某一空表替代缺失 provider。
Status finite_sources(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->director)
    return common_failure("CompleteDirectorOriginalCatalogRequired");
  const auto &d = *w.catalog->director;
  if (d.governance_intents || d.governance_agency_intents ||
      d.office_resignation_responses || d.subordinate_appointments ||
      d.shop_registry_intents || d.personal_appointment_attendance ||
      d.visit_reception || d.supply_renewals || d.military_appointments ||
      d.field_appointments || d.diplomatic_response ||
      d.diplomatic_delegation || d.treaty_passage ||
      d.authored_task_continuation)
    return common_failure("CompleteDirectorOriginalSelectedSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  for (const auto &r : d.subjects.rows()) {
    auto valid = finite_policy(r.second);
    if (!valid.ok)
      return valid;
  }
  for (const auto &r : d.policies.rows()) {
    auto valid = finite_policy(r.second);
    if (!valid.ok)
      return valid;
  }
  for (const auto &r : d.admissions.rows())
    if (r.second.kind != DirectorAdmissionKind::all_actors)
      return common_failure("CompleteDirectorOriginalAdmissionSourcesRequired",
                            r.first, ErrorKind::unsupported_capability);
  auto work_layout = validate_native_task_work_layout(*w.catalog, w.tasks);
  if (!work_layout.ok)
    return work_layout;
  auto work = selected_native_task_work_version(w.catalog->identity);
  if (!work.ok)
    return {false, {}, std::move(work.error)};
  if (work.value)
    return common_failure("CompleteDirectorOriginalWorkScopeRequired", {},
                          ErrorKind::unsupported_capability);
  auto social = validate_native_social_state_selection(w.social, *w.catalog);
  if (!social.ok)
    return social;
  auto propagation =
      selected_native_social_propagation_cut(w.catalog->identity);
  if (!propagation.ok)
    return {false, {}, std::move(propagation.error)};
  if (propagation.value)
    return common_failure("CompleteDirectorOriginalPropagationScopeRequired",
                          {}, ErrorKind::unsupported_capability);
  if (w.social.dynamic && w.social.dynamic->time2 &&
      (!w.social.dynamic->time2->propagation_decisions.empty() ||
       !w.social.dynamic->time2->propagation_cooldowns.empty()))
    return common_failure("CompleteDirectorUnexpectedPropagationSource");
  for (const auto &r : w.common.state().facts.rows())
    if (r.second.kind == "propagation_decision")
      return common_failure("CompleteDirectorUnexpectedPropagationSource",
                            r.first);
  if (w.catalog->economic_supply_policies_enabled)
    return common_failure("CompleteDirectorOriginalIssuedSupplyScopeRequired",
                          {}, ErrorKind::unsupported_capability);
  return validate_economic_supply_world(w);
}
Outcome<NativeDirectorWorldScopeDependencies>
dependencies(const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
             const NativeCompleteDomainPrerequisiteOwner &original,
             const WorldMoment &at) {
  if (&core.characters.early.world != &w || !original.bound_to(w))
    return {
        false, {core}, common_failure("CompleteDirectorWorldMismatch").error};
  auto valid = finite_sources(w);
  if (!valid.ok)
    return {false, {core}, std::move(valid.error)};
  NativePoliticalWorldLateRead late;
  const auto &installed = original.sources().installed_original;
  late.installation = installed ? &installed->installation() : nullptr;
  auto political = NativePoliticalWorldScope::create_read(w, core, at, late);
  if (!political.ok)
    return {false, {core}, std::move(political.error)};
  NativeDirectorWorldScopeDependencies out{core};
  out.authority_owner = std::shared_ptr<const PoliticalExactReadLease>(
      political.value, &political.value->political_read_lease());
  if (w.catalog->household_purchases) {
    NativeHouseholdWorldLateSources household_late;
    household_late.transport_business =
        core.characters.early.transport_business;
    const auto &market = original.sources().market_original;
    if (market) {
      valid = market->validate_bindings();
      if (!valid.ok)
        return {false, {core}, std::move(valid.error)};
      if (!market->bound_to(w))
        return {false,
                {core},
                common_failure("CompleteDirectorMarketWorldMismatch").error};
      household_late.actual_market =
          std::shared_ptr<const NativeEconomicFifoRead>(market,
                                                        &market->fifo());
    }
    auto household =
        NativeHouseholdWorldScope::create(core, std::move(household_late));
    if (!household.ok)
      return {false, {core}, std::move(household.error)};
    auto owner = checkpoint_share(std::move(household.value));
    out.household = std::shared_ptr<const NativeHouseholdServiceLease>(
        owner, &owner->service());
  }
  return {true, std::move(out), {}};
}
} // namespace
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_director_read_scope(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    const NativeCompleteDomainPrerequisiteOwner &original,
    const WorldMoment &at) {
  auto d = dependencies(w, core, original, at);
  if (!d.ok)
    return {false, {}, std::move(d.error)};
  return make_native_director_world_read_scope(w, std::move(d.value), at);
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_director_write_scope(
    WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    NativeCompleteDomainPrerequisiteOwner &original,
    CompleteExactWorldBindingOwner &complete, const WorldMoment &at,
    ExactPhaseWriteContext *, const DynamicStepInput *phase) {
  if (&core.characters.early.complete != &complete || phase)
    return {false,
            {},
            common_failure("CompleteDirectorActualReviewScopeRequired").error};
  auto d = dependencies(w, core, original, at);
  if (!d.ok)
    return {false, {}, std::move(d.error)};
  return make_native_director_world_write_scope(w, {std::move(d.value)}, at);
}
} // namespace hanmo::resident
