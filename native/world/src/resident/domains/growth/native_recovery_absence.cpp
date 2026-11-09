#include "native_recovery_absence.hpp"
#include "resident/domains/actor_foundation/battle_injury_catalog_v1.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/injury_grade_effects_v3.hpp"
#include "resident/domains/actor_foundation/injury_original_results_v1.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1.hpp"
#include "resident/domains/actor_foundation/service_condition.hpp"
#include "resident/kernel/native_character_injury_state.hpp"
#include "resident/kernel/native_condition_application_world.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_bundle.hpp"

namespace hanmo::resident {
namespace {
// 原 Complete 目录和人物根必须成套；不能借另一目录的空表作为缺席证明。
Status roots(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) ||
      !w.catalog->actor_foundation || !w.actor_foundation ||
      !w.action_elapsed || !w.world_timeline ||
      !same_original_catalog_identity(w.catalog->identity,
                                      w.catalog->actor_foundation->identity))
    return common_failure("RecoveryAbsenceActualCompleteRootsRequired");
  return common_success();
}
// 尚无原 typed producer 的到期种类不能以孤 Common 回执冒作已执行。
bool due_kind(std::string_view kind) {
  return kind == "condition_due" || kind == "treatment_credit" ||
         kind == "condition_episode" || kind == "recovery_resolution";
}
bool recovery_kind(std::string_view kind) {
  return kind.starts_with("personal_recovery_") ||
         kind.starts_with("recovery_");
}
Status unavailable(const char *code, const StableId &id = {}) {
  return common_failure(code, id, ErrorKind::unsupported_capability);
}
} // namespace

Status
validate_native_condition_due_absence(const WorldBundle &w,
                                      const CharacterConditionSources &body) {
  auto valid = roots(w);
  if (!valid.ok)
    return valid;
  if (&body.common != &w.common || &body.state != &*w.actor_foundation ||
      &body.catalog != w.catalog->actor_foundation.get() ||
      body.growth != &w.growth ||
      body.growth_catalog != w.catalog->growth.get() ||
      body.shared != w.catalog->common.get() ||
      body.elapsed != &*w.action_elapsed)
    return common_failure("ConditionDueSameWorldSourcesRequired");
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value, body.actual_now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("ConditionDueActualMomentMismatch");
  std::size_t selected = 0;
  for (const auto &module : w.catalog->identity.module_versions)
    if (module.first == "character_condition_extensions") {
      ++selected;
      // 既有 1–4、具名看管 6 及仅扩原3来源的7均不拥有伤效三表；5 不能空表降级。
      if (module.second != "1" && module.second != "2" &&
          module.second != "3" && module.second != "4" && module.second != "6" &&
          module.second != "7")
        return unavailable("ConditionDueOriginalTypedProducerRequired");
    }
  if (selected != 1)
    return common_failure("ConditionDuePhysicalSelectionMismatch");
  const auto &catalog = *w.catalog->actor_foundation;
  if (catalog.injury_capability_map ||
      (catalog.original_military_wound_rule &&
       catalog.original_military_wound_rule->selected_applied))
    return unavailable("ConditionDueOriginalTypedProducerRequired");
  for (const auto &row :
       w.actor_foundation->actor_start_context.initial_condition_seeds.rows())
    if (!row.second.episodes.empty())
      return unavailable("ConditionDueOriginalTypedProducerRequired",
                         row.first);
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (due_kind(row.second.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.actions.rows())
    if (due_kind(row.second.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.history)
    if (due_kind(row.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.result_id);
  for (const auto &row : common.results.rows())
    if (row.second && due_kind(row.second->kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  valid = validate_native_condition_application_headers(w.common, *w.catalog,
                                                        &*w.actor_foundation);
  if (!valid.ok)
    return valid;
  // 唯一原身体链校验同时证明伤效/到期/恢复三表为空，以及全部真实变更反覆盖。
  return validate_complete_character_conditions(body);
}

// 原 map3 只产生由真实 Field/Personal 原收据决定的永久旧伤，没有日历到期
// writer。这里正向验证已选完整来源，而非把 Condition8 加入旧空表允许集。
Status validate_native_permanent_injury_calendar_sources(
    const WorldBundle &w, const CharacterConditionSources &body) {
  auto valid = roots(w);
  if (!valid.ok)
    return valid;
  const auto selected =
      selected_native_character_injury_source_version(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (selected.value != 1 ||
      !battle_injury_source1_selected(w.catalog->identity) ||
      !injury_grade_effects_v3_selected(*w.catalog->actor_foundation))
    return unavailable("ConditionDueOriginalTypedProducerRequired");
  if (!w.catalog->growth || !w.catalog->complete_exact_time_runtime ||
      !w.character_injury_sources || !w.field_battle)
    return unavailable("ConditionCalendarOriginalOwningRootsRequired");
  if (&body.common != &w.common || &body.state != &*w.actor_foundation ||
      &body.catalog != w.catalog->actor_foundation.get() ||
      body.growth != &w.growth ||
      body.growth_catalog != w.catalog->growth.get() ||
      body.shared != w.catalog->common.get() ||
      body.elapsed != &*w.action_elapsed ||
      body.injury_sources != &*w.character_injury_sources ||
      body.injury_field != &*w.field_battle ||
      body.injury_personal != &w.personal || body.military != &w.military ||
      body.personal_catalog != w.catalog->personal.get())
    return common_failure("ConditionDueSameWorldSourcesRequired");
  const auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto same = compare_world_moments(now.value, body.actual_now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("ConditionDueActualMomentMismatch");

  // 原 Complete 预置 long_rest rule 不是作者行动选择；只读实际注册与
  // 编译后的 role 活动。普通rest只接真实Time2完成/source1；治疗仍不能空表放行。
  // long_rest 的真实作者能力来自 GrowthCatalog.family；原 phase 已有其
  // writer，但本永久伤效日历闭包不冒称接通它的 source1 恢复/到期链。
  if (w.catalog->growth->family)
    return unavailable("ConditionDueOriginalTypedProducerRequired");
  bool service_selected = false;
  for (const auto &action :
       w.catalog->complete_exact_time_runtime->action_times) {
    if (action.owner_domain != "growth")
      continue;
    if (action.action_kind == "long_rest")
      return unavailable("ConditionDueOriginalTypedProducerRequired");
    service_selected = service_selected || action.action_kind == "service";
  }
  bool ordinary_rest_selected = false;
  if (service_selected)
    for (const auto &role : w.catalog->growth->roles.rows())
      if (role.second.activities.find("rest"))
        ordinary_rest_selected = true;
  if (ordinary_rest_selected) {
    valid = validate_original_injury_rest_service_sources(body);
    if (!valid.ok)
      return valid;
  }
  valid = validate_native_growth_recovery_absence(w);
  if (!valid.ok)
    return valid;
  for (const auto &row : w.character_injury_sources->applications.rows()) {
    if (std::holds_alternative<OriginalRestProvidedCause>(
            row.second.original_source)) {
      const auto *receipt =
          body.state.character_condition_extensions.applied_receipts.find(
              row.first);
      if (!ordinary_rest_selected || !receipt)
        return unavailable("ConditionDueOriginalTypedProducerRequired",
                           row.first);
      valid = validate_rest_injury_original_result_v1(
          body, *w.character_injury_sources, *receipt);
      if (!valid.ok)
        return valid;
    } else if (!std::holds_alternative<CharacterBattleReceiptSourceV1>(
                   row.second.original_source))
      return unavailable("ConditionDueOriginalTypedProducerRequired", row.first);
  }

  valid = validate_battle_injury_catalog_v1(*w.catalog->actor_foundation);
  if (!valid.ok)
    return valid;
  valid = validate_injury_grade_effects_map_v3(
      *w.catalog->actor_foundation->injury_capability_map, w.catalog->identity);
  if (!valid.ok)
    return valid;
  valid = validate_native_condition_application_headers(w.common, *w.catalog,
                                                        &*w.actor_foundation);
  if (!valid.ok)
    return valid;
  // 全验包括每名人物原链、原 Field/Personal producer 的反覆盖、永久 episode
  // stage_revision=0/no next_calendar_due 与原效果 reader，不能换成 prefix。
  valid = validate_complete_character_conditions(body);
  if (!valid.ok)
    return valid;
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (due_kind(row.second.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.actions.rows())
    if (due_kind(row.second.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.history)
    if (due_kind(row.kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.result_id);
  for (const auto &row : common.results.rows())
    if (row.second && due_kind(row.second->kind))
      return unavailable("ConditionDueRetainedSourceNotEmpty", row.first);
  return common_success();
}

Status validate_native_growth_recovery_absence(const WorldBundle &w) {
  auto valid = roots(w);
  if (!valid.ok)
    return valid;
  if (w.personal_recovery_enabled() || w.personal_recovery_seed_complete ||
      (w.catalog->actor_foundation->original_long_rest_rule &&
       w.catalog->actor_foundation->original_long_rest_rule
           ->personal_injury_recovery))
    return unavailable("GrowthRecoveryOriginalExactWriterRequired");
  // 原模块/目录/seed、services/credits/closures 和原事实/命令绑定正文仍唯一。
  valid = validate_growth_recovery_world(w);
  if (!valid.ok)
    return valid;
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (row.second.domain == "personal_injury_recovery" ||
        recovery_kind(row.second.kind))
      return unavailable("GrowthRecoveryRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.actions.rows())
    if (recovery_kind(row.second.kind))
      return unavailable("GrowthRecoveryRetainedSourceNotEmpty", row.first);
  for (const auto &row : common.history)
    if (recovery_kind(row.kind))
      return unavailable("GrowthRecoveryRetainedSourceNotEmpty", row.result_id);
  for (const auto &row : common.results.rows())
    if (row.second) {
      const auto *effect =
          row.second->details
              ? std::get_if<GrowthEffect>(&row.second->details->data)
              : nullptr;
      if (recovery_kind(row.second->kind) ||
          (effect && recovery_kind(effect->kind)))
        return unavailable("GrowthRecoveryRetainedSourceNotEmpty", row.first);
    }
  return common_success();
}
} // namespace hanmo::resident
