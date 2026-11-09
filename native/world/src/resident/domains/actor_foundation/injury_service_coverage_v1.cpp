#include "injury_service_coverage_v1.hpp"
#include "character_condition_read.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_recovery_model.hpp"
#include "resident/domains/growth/growth_recovery_qualification.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status
validate_injury_recovery_selection_v1(const CharacterConditionSources &body) {
  if (!body.growth || !body.shared || !body.growth_catalog)
    return common_failure("CharacterInjuryServiceOwningCatalogRequired");
  std::size_t selected = 0;
  for (const auto &module : body.catalog.identity.module_versions)
    if (module.first == "personal_injury_recovery") {
      if (++selected != 1 || module.second != "1")
        return common_failure("CharacterInjuryRecoverySelectionMismatch");
    }
  if (bool(selected) != bool(body.injury_recovery_catalog))
    return common_failure("CharacterInjuryRecoverySelectionMismatch");
  if (selected) {
    auto catalog = validate_growth_recovery_catalog(
        *body.shared, *body.growth_catalog, *body.injury_recovery_catalog);
    if (!catalog.ok)
      return catalog;
  }
  // source1只借服务资格目录，积分归原应用表。原逐期credit/closed属于
  // 另一旧拥有者；即使服务目录已选也不能并存，不能用空表或普通Rest掩盖。
  for (const auto &row : body.common.state().facts.rows())
    if (row.second.domain == "personal_injury_recovery" ||
        row.second.kind == "personal_recovery_credit" ||
        row.second.kind == "personal_recovery_closed")
      return common_failure(selected ? "CharacterInjuryMixedRecoveryOwners"
                                     : "PersonalInjuryRecoveryUnavailable",
                            row.first);
  return common_success();
}
Status validate_injury_service_source_coverage_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources) {
  auto valid = validate_injury_recovery_selection_v1(body);
  if (!valid.ok)
    return valid;
  valid = validate_character_injury_source_links_v1(body.common, body.state,
                                                    body.catalog, sources);
  if (!valid.ok)
    return valid;
  // 原服务资格目录决定休养/治疗分支，应用表不得自行挑选减一级或给点。
  for (const auto &row : sources.applications.rows()) {
    const StableId *action = nullptr;
    const auto *rest =
        std::get_if<OriginalRestProvidedCause>(&row.second.original_source);
    const auto *treatment = std::get_if<CharacterTreatmentActionSourceV1>(
        &row.second.original_source);
    if (rest)
      action = &rest->action_id;
    else if (treatment)
      action = &treatment->action_id;
    else
      continue;
    const auto *activity = body.growth->life_activities.find(*action);
    if (!activity)
      return common_failure("CharacterInjuryServiceOriginalMissing", *action);
    const bool qualified = body.injury_recovery_catalog &&
                           body.injury_recovery_catalog->service_roles.find(
                               activity->context.role_id) != nullptr;
    if (qualified != bool(treatment))
      return common_failure("CharacterInjuryServiceMethodOwnerMismatch",
                            *action);
    const auto *receipt =
        body.state.character_condition_extensions.applied_receipts.find(
            row.first);
    if (!receipt)
      return common_failure("CharacterInjuryServiceReceiptMissing", row.first);
    valid =
        treatment
            ? validate_treatment_injury_original_result_v1(body, sources,
                                                           *receipt)
            : validate_rest_injury_original_result_v1(body, sources, *receipt);
    if (!valid.ok)
      return valid;
  }
  // 原完成表是覆盖枚举源，不能空应用表冒充所有休养/治疗已接通。
  for (const auto &row : body.growth->life_activities.rows()) {
    const auto &activity = row.second;
    if (activity.view.kind != LifeActivityKind::service ||
        activity.view.activity != "rest" ||
        activity.view.phase != LifeActivityPhase::completed ||
        !activity.view.success || !activity.view.code.empty())
      continue;
    const bool qualified = body.injury_recovery_catalog &&
                           body.injury_recovery_catalog->service_roles.find(
                               activity.context.role_id) != nullptr;
    std::size_t matches = 0;
    for (const auto &app : sources.applications.rows()) {
      const auto *rest =
          std::get_if<OriginalRestProvidedCause>(&app.second.original_source);
      const auto *treatment = std::get_if<CharacterTreatmentActionSourceV1>(
          &app.second.original_source);
      const StableId *id = rest        ? &rest->action_id
                           : treatment ? &treatment->action_id
                                       : nullptr;
      if (id && *id == row.first &&
          app.second.actor_id == activity.view.actor_id) {
        if (bool(treatment) != qualified)
          return common_failure("CharacterInjuryServiceMethodOwnerMismatch",
                                row.first);
        ++matches;
      }
    }
    if (matches != 1)
      return common_failure("CharacterInjuryServiceCompletionCoverageMismatch",
                            row.first);
  }
  return common_success();
}
} // namespace hanmo::resident
