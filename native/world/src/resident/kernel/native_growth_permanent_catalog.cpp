#include "resident/kernel/catalog_section_observation.hpp"
#include "native_character_injury_state.hpp"
#include "native_growth_permanent_catalog.hpp"
#include "exact_time_profile.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
namespace {
Status growth_format_mismatch(const StableId &domain, const StableId &observed,
                              const StableId &expected) {
  auto error = common_failure("GrowthFormatVersionUnsupported", domain,
                              ErrorKind::unsupported_capability)
                   .error;
  error.domain = domain;
  error.reason = "observed=" + observed + ", expected=" + expected;
  return {false, {}, std::move(error)};
}
} // namespace

// 仅 phase4 可以安装这一显式后继；Condition/2 留给原公益 wrapper。
Status
validate_native_growth_permanent_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 139, "PreparedCatalog", "validate_native_growth_permanent_catalog", &catalog);

  if (!catalog.growth || !catalog.common)
    return common_failure("GrowthPermanentCatalogRequired");
  const bool selected = catalog.growth->complete_capability_application_chain;
  auto injury =
      selected_native_character_injury_source_version(catalog.identity);
  if (!injury.ok)
    return {false, {}, std::move(injury.error)};
  if (injury.value && !selected)
    return common_failure("CharacterInjuryCompleteGrowth4Required");
  std::size_t permanent = 0, condition = 0;
  for (const auto &row : catalog.identity.module_versions) {
    if (row.first == "growth_permanent_contributions") {
      if (++permanent != 1)
        return common_failure("GrowthPermanentProfileMismatch");
      if (row.second != "3")
        return growth_format_mismatch(row.first, row.second, "3");
    }
    if (row.first == "character_condition_extensions") {
      if (++condition != 1)
        return common_failure("GrowthConditionProfileMismatch");
      if (row.second != (injury.value ? "8" : selected ? "7" : "1"))
        return growth_format_mismatch(row.first, row.second,
                                      injury.value ? "8"
                                      : selected   ? "7"
                                                   : "1");
    }
  }
  if (!selected) {
    if (permanent || catalog.growth_permanent)
      return common_failure("UnexpectedGrowthPermanentCatalog");
    return common_success();
  }
  if (permanent != 1 || condition != 1 || !catalog.growth_permanent ||
      !catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.growth->complete_study_sources ||
      !catalog.growth->complete_exact_period_sources ||
      !catalog.growth->complete_exact_command_cuts ||
      !catalog.growth->complete_exact_rest_mouth_sources)
    return common_failure("GrowthPermanentSelectedCatalogPairRequired");
  auto phase = read_complete_growth_phase_version(*catalog.exact_time);
  if (!phase.ok)
    return {false, {}, std::move(phase.error)};
  if (phase.value != 4)
    return common_failure("GrowthCapabilityPhase4Required");
  const auto &actual = catalog.growth_permanent->identity;
  const auto &locked = catalog.identity;
  if (actual.content_id != locked.content_id ||
      actual.content_version != locked.content_version ||
      actual.content_digest != locked.content_digest ||
      actual.ruleset_id != locked.ruleset_id ||
      actual.schema_sha256 != locked.schema_sha256 ||
      actual.pack_set_digest != locked.pack_set_digest ||
      actual.artifact_sha256 != locked.artifact_sha256 ||
      actual.module_versions != locked.module_versions)
    return common_failure("GrowthPermanentLockedCatalogMismatch");
  return validate_growth_permanent_catalog(*catalog.growth_permanent,
                                           *catalog.common);
}
// 两个 data 字典可含未选规则；只有显式 scenario 列表被原 typed decoder 采用。
Status prepare_native_growth_permanent_catalog(const BoundaryValue &data,
                                               const BoundaryValue &scenario,
                                               PreparedCatalog &catalog) {
  try {
    using namespace boundary;
    if (catalog.growth_permanent)
      return common_failure("GrowthPermanentCatalogAlreadyPrepared");
    if (!catalog.growth || !catalog.common)
      return common_failure("GrowthPermanentCatalogRequired");
    const BoundaryValue *config = nullptr;
    for (const auto &row : object(scenario, "scenario"))
      if (row.first == "growth_permanent_contributions") {
        if (config)
          fail("DuplicateGrowthPermanentSelection",
               "scenario/growth_permanent_contributions");
        config = &row.second;
      }
    const bool selected = catalog.growth->complete_capability_application_chain;
    if (bool(config) != selected)
      return common_failure("GrowthPermanentExplicitSelectionRequired");
    if (!selected)
      return validate_native_growth_permanent_catalog(catalog);
    const BoundaryValue *practice = nullptr;
    const BoundaryValue *milestones = nullptr;
    for (const auto &row : object(data, "data")) {
      if (row.first == "practice_award_rules") {
        if (practice)
          fail("DuplicateGrowthPermanentDefinitionTable", row.first);
        practice = &row.second;
      }
      if (row.first == "attribute_milestone_rules") {
        if (milestones)
          fail("DuplicateGrowthPermanentDefinitionTable", row.first);
        milestones = &row.second;
      }
    }
    if (!practice || !milestones)
      return common_failure("GrowthPermanentAuthorDefinitionsRequired");
    auto decoded = decode_growth_permanent_catalog(
        *practice, *milestones, *config, catalog.identity, *catalog.common);
    if (!decoded.ok)
      return {false, {}, std::move(decoded.error)};
    catalog.growth_permanent =
        checkpoint_make_shared<const GrowthPermanentCatalog>(
            std::move(decoded.value));
    return validate_native_growth_permanent_catalog(catalog);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
