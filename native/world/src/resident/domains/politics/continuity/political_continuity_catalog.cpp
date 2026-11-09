#include "resident/kernel/catalog_section_observation.hpp"
#include "political_continuity_catalog.hpp"
#include "political_continuity_codec.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status
validate_political_continuity_catalog(const PoliticalContinuityCatalog &catalog,
                                      const CatalogIdentity &actual) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 88, "PoliticalContinuityCatalog", "validate_political_continuity_catalog", &catalog);

  if (actual.content_digest.empty() ||
      !same_original_catalog_identity(catalog.founding_requirements.identity,
                                      actual) ||
      !same_original_catalog_identity(catalog.founding_institutions.identity,
                                      actual))
    return common_failure("FoundationCatalogIdentityMismatch");
  auto valid =
      validate_foundation_requirement_catalog(catalog.founding_requirements);
  if (!valid.ok)
    return valid;
  valid =
      validate_foundation_institution_catalog(catalog.founding_institutions);
  if (!valid.ok)
    return valid;
  // 章程实际可成立形态必须有同锁原要求；无章程的要求不凭空补一个模板。
  for (const auto &row : catalog.founding_institutions.constitutions.rows()) {
    auto requirement = read_original_founding_requirement(
        catalog.founding_requirements, actual, row.second.organization_kind);
    if (!requirement.ok)
      return {false, {}, requirement.error};
  }
  return common_success();
}
Outcome<PoliticalContinuityState> make_fresh_political_continuity_body(
    const CommonStore &common, const PoliticalContinuityCatalog &catalog,
    const CatalogIdentity &actual, const CurrentMomentRead &clock) {
  auto valid = validate_political_continuity_catalog(catalog, actual);
  if (!valid.ok)
    return {false, {}, valid.error};
  bool selected = false;
  for (const auto &module : actual.module_versions)
    if (module.first == political_continuity_module) {
      if (selected || module.second != "1")
        return {false,
                {},
                common_failure("FoundationContinuitySelectionMismatch").error};
      selected = true;
    }
  if (!selected)
    return {false,
            {},
            common_failure("FoundationContinuitySelectionMismatch").error};
  const auto &metadata = common.state().metadata;
  if (metadata.ruleset_id != actual.ruleset_id ||
      metadata.content_id != actual.content_id ||
      metadata.content_version != actual.content_version ||
      metadata.content_digest != actual.content_digest)
    return {
        false, {}, common_failure("FoundationCatalogIdentityMismatch").error};
  if (common.revision() != 0 || common.tick() != 0)
    return {false,
            {},
            common_failure("PoliticalContinuityFreshWorldRequired").error};
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto at_origin = compare_world_moments(now.value, WorldMoment{});
  if (!at_origin.ok)
    return {false, {}, at_origin.error};
  if (at_origin.value != 0)
    return {false,
            {},
            common_failure("PoliticalContinuityFreshMomentRequired").error};
  // 不将已有实际本人决定丢掉后重建空程序；其他域的真实初始安装事实可保留。
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "politics" &&
        (row.second.kind == foundation_intent_kind ||
         row.second.kind == foundation_support_kind))
      return {
          false,
          {},
          common_failure("PoliticalContinuityFreshCandidateRequired", row.first)
              .error};
  PoliticalContinuityState body;
  valid = validate_political_continuity_shape(body);
  return {valid.ok, std::move(body), valid.error};
}
} // namespace hanmo::resident
