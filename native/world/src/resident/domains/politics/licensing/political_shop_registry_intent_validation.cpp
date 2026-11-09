#include "political_shop_registry_intent_validation.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_shop_registry_intent_modules(
    bool enabled, const std::vector<std::pair<StableId, StableId>> &modules) {
  Integer own = 0, registry = 0, law = 0, shop = 0, takeover = 0;
  for (const auto &m : modules) {
    if (m.first == political_shop_registry_intents_module) {
      ++own;
      if (m.second != "1")
        return common_failure("UnsupportedPoliticalShopRegistryIntentVersion");
    }
    if (m.first == political_shop_registry_succession_module && m.second == "1")
      ++registry;
    if (m.first == political_shop_licensing_module && m.second == "1")
      ++law;
    if (m.first == "private_shops" && (m.second == "1" || m.second == "2"))
      ++shop;
    if (m.first == "political_takeover" && m.second == "1")
      ++takeover;
  }
  if (own != (enabled ? 1 : 0) ||
      (enabled && (registry != 1 || law != 1 || shop != 1 || takeover != 1)))
    return common_failure("PoliticalShopRegistryIntentProfileMismatch");
  return common_success();
}
Status validate_political_shop_registry_intent_director(
    const DirectorCatalog &catalog, const DirectorState &state,
    const PoliticalShopRegistryContext *x) {
  const auto *intents = catalog.shop_registry_intents.get();
  if (intents) {
    if (!x || x->legal.phase || x->control.phase)
      return common_failure("PoliticalShopRegistryIntentProfileRequired");
    auto v = validate_political_shop_registry_intents(
        *intents, x->legal.common_catalog, x->catalog);
    if (!v.ok)
      return v;
  }
  const auto &policies =
      catalog.dynamic_subjects ? catalog.policies : catalog.subjects;
  std::set<StableId> used;
  for (const auto &row : policies.rows()) {
    const auto &p = row.second;
    const bool selected =
        std::find(p.templates.begin(), p.templates.end(),
                  DirectorTemplate::shop_registry_acceptance) !=
        p.templates.end();
    if (selected != p.shop_registry_intent_id.has_value())
      return common_failure("InvalidPoliticalShopRegistryDirectorPolicy",
                            row.first);
    if (!selected)
      continue;
    const auto *intent =
        intents ? intents->intents.find(*p.shop_registry_intent_id) : nullptr;
    const auto *target =
        intent && x ? x->catalog.policies.find(intent->registry_policy_id)
                    : nullptr;
    if (!target || p.organization_id != target->organization_id ||
        (!catalog.dynamic_subjects && row.first != intent->actor_id))
      return common_failure("PoliticalShopRegistryDirectorIntentMismatch",
                            row.first);
    used.insert(intent->id);
  }
  if (intents && used.size() != intents->intents.size())
    return common_failure("PoliticalShopRegistryUnusedIntent");
  const auto check = [&](const auto &plans) -> Status {
    for (const auto &row : plans.rows()) {
      const auto &p = row.second;
      if (p.template_id != political_shop_registry_intent_template)
        continue;
      const auto *policy =
          policies.find(catalog.dynamic_subjects ? p.policy_id : p.subject.id);
      const auto *intent =
          policy && policy->shop_registry_intent_id && intents
              ? intents->intents.find(*policy->shop_registry_intent_id)
              : nullptr;
      const auto *target =
          intent && x ? x->catalog.policies.find(intent->registry_policy_id)
                      : nullptr;
      if (!target || p.subject.kind != SubjectKind::actor ||
          p.subject.id != intent->actor_id ||
          policy->organization_id != target->organization_id ||
          std::find(policy->templates.begin(), policy->templates.end(),
                    DirectorTemplate::shop_registry_acceptance) ==
              policy->templates.end() ||
          !p.commitment_ids.empty() || p.livelihood ||
          p.pending_life_action_id || p.last_life_fact_id ||
          (p.phase != PlanPhase::waiting && p.phase != PlanPhase::delivering &&
           p.phase != PlanPhase::completed))
        return common_failure("PoliticalShopRegistryDirectorPlanMismatch",
                              row.first);
      if (p.goal_ref == intent->id) {
        if (p.phase == PlanPhase::completed)
          return common_failure("PoliticalShopRegistryDirectorResultMissing",
                                row.first);
        continue;
      }
      const auto *court = x->legal.catalog.courts.find(target->court_id);
      const auto *registration =
          x->control.state.registrations.find(p.goal_ref);
      const auto *rule =
          registration ? x->control.catalog.rules.find(registration->rule_id)
                       : nullptr;
      if (!court || !rule || rule->jurisdiction_id != court->jurisdiction_id ||
          registration->successor_organization_id != target->organization_id ||
          p.current_action_id || p.phase == PlanPhase::delivering)
        return common_failure("PoliticalShopRegistryDirectorPlanSourceMissing",
                              row.first);
      if (p.phase != PlanPhase::completed)
        continue;
      bool found = false;
      for (const auto &accepted : x->state.acceptances.rows())
        if (accepted.second.registration_fact_id == registration->id &&
            accepted.second.policy_id == target->id &&
            accepted.second.authority.actor_id == p.subject.id &&
            accepted.second.authority.organization_id ==
                target->organization_id)
          found = true;
      if (!found)
        return common_failure("PoliticalShopRegistryDirectorResultMissing",
                              row.first);
    }
    return common_success();
  };
  auto checked = check(state.plans);
  return checked.ok ? check(state.archived_plans) : checked;
}
} // namespace hanmo::resident
